#ifndef ESPNOW_CONTROL_H
#define ESPNOW_CONTROL_H

#include <Arduino.h>
#include <atomic>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <vector>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "output_control.h"

#define ESPNOW_DMX_CHUNK_SIZE 230

// Custom ESP-NOW packet layout
struct EspNowDmxPacket {
    char header[4];       // "DMX"
    uint16_t universe;
    uint16_t offset;
    uint16_t totalLength;
    uint16_t length;
    uint8_t data[ESPNOW_DMX_CHUNK_SIZE];
};

struct PeerRoute {
    uint8_t mac[6];
    uint16_t startUniverse = 0;
    uint16_t startAddress = 1;
    uint16_t endUniverse = 32767;
    uint16_t endAddress = 512;
};

#include <freertos/queue.h>

extern QueueHandle_t espNowQueue;

class EspNowControl;
extern EspNowControl espNowCtrl;

class EspNowControl {
private:
    bool initialized = false;
    esp_now_peer_info_t peerInfo;
    std::vector<PeerRoute> peerList;

    bool parseMacAddress(const char* macStr, uint8_t* macBytes);
    uint16_t clampDmxAddress(int value, uint16_t fallback);
    bool peerAllowsUniverse(const PeerRoute& peer, uint16_t universe, uint16_t& firstOffset, uint16_t& lastOffsetExclusive);
    static void onDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len);

public:
    std::atomic<bool> channelLocked{false};
    std::atomic<bool> reloadPeersPending{false};
    uint8_t currentScanChannel = 1;
    unsigned long lastChannelSwitchTime = 0;
    std::atomic<unsigned long> lastPacketRecvTime{0};

    static uint8_t rxDmxBuffer[512];
    static uint16_t rxDmxLength;
    static unsigned long lastRxTime;
    static std::atomic<bool> newRxData;

    void loadPeers();
    void loop();
    void begin();
    void sendDmx(uint16_t universe, const uint8_t* dmxData, uint16_t length);
};

extern EspNowControl espNowCtrl;

#endif // ESPNOW_CONTROL_H
