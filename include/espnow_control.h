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

public:
    std::atomic<bool> channelLocked{false};
    std::atomic<bool> reloadPeersPending{false};
    uint8_t currentScanChannel = 1;
    unsigned long lastChannelSwitchTime = 0;
    std::atomic<unsigned long> lastPacketRecvTime{0};

private:
    // Parser for "AA:BB:CC:DD:EE:FF" MAC addresses
    bool parseMacAddress(const char* macStr, uint8_t* macBytes) {
        int values[6];
        if (sscanf(macStr, "%x:%x:%x:%x:%x:%x", 
                   &values[0], &values[1], &values[2], 
                   &values[3], &values[4], &values[5]) == 6) {
            for (int i = 0; i < 6; ++i) {
                macBytes[i] = (uint8_t)values[i];
            }
            return true;
        }
        return false;
    }

    uint16_t clampDmxAddress(int value, uint16_t fallback) {
        if (value < 1 || value > 512) return fallback;
        return (uint16_t)value;
    }

    bool peerAllowsUniverse(const PeerRoute& peer, uint16_t universe, uint16_t& firstOffset, uint16_t& lastOffsetExclusive) {
        if (universe < peer.startUniverse || universe > peer.endUniverse) return false;

        uint16_t startCh = (universe == peer.startUniverse) ? peer.startAddress : 1;
        uint16_t endCh = (universe == peer.endUniverse) ? peer.endAddress : 512;
        if (endCh < startCh) return false;

        firstOffset = startCh - 1;
        lastOffsetExclusive = endCh;
        return firstOffset < lastOffsetExclusive;
    }

public:
    // Shared buffer for received wireless data
    static uint8_t rxDmxBuffer[512];
    static uint16_t rxDmxLength;
    static unsigned long lastRxTime;
    static std::atomic<bool> newRxData;

    void loadPeers() {
        if (sysCfg.device_mode != MODE_ESPNOW_MASTER) return;

        // Clear existing peers from ESP-NOW stack if any
        for (const auto& peer : peerList) {
            if (esp_now_is_peer_exist(peer.mac)) {
                esp_now_del_peer(peer.mac);
            }
        }
        peerList.clear();

        if (LittleFS.exists("/espnow_peers.json")) {
            File file = LittleFS.open("/espnow_peers.json", "r");
            if (file) {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, file);
                file.close();

                if (!error && doc["peers"].is<JsonArray>()) {
                    JsonArray arr = doc["peers"].as<JsonArray>();
                    for (JsonVariant v : arr) {
                        if (v.is<JsonObject>()) {
                            JsonObject obj = v.as<JsonObject>();
                            const char* mac = obj["mac"] | "";
                            PeerRoute peer;
                            if (parseMacAddress(mac, peer.mac)) {
                                peer.startUniverse = obj["start_universe"] | 0;
                                peer.startAddress = clampDmxAddress(obj["start_address"] | 1, 1);
                                peer.endUniverse = obj["end_universe"] | peer.startUniverse;
                                peer.endAddress = clampDmxAddress(obj["end_address"] | 512, 512);
                                if (peer.endUniverse < peer.startUniverse) peer.endUniverse = peer.startUniverse;
                                peerList.push_back(peer);
                            }
                        }
                    }
                }
            }
        }

        // Fallback to broadcast if empty
        if (peerList.empty()) {
            PeerRoute broadcast;
            memset(broadcast.mac, 0xFF, 6);
            peerList.push_back(broadcast);
            Serial.println("ESP-NOW Peer list empty. Using Broadcast FF:FF:FF:FF:FF:FF.");
        }

        // Register peers
        for (const auto& peer : peerList) {
            memset(&peerInfo, 0, sizeof(peerInfo));
            memcpy(peerInfo.peer_addr, peer.mac, 6);
            peerInfo.channel = 0; // Current Wi-Fi channel
            peerInfo.encrypt = false;
            
            if (!esp_now_is_peer_exist(peer.mac)) {
                if (esp_now_add_peer(&peerInfo) != ESP_OK) {
                    Serial.println("Failed to add ESP-NOW peer");
                }
            }
        }
        Serial.printf("ESP-NOW: Loaded %d peers\n", peerList.size());
    }

    void loop() {
        if (sysCfg.device_mode == MODE_ESPNOW_SLAVE && sysCfg.espnow_channel == 0) {
            unsigned long now = millis();
            if (!channelLocked) {
                if (now - lastChannelSwitchTime > 150) {
                    currentScanChannel++;
                    if (currentScanChannel > 13) currentScanChannel = 1;
                    
                    esp_wifi_set_channel(currentScanChannel, WIFI_SECOND_CHAN_NONE);
                    lastChannelSwitchTime = now;
                }
            } else {
                if (now - lastPacketRecvTime > 5000) {
                    channelLocked = false;
                    currentScanChannel = 1;
                    esp_wifi_set_channel(currentScanChannel, WIFI_SECOND_CHAN_NONE);
                    lastChannelSwitchTime = now;
                    lastPacketRecvTime = now;
                    Serial.println("ESP-NOW Slave: Lost contact with Master. Restarting channel scan...");
                }
            }
        }
    }

    void begin() {
        if (initialized) return;

        // ESP-NOW requires Wi-Fi in AP or STA mode
        if (WiFi.getMode() == WIFI_OFF) {
            WiFi.mode(WIFI_AP_STA);
        }

        if (esp_now_init() != ESP_OK) {
            Serial.println("Error initializing ESP-NOW");
            return;
        }
        
        initialized = true;
        Serial.println("ESP-NOW initialized successfully");

        // Setup send/receive callbacks depending on mode
        if (sysCfg.device_mode == MODE_ESPNOW_MASTER) {
            loadPeers();
            if (sysCfg.espnow_channel > 0) {
                esp_wifi_set_channel(sysCfg.espnow_channel, WIFI_SECOND_CHAN_NONE);
                Serial.printf("ESP-NOW Master: set Wi-Fi channel to %d\n", sysCfg.espnow_channel);
            }
        } else if (sysCfg.device_mode == MODE_ESPNOW_SLAVE) {
            esp_now_register_recv_cb(onDataRecv);
            Serial.println("ESP-NOW Slave callback registered");
            
            channelLocked = false;
            currentScanChannel = (sysCfg.espnow_channel > 0) ? sysCfg.espnow_channel : 1;
            lastChannelSwitchTime = millis();
            lastPacketRecvTime = millis();
            
            if (sysCfg.espnow_channel > 0) {
                esp_wifi_set_channel(sysCfg.espnow_channel, WIFI_SECOND_CHAN_NONE);
                Serial.printf("ESP-NOW Slave: locked on Wi-Fi channel %d\n", sysCfg.espnow_channel);
            } else {
                esp_wifi_set_channel(currentScanChannel, WIFI_SECOND_CHAN_NONE);
                Serial.printf("ESP-NOW Slave: auto-scan started on channel %d\n", currentScanChannel);
            }
        }
    }

    void sendDmx(uint16_t universe, const uint8_t* dmxData, uint16_t length) {
        if (!initialized || sysCfg.device_mode != MODE_ESPNOW_MASTER) return;

        uint16_t totalLength = length > 512 ? 512 : length;
        for (const auto& peer : peerList) {
            uint16_t firstOffset = 0;
            uint16_t lastOffsetExclusive = 0;
            if (!peerAllowsUniverse(peer, universe, firstOffset, lastOffsetExclusive)) continue;
            if (firstOffset >= totalLength) continue;
            if (lastOffsetExclusive > totalLength) lastOffsetExclusive = totalLength;

            uint16_t chunkSize = sysCfg.espnow_chunk_size;
            if (chunkSize < 16 || chunkSize > ESPNOW_DMX_CHUNK_SIZE) chunkSize = 200;

            for (uint16_t offset = firstOffset; offset < lastOffsetExclusive; offset += chunkSize) {
                EspNowDmxPacket packet;
                memcpy(packet.header, "DMX", 4);
                packet.universe = universe;
                packet.offset = offset;
                packet.totalLength = 512;
                packet.length = min((uint16_t)chunkSize, (uint16_t)(lastOffsetExclusive - offset));
                memcpy(packet.data, dmxData + offset, packet.length);

                size_t packetSize = sizeof(packet.header) + sizeof(packet.universe) +
                                    sizeof(packet.offset) + sizeof(packet.totalLength) +
                                    sizeof(packet.length) + packet.length;

                esp_err_t result = esp_now_send(peer.mac, (uint8_t*)&packet, packetSize);
                if (result != ESP_OK) {
                    // Log ESP-NOW errors occasionally to prevent serial clogging
                    static unsigned long lastErr = 0;
                    if (millis() - lastErr > 5000) {
                        Serial.println("ESP-NOW send error");
                        lastErr = millis();
                    }
                }
            }
        }
    }

private:
    static void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
        size_t minHeaderLen = 4 + 2 + 2 + 2 + 2; // header(4) + universe(2) + offset(2) + totalLength(2) + length(2)
        if (len < (int)minHeaderLen) {
            return;
        }

        // Copy safely into aligned local struct to prevent Xtensa alignment exception
        EspNowDmxPacket packet;
        memcpy(packet.header, incomingData, 4);
        memcpy(&packet.universe, incomingData + 4, 2);
        memcpy(&packet.offset, incomingData + 6, 2);
        memcpy(&packet.totalLength, incomingData + 8, 2);
        memcpy(&packet.length, incomingData + 10, 2);

        if (strncmp(packet.header, "DMX", 3) == 0) {
            if (packet.offset >= 512) return;
            uint16_t available = 512 - packet.offset;
            packet.length = packet.length > available ? available : packet.length;
            if (packet.length > ESPNOW_DMX_CHUNK_SIZE) return;
            if ((int)(minHeaderLen + packet.length) > len) return;

            memcpy(packet.data, incomingData + minHeaderLen, packet.length);

            // Channel lock logic for Auto-Scan mode on Slave
            if (sysCfg.device_mode == MODE_ESPNOW_SLAVE && sysCfg.espnow_channel == 0) {
                if (!espNowCtrl.channelLocked) {
                    uint8_t primaryChan = 0;
                    wifi_second_chan_t secondChan;
                    if (esp_wifi_get_channel(&primaryChan, &secondChan) == ESP_OK) {
                        espNowCtrl.channelLocked = true;
                        espNowCtrl.currentScanChannel = primaryChan;
                        Serial.printf("ESP-NOW Slave: Locked onto Master channel %d\n", primaryChan);
                    }
                }
                espNowCtrl.lastPacketRecvTime = millis();
            }

            // Queue for thread-safe processing on Core 1 loop
            if (espNowQueue != nullptr) {
                if (xQueueSend(espNowQueue, &packet, 0) != pdTRUE) {
                    static uint32_t lastDropLog = 0;
                    uint32_t now = millis();
                    if (now - lastDropLog > 5000) {
                        lastDropLog = now;
                        Serial.println("ESP-NOW: queue full, packet dropped");
                    }
                }
            }
        }
    }
};

extern EspNowControl espNowCtrl;

#endif // ESPNOW_CONTROL_H
