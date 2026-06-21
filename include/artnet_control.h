#ifndef ARTNET_CONTROL_H
#define ARTNET_CONTROL_H

#include <Arduino.h>
#include <atomic>
#include <WiFiUdp.h>
#include <vector>
#include <algorithm>
#include "config.h"
#include "output_control.h"
#include "espnow_control.h"

// Art-Net packet offsets
#define ARTNET_HEADER_SIZE 18

class ArtNetControl {
private:
    WiFiUDP udp;
    bool initialized = false;
    uint8_t lastSequence[4] = {0}; // track sequence per universe (up to 4)

    // Buffer for sending ArtPollReply
    struct ArtPollReplyPacket {
        char id[8];
        uint16_t opCode;
        uint8_t ip[4];
        uint16_t port;
        uint16_t ver;
        uint8_t net;
        uint8_t sub;
        uint16_t oem;
        uint8_t ubea;
        uint8_t status1;
        uint16_t esta;
        char shortName[18];
        char longName[64];
        char nodeReport[64];
        uint16_t numPorts;
        uint8_t portTypes[4];
        uint8_t goodInput[4];
        uint8_t goodOutput[4];
        uint8_t swIn[4];
        uint8_t swOut[4];
        uint8_t swVideo;
        uint8_t swMacro;
        uint8_t swRemote;
        uint8_t spare[3];
        uint8_t style;
        uint8_t mac[6];
        uint8_t bindIp[4];
        uint8_t bindIndex;
        uint8_t status2;
        uint8_t filler[26];
    } __attribute__((packed));

public:
    static unsigned long packetCount;
    static std::atomic<bool> newRxData;

    void begin() {
        if (initialized) return;
        udp.begin(sysCfg.artnet_port);
        initialized = true;
        Serial.printf("Art-Net UDP listener initialized on port %u\n", sysCfg.artnet_port);
    }

    void loop() {
        if (!initialized) return;

        int packetSize = udp.parsePacket();
        if (packetSize) {
            uint8_t packetBuffer[768];
            int readBytes = udp.read(packetBuffer, sizeof(packetBuffer));
            if (readBytes < 12) return;

            // Check Art-Net Header "Art-Net\0"
            if (memcmp(packetBuffer, "Art-Net\0", 8) != 0) return;

            // Extract OpCode (Little Endian)
            uint16_t opCode = packetBuffer[8] | (packetBuffer[9] << 8);

            if (opCode == 0x5000) { // OpDmx
                parseArtDmx(packetBuffer, readBytes);
            } else if (opCode == 0x2000) { // OpPoll
                sendArtPollReply();
            }
        }
    }

private:
    void parseArtDmx(const uint8_t* buf, int size) {
        if (size < ARTNET_HEADER_SIZE) return;

        // Protocol version check (Art-Net 4 requires >= 14)
        uint16_t protVer = buf[10] | (buf[11] << 8);
        if (protVer < 14) return;
        (void)protVer;

        // Universe (Little Endian, low-byte first)
        uint16_t universe = buf[14] | (buf[15] << 8);

        // Length (Big Endian, high-byte first)
        uint16_t length = (buf[16] << 8) | buf[17];
        
        if (size < ARTNET_HEADER_SIZE + length) return;

        bool matched = outputCtrl.mapDmxDataToChannels(universe, buf + ARTNET_HEADER_SIZE, length);

        if (matched) {
            lastDmxUpdateTime = millis();
            packetCount++;
            newRxData = true;
            networkFramePending = true;
            systemActive = true; // Flag system as receiving network data
            
            // Forward wirelessly in ESP-NOW Master mode
            if (sysCfg.device_mode == MODE_ESPNOW_MASTER) {
                espNowCtrl.sendDmx(universe, buf + ARTNET_HEADER_SIZE, length);
            }
        }
    }

    void sendArtPollReply() {
        ArtPollReplyPacket reply;
        memset(&reply, 0, sizeof(reply));

        memcpy(reply.id, "Art-Net\0", 8);
        reply.opCode = 0x2100; // OpPollReply

        // Set IP address
        IPAddress localIp = WiFi.localIP();
        #if defined(ETH_PHY_TYPE)
        if (ETH.localIP() != IPAddress(0,0,0,0)) {
            localIp = ETH.localIP();
        }
        #endif

        for (int i = 0; i < 4; i++) {
            reply.ip[i] = localIp[i];
            reply.bindIp[i] = localIp[i];
        }

        reply.port = sysCfg.artnet_port;
        reply.ver = 14;
        reply.oem = 0x00FF; // Generic development board OEM code
        reply.status1 = 0xD2; // Indicator state

        String macStr = WiFi.macAddress();
        macStr.replace(":", "");
        String suffix = macStr.substring(macStr.length() - 4);
        String shortName = String("CHAL Node-") + suffix;
        String longName = String("CHAL WT32-ETH01 Converter - ") + suffix;

        strncpy(reply.shortName, shortName.c_str(), sizeof(reply.shortName) - 1);
        reply.shortName[sizeof(reply.shortName) - 1] = '\0';
        strncpy(reply.longName, longName.c_str(), sizeof(reply.longName) - 1);
        reply.longName[sizeof(reply.longName) - 1] = '\0';
        strncpy(reply.nodeReport, "#0001 [OK] System healthy and ready.", sizeof(reply.nodeReport));

        // Build unique active universes list from dynamic channels
        std::vector<uint16_t> activeUniverses;
        
        for (const auto& ch : outputCtrl.getChannels()) {
            uint16_t numUniverses = outputCtrl.getUniverseCount(ch);
            for (uint16_t offset = 0; offset < numUniverses; offset++) {
                uint16_t u = ch.start_universe + offset;
                if (std::find(activeUniverses.begin(), activeUniverses.end(), u) == activeUniverses.end()) {
                    activeUniverses.push_back(u);
                }
            }
        }

        reply.numPorts = activeUniverses.size() > 4 ? 4 : activeUniverses.size(); // Art-Net limits description to 4 ports
        for (int i = 0; i < reply.numPorts; i++) {
            reply.portTypes[i] = 0x80; // Output port DMX512
            reply.goodOutput[i] = 0x80;
            reply.swOut[i] = activeUniverses[i] & 0xFF;
        }

        reply.style = 0; // StNode
        
        uint8_t mac[6];
        WiFi.macAddress(mac);
        memcpy(reply.mac, mac, 6);

        reply.status2 = 0x08; // Node supports DHCP/Static config

        udp.beginPacket(udp.remoteIP(), sysCfg.artnet_port);
        udp.write((const uint8_t*)&reply, sizeof(reply));
        udp.endPacket();
    }
};

extern ArtNetControl artNetCtrl;

#endif // ARTNET_CONTROL_H
