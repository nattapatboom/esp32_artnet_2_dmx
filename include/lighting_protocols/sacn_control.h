#ifndef LIGHTING_PROTOCOLS_SACN_CONTROL_H
#define LIGHTING_PROTOCOLS_SACN_CONTROL_H

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

    uint16_t readUint16BE(const uint8_t* buf, int offset);
    uint32_t readUint32BE(const uint8_t* buf, int offset);
    bool validatePduFlagsLength(const uint8_t* buf, int len, int offset);
    bool validatePacket(const uint8_t* buf, int len);
    bool shouldAcceptSource(const uint8_t* cid, uint8_t priority);
    void expireSources();
    void leaveAllMulticast();
    void joinMulticast(uint16_t universe);
    void reloadMulticastGroups();

public:
    void begin(bool useMulticast = false);
    void process();

    uint32_t getPacketCount() const { return packetCount; }
    uint32_t getErrorCount() const { return errorCount; }

    uint16_t getActiveUniverses() const;
};

extern SACNControl sacnCtrl;

#endif // LIGHTING_PROTOCOLS_SACN_CONTROL_H
