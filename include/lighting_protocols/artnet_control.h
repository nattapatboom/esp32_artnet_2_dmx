#ifndef LIGHTING_PROTOCOLS_ARTNET_CONTROL_H
#define LIGHTING_PROTOCOLS_ARTNET_CONTROL_H

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

    void parseArtDmx(const uint8_t* buf, int size);
    void sendArtPollReply();

public:
    static unsigned long packetCount;

    void begin();
    void loop();
};

extern ArtNetControl artNetCtrl;

#endif // LIGHTING_PROTOCOLS_ARTNET_CONTROL_H
