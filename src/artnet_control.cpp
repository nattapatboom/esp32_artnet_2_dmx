#include "lighting_protocols/artnet_control.h"
#include <ETH.h>

unsigned long ArtNetControl::packetCount = 0;

void ArtNetControl::begin() {
    if (initialized) return;
    udp.begin(sysCfg.artnet_port);
    initialized = true;
    Serial.printf("Art-Net UDP listener initialized on port %u\n", sysCfg.artnet_port);
}

void ArtNetControl::loop() {
    if (!initialized) return;

    int packetSize = udp.parsePacket();
    if (packetSize) {
        uint8_t packetBuffer[768];
        int readBytes = udp.read(packetBuffer, sizeof(packetBuffer));
        if (readBytes < 12) return;

        if (memcmp(packetBuffer, "Art-Net\0", 8) != 0) return;

        uint16_t opCode = packetBuffer[8] | (packetBuffer[9] << 8);

        if (opCode == 0x5000) {
            parseArtDmx(packetBuffer, readBytes);
        } else if (opCode == 0x2000) {
            sendArtPollReply();
        }
    }
}

void ArtNetControl::parseArtDmx(const uint8_t* buf, int size) {
    if (size < ARTNET_HEADER_SIZE) return;

    if (((uint16_t)buf[10] << 8 | buf[11]) < 14) return;

    uint16_t universe = buf[14] | (buf[15] << 8);
    uint16_t length = (buf[16] << 8) | buf[17];

    if (size < ARTNET_HEADER_SIZE + length) return;

    bool matched = outputCtrl.mapDmxDataToChannels(universe, buf + ARTNET_HEADER_SIZE, length);
    if (matched) outputCtrl.swapBuffers();

    if (matched) {
        lastDmxUpdateTime = millis();
        packetCount++;
        networkFramePending = true;
        systemActive = true;

        if (sysCfg.device_mode == MODE_ESPNOW_MASTER) {
            espNowCtrl.sendDmx(universe, buf + ARTNET_HEADER_SIZE, length);
        }
    }
}

void ArtNetControl::sendArtPollReply() {
    ArtPollReplyPacket reply;
    memset(&reply, 0, sizeof(reply));

    memcpy(reply.id, "Art-Net\0", 8);
    reply.opCode = 0x2100;

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
    reply.oem = 0x00FF;
    reply.status1 = 0xD2;

    String macStr = WiFi.macAddress();
    macStr.replace(":", "");
    String suffix = macStr.substring(macStr.length() - 4);
    String shortName = String(sysCfg.artnet_short_name) + suffix;
    String longName = String(sysCfg.artnet_long_name) + suffix;

    strncpy(reply.shortName, shortName.c_str(), sizeof(reply.shortName) - 1);
    reply.shortName[sizeof(reply.shortName) - 1] = '\0';
    strncpy(reply.longName, longName.c_str(), sizeof(reply.longName) - 1);
    reply.longName[sizeof(reply.longName) - 1] = '\0';
    strncpy(reply.nodeReport, "#0001 [OK] System healthy and ready.", sizeof(reply.nodeReport));

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

    reply.numPorts = activeUniverses.size() > 4 ? 4 : activeUniverses.size();
    for (int i = 0; i < reply.numPorts; i++) {
        reply.portTypes[i] = 0x80;
        reply.goodOutput[i] = 0x80;
        reply.swOut[i] = activeUniverses[i] & 0xFF;
    }

    reply.style = 0;

    uint8_t mac[6];
    WiFi.macAddress(mac);
    memcpy(reply.mac, mac, 6);

    reply.status2 = 0x08;

    udp.beginPacket(udp.remoteIP(), sysCfg.artnet_port);
    udp.write((const uint8_t*)&reply, sizeof(reply));
    udp.endPacket();
}
