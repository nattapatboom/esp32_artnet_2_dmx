#ifndef SACN_CONTROL_H
#define SACN_CONTROL_H

/*
 * sacn_control.h - sACN (ANSI E1.31) Protocol Receiver
 *
 * Supports:
 *   - Unicast UDP (default port 5568, configurable)
 *   - Multicast UDP (239.255.X.Y where X.Y = universe number)
 *   - Multiple universes -> OutputChannel.dmxBuffer (shared with Art-Net)
 *   - Priority-based source selection
 */

#include <Arduino.h>
#include <WiFiUdp.h>
#include "output_control.h"
#include "config.h"

#define SACN_PACKET_SIZE       638
#define SACN_MAX_SOURCES       4
#define SACN_SOURCE_TIMEOUT_MS 2500

#define SACN_ROOT_VECTOR       18
#define SACN_CID               22
#define SACN_FRAMING_VECTOR    40
#define SACN_PRIORITY          108
#define SACN_UNIVERSE          113
#define SACN_DMP_VECTOR        117
#define SACN_DMP_ADDR_TYPE     118
#define SACN_DMP_PROP_COUNT    123
#define SACN_DMP_DATA          125

static const uint32_t SACN_ROOT_VECTOR_VAL = 0x00000004;
static const uint32_t SACN_FRAMING_VECTOR_VAL = 0x00000002;
static const uint8_t SACN_DMP_VECTOR_VAL = 0x02;

struct SacnSource {
    uint8_t cid[16];
    uint8_t priority;
    uint32_t lastSeen;
    bool active;
};

class SACNControl {
private:
    WiFiUDP udp;
    std::vector<WiFiUDP*> mcastInstances;
    uint8_t rxBuf[SACN_PACKET_SIZE];
    bool initialized = false;
    bool multicastMode = false;

    uint32_t packetCount = 0;
    uint32_t errorCount = 0;

    SacnSource sources[SACN_MAX_SOURCES];

    uint16_t readUint16BE(const uint8_t* buf, int offset) {
        return ((uint16_t)buf[offset] << 8) | buf[offset + 1];
    }

    uint32_t readUint32BE(const uint8_t* buf, int offset) {
        return ((uint32_t)buf[offset] << 24) |
               ((uint32_t)buf[offset + 1] << 16) |
               ((uint32_t)buf[offset + 2] << 8) |
               (uint32_t)buf[offset + 3];
    }

    bool validatePduFlagsLength(const uint8_t* buf, int len, int offset) {
        if (offset + 2 > len) return false;
        uint16_t flagsLength = readUint16BE(buf, offset);
        if ((flagsLength & 0xF000) != 0x7000) return false;
        uint16_t pduLength = flagsLength & 0x0FFF;
        return pduLength >= 2 && offset + pduLength <= len;
    }

    bool validatePacket(const uint8_t* buf, int len) {
        if (len < 126) return false;
        if (!validatePduFlagsLength(buf, len, 16)) return false;
        if (!validatePduFlagsLength(buf, len, 38)) return false;
        if (!validatePduFlagsLength(buf, len, 115)) return false;
        if (readUint32BE(buf, SACN_ROOT_VECTOR) != SACN_ROOT_VECTOR_VAL) return false;
        if (readUint32BE(buf, SACN_FRAMING_VECTOR) != SACN_FRAMING_VECTOR_VAL) return false;
        if (buf[SACN_DMP_VECTOR] != SACN_DMP_VECTOR_VAL) return false;
        if (buf[SACN_DMP_ADDR_TYPE] != 0xa1) return false;
        if (buf[SACN_DMP_DATA] != 0x00) return false;
        return true;
    }

    bool shouldAcceptSource(const uint8_t* cid, uint8_t priority) {
        int freeSlot = -1;
        int existingSlot = -1;

        for (int i = 0; i < SACN_MAX_SOURCES; i++) {
            if (!sources[i].active) {
                if (freeSlot < 0) freeSlot = i;
                continue;
            }

            if (memcmp(sources[i].cid, cid, 16) == 0) {
                existingSlot = i;
                sources[i].priority = priority;
                sources[i].lastSeen = millis();
            }
        }

        if (existingSlot < 0 && freeSlot >= 0) {
            memcpy(sources[freeSlot].cid, cid, 16);
            sources[freeSlot].priority = priority;
            sources[freeSlot].lastSeen = millis();
            sources[freeSlot].active = true;
            existingSlot = freeSlot;
        }

        uint8_t maxPriority = 0;
        for (int i = 0; i < SACN_MAX_SOURCES; i++) {
            if (sources[i].active && sources[i].priority > maxPriority) maxPriority = sources[i].priority;
        }

        if (existingSlot >= 0) return priority >= maxPriority;

        if (priority >= maxPriority) {
            if (freeSlot >= 0) {
                memcpy(sources[freeSlot].cid, cid, 16);
                sources[freeSlot].priority = priority;
                sources[freeSlot].lastSeen = millis();
                sources[freeSlot].active = true;
            }
            return true;
        }
        return false;
    }

    void expireSources() {
        uint32_t now = millis();
        for (int i = 0; i < SACN_MAX_SOURCES; i++) {
            if (sources[i].active && (now - sources[i].lastSeen) > SACN_SOURCE_TIMEOUT_MS) {
                sources[i].active = false;
                Serial.printf("[sACN] Source expired (slot %d)\n", i);
            }
        }
    }

    void leaveAllMulticast() {
        for (auto* m : mcastInstances) {
            m->stop();
            delete m;
        }
        mcastInstances.clear();
    }

    void joinMulticast(uint16_t universe) {
        WiFiUDP* m = new WiFiUDP();
        uint8_t hi = (universe >> 8) & 0xFF;
        uint8_t lo = universe & 0xFF;
        IPAddress mcast(239, 255, hi, lo);
        m->beginMulticast(mcast, sysCfg.sacn_port);
        mcastInstances.push_back(m);
        Serial.printf("[sACN] Joined multicast 239.255.%d.%d for universe %d on port %u\n", hi, lo, universe, sysCfg.sacn_port);
    }

    void reloadMulticastGroups() {
        leaveAllMulticast();
        for (auto& ch : outputCtrl.getChannels()) {
            for (uint16_t u = 0; u < outputCtrl.getUniverseCount(ch); u++) {
                joinMulticast(ch.start_universe + u);
            }
        }
    }

public:
    void begin(bool useMulticast = false) {
        if (initialized) return;
        multicastMode = useMulticast;
        memset(sources, 0, sizeof(sources));

        udp.begin(sysCfg.sacn_port);

        if (useMulticast) {
            for (auto& ch : outputCtrl.getChannels()) {
                for (uint16_t u = 0; u < outputCtrl.getUniverseCount(ch); u++) {
                    joinMulticast(ch.start_universe + u);
                }
            }
        }

        initialized = true;
        Serial.printf("[sACN] Initialized (%s mode, port %u, %u multicast groups)\n",
                      useMulticast ? "multicast" : "unicast", sysCfg.sacn_port,
                      useMulticast ? (unsigned)mcastInstances.size() : 0);
    }

    void process() {
        if (!initialized) return;

        static uint32_t lastExpire = 0;
        if (millis() - lastExpire > 1000) {
            expireSources();
            lastExpire = millis();
        }

        WiFiUDP* src = &udp;
        int len = udp.parsePacket();
        if (len <= 0) {
            for (auto* m : mcastInstances) {
                len = m->parsePacket();
                if (len > 0) { src = m; break; }
            }
        }
        if (len <= 0) return;

        if (len > SACN_PACKET_SIZE) len = SACN_PACKET_SIZE;
        src->read(rxBuf, len);

        if (!validatePacket(rxBuf, len)) {
            errorCount++;
            return;
        }

        packetCount++;

        uint8_t priority = rxBuf[SACN_PRIORITY];
        uint16_t universe = readUint16BE(rxBuf, SACN_UNIVERSE);
        const uint8_t* cid = rxBuf + SACN_CID;

        if (!shouldAcceptSource(cid, priority)) return;

        const uint8_t* dmxData = rxBuf + SACN_DMP_DATA + 1;
        uint16_t dmxLen = readUint16BE(rxBuf, SACN_DMP_PROP_COUNT);
        if (dmxLen == 0 || SACN_DMP_DATA + dmxLen > len) {
            errorCount++;
            return;
        }
        dmxLen -= 1;
        if (dmxLen > 512) dmxLen = 512;

        if (outputCtrl.mapDmxDataToChannels(universe, dmxData, dmxLen)) {
            outputCtrl.swapBuffers();
            systemActive = true;
            lastDmxUpdateTime = millis();
            networkFramePending = true;
        }
    }

    uint32_t getPacketCount() const { return packetCount; }
    uint32_t getErrorCount() const { return errorCount; }

    uint16_t getActiveUniverses() const {
        uint16_t count = 0;
        for (int i = 0; i < SACN_MAX_SOURCES; i++) {
            if (sources[i].active) count++;
        }
        return count;
    }
};

extern SACNControl sacnCtrl;

#endif // SACN_CONTROL_H
