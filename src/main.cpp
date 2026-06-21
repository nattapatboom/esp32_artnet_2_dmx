#include <Arduino.h>
#include <atomic>
#include <ETH.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <Wire.h>

#include "config.h"
#include "web_pages.h"
#include "pca9685_control.h"
#include "i2c_gpio_expander.h"
#include "output_control.h"
#include "dimmer_control.h"
#include "motion_control.h"
#include "espnow_control.h"
#include "artnet_control.h"
#include "sacn_control.h"
#include "display_driver.h"
#include "scoring.h"

const char* getFirmwareVersion() {
    static char version[32] = {0};
    if (version[0] != '\0') return version;

    const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon[4] = {0};
    int day = 1;
    int year = 1970;
    int hour = 0;
    int minute = 0;
    int second = 0;
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

    const char* pos = strstr(months, mon);
    int month = pos ? ((pos - months) / 3 + 1) : 1;
    snprintf(version, sizeof(version), "firmware_%04d%02d%02d_%02d%02d%02d",
             year, month, day, hour, minute, second);
    return version;
}

// Instantiate Global Control Instances
SystemConfig sysCfg;
uint8_t activeDmxBuffer[DMX_BUFFER_SIZE] = {0};
unsigned long lastDmxUpdateTime = 0;
bool systemActive = false;
std::atomic<bool> networkFramePending(false);

OutputControl outputCtrl;
ArtNetControl artNetCtrl;
SACNControl sacnCtrl;
EspNowControl espNowCtrl;
MotionControl motionCtrl;
DimmerControl dimmerCtrl;
hw_timer_t *dimmerTimer = NULL;
volatile uint32_t dimmer_tick = 0;
DimmerCh dimmerChannels[MAX_DIMMER_CHANNELS];
volatile uint8_t numDimmerChannels = 0;
volatile bool dimmerEnabled = false;

PCA9685Manager pcaManager;
DigitalExpanderManager digitalExpanderManager;

void updateMotionControl() {
    motionCtrl.update();
}

// Static member definitions (out-of-class, C++14 compatible)
unsigned long ArtNetControl::packetCount = 0;
std::atomic<bool> ArtNetControl::newRxData(false);
uint8_t EspNowControl::rxDmxBuffer[512] = {0};
uint16_t EspNowControl::rxDmxLength = 0;
unsigned long EspNowControl::lastRxTime = 0;
std::atomic<bool> EspNowControl::newRxData(false);

AsyncWebServer server(80);

// Network state trackers
bool ethConnected = false;
unsigned long apStartTime = 0;
bool apActive = false;
bool otaUpdateOk = false;
String otaUpdateError;
volatile bool otaInProgress = false;
unsigned long lastWifiReconnectAttempt = 0;
SemaphoreHandle_t i2cMutex = NULL;
SemaphoreHandle_t i2cScanMutex = NULL;
String i2cScanJson = "[]";
volatile bool i2cScanRequested = false;
volatile bool i2cScanPending = false;
QueueHandle_t espNowQueue = NULL;
DisplayDriver display;
volatile bool outputTestActive = false;
volatile bool outputTestClearRequested = false;
int outputTestIndex = -1;
unsigned long outputTestUntil = 0;

// Network functions forward declarations
void startAP();
void stopAP();
void setupNetwork();
void setupWebServer();
void startWiFiClient(bool keepApActive = false);
void startRecoveryEthernetOnly();

void configureWiFiRadioForBoot() {
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
}

void resetEthernetPhy() {
    pinMode(ETH_PHY_POWER, OUTPUT);
    digitalWrite(ETH_PHY_POWER, LOW);
    delay(150);
    digitalWrite(ETH_PHY_POWER, HIGH);
    delay(300);
}

void powerDownEthernetPhy() {
    pinMode(ETH_PHY_POWER, OUTPUT);
    digitalWrite(ETH_PHY_POWER, LOW);
    delay(150);
}

void blinkStatusLed(uint8_t times, uint16_t onMs, uint16_t offMs) {
    if (sysCfg.status_led_pin == 255) return;
    pinMode(sysCfg.status_led_pin, OUTPUT);
    for (uint8_t i = 0; i < times; i++) {
        digitalWrite(sysCfg.status_led_pin, HIGH);
        delay(onMs);
        digitalWrite(sysCfg.status_led_pin, LOW);
        delay(offMs);
    }
}

void clearOutputTest() {
    for (auto& ch : outputCtrl.getChannels()) {
        if (ch.dmxBuffer != nullptr && ch.bufferSize > 0) {
            memset(ch.dmxBuffer, 0, ch.bufferSize);
        }
    }
    outputTestActive = false;
    outputTestClearRequested = false;
    outputTestIndex = -1;
    outputTestUntil = 0;
    systemActive = false;
    networkFramePending = false;
    outputCtrl.updateLeds();
    motionCtrl.update();
}

void refreshTestOutputNow() {
    lastDmxUpdateTime = millis();
    systemActive = true;
    networkFramePending = true;
    outputCtrl.updateLeds();
    motionCtrl.update();
}

void updateStatusLedPattern() {
    if (sysCfg.status_led_pin == 255) return;

    struct StatusPattern {
        uint8_t pulses;
        uint16_t onMs;
        uint16_t offMs;
        uint16_t pauseMs;
    };

    StatusPattern pattern;
    if (outputTestActive) {
        pattern = {6, 80, 120, 1200};   // Test override active
    } else if (apActive) {
        pattern = {2, 120, 180, 1200};  // Config AP active
    } else if (sysCfg.device_mode == MODE_ESPNOW_SLAVE && (millis() - EspNowControl::lastRxTime < 2000)) {
        pattern = {5, 70, 90, 900};     // ESP-NOW slave receiving
    } else if (ethConnected) {
        pattern = {3, 120, 180, 1200};  // Ethernet connected
    } else if (WiFi.isConnected()) {
        pattern = {4, 100, 150, 1200};  // Wi-Fi STA connected
    } else {
        pattern = {1, 80, 120, 2200};   // Idle / disconnected
    }

    uint32_t cycleMs = (uint32_t)pattern.pulses * (pattern.onMs + pattern.offMs) + pattern.pauseMs;
    uint32_t t = millis() % cycleMs;
    uint32_t activeMs = (uint32_t)pattern.pulses * (pattern.onMs + pattern.offMs);
    bool ledOn = false;
    if (t < activeMs) {
        uint32_t step = t % (pattern.onMs + pattern.offMs);
        ledOn = step < pattern.onMs;
    }

    pinMode(sysCfg.status_led_pin, OUTPUT);
    digitalWrite(sysCfg.status_led_pin, ledOn ? HIGH : LOW);
}

void restartAfterOtaTask(void* pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(2500));
    blinkStatusLed(10, 150, 150);
    WiFi.disconnect(true, true);
    WiFi.softAPdisconnect(true);
    powerDownEthernetPhy();
    ESP.restart();
    vTaskDelete(NULL);
}

bool wifiClientConfigured() {
    if (strlen(sysCfg.wifi_ssid) == 0) return false;
    if (sysCfg.device_mode == MODE_ESPNOW_MASTER) return true;
    return sysCfg.device_mode == MODE_ARTNET_ETHERNET && sysCfg.wifi_enable_in_eth_mode;
}

void reconnectWiFiClient(bool force = false) {
    if (!wifiClientConfigured()) return;
    if (WiFi.isConnected()) return;
    if (sysCfg.device_mode == MODE_ARTNET_ETHERNET && (ethConnected || ETH.linkUp())) return;

    unsigned long now = millis();
    if (!force && now - lastWifiReconnectAttempt < 10000) return;
    lastWifiReconnectAttempt = now;

    wl_status_t status = WiFi.status();
    Serial.printf("Wi-Fi reconnect attempt. Current status=%d, SSID=%s\n", status, sysCfg.wifi_ssid);

    startWiFiClient(apActive);
}

void copyConfigString(char* dest, size_t destSize, const char* src) {
    if (dest == nullptr || destSize == 0) return;
    if (src == nullptr) src = "";
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}

bool collectRequestBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total, String& body) {
    if (total > 65536) { // Reject requests larger than 64KB
        request->send(413, "application/json", "{\"status\":\"error\",\"message\":\"Payload too large\"}");
        return false;
    }
    if (index == 0) {
        if (request->_tempObject != nullptr) {
            delete static_cast<String*>(request->_tempObject);
        }
        String* buffer = new String();
        if (total > 0) buffer->reserve(total);
        request->_tempObject = buffer;
    }

    String* buffer = static_cast<String*>(request->_tempObject);
    if (buffer == nullptr) {
        request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to allocate request buffer\"}");
        return false;
    }

    buffer->concat((const char*)data, len);
    if (index + len < total) return false;

    body = *buffer;
    delete buffer;
    request->_tempObject = nullptr;
    return true;
}

bool jsonPinMatches(JsonVariantConst value, uint8_t reservedPin) {
    int pin = value | 255;
    return reservedPin != 255 && pin != 255 && pin == reservedPin;
}

bool sourceAddressValid(uint8_t source, uint8_t address) {
    switch (source) {
        case 1: return address >= 0x40 && address <= 0x47; // PCA9685
        case 2: return address >= 0x20 && address <= 0x27; // MCP23017
        case 3: return address >= 0x20 && address <= 0x27; // TCA9555
        case 4: return (address >= 0x20 && address <= 0x27) || (address >= 0x38 && address <= 0x3F); // PCF857x / PCF8574A
        case 5: return (address >= 0x4C && address <= 0x5B) || address == 0x60 || address == 0x61; // I2C DAC family
        default: return source == 0;
    }
}

const char* sourceAddressRangeLabel(uint8_t source) {
    switch (source) {
        case 1: return "PCA9685 address must be 0x40-0x47";
        case 2: return "MCP23017 address must be 0x20-0x27";
        case 3: return "TCA9555 address must be 0x20-0x27";
        case 4: return "PCF857x address must be 0x20-0x27 or 0x38-0x3F";
        case 5: return "I2C DAC address must match the selected DAC model";
        default: return "Unsupported I2C source";
    }
}

bool validateSourceAddress(uint8_t source, uint8_t address, const String& label, String& message) {
    if (source == 0) return true;
    if (sourceAddressValid(source, address)) return true;
    message = label + " has invalid I2C address 0x" + String(address, HEX) + ". " + sourceAddressRangeLabel(source);
    return false;
}

bool displayAddressValid(uint8_t displayType, uint8_t address) {
    if (displayType == 0) return true;
    if (displayType == 1 || displayType == 2) return address == 0x3C || address == 0x3D;
    if (displayType == 3) return address == 0x27 || address == 0x3F;
    return false;
}

bool dacAddressValid(uint8_t model, uint8_t address) {
    if (model == 0) return address == 0x60 || address == 0x61; // MCP4725
    if (model == 1) return address == 0x4C || address == 0x4D; // DAC7571
    if (model == 2) return address >= 0x4C && address <= 0x5B; // DAC7573
    return false;
}

const char* dacAddressRangeLabel(uint8_t model) {
    if (model == 0) return "MCP4725 address must be 0x60 or 0x61";
    if (model == 1) return "DAC7571 address must be 0x4C or 0x4D";
    if (model == 2) return "DAC7573 address must be 0x4C-0x5B";
    return "Unsupported I2C DAC model";
}

bool outputJsonUsesPin(JsonObjectConst output, uint8_t reservedPin) {
    if (reservedPin == 255) return false;
    uint8_t source = output["source"] | 0;
    uint8_t type = output["type"] | 0;
    uint8_t mcMode = output["mc_mode"] | 0;
    uint8_t homingMode = output["mc_homing_mode"] | 0;
    uint8_t pin2Source = output["pin2_source"] | 0;
    uint8_t pin3Source = output["pin3_source"] | 0;
    uint8_t pin4Source = output["pin4_source"] | 0;
    uint8_t colorOrder = output["color_order"] | 0;

    if (source == 0 && jsonPinMatches(output["pin"], reservedPin)) return true;

    if (type == 12 || type == 13) {
        if (pin2Source == 0) {
            uint8_t numSeg = (type == 13) ? 8 : 7;
            bool isCommonDim = (mcMode >= 6 && mcMode <= 9);
            uint8_t startIdx = isCommonDim ? 0 : 1;
            if (output.containsKey("seg_pins")) {
                JsonArrayConst segArr = output["seg_pins"].as<JsonArrayConst>();
                JsonArrayConst segSources = output["seg_sources"].as<JsonArrayConst>();
                for (int s = startIdx; s < numSeg; s++) {
                    if (s < segSources.size() && (uint8_t)(segSources[s] | 0) != 0) continue;
                    if (s < segArr.size()) {
                        if (jsonPinMatches(segArr[s], reservedPin)) return true;
                    } else if (source == 0) {
                        int basePin = output["pin"] | 255;
                        if (basePin != 255 && (basePin + s) == reservedPin) return true;
                    }
                }
            } else if (source == 0) {
                int basePin = output["pin"] | 255;
                if (basePin != 255) {
                    for (int s = startIdx; s < numSeg; s++) {
                        if ((basePin + s) == reservedPin) return true;
                    }
                }
            }
        }
    } else {
        bool p2IsGpio = false;
        if (type == 6 || type == CHAN_TYPE_ANALOG_RGB || type == 18 || (type == 7 && pin2Source == 0)) {
            p2IsGpio = (pin2Source == 0);
        } else if (type == 11 || type == 10) {
            p2IsGpio = (source == 0);
        }
        if (p2IsGpio && jsonPinMatches(output["pin2"], reservedPin)) return true;

        bool p3IsGpio = false;
        if (type == CHAN_TYPE_ANALOG_RGB) {
            p3IsGpio = (pin3Source == 0);
        } else if (type == 6 && mcMode == 2) {
            p3IsGpio = (pin3Source == 0);
        } else if (type == 7 && pin3Source == 0) {
            p3IsGpio = true;
        }
        if (p3IsGpio && jsonPinMatches(output["pin3"], reservedPin)) return true;

        bool p4IsGpio = false;
        if (type == CHAN_TYPE_ANALOG_RGB && colorOrder >= 4) {
            p4IsGpio = (pin4Source == 0);
        } else if (type == 7 && homingMode == 0) {
            p4IsGpio = (pin4Source == 0);
        }
        if (p4IsGpio && jsonPinMatches(output["pin4"], reservedPin)) return true;
    }
    return false;
}

bool outputsUseReservedPin(JsonArray outputs, uint8_t reservedPin, const char* label, String& message) {
    if (reservedPin == 255) return false;
    uint8_t index = 1;
    for (JsonObject output : outputs) {
        if (outputJsonUsesPin(output, reservedPin)) {
            message = String(label) + " GPIO " + String(reservedPin) + " is already used by output channel " + String(index);
            return true;
        }
        index++;
    }
    return false;
}

bool savedOutputsUseReservedPin(uint8_t reservedPin, const char* label, String& message) {
    if (reservedPin == 255 || !LittleFS.exists("/outputs.json")) return false;
    File file = LittleFS.open("/outputs.json", "r");
    if (!file) return false;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error || !doc["outputs"].is<JsonArray>()) return false;
    return outputsUseReservedPin(doc["outputs"].as<JsonArray>(), reservedPin, label, message);
}

bool outputsHaveDuplicateGpio(JsonArray outputs, String& message) {
    struct UsedPin {
        uint8_t pin;
        uint8_t channel;
    };
    UsedPin used[32];
    uint8_t usedCount = 0;

    auto addPin = [&](int rawPin, uint8_t channel) -> bool {
        if (rawPin == 255 || rawPin < 0) return false;
        uint8_t pin = (uint8_t)rawPin;
        for (uint8_t i = 0; i < usedCount; i++) {
            if (used[i].pin == pin) {
                message = "GPIO " + String(pin) + " is already used by output channel " +
                          String(used[i].channel) + " and channel " + String(channel);
                return true;
            }
        }
        if (usedCount < 32) {
            used[usedCount++] = {pin, channel};
        }
        return false;
    };

    uint8_t channel = 1;
    for (JsonObject output : outputs) {
        uint8_t source = output["source"] | 0;
        uint8_t type = output["type"] | 0;
        uint8_t mcMode = output["mc_mode"] | 0;
        uint8_t homingMode = output["mc_homing_mode"] | 0;
        uint8_t colorOrder = output["color_order"] | 0;
        uint8_t pin2Source = output["pin2_source"] | 0;
        uint8_t pin3Source = output["pin3_source"] | 0;
        uint8_t pin4Source = output["pin4_source"] | 0;

        if (source == 0) {
            if (addPin(output["pin"] | 255, channel)) return true;
        }

        if (type == 12 || type == 13) {
            if (pin2Source == 0) {
                uint8_t numSeg = (type == 13) ? 8 : 7;
                bool isCommonDim = (mcMode >= 6 && mcMode <= 9);
                uint8_t startIdx = isCommonDim ? 0 : 1;
                if (output.containsKey("seg_pins")) {
                    JsonArrayConst segArr = output["seg_pins"].as<JsonArrayConst>();
                    JsonArrayConst segSources = output["seg_sources"].as<JsonArrayConst>();
                    for (int s = startIdx; s < numSeg; s++) {
                        if (s < segSources.size() && (uint8_t)(segSources[s] | 0) != 0) continue;
                        int pVal = 255;
                        if (s < segArr.size()) {
                            pVal = segArr[s] | 255;
                        }
                        if (pVal == 255 && source == 0) {
                            int basePin = output["pin"] | 255;
                            pVal = (basePin != 255) ? (basePin + s) : 255;
                        }
                        if (addPin(pVal, channel)) return true;
                    }
                } else {
                    if (source == 0) {
                        int basePin = output["pin"] | 255;
                        for (int s = startIdx; s < numSeg; s++) {
                            int pVal = (basePin != 255) ? (basePin + s) : 255;
                            if (addPin(pVal, channel)) return true;
                        }
                    }
                }
            }
        } else {
            bool p2IsGpio = false;
            if (type == 6 || type == CHAN_TYPE_ANALOG_RGB || type == 18 || (type == 7 && pin2Source == 0)) {
                p2IsGpio = (pin2Source == 0);
            } else if (type == 11 || type == 10) {
                p2IsGpio = (source == 0);
            }
            if (p2IsGpio && addPin(output["pin2"] | 255, channel)) return true;

            bool p3IsGpio = false;
            if (type == CHAN_TYPE_ANALOG_RGB) {
                p3IsGpio = (pin3Source == 0);
            } else if (type == 6 && mcMode == 2) {
                p3IsGpio = (pin3Source == 0);
            } else if (type == 7 && pin3Source == 0) {
                p3IsGpio = true;
            }
            if (p3IsGpio && addPin(output["pin3"] | 255, channel)) return true;

            bool p4IsGpio = false;
            if (type == CHAN_TYPE_ANALOG_RGB && colorOrder >= 4) {
                p4IsGpio = (pin4Source == 0);
            } else if (type == 7 && homingMode == 0) {
                p4IsGpio = (pin4Source == 0);
            }
            if (p4IsGpio && addPin(output["pin4"] | 255, channel)) return true;
        }
        channel++;
    }
    return false;
}

bool outputsHaveDuplicateExpanderChannel(JsonArray outputs, String& message) {
    struct UsedChannel {
        uint8_t source;
        uint8_t address;
        uint8_t channel;
        uint8_t outputIndex;
    };
    UsedChannel used[64];
    uint8_t usedCount = 0;

    auto addChannel = [&](uint8_t source, uint8_t address, int rawChannel, uint8_t outputIndex) -> bool {
        if (source == 0 || rawChannel == 255 || rawChannel < 0) return false;
        uint8_t channel = (uint8_t)rawChannel;
        for (uint8_t i = 0; i < usedCount; i++) {
            if (used[i].source == source && used[i].address == address && used[i].channel == channel) {
                message = "Expander source " + String(source) + " address 0x" + String(address, HEX) +
                          " channel " + String(channel) + " is already used by output channel " +
                          String(used[i].outputIndex) + " and channel " + String(outputIndex);
                return true;
            }
        }
        if (usedCount < 64) {
            used[usedCount++] = {source, address, channel, outputIndex};
        }
        return false;
    };

    uint8_t outputIndex = 1;
    for (JsonObject output : outputs) {
        uint8_t source = output["source"] | 0;
        uint8_t type = output["type"] | 0;
        uint8_t address = output["pca_addr"] | 0x40;
        uint8_t mcMode = output["mc_mode"] | 0;
        uint8_t colorOrder = output["color_order"] | 0;
        if (source != 0) {
            if (addChannel(source, address, output["pca_channel"] | 0, outputIndex)) return true;
            if ((type == 6 || type == 7 || type == CHAN_TYPE_ANALOG_RGB || type == 18) &&
                addChannel(source, address, output["pca_channel2"] | 255, outputIndex)) return true;
            if ((type == 7 || type == CHAN_TYPE_ANALOG_RGB || (type == 6 && mcMode == 2)) &&
                addChannel(source, address, output["pca_channel3"] | 255, outputIndex)) return true;
            if ((type == CHAN_TYPE_ANALOG_RGB && colorOrder >= 4) &&
                addChannel(source, address, output["pca_channel4"] | 255, outputIndex)) return true;
        }

        uint8_t pin2Source = output["pin2_source"] | 0;
        uint8_t pin3Source = output["pin3_source"] | 0;
        uint8_t pin4Source = output["pin4_source"] | 0;

        bool is7SegDD = (type == 12 || type == 13) && (mcMode >= 2 && mcMode <= 9);
        if (pin2Source != 0 && !is7SegDD) {
            uint8_t pin2Addr = output["pin2_addr"] | (pin2Source == 1 ? 0x40 : 0x20);
            if (addChannel(pin2Source, pin2Addr, output["pin2_channel"] | 255, outputIndex)) return true;
        }
        if (pin3Source != 0) {
            uint8_t pin3Addr = output["pin3_addr"] | (pin3Source == 1 ? 0x40 : 0x20);
            if (addChannel(pin3Source, pin3Addr, output["pin3_channel"] | 255, outputIndex)) return true;
        }
        if (pin4Source != 0) {
            uint8_t pin4Addr = output["pin4_addr"] | (pin4Source == 1 ? 0x40 : 0x20);
            if (addChannel(pin4Source, pin4Addr, output["pin4_channel"] | 255, outputIndex)) return true;
        }
        if (is7SegDD) {
            uint8_t pin2Source = output["pin2_source"] | 0;
            if (pin2Source != 0) {
                uint8_t baseCh = output["pin2_channel"] | 255;
                uint8_t pin2Addr = output["pin2_addr"] | (pin2Source == 1 ? 0x40 : 0x20);
                uint8_t numSeg = (type == 13) ? 8 : 7;
                for (uint8_t s = 0; s < numSeg; s++) {
                    if (addChannel(pin2Source, pin2Addr, baseCh != 255 ? baseCh + s : 255, outputIndex)) return true;
                }
            } else if (output.containsKey("seg_sources")) {
                JsonArrayConst segSources = output["seg_sources"].as<JsonArrayConst>();
                JsonArrayConst segAddrs = output["seg_addrs"].as<JsonArrayConst>();
                JsonArrayConst segChannels = output["seg_channels"].as<JsonArrayConst>();
                uint8_t numSeg = (type == 13) ? 8 : 7;
                for (uint8_t s = 0; s < numSeg; s++) {
                    if (s < segSources.size() && s < segAddrs.size() && s < segChannels.size()) {
                        uint8_t sSrc = segSources[s] | 0;
                        if (sSrc != 0) {
                            uint8_t sAddr = segAddrs[s] | (sSrc == 1 ? 0x40 : 0x20);
                            if (addChannel(sSrc, sAddr, segChannels[s] | 255, outputIndex)) return true;
                        }
                    }
                }
            }
        }
        outputIndex++;
    }
    return false;
}

void addSettingsToJson(JsonObject target) {
    target["device_mode"] = sysCfg.device_mode;
    target["eth_dhcp"] = sysCfg.eth_dhcp;
    target["eth_ip"] = sysCfg.eth_ip;
    target["eth_netmask"] = sysCfg.eth_netmask;
    target["eth_gateway"] = sysCfg.eth_gateway;
    target["eth_dns"] = sysCfg.eth_dns;
    target["ap_ssid"] = sysCfg.ap_ssid;
    target["ap_pass"] = sysCfg.ap_pass;
    target["artnet_port"] = sysCfg.artnet_port;
    target["sacn_enabled"] = sysCfg.sacn_enabled;
    target["sacn_multicast"] = sysCfg.sacn_multicast;
    target["sacn_port"] = sysCfg.sacn_port;
    target["status_led_pin"] = sysCfg.status_led_pin;
    target["zc_pin"] = sysCfg.zc_pin;
    target["i2c_sda"] = sysCfg.i2c_sda;
    target["i2c_scl"] = sysCfg.i2c_scl;
    target["i2c_speed"] = sysCfg.i2c_speed;
    target["output_fps"] = sysCfg.output_fps;
    target["wifi_ssid"] = sysCfg.wifi_ssid;
    target["wifi_pass"] = sysCfg.wifi_pass;
    target["wifi_dhcp"] = sysCfg.wifi_dhcp;
    target["wifi_ip"] = sysCfg.wifi_ip;
    target["wifi_netmask"] = sysCfg.wifi_netmask;
    target["wifi_gateway"] = sysCfg.wifi_gateway;
    target["wifi_dns"] = sysCfg.wifi_dns;
    target["wifi_enable_in_eth_mode"] = sysCfg.wifi_enable_in_eth_mode;
    target["ap_enable_in_eth_mode"] = sysCfg.ap_enable_in_eth_mode;
    target["mdns_name"] = sysCfg.mdns_name;
    target["display_enabled"] = sysCfg.display_enabled;
    target["display_i2c_addr"] = sysCfg.display_i2c_addr;
    target["display_brightness"] = sysCfg.display_brightness;
}

void applySettingsFromJson(JsonObjectConst doc) {
    if (doc["device_mode"].is<int>()) sysCfg.device_mode = doc["device_mode"];
    if (doc["eth_dhcp"].is<bool>()) sysCfg.eth_dhcp = doc["eth_dhcp"];
    if (doc["eth_ip"].is<const char*>()) copyConfigString(sysCfg.eth_ip, sizeof(sysCfg.eth_ip), doc["eth_ip"]);
    if (doc["eth_netmask"].is<const char*>()) copyConfigString(sysCfg.eth_netmask, sizeof(sysCfg.eth_netmask), doc["eth_netmask"]);
    if (doc["eth_gateway"].is<const char*>()) copyConfigString(sysCfg.eth_gateway, sizeof(sysCfg.eth_gateway), doc["eth_gateway"]);
    if (doc["eth_dns"].is<const char*>()) copyConfigString(sysCfg.eth_dns, sizeof(sysCfg.eth_dns), doc["eth_dns"]);
    if (doc["ap_ssid"].is<const char*>()) copyConfigString(sysCfg.ap_ssid, sizeof(sysCfg.ap_ssid), doc["ap_ssid"]);
    if (doc["ap_pass"].is<const char*>()) copyConfigString(sysCfg.ap_pass, sizeof(sysCfg.ap_pass), doc["ap_pass"]);
    if (doc["artnet_port"].is<int>()) {
        int port = doc["artnet_port"];
        sysCfg.artnet_port = (port > 0 && port <= 65535) ? port : 6454;
    }
    if (doc["sacn_enabled"].is<bool>()) sysCfg.sacn_enabled = doc["sacn_enabled"];
    if (doc["sacn_multicast"].is<bool>()) sysCfg.sacn_multicast = doc["sacn_multicast"];
    if (doc["sacn_port"].is<int>()) {
        int port = doc["sacn_port"];
        sysCfg.sacn_port = (port > 0 && port <= 65535) ? port : 5568;
    }
    if (doc["status_led_pin"].is<int>()) sysCfg.status_led_pin = doc["status_led_pin"];
    if (doc["zc_pin"].is<int>()) sysCfg.zc_pin = doc["zc_pin"];
    if (doc["i2c_sda"].is<int>()) sysCfg.i2c_sda = doc["i2c_sda"];
    if (doc["i2c_scl"].is<int>()) sysCfg.i2c_scl = doc["i2c_scl"];
    if (doc["i2c_speed"].is<int>()) sysCfg.i2c_speed = doc["i2c_speed"];
    if (doc["output_fps"].is<int>()) sysCfg.output_fps = doc["output_fps"];
    if (doc["wifi_ssid"].is<const char*>()) copyConfigString(sysCfg.wifi_ssid, sizeof(sysCfg.wifi_ssid), doc["wifi_ssid"]);
    if (doc["wifi_pass"].is<const char*>()) copyConfigString(sysCfg.wifi_pass, sizeof(sysCfg.wifi_pass), doc["wifi_pass"]);
    if (doc["wifi_dhcp"].is<bool>()) sysCfg.wifi_dhcp = doc["wifi_dhcp"];
    if (doc["wifi_ip"].is<const char*>()) copyConfigString(sysCfg.wifi_ip, sizeof(sysCfg.wifi_ip), doc["wifi_ip"]);
    if (doc["wifi_netmask"].is<const char*>()) copyConfigString(sysCfg.wifi_netmask, sizeof(sysCfg.wifi_netmask), doc["wifi_netmask"]);
    if (doc["wifi_gateway"].is<const char*>()) copyConfigString(sysCfg.wifi_gateway, sizeof(sysCfg.wifi_gateway), doc["wifi_gateway"]);
    if (doc["wifi_dns"].is<const char*>()) copyConfigString(sysCfg.wifi_dns, sizeof(sysCfg.wifi_dns), doc["wifi_dns"]);
    if (doc["wifi_enable_in_eth_mode"].is<bool>()) sysCfg.wifi_enable_in_eth_mode = doc["wifi_enable_in_eth_mode"];
    if (doc["ap_enable_in_eth_mode"].is<bool>()) sysCfg.ap_enable_in_eth_mode = doc["ap_enable_in_eth_mode"];
    if (doc["mdns_name"].is<const char*>()) copyConfigString(sysCfg.mdns_name, sizeof(sysCfg.mdns_name), doc["mdns_name"]);
    if (doc["display_enabled"].is<int>()) sysCfg.display_enabled = doc["display_enabled"];
    if (doc["display_i2c_addr"].is<int>()) sysCfg.display_i2c_addr = doc["display_i2c_addr"];
    if (doc["display_brightness"].is<int>()) sysCfg.display_brightness = doc["display_brightness"];
}

bool validateSettingsAndOutputs(JsonObjectConst settings, JsonArray outputs, String& message) {
    if (settings.containsKey("output_fps")) {
        int fps = settings["output_fps"] | 0;
        if (fps <= 0 || fps > 44) {
            message = "Global Output FPS must be between 1 and 44";
            return false;
        }
    }
    uint8_t statusPin = settings["status_led_pin"].is<int>() ? (uint8_t)(int)settings["status_led_pin"] : sysCfg.status_led_pin;
    uint8_t zcPin = settings["zc_pin"].is<int>() ? (uint8_t)(int)settings["zc_pin"] : sysCfg.zc_pin;
    uint8_t sdaPin = settings["i2c_sda"].is<int>() ? (uint8_t)(int)settings["i2c_sda"] : sysCfg.i2c_sda;
    uint8_t sclPin = settings["i2c_scl"].is<int>() ? (uint8_t)(int)settings["i2c_scl"] : sysCfg.i2c_scl;
    uint8_t displayType = settings["display_enabled"].is<int>() ? (uint8_t)(int)settings["display_enabled"] : sysCfg.display_enabled;
    uint8_t displayAddr = settings["display_i2c_addr"].is<int>() ? (uint8_t)(int)settings["display_i2c_addr"] : sysCfg.display_i2c_addr;

    if (statusPin != 255 && zcPin != 255 && statusPin == zcPin) {
        message = "Status LED GPIO and Zero-Crossing GPIO cannot use the same pin";
        return false;
    }
    if (sdaPin != 255 && sclPin != 255 && sdaPin == sclPin) {
        message = "I2C SDA and SCL pins cannot be the same";
        return false;
    }
    if (statusPin != 255 && (statusPin == sdaPin || statusPin == sclPin)) {
        message = "Status LED pin cannot overlap with I2C SDA or SCL";
        return false;
    }
    if (zcPin != 255 && (zcPin == sdaPin || zcPin == sclPin)) {
        message = "Zero-Crossing pin cannot overlap with I2C SDA or SCL";
        return false;
    }
    if (!displayAddressValid(displayType, displayAddr)) {
        message = "I2C display address is invalid for the selected display type";
        return false;
    }
    if (outputsUseReservedPin(outputs, statusPin, "Status LED", message)) return false;
    if (outputsUseReservedPin(outputs, zcPin, "Zero-Crossing", message)) return false;
    if (outputsUseReservedPin(outputs, sdaPin, "I2C SDA", message)) return false;
    if (outputsUseReservedPin(outputs, sclPin, "I2C SCL", message)) return false;
    if (outputsHaveDuplicateGpio(outputs, message)) return false;
    if (outputsHaveDuplicateExpanderChannel(outputs, message)) return false;
    return true;
}

bool validateOutputJson(JsonArray outputs, String& message) {
    // No hard channel limit — enforced by scoring + hardware resource validation
    // Check UART and RMT hardware limits
    uint8_t dmxCount = 0;
    uint8_t dfPlayerCount = 0;
    uint8_t ledCount = 0;
    for (JsonObject output : outputs) {
        uint8_t type = output["type"] | 0;
        uint8_t source = output["source"] | 0;
        if (type == 1 && source == 0) { // DMX on local GPIO
            dmxCount++;
        } else if (type == 10) { // DFPlayer
            dfPlayerCount++;
        } else if (type == 3) { // LED Strip
            ledCount++;
        }
    }
    if (dfPlayerCount > 2) {
        message = "Hardware limit exceeded: Maximum 2 DFPlayer channels (requires UART1/UART2)";
        return false;
    }
    uint8_t freeUarts = 2 - dfPlayerCount;
    uint8_t dmxUartUse = dmxCount < freeUarts ? dmxCount : freeUarts;
    uint8_t dmxRmtUse = dmxCount - dmxUartUse;
    if (ledCount + dmxRmtUse > 8) {
        message = "Hardware limit exceeded: Total RMT channels (LED strips + extra DMX fallback) cannot exceed 8";
        return false;
    }
    uint8_t channelNumber = 1;
    for (JsonObject output : outputs) {
        uint8_t type = output["type"] | 0;
        uint8_t source = output["source"] | 0;
        if (type > 18) {
            message = "Invalid output type on channel " + String(channelNumber) + ".";
            return false;
        }
        uint8_t mcMode = output["mc_mode"] | 0;
        bool pcaOk = (type == 2 || type == 4 || type == 6 || type == 7 || type == 8 || type == CHAN_TYPE_ANALOG_RGB || type == 15 || type == 17 || type == 18 || type == 12 || type == 13);
        bool digitalOk = (type == 2 || type == 17 || type == 18 || ((type == 12 || type == 13) && (mcMode == 2 || mcMode == 3)));
        if (source == 1 && !pcaOk) {
            message = "PCA9685 source is not supported by output channel " + String(channelNumber);
            return false;
        }
        if (source >= 2 && source <= 4 && !digitalOk) {
            message = "Digital GPIO expander source is not supported by output channel " + String(channelNumber);
            return false;
        }
        if (source == 5 && type != 14) {
            message = "I2C DAC source is only supported by DAC output channel " + String(channelNumber);
            return false;
        }
        if (source > 5) {
            message = "Unsupported output source on channel " + String(channelNumber);
            return false;
        }
        auto defaultAddrForSource = [](uint8_t s) -> uint8_t {
            if (s == 1) return 0x40;
            if (s == 5) return 0x60;
            return 0x20;
        };
        if (source != 0 && source != 5) {
            uint8_t addr = output["pca_addr"] | defaultAddrForSource(source);
            if (!validateSourceAddress(source, addr, "Primary I2C source on channel " + String(channelNumber), message)) return false;
        }
        if (type == 14 && source == 5) {
            uint8_t dacModel = output["dac_model"] | 0;
            uint8_t addr = output["pca_addr"] | (dacModel == 0 ? 0x60 : 0x4C);
            uint8_t dacChannel = output["pca_channel"] | 0;
            if (dacModel > 2) {
                message = "Unsupported I2C DAC model on channel " + String(channelNumber);
                return false;
            }
            if (!dacAddressValid(dacModel, addr)) {
                message = "I2C DAC source on channel " + String(channelNumber) + " has invalid address 0x" + String(addr, HEX) + ". " + dacAddressRangeLabel(dacModel);
                return false;
            }
            if (dacModel != 2 && dacChannel != 0) {
                message = "Only DAC7573 supports DAC channels B-D on channel " + String(channelNumber);
                return false;
            }
            if (dacModel == 2 && dacChannel > 3) {
                message = "DAC7573 channel must be 0-3 on channel " + String(channelNumber);
                return false;
            }
        }
        if (type == 12 || type == 13) {
            uint8_t pin2Source = output["pin2_source"] | 0;
            if (pin2Source != 0 && pin2Source > 4) {
                message = "Unsupported segment expander source on channel " + String(channelNumber);
                return false;
            }
            if (pin2Source != 0) {
                uint8_t pin2Addr = output["pin2_addr"] | defaultAddrForSource(pin2Source);
                if (!validateSourceAddress(pin2Source, pin2Addr, "Segment base I2C source on channel " + String(channelNumber), message)) return false;
            }
            if ((mcMode == 4 || mcMode == 5) && pin2Source >= 2 && pin2Source <= 4) {
                message = "7-Segment Direct Dim requires ESP32 GPIO or PCA9685 segment source on channel " + String(channelNumber);
                return false;
            }
            if (pin2Source != 0 && (int)(output["pin2_channel"] | 255) == 255) {
                message = "Segment expander base channel is missing on channel " + String(channelNumber);
                return false;
            }
            if (output.containsKey("seg_sources")) {
                JsonArrayConst segSources = output["seg_sources"].as<JsonArrayConst>();
                JsonArrayConst segAddrs = output["seg_addrs"].as<JsonArrayConst>();
                JsonArrayConst segChannels = output["seg_channels"].as<JsonArrayConst>();
                uint8_t numSeg = (type == 13) ? 8 : 7;
                for (int s = 0; s < segSources.size(); s++) {
                    uint8_t sSrc = segSources[s] | 0;
                    if (sSrc != 0 && sSrc > 4) {
                        message = "Unsupported segment expander source on channel " + String(channelNumber);
                        return false;
                    }
                    if (sSrc != 0) {
                        uint8_t sAddr = (s < segAddrs.size()) ? (uint8_t)(segAddrs[s] | defaultAddrForSource(sSrc)) : defaultAddrForSource(sSrc);
                        if (!validateSourceAddress(sSrc, sAddr, "Segment " + String(s + 1) + " I2C source on channel " + String(channelNumber), message)) return false;
                    }
                    if ((mcMode == 4 || mcMode == 5) && sSrc >= 2 && sSrc <= 4) {
                        message = "7-Segment Direct Dim requires ESP32 GPIO or PCA9685 segment source on channel " + String(channelNumber);
                        return false;
                    }
                    if (s < numSeg && sSrc != 0 && (s >= segChannels.size() || (int)(segChannels[s] | 255) == 255)) {
                        message = "Segment expander channel is missing on channel " + String(channelNumber);
                        return false;
                    }
                }
            }
        }
        if (type == 7) {
            uint8_t pin2Source = output["pin2_source"] | 0;
            uint8_t pin3Source = output["pin3_source"] | 0;
            uint8_t pin4Source = output["pin4_source"] | 0;
            uint8_t homingMode = output["mc_homing_mode"] | 0;
            if (source != 0) {
                message = "Stepper STEP pin must use ESP32 GPIO on channel " + String(channelNumber);
                return false;
            }
            if (pin2Source > 4 || pin3Source > 4 || pin4Source > 4) {
                message = "Unsupported stepper hybrid pin source on channel " + String(channelNumber);
                return false;
            }
            if (pin2Source != 0) {
                uint8_t pin2Addr = output["pin2_addr"] | defaultAddrForSource(pin2Source);
                if (!validateSourceAddress(pin2Source, pin2Addr, "Stepper DIR I2C source on channel " + String(channelNumber), message)) return false;
            }
            if (pin3Source != 0) {
                uint8_t pin3Addr = output["pin3_addr"] | defaultAddrForSource(pin3Source);
                if (!validateSourceAddress(pin3Source, pin3Addr, "Stepper ENABLE I2C source on channel " + String(channelNumber), message)) return false;
            }
            if (homingMode == 0 && pin4Source != 0) {
                uint8_t pin4Addr = output["pin4_addr"] | defaultAddrForSource(pin4Source);
                if (!validateSourceAddress(pin4Source, pin4Addr, "Stepper HOME I2C source on channel " + String(channelNumber), message)) return false;
            }
            if ((pin2Source != 0 && (int)(output["pin2_channel"] | 255) == 255) ||
                (pin3Source != 0 && (int)(output["pin3_channel"] | 255) == 255)) {
                message = "Stepper hybrid expander channel is missing on channel " + String(channelNumber);
                return false;
            }
            if (homingMode == 0 && pin4Source != 0 && (int)(output["pin4_channel"] | 255) == 255) {
                message = "Stepper HOME expander channel is missing on channel " + String(channelNumber);
                return false;
            }
        }
        if (type == CHAN_TYPE_ANALOG_RGB) {
            uint8_t pin2Source = output["pin2_source"] | 0;
            uint8_t pin3Source = output["pin3_source"] | 0;
            uint8_t pin4Source = output["pin4_source"] | 0;
            uint8_t colorOrder = output["color_order"] | 0;
            if (source > 1 || pin2Source > 1 || pin3Source > 1 || pin4Source > 1) {
                message = "Analog RGB/RGBW channels only support ESP32 GPIO or PCA9685 sources on channel " + String(channelNumber);
                return false;
            }
            if (pin2Source != 0) {
                uint8_t pin2Addr = output["pin2_addr"] | defaultAddrForSource(pin2Source);
                if (!validateSourceAddress(pin2Source, pin2Addr, "Analog RGB/RGBW Green I2C source on channel " + String(channelNumber), message)) return false;
            }
            if (pin3Source != 0) {
                uint8_t pin3Addr = output["pin3_addr"] | defaultAddrForSource(pin3Source);
                if (!validateSourceAddress(pin3Source, pin3Addr, "Analog RGB/RGBW Blue I2C source on channel " + String(channelNumber), message)) return false;
            }
            if (colorOrder >= 4 && pin4Source != 0) {
                uint8_t pin4Addr = output["pin4_addr"] | defaultAddrForSource(pin4Source);
                if (!validateSourceAddress(pin4Source, pin4Addr, "Analog RGB/RGBW White I2C source on channel " + String(channelNumber), message)) return false;
            }
            if ((source == 1 && (int)(output["pca_channel"] | 255) == 255) ||
                (pin2Source == 1 && (int)(output["pin2_channel"] | 255) == 255) ||
                (pin3Source == 1 && (int)(output["pin3_channel"] | 255) == 255) ||
                (colorOrder >= 4 && pin4Source == 1 && (int)(output["pin4_channel"] | 255) == 255)) {
                message = "Analog RGB/RGBW PCA9685 channel is missing on channel " + String(channelNumber);
                return false;
            }
        }
        if (type == 18) {
            uint8_t pin2Source = output["pin2_source"] | 0;
            if (pin2Source > 4) {
                message = "Unsupported smoke shooter hybrid pin source on channel " + String(channelNumber);
                return false;
            }
            if (pin2Source != 0) {
                uint8_t pin2Addr = output["pin2_addr"] | defaultAddrForSource(pin2Source);
                if (!validateSourceAddress(pin2Source, pin2Addr, "Smoke Shooter shoot I2C source on channel " + String(channelNumber), message)) return false;
            }
            if (pin2Source != 0 && (int)(output["pin2_channel"] | 255) == 255) {
                message = "Smoke Shooter shoot expander channel is missing on channel " + String(channelNumber);
                return false;
            }
        }
        if (type == 6) {
            uint8_t pin2Source = output["pin2_source"] | 0;
            uint8_t pin3Source = output["pin3_source"] | 0;
            if (pin2Source > 4 || pin3Source > 4) {
                message = "Unsupported motor hybrid pin source on channel " + String(channelNumber);
                return false;
            }
            if (pin2Source != 0) {
                uint8_t pin2Addr = output["pin2_addr"] | defaultAddrForSource(pin2Source);
                if (!validateSourceAddress(pin2Source, pin2Addr, "Motor IN2/DIR I2C source on channel " + String(channelNumber), message)) return false;
            }
            if (pin3Source != 0) {
                uint8_t pin3Addr = output["pin3_addr"] | defaultAddrForSource(pin3Source);
                if (!validateSourceAddress(pin3Source, pin3Addr, "Motor EN I2C source on channel " + String(channelNumber), message)) return false;
            }
        }
        if (type == 6 && (uint8_t)(output["mc_mode"] | 0) == 2) {
            uint8_t enSource = output["pin3_source"] | 0;
            if (enSource >= 2) {
                message = "Motor EN pin needs PWM; use ESP32 GPIO or PCA9685 on channel " + String(channelNumber);
                return false;
            }
            if (enSource == 1 && (int)(output["pin3_channel"] | 255) == 255) {
                message = "Motor EN PCA9685 channel is missing on channel " + String(channelNumber);
                return false;
            }
        }
        if (type == 15) {
            int minDuty = output["pwm_dac_min"] | 0;
            int maxDuty = output["pwm_dac_max"] | 10000;
            int mode = output["pwm_dac_mode"] | 0;
            if (minDuty < 0 || minDuty > 10000 || maxDuty < 0 || maxDuty > 10000) {
                message = "PWM DAC calibration duty must be between 0.00% and 100.00% on channel " + String(channelNumber);
                return false;
            }
            if (mode > 2) {
                message = "Unsupported PWM DAC calibration mode on channel " + String(channelNumber);
                return false;
            }
        }
        channelNumber++;
    }
    return true;
}

void onWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            Serial.println("ETH Started");
            ETH.setHostname(sysCfg.mdns_name);
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            Serial.println("ETH Connected");
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            Serial.print("ETH Got IP: ");
            Serial.println(ETH.localIP());
            ethConnected = true;
            // Stop fallback Access Point if Ethernet connected and we are in Ethernet Mode
            if (sysCfg.device_mode == MODE_ARTNET_ETHERNET || sysCfg.device_mode == MODE_ESPNOW_MASTER) {
                stopAP();
            }
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            Serial.println("ETH Disconnected");
            ethConnected = false;
            // Fall back to configuration AP when connection is lost
            if (sysCfg.device_mode == MODE_ESPNOW_MASTER && !WiFi.isConnected()) {
                startAP();
            }
            break;
        case ARDUINO_EVENT_ETH_STOP:
            Serial.println("ETH Stopped");
            ethConnected = false;
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print("Wi-Fi Connected. IP: ");
            Serial.println(WiFi.localIP());
            lastWifiReconnectAttempt = 0;
            // Always stop AP when Wi-Fi STA has a valid IP — no need to keep AP alive
            stopAP();
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("Wi-Fi Disconnected");
            reconnectWiFiClient(true);
            // Restart AP only if Ethernet is also down and AP fallback is allowed
            if (!ethConnected &&
                (sysCfg.device_mode == MODE_ESPNOW_MASTER ||
                 (sysCfg.device_mode == MODE_ARTNET_ETHERNET && sysCfg.ap_enable_in_eth_mode))) {
                startAP();
            }
            break;
        default:
            break;
    }
}

void startAP() {
    if (apActive) return;

    bool keepSta = wifiClientConfigured() || WiFi.isConnected();
    WiFi.mode(keepSta ? WIFI_AP_STA : WIFI_AP);
    configureWiFiRadioForBoot();
    WiFi.enableAP(true);
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    
    if (strlen(sysCfg.ap_pass) >= 8) {
        WiFi.softAP(sysCfg.ap_ssid, sysCfg.ap_pass);
    } else {
        WiFi.softAP(sysCfg.ap_ssid);
    }
    
    Serial.printf("Access Point started: SSID = %s, IP = %s\n", sysCfg.ap_ssid, WiFi.softAPIP().toString().c_str());
    apStartTime = millis();
    apActive = true;
}

void stopAP() {
    if (!apActive) return;
    if (sysCfg.device_mode == MODE_ESPNOW_SLAVE) return;
    WiFi.softAPdisconnect(true);
    if (WiFi.isConnected()) {
        WiFi.mode(WIFI_STA);
    } else {
        WiFi.enableAP(false);
    }
    apActive = false;
    Serial.println("Access Point stopped to save RF bandwidth");
}

void startWiFiClient(bool keepApActive) {
    if (!wifiClientConfigured()) return;

    Serial.printf("Connecting to Wi-Fi SSID: %s\n", sysCfg.wifi_ssid);
    WiFi.mode(keepApActive ? WIFI_AP_STA : WIFI_STA);
    configureWiFiRadioForBoot();
    WiFi.setAutoReconnect(true);

    if (!sysCfg.wifi_dhcp) {
        IPAddress local_ip, gateway, subnet, dns;
        if (local_ip.fromString(sysCfg.wifi_ip) && 
            gateway.fromString(sysCfg.wifi_gateway) && 
            subnet.fromString(sysCfg.wifi_netmask) && 
            dns.fromString(sysCfg.wifi_dns)) {
            WiFi.config(local_ip, gateway, subnet, dns);
            Serial.println("Static IP configured for Wi-Fi Client");
        } else {
            Serial.println("Error parsing Wi-Fi static IP settings. Falling back to DHCP.");
        }
    }

    WiFi.begin(sysCfg.wifi_ssid, sysCfg.wifi_pass);
}

void setupNetwork() {
    // Hook WiFi/Ethernet events
    WiFi.onEvent(onWiFiEvent);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

    // Connect to Ethernet
    resetEthernetPhy();
    ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLK_MODE);

    // If DHCP is disabled, configure static IP after ETH.begin()
    if (!sysCfg.eth_dhcp) {
        IPAddress local_ip, gateway, subnet, dns;
        if (local_ip.fromString(sysCfg.eth_ip) && 
            gateway.fromString(sysCfg.eth_gateway) && 
            subnet.fromString(sysCfg.eth_netmask) && 
            dns.fromString(sysCfg.eth_dns)) {
            ETH.config(local_ip, gateway, subnet, dns);
            Serial.println("Static IP configured for Ethernet");
        } else {
            Serial.println("Error parsing static IP settings. Falling back to DHCP.");
        }
    }
    
    // Wait up to 3 seconds to check if Ethernet Link is active
    unsigned long startWait = millis();
    bool hasLink = false;
    while (millis() - startWait < 3000) {
        if (ETH.linkUp()) {
            hasLink = true;
            break;
        }
        delay(100);
    }

    // Determine whether to launch wireless services immediately or stay wired-only.
    if (sysCfg.device_mode == MODE_ESPNOW_SLAVE) {
        if (strcmp(sysCfg.ap_ssid, "ESP32-ArtNet-Setup") == 0) {
            String mac = WiFi.macAddress();
            mac.replace(":", "");
            String suffix = mac.substring(mac.length() - 4);
            String newSsid = String("ESP32-ArtNet-Setup-") + suffix;
            strncpy(sysCfg.ap_ssid, newSsid.c_str(), sizeof(sysCfg.ap_ssid) - 1);
            sysCfg.ap_ssid[sizeof(sysCfg.ap_ssid) - 1] = '\0';
        }
        startAP();
    } else {
        if (hasLink) {
            Serial.println("Ethernet link detected. Skipping Wi-Fi STA during boot.");
            if (sysCfg.wifi_enable_in_eth_mode) {
                Serial.println("Wi-Fi STA is enabled in Ethernet mode, but wired link is active so radio stays off.");
            }
        } else if (wifiClientConfigured()) {
            Serial.println("Ethernet link down. Wi-Fi STA enabled in Ethernet mode. Starting Wi-Fi client.");
            startWiFiClient(false);
            unsigned long wifiStart = millis();
            while (millis() - wifiStart < 10000) {
                if (WiFi.isConnected()) break;
                if (ETH.linkUp()) break;
                delay(100);
            }
            if (WiFi.isConnected()) {
                Serial.print("Wi-Fi fallback connected. IP: ");
                Serial.println(WiFi.localIP());
            } else if (sysCfg.ap_enable_in_eth_mode) {
                Serial.println("Wi-Fi fallback did not connect. Starting setup Access Point.");
                startAP();
            } else {
                Serial.println("Wi-Fi fallback did not connect. Setup AP fallback disabled.");
            }
        } else if (sysCfg.ap_enable_in_eth_mode) {
            Serial.println("Ethernet link down. Setup AP fallback enabled in Ethernet mode.");
            startAP();
        } else {
            Serial.println("Ethernet link down. Wireless auto-start disabled in Art-Net Ethernet mode.");
        }
    }
}

void startRecoveryEthernetOnly() {
    WiFi.mode(WIFI_OFF);
    WiFi.disconnect(true, true);
    WiFi.softAPdisconnect(true);

    WiFi.onEvent(onWiFiEvent);
    WiFi.persistent(false);
    resetEthernetPhy();
    ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLK_MODE);

    if (!sysCfg.eth_dhcp) {
        IPAddress local_ip, gateway, subnet, dns;
        if (local_ip.fromString(sysCfg.eth_ip) &&
            gateway.fromString(sysCfg.eth_gateway) &&
            subnet.fromString(sysCfg.eth_netmask) &&
            dns.fromString(sysCfg.eth_dns)) {
            ETH.config(local_ip, gateway, subnet, dns);
            Serial.println("Static IP configured for Ethernet recovery");
        }
    }

    setupWebServer();
    Serial.println("Recovery Mode ready on Ethernet only. Wi-Fi/AP disabled to avoid brownout.");
}

void setupWebServer() {
    // Serve Setup UI Page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse* response = request->beginResponse_P(200, "text/html; charset=utf-8", CONFIG_HTML_GZ, CONFIG_HTML_GZ_LEN);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    });

    // API Route: Get Config settings
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["device_mode"] = sysCfg.device_mode;
        doc["eth_dhcp"] = sysCfg.eth_dhcp;
        doc["eth_ip"] = sysCfg.eth_ip;
        doc["eth_netmask"] = sysCfg.eth_netmask;
        doc["eth_gateway"] = sysCfg.eth_gateway;
        doc["eth_dns"] = sysCfg.eth_dns;
        doc["ap_ssid"] = sysCfg.ap_ssid;
        doc["ap_pass"] = sysCfg.ap_pass;
        doc["artnet_port"] = sysCfg.artnet_port;
        doc["sacn_enabled"] = sysCfg.sacn_enabled;
        doc["sacn_multicast"] = sysCfg.sacn_multicast;
        doc["sacn_port"] = sysCfg.sacn_port;
        doc["status_led_pin"] = sysCfg.status_led_pin;
        doc["zc_pin"] = sysCfg.zc_pin;
        doc["i2c_sda"] = sysCfg.i2c_sda;
        doc["i2c_scl"] = sysCfg.i2c_scl;
        doc["i2c_speed"] = sysCfg.i2c_speed;
        doc["output_fps"] = sysCfg.output_fps;
        doc["wifi_ssid"] = sysCfg.wifi_ssid;
        doc["wifi_pass"] = sysCfg.wifi_pass;
        doc["wifi_dhcp"] = sysCfg.wifi_dhcp;
        doc["wifi_ip"] = sysCfg.wifi_ip;
        doc["wifi_netmask"] = sysCfg.wifi_netmask;
        doc["wifi_gateway"] = sysCfg.wifi_gateway;
        doc["wifi_dns"] = sysCfg.wifi_dns;
        doc["wifi_enable_in_eth_mode"] = sysCfg.wifi_enable_in_eth_mode;
        doc["ap_enable_in_eth_mode"] = sysCfg.ap_enable_in_eth_mode;
        doc["mdns_name"] = sysCfg.mdns_name;
        doc["display_enabled"] = sysCfg.display_enabled;
        doc["display_i2c_addr"] = sysCfg.display_i2c_addr;
        doc["display_brightness"] = sysCfg.display_brightness;
        doc["firmware_version"] = getFirmwareVersion();
        doc["build_date"] = __DATE__;
        doc["build_time"] = __TIME__;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API Route: Save Config settings
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, 
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body;
            if (!collectRequestBody(request, data, len, index, total, body)) return;

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, body);
            if (error) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
                return;
            }

            uint8_t requestedStatusPin = doc["status_led_pin"].is<int>() ? (uint8_t)(int)doc["status_led_pin"] : sysCfg.status_led_pin;
            uint8_t requestedZcPin = doc["zc_pin"].is<int>() ? (uint8_t)(int)doc["zc_pin"] : sysCfg.zc_pin;
            uint8_t requestedSda = doc["i2c_sda"].is<int>() ? (uint8_t)(int)doc["i2c_sda"] : sysCfg.i2c_sda;
            uint8_t requestedScl = doc["i2c_scl"].is<int>() ? (uint8_t)(int)doc["i2c_scl"] : sysCfg.i2c_scl;
            uint8_t requestedDisplayType = doc["display_enabled"].is<int>() ? (uint8_t)(int)doc["display_enabled"] : sysCfg.display_enabled;
            uint8_t requestedDisplayAddr = doc["display_i2c_addr"].is<int>() ? (uint8_t)(int)doc["display_i2c_addr"] : sysCfg.display_i2c_addr;
            String validationMessage;
            if (requestedStatusPin != 255 && requestedZcPin != 255 && requestedStatusPin == requestedZcPin) {
                validationMessage = "Status LED GPIO and Zero-Crossing GPIO cannot use the same pin";
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }
            if (requestedSda != 255 && requestedScl != 255 && requestedSda == requestedScl) {
                validationMessage = "I2C SDA and SCL pins cannot be the same";
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }
            if (requestedStatusPin != 255 && (requestedStatusPin == requestedSda || requestedStatusPin == requestedScl)) {
                validationMessage = "Status LED pin cannot overlap with I2C SDA or SCL";
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }
            if (requestedZcPin != 255 && (requestedZcPin == requestedSda || requestedZcPin == requestedScl)) {
                validationMessage = "Zero-Crossing pin cannot overlap with I2C SDA or SCL";
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }
            if (!displayAddressValid(requestedDisplayType, requestedDisplayAddr)) {
                validationMessage = "I2C display address is invalid for the selected display type";
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }
            if (savedOutputsUseReservedPin(requestedStatusPin, "Status LED", validationMessage)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }
            if (savedOutputsUseReservedPin(requestedZcPin, "Zero-Crossing", validationMessage)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }
            if (savedOutputsUseReservedPin(requestedSda, "I2C SDA", validationMessage)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }
            if (savedOutputsUseReservedPin(requestedScl, "I2C SCL", validationMessage)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }

            // Apply only fields present in the JSON payload (partial update support)
            if (doc["device_mode"].is<int>()) sysCfg.device_mode = doc["device_mode"];
            if (doc["eth_dhcp"].is<bool>()) sysCfg.eth_dhcp = doc["eth_dhcp"];
            if (doc["eth_ip"].is<const char*>()) copyConfigString(sysCfg.eth_ip, sizeof(sysCfg.eth_ip), doc["eth_ip"]);
            if (doc["eth_netmask"].is<const char*>()) copyConfigString(sysCfg.eth_netmask, sizeof(sysCfg.eth_netmask), doc["eth_netmask"]);
            if (doc["eth_gateway"].is<const char*>()) copyConfigString(sysCfg.eth_gateway, sizeof(sysCfg.eth_gateway), doc["eth_gateway"]);
            if (doc["eth_dns"].is<const char*>()) copyConfigString(sysCfg.eth_dns, sizeof(sysCfg.eth_dns), doc["eth_dns"]);
            if (doc["ap_ssid"].is<const char*>()) copyConfigString(sysCfg.ap_ssid, sizeof(sysCfg.ap_ssid), doc["ap_ssid"]);
            if (doc["ap_pass"].is<const char*>()) copyConfigString(sysCfg.ap_pass, sizeof(sysCfg.ap_pass), doc["ap_pass"]);
            if (doc["artnet_port"].is<int>()) {
                int port = doc["artnet_port"];
                sysCfg.artnet_port = (port > 0 && port <= 65535) ? port : 6454;
            }
            if (doc["sacn_enabled"].is<bool>()) sysCfg.sacn_enabled = doc["sacn_enabled"];
            if (doc["sacn_multicast"].is<bool>()) sysCfg.sacn_multicast = doc["sacn_multicast"];
            if (doc["sacn_port"].is<int>()) {
                int port = doc["sacn_port"];
                sysCfg.sacn_port = (port > 0 && port <= 65535) ? port : 5568;
            }
            if (doc["status_led_pin"].is<int>()) sysCfg.status_led_pin = doc["status_led_pin"];
            if (doc["zc_pin"].is<int>()) sysCfg.zc_pin = doc["zc_pin"];
            if (doc["i2c_sda"].is<int>()) sysCfg.i2c_sda = doc["i2c_sda"];
            if (doc["i2c_scl"].is<int>()) sysCfg.i2c_scl = doc["i2c_scl"];
            if (doc["i2c_speed"].is<int>()) sysCfg.i2c_speed = doc["i2c_speed"];
            if (doc["output_fps"].is<int>()) sysCfg.output_fps = doc["output_fps"];
            if (doc["wifi_ssid"].is<const char*>()) copyConfigString(sysCfg.wifi_ssid, sizeof(sysCfg.wifi_ssid), doc["wifi_ssid"]);
            if (doc["wifi_pass"].is<const char*>()) copyConfigString(sysCfg.wifi_pass, sizeof(sysCfg.wifi_pass), doc["wifi_pass"]);
            if (doc["wifi_dhcp"].is<bool>()) sysCfg.wifi_dhcp = doc["wifi_dhcp"];
            if (doc["wifi_ip"].is<const char*>()) copyConfigString(sysCfg.wifi_ip, sizeof(sysCfg.wifi_ip), doc["wifi_ip"]);
            if (doc["wifi_netmask"].is<const char*>()) copyConfigString(sysCfg.wifi_netmask, sizeof(sysCfg.wifi_netmask), doc["wifi_netmask"]);
            if (doc["wifi_gateway"].is<const char*>()) copyConfigString(sysCfg.wifi_gateway, sizeof(sysCfg.wifi_gateway), doc["wifi_gateway"]);
            if (doc["wifi_dns"].is<const char*>()) copyConfigString(sysCfg.wifi_dns, sizeof(sysCfg.wifi_dns), doc["wifi_dns"]);
            if (doc["wifi_enable_in_eth_mode"].is<bool>()) sysCfg.wifi_enable_in_eth_mode = doc["wifi_enable_in_eth_mode"];
            if (doc["ap_enable_in_eth_mode"].is<bool>()) sysCfg.ap_enable_in_eth_mode = doc["ap_enable_in_eth_mode"];
            if (doc["mdns_name"].is<const char*>()) copyConfigString(sysCfg.mdns_name, sizeof(sysCfg.mdns_name), doc["mdns_name"]);
            if (doc["display_enabled"].is<int>()) sysCfg.display_enabled = doc["display_enabled"];
            if (doc["display_i2c_addr"].is<int>()) sysCfg.display_i2c_addr = doc["display_i2c_addr"];
            if (doc["display_brightness"].is<int>()) sysCfg.display_brightness = doc["display_brightness"];

            saveConfig(sysCfg);
            request->send(200, "application/json", "{\"status\":\"ok\"}");

            // Perform reboot to apply pin layouts and network assignments
            xTaskCreate([](void*){
                vTaskDelay(pdMS_TO_TICKS(1500));
                ESP.restart();
            }, "rebootTask", 2048, NULL, 1, NULL);
        }
    );

    // API Route: Get Dynamic Output Channels
    server.on("/api/outputs", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!LittleFS.exists("/outputs.json")) {
            request->send(200, "application/json", "{\"outputs\":[{\"type\":3,\"pin\":4,\"start_universe\":0,\"start_address\":1,\"led_count\":170,\"color_order\":0}]}");
            return;
        }
        request->send(LittleFS, "/outputs.json", "application/json");
    });

    // API Route: Export Settings + Outputs as one backup JSON file
    server.on("/api/config/backup", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["device_model"] = "WT32-ETH01";
        doc["firmware_version"] = getFirmwareVersion();
        JsonObject settings = doc["settings"].to<JsonObject>();
        addSettingsToJson(settings);

        JsonArray outputs = doc["outputs"].to<JsonArray>();
        if (LittleFS.exists("/outputs.json")) {
            File file = LittleFS.open("/outputs.json", "r");
            JsonDocument outputsDoc;
            DeserializationError error = deserializeJson(outputsDoc, file);
            file.close();
            if (!error && outputsDoc["outputs"].is<JsonArray>()) {
                for (JsonVariant item : outputsDoc["outputs"].as<JsonArray>()) {
                    outputs.add(item);
                }
            }
        }
        if (outputs.size() == 0) {
            JsonObject item = outputs.add<JsonObject>();
            item["type"] = 3;
            item["pin"] = 4;
            item["start_universe"] = 0;
            item["start_address"] = 1;
            item["led_count"] = 170;
            item["color_order"] = 0;
        }

        String response;
        serializeJsonPretty(doc, response);
        AsyncWebServerResponse* res = request->beginResponse(200, "application/json", response);
        res->addHeader("Content-Disposition", "attachment; filename=wt32_eth01_config_backup.json");
        res->addHeader("Cache-Control", "no-store");
        request->send(res);
    });

    // API Route: Import Settings + Outputs from one backup JSON payload
    server.on("/api/config/import", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body;
            if (!collectRequestBody(request, data, len, index, total, body)) return;

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, body);
            if (error || !doc["settings"].is<JsonObject>() || !doc["outputs"].is<JsonArray>()) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid backup JSON. Expected settings object and outputs array.\"}");
                return;
            }

            JsonObjectConst settings = doc["settings"].as<JsonObjectConst>();
            JsonArray outputs = doc["outputs"].as<JsonArray>();
            String validationMessage;
            if (!validateOutputJson(outputs, validationMessage) ||
                !validateSettingsAndOutputs(settings, outputs, validationMessage)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }

            applySettingsFromJson(settings);
            saveConfig(sysCfg);

            JsonDocument outDoc;
            JsonArray outArr = outDoc["outputs"].to<JsonArray>();
            for (JsonVariant item : outputs) {
                outArr.add(item);
            }

            File file = LittleFS.open("/outputs.json", "w");
            if (!file) {
                request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to open outputs file for writing\"}");
                return;
            }
            size_t written = serializeJson(outDoc, file);
            file.close();
            if (written == 0) {
                request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to write outputs file\"}");
                return;
            }

            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration imported. Rebooting...\"}");
            response->addHeader("Connection", "close");
            response->addHeader("Cache-Control", "no-store");
            request->send(response);

            xTaskCreate([](void*) {
                vTaskDelay(pdMS_TO_TICKS(3000));
                ESP.restart();
            }, "importRebootTask", 2048, NULL, 1, NULL);
        }
    );

    // API Route: Save Dynamic Output Channels
    server.on("/api/outputs", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body;
            if (!collectRequestBody(request, data, len, index, total, body)) return;

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, body);
            if (error || !doc["outputs"].is<JsonArray>()) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid outputs JSON\"}");
                return;
            }
            JsonArray outputs = doc["outputs"].as<JsonArray>();
            String validationMessage;
            if (!validateOutputJson(outputs, validationMessage)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }
            if (sysCfg.status_led_pin != 255 && sysCfg.zc_pin != 255 && sysCfg.status_led_pin == sysCfg.zc_pin) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Status LED GPIO and Zero-Crossing GPIO cannot use the same pin\"}");
                return;
            }
            if (outputsUseReservedPin(outputs, sysCfg.status_led_pin, "Status LED", validationMessage)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }
            if (outputsUseReservedPin(outputs, sysCfg.zc_pin, "Zero-Crossing", validationMessage)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }
            if (outputsUseReservedPin(outputs, sysCfg.i2c_sda, "I2C SDA", validationMessage)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }
            if (outputsUseReservedPin(outputs, sysCfg.i2c_scl, "I2C SCL", validationMessage)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }
            if (outputsHaveDuplicateGpio(outputs, validationMessage)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }
            if (outputsHaveDuplicateExpanderChannel(outputs, validationMessage)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }

            // Check combined resource + compute scoring
            uint8_t fps = sysCfg.output_fps > 0 ? sysCfg.output_fps : 40;
            float score = totalCombinedScoreFromJson(outputs, fps);
            if (score > SCORE_LIMIT) {
                char msg[96];
                snprintf(msg, sizeof(msg), "Combined score %.1f (resource+compute) exceeds limit of %.0f at %d FPS. Reduce channels or FPS.", score, SCORE_LIMIT, fps);
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + String(msg) + "\"}");
                return;
            }

            doc["version"] = 3;
            File file = LittleFS.open("/outputs.json", "w");
            if (!file) {
                request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to open file for writing\"}");
                return;
            }
            size_t written = serializeJson(doc, file);
            file.close();
            if (written == 0) {
                request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to write outputs file\"}");
                return;
            }
            
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"status\":\"ok\",\"message\":\"Outputs saved. Rebooting...\"}");
            response->addHeader("Connection", "close");
            response->addHeader("Cache-Control", "no-store");
            request->send(response);

            // Perform reboot to apply new hardware configurations
            xTaskCreate([](void*){
                vTaskDelay(pdMS_TO_TICKS(3000));
                ESP.restart();
            }, "rebootTask", 2048, NULL, 1, NULL);
        }
    );

    // API Route: Manual Output Test
    server.on("/api/output-test", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body;
            if (!collectRequestBody(request, data, len, index, total, body)) return;

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, body);
            if (error) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
                return;
            }

            bool off = doc["off"] | false;
            if (off) {
                request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Output test stopped\"}");
                outputTestClearRequested = true;
                return;
            }

            int channelIndex = doc["index"] | -1;
            auto& channels = outputCtrl.getChannels();
            if (channelIndex < 0 || channelIndex >= (int)channels.size()) {
                request->send(404, "application/json", "{\"status\":\"error\",\"message\":\"Output channel not found\"}");
                return;
            }

            OutputChannel& ch = channels[channelIndex];
            if (ch.dmxBuffer == nullptr || ch.bufferSize == 0) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Output channel is not initialized\"}");
                return;
            }

            memset(ch.dmxBuffer, 0, ch.bufferSize);

            JsonArray values = doc["values"].as<JsonArray>();
            uint16_t pos = 0;
            for (JsonVariant value : values) {
                if (pos >= ch.bufferSize) break;
                int v = value.as<int>();
                ch.dmxBuffer[pos++] = constrain(v, 0, 255);
            }

            uint32_t durationMs = doc["duration_ms"] | 30000;
            if (durationMs < 1000) durationMs = 1000;
            if (durationMs > 300000) durationMs = 300000;

            outputTestIndex = channelIndex;
            outputTestUntil = millis() + durationMs;
            outputTestActive = true;
            refreshTestOutputNow();

            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Output test applied\"}");
        }
    );

    // API Route: Get Wi-Fi Scan Results
    server.on("/api/wifi-scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        int n = WiFi.scanComplete();
        if (n == -2) {
            WiFi.scanNetworks(true); // Start asynchronous scan
            request->send(202, "application/json", "{\"status\":\"scanning\"}");
        } else if (n == -1) {
            request->send(202, "application/json", "{\"status\":\"scanning\"}");
        } else {
            JsonDocument doc;
            JsonArray arr = doc["networks"].to<JsonArray>();
            for (int i = 0; i < n; ++i) {
                JsonObject item = arr.add<JsonObject>();
                item["ssid"] = WiFi.SSID(i);
                item["rssi"] = WiFi.RSSI(i);
                item["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
            }
            WiFi.scanDelete();
            
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        }
    });



    // API Route: Clear Output Channels (reset to default single LED strip)
    server.on("/api/outputs/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        JsonArray arr = doc["outputs"].to<JsonArray>();
        JsonObject item = arr.add<JsonObject>();
        item["type"] = 3;
        item["pin"] = 4;
        item["start_universe"] = 0;
        item["start_address"] = 1;
        item["led_count"] = 170;
        item["color_order"] = 0;

        File file = LittleFS.open("/outputs.json", "w");
        if (!file) {
            request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to write outputs file\"}");
            return;
        }
        serializeJson(doc, file);
        file.close();
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Outputs reset to default. Reboot to apply.\"}");
    });

    // API Route: Factory Reset (all settings + outputs)
    server.on("/api/config/factory-reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        // Clear all preferences
        Preferences prefs;
        prefs.begin("system", false);
        prefs.clear();
        prefs.end();

        // Reset outputs to default
        JsonDocument doc;
        JsonArray arr = doc["outputs"].to<JsonArray>();
        JsonObject item = arr.add<JsonObject>();
        item["type"] = 3;
        item["pin"] = 4;
        item["start_universe"] = 0;
        item["start_address"] = 1;
        item["led_count"] = 170;
        item["color_order"] = 0;

        File file = LittleFS.open("/outputs.json", "w");
        if (file) {
            serializeJson(doc, file);
            file.close();
        }

        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Factory reset complete. Rebooting...\"}");

        xTaskCreate([](void*){
            vTaskDelay(pdMS_TO_TICKS(1500));
            ESP.restart();
        }, "factoryResetTask", 2048, NULL, 1, NULL);
    });

    // API Route: Get ESP-NOW Peers
    server.on("/api/espnow-peers", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!LittleFS.exists("/espnow_peers.json")) {
            request->send(200, "application/json", "{\"peers\":[]}");
            return;
        }
        request->send(LittleFS, "/espnow_peers.json", "application/json");
    });

    // API Route: Save ESP-NOW Peers
    server.on("/api/espnow-peers", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body;
            if (!collectRequestBody(request, data, len, index, total, body)) return;

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, body);
            if (error || !doc["peers"].is<JsonArray>()) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid peers JSON\"}");
                return;
            }

            for (JsonVariant peer : doc["peers"].as<JsonArray>()) {
                if (!peer.is<JsonObject>()) {
                    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Each peer must be a route object\"}");
                    return;
                }
                JsonObject obj = peer.as<JsonObject>();
                if (!obj["mac"].is<const char*>()) {
                    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Each peer route needs a MAC address\"}");
                    return;
                }
                uint16_t su = obj["start_universe"] | 0;
                uint16_t sa = obj["start_address"] | 1;
                uint16_t eu = obj["end_universe"] | su;
                uint16_t ea = obj["end_address"] | 512;
                if (sa < 1 || sa > 512 || ea < 1 || ea > 512 || eu < su || (eu == su && ea < sa)) {
                    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid peer DMX range\"}");
                    return;
                }
            }

            File file = LittleFS.open("/espnow_peers.json", "w");
            if (!file) {
                request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to open file for writing\"}");
                return;
            }
            serializeJson(doc, file);
            file.close();
            
            // Reload peers in ESP-NOW controller
            espNowCtrl.loadPeers();

            request->send(200, "application/json", "{\"status\":\"ok\"}");
        }
    );

    // API Route: Get Telemetry Live Status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["device_mode"] = sysCfg.device_mode;
        
        // Network connectivity status
        bool wifiUp = WiFi.isConnected();
        doc["eth_up"]  = ethConnected;
        doc["wifi_up"] = wifiUp;
        doc["ap_up"]   = apActive;
        doc["wifi_ssid_active"] = wifiUp ? WiFi.SSID().c_str() : "";

        // Active IP (Ethernet priority)
        IPAddress localIp;
        if (ethConnected) {
            localIp = ETH.localIP();
        } else if (wifiUp) {
            localIp = WiFi.localIP();
        } else if (apActive) {
            localIp = WiFi.softAPIP();
        }
        doc["ip"] = (ethConnected || wifiUp || apActive) ? localIp.toString() : "Not Connected";

        doc["packets_received"] = ArtNetControl::packetCount;
        doc["sacn_packets"] = sacnCtrl.getPacketCount();
        doc["output_fps"]  = sysCfg.output_fps;
        doc["time"]     = (sysCfg.device_mode == MODE_ARTNET_ETHERNET) ? "Art-Net" :
                          (sysCfg.device_mode == MODE_ESPNOW_MASTER) ? "ESP-NOW Master" :
                          (sysCfg.device_mode == MODE_ESPNOW_SLAVE) ? "ESP-NOW Slave" : "Unknown";
        doc["active"]   = systemActive;

        // System resource metrics
        doc["heap_free"]  = ESP.getFreeHeap();
        doc["min_heap"]   = ESP.getMinFreeHeap();
        doc["cpu_freq"]   = ESP.getCpuFreqMHz();

        // Board MAC address (useful for ESP-NOW peer config)
        doc["board_mac"] = WiFi.macAddress();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API Route: sACN Protocol Status
    server.on("/api/sacn/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["enabled"] = sysCfg.sacn_enabled;
        doc["multicast_mode"] = sysCfg.sacn_multicast;
        doc["packets_received"] = sacnCtrl.getPacketCount();
        doc["errors"] = sacnCtrl.getErrorCount();
        doc["active_universes"] = sacnCtrl.getActiveUniverses();
        doc["listening_port"] = sysCfg.sacn_port;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API Route: I2C Bus Scan (Async, queue-based on Core 0)
    server.on("/api/i2c-scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (i2cScanPending) {
            request->send(202, "application/json", "{\"status\":\"scanning\"}");
            return;
        }
        if (request->hasParam("refresh")) {
            i2cScanRequested = true;
            if (i2cScanJson == "[]") {
                request->send(202, "application/json", "{\"status\":\"scanning\"}");
                return;
            }
        }
        if (i2cScanMutex) xSemaphoreTake(i2cScanMutex, portMAX_DELAY);
        String resp = i2cScanJson;
        if (i2cScanMutex) xSemaphoreGive(i2cScanMutex);
        request->send(200, "application/json", resp);
    });

    // API Route: Output Resource + Compute Scoring
    server.on("/api/scoring", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        std::vector<OutputChannel>& chs = outputCtrl.getChannels();
        uint8_t fps = sysCfg.output_fps > 0 ? sysCfg.output_fps : 40;
        float resourceTotal = totalOutputScore(chs);
        float computeTotal = totalComputeScore(chs, fps);
        float combinedTotal = resourceTotal + computeTotal;
        doc["resource_total"] = resourceTotal;
        doc["compute_total"] = computeTotal;
        doc["compute_detail"] = totalComputeScore(chs, 0);
        doc["fps_factor"] = fpsComputeFactor(fps);
        doc["total"] = combinedTotal;
        doc["limit"] = SCORE_LIMIT;
        doc["remaining"] = SCORE_LIMIT - combinedTotal;
        JsonArray arr = doc["channels"].to<JsonArray>();
        for (size_t i = 0; i < chs.size(); i++) {
            JsonObject item = arr.add<JsonObject>();
            item["index"] = (uint8_t)i;
            item["type"] = chs[i].type;
            item["name"] = outputTypeName(chs[i].type);
            item["resource_score"] = outputChannelScore(chs[i]);
            item["compute_score"] = channelComputeScore(chs[i]);
        }
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API Route: OTA Web Update Post Handler
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool shouldReboot = otaUpdateOk && !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(
            shouldReboot ? 200 : 500,
            "text/plain",
            shouldReboot ? "OK - Rebooting..." : ("FAIL: " + otaUpdateError)
        );
        response->addHeader("Connection", "close");
        request->send(response);
        if (shouldReboot) {
            xTaskCreate(restartAfterOtaTask, "otaRestart", 2048, NULL, 1, NULL);
        } else {
            otaInProgress = false;
        }
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!index) {
            Serial.printf("Update Start: %s\n", filename.c_str());
            otaUpdateOk = false;
            otaUpdateError = "";
            otaInProgress = true;
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                otaUpdateError = Update.errorString();
                if (otaUpdateError.length() == 0) otaUpdateError = "Update.begin failed";
                Update.printError(Serial);
            }
        }
        if (len > 0 && !Update.hasError()) {
            if (Update.write(data, len) != len) {
                otaUpdateError = Update.errorString();
                if (otaUpdateError.length() == 0) otaUpdateError = "Update.write failed";
                Update.printError(Serial);
            }
        }
        if (final) {
            if (Update.end(true)) {
                Serial.printf("Update Success: %uB\nRebooting...\n", index + len);
                otaUpdateOk = true;
            } else {
                otaUpdateError = Update.errorString();
                if (otaUpdateError.length() == 0) otaUpdateError = "Update.end failed";
                Update.printError(Serial);
            }
        }
    });

    // Start Server
    server.begin();
    Serial.println("Async Web Server started");
}

// Helper: compute active universe range from output channels
uint16_t activeUniverseMin() {
    uint16_t minU = 0xFFFF;
    bool found = false;
    for (const auto& ch : outputCtrl.getChannels()) {
        if (ch.type != 3) {
            uint16_t u = ch.start_universe;
            if (u < minU) { minU = u; found = true; }
        }
    }
    return found ? minU : 0;
}
uint16_t activeUniverseMax() {
    uint16_t maxU = 0;
    for (const auto& ch : outputCtrl.getChannels()) {
        if (ch.type != 3) {
            uint16_t u = ch.start_universe;
            if (u > maxU) maxU = u;
        }
    }
    return maxU;
}

// ----------------------------------------------------
// RTOS TASK: Core 0 (Network Operations)
// ----------------------------------------------------
void networkTask(void* pvParameters) {
    Serial.println("Network Task started on Core 0");
    
    // Initialize Networking Services
    if (sysCfg.device_mode == MODE_ARTNET_ETHERNET || sysCfg.device_mode == MODE_ESPNOW_MASTER) {
        artNetCtrl.begin();
        
        // Initialize sACN if enabled
        if (sysCfg.sacn_enabled) {
            sacnCtrl.begin(sysCfg.sacn_multicast);
        }
    }
    
    while (true) {
        if (otaInProgress) {
            updateStatusLedPattern();
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        // Run network stack loops
        artNetCtrl.loop();
        reconnectWiFiClient();
        
        // Process sACN packets if enabled
        if (sysCfg.sacn_enabled && (sysCfg.device_mode == MODE_ARTNET_ETHERNET || sysCfg.device_mode == MODE_ESPNOW_MASTER)) {
            sacnCtrl.process();
        }
        
        // ESP-NOW Slave keeps AP active so it can always be configured in the field.

        // Update I2C Display (non-blocking, ~2 Hz)
        if (display.isActive()) {
            static unsigned long lastDisplayUpdate = 0;
            unsigned long now = millis();
            if (now - lastDisplayUpdate >= 500) {
                lastDisplayUpdate = now;

                // Build display lines
                IPAddress ip = ETH.localIP();
                String line1 = "IP " + (ethConnected ? ip.toString() : String("--.---.-.--"));
                String line2 = ethConnected ? "ETH Link Up" : (WiFi.isConnected() ? "Wi-Fi Connected" : "No Link");
                String line3 = "U " + String(activeUniverseMin()) + "-" + String(activeUniverseMax());
                String line4 = "FPS " + String(sysCfg.output_fps) + "  Heap " + String(ESP.getFreeHeap() / 1024) + "K";

                display.update(line1, line2, line3, line4);
            }
        }

        // Process queued I2C scan request (runs on Core 0, non-blocking for HTTP)
        if (i2cScanRequested && !i2cScanPending) {
            i2cScanRequested = false;
            i2cScanPending = true;

            JsonDocument doc;
            JsonArray arr = doc.to<JsonArray>();
            if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                for (uint8_t addr = 1; addr < 128; addr++) {
                    Wire.beginTransmission(addr);
                    uint8_t error = Wire.endTransmission();
                    if (error == 0) {
                        JsonObject obj = arr.add<JsonObject>();
                        obj["address"] = addr;
                        char hex[6];
                        snprintf(hex, sizeof(hex), "0x%02X", addr);
                        obj["hex"] = hex;
                        String usedBy = "";
                        int idx = 0;
                        for (const auto& ch : outputCtrl.getChannels()) {
                            idx++;
                            if ((ch.source == 1 || (ch.source >= 2 && ch.source <= 4)) && ch.pca_addr == addr) {
                                if (usedBy.length() > 0) usedBy += ", ";
                                usedBy += "CH" + String(idx);
                            }
                            if (ch.pin2_source >= 1 && ch.pin2_source <= 4 && ch.pin2_addr == addr) {
                                if (usedBy.length() > 0) usedBy += ", ";
                                usedBy += "CH" + String(idx) + "(P2)";
                            }
                            if (ch.pin3_source >= 1 && ch.pin3_source <= 4 && ch.pin3_addr == addr) {
                                if (usedBy.length() > 0) usedBy += ", ";
                                usedBy += "CH" + String(idx) + "(P3)";
                            }
                            if (ch.pin4_source >= 2 && ch.pin4_source <= 4 && ch.pin4_addr == addr) {
                                if (usedBy.length() > 0) usedBy += ", ";
                                usedBy += "CH" + String(idx) + "(HOM)";
                            }
                        }
                        if (usedBy.length() > 0) obj["used_by"] = usedBy;
                    }
                }
                xSemaphoreGive(i2cMutex);
            }

            String result;
            serializeJson(doc, result);
            if (i2cScanMutex) xSemaphoreTake(i2cScanMutex, portMAX_DELAY);
            i2cScanJson = result;
            if (i2cScanMutex) xSemaphoreGive(i2cScanMutex);

            i2cScanPending = false;
        }

        // Yield to let ESP32 TCP stack run smoothly
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ----------------------------------------------------
// RTOS TASK: Core 1 (DMX / LED Engine Rendering)
// ----------------------------------------------------
void outputTask(void* pvParameters) {
    Serial.println("Output Task started on Core 1");

    while (true) {
        if (otaInProgress) {
            updateStatusLedPattern();
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (outputTestClearRequested) {
            clearOutputTest();
        }

        // Run outputs update clock loop (triggers DMX serial send)
        outputCtrl.loop();

        if (outputTestActive) {
            if ((int32_t)(millis() - outputTestUntil) >= 0) {
                clearOutputTest();
            } else {
                outputCtrl.updateLeds();
                motionCtrl.update();
                updateStatusLedPattern();
                vTaskDelay(pdMS_TO_TICKS(2));
                continue;
            }
        }

        // Process queued ESP-NOW packets (Core 1 safe processing)
        if (espNowQueue != NULL) {
            EspNowDmxPacket packet;
            while (xQueueReceive(espNowQueue, &packet, 0) == pdTRUE) {
                uint16_t dmxLen = packet.length;
                bool matched = outputCtrl.mapDmxDataToChannels(packet.universe, packet.data, dmxLen, false, packet.offset);

                // Keep the local universe-0 frame buffer available for shared output state.
                if (packet.universe == 0) {
                    memcpy(EspNowControl::rxDmxBuffer + packet.offset, packet.data, dmxLen);
                    EspNowControl::rxDmxLength = min((uint16_t)512, packet.totalLength);
                    matched = true;
                }

                if (matched) {
                    EspNowControl::lastRxTime = millis();
                    EspNowControl::newRxData = true;
                }
            }
        }

        // If in ESP-NOW Slave mode, check for incoming wireless packages
        if (sysCfg.device_mode == MODE_ESPNOW_SLAVE) {
            if (EspNowControl::newRxData) {
                memcpy(activeDmxBuffer, EspNowControl::rxDmxBuffer, EspNowControl::rxDmxLength);
                EspNowControl::newRxData = false;
                systemActive = true;
                lastDmxUpdateTime = millis();
                
                outputCtrl.updateLeds();
            }
        }

        // If network DMX is active, copy and display data on Core 1 loop
        if ((sysCfg.device_mode == MODE_ARTNET_ETHERNET || sysCfg.device_mode == MODE_ESPNOW_MASTER) && networkFramePending.exchange(false)) {
            // Render local LED strips
            outputCtrl.updateLeds();
            ArtNetControl::newRxData = false;
        }

        updateStatusLedPattern();

        // Minor yield to prevent core hogging
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

// Recovery Mode tracking
#define RECOVERY_BUTTON_PIN 0
RTC_DATA_ATTR uint32_t bootCount = 0;
bool isRecoveryMode = false;

void resetBootCountTask(void *pvParameters) {
    vTaskDelay(15000 / portTICK_PERIOD_MS);
    bootCount = 0;
    Serial.println("Stable boot confirmed, boot count reset.");
    vTaskDelete(NULL);
}

void setup() {
    Serial.begin(115200);
    delay(100);

    bootCount++;
    Serial.printf("Boot count: %d\n", bootCount);

    if (bootCount >= 3) {
        Serial.println("Recovery Mode Triggered! (3 consecutive resets)");
        isRecoveryMode = true;
        bootCount = 0; // Reset so next boot is normal
    }

    if (!LittleFS.begin(true)) {
        Serial.println("An Error has occurred while mounting LittleFS");
    }

    // Always load system configuration
    loadConfig(sysCfg);
    blinkStatusLed(5, 250, 250);

    pinMode(RECOVERY_BUTTON_PIN, INPUT_PULLUP);
    if (digitalRead(RECOVERY_BUTTON_PIN) == LOW) {
        Serial.println("Recovery Mode Triggered by GPIO0 button.");
        isRecoveryMode = true;
        bootCount = 0;
    }

    if (isRecoveryMode) {
        startRecoveryEthernetOnly();
        return; // Skip normal setup
    }

    // Bring up network first so Wi-Fi/Ethernet current spikes do not overlap output driver init.
    setupNetwork();

    // Initialize shared I2C bus if valid pins are set
    if (sysCfg.i2c_sda != 255 && sysCfg.i2c_scl != 255) {
        Wire.begin(sysCfg.i2c_sda, sysCfg.i2c_scl, sysCfg.i2c_speed);
        Serial.printf("I2C initialized: SDA = GPIO %d, SCL = GPIO %d, Speed = %d Hz\n",
                      sysCfg.i2c_sda, sysCfg.i2c_scl, sysCfg.i2c_speed);
    }
    i2cMutex = xSemaphoreCreateMutex();
    i2cScanMutex = xSemaphoreCreateMutex();

    // Initialize I2C Display
    display.begin(sysCfg.display_enabled, sysCfg.display_i2c_addr);
    if (display.isActive()) {
        display.setBrightness(sysCfg.display_brightness);
        Serial.printf("Display initialized: type=%d, addr=0x%02X\n", sysCfg.display_enabled, sysCfg.display_i2c_addr);
    }

    // Initialize Outputs System after network startup current settles.
    delay(250);
    outputCtrl.begin();
    dimmerCtrl.begin();
    motionCtrl.begin();

    // Initialize ESP-NOW wireless transceiver if device mode requires it.
    if (sysCfg.device_mode == MODE_ESPNOW_MASTER || sysCfg.device_mode == MODE_ESPNOW_SLAVE) {
        espNowQueue = xQueueCreate(16, sizeof(EspNowDmxPacket));
        espNowCtrl.begin();
    }

    // Configure mDNS Responder
    if (MDNS.begin(sysCfg.mdns_name)) {
        Serial.printf("mDNS responder started: http://%s.local\n", sysCfg.mdns_name);
        MDNS.addService("http", "tcp", 80);
    }

    // Configure Web Server endpoints
    setupWebServer();

    // Create Dual-Core concurrent processing loops
    xTaskCreatePinnedToCore(
        networkTask,    /* Task function */
        "networkTask",  /* Name of task */
        8192,           /* Stack size in words */
        NULL,           /* Task input parameter */
        3,              /* Priority of the task */
        NULL,           /* Task handle */
        0               /* Core ID (Network loop on Core 0) */
    );

    xTaskCreatePinnedToCore(
        outputTask,     /* Task function */
        "outputTask",   /* Name of task */
        8192,           /* Stack size in words */
        NULL,           /* Task input parameter */
        4,              /* Priority of the task (Higher priority for outputs) */
        NULL,           /* Task handle */
        1               /* Core ID (Output loop on Core 1) */
    );

    // Count the boot as successful only after setup is complete and the system stays alive.
    xTaskCreate(resetBootCountTask, "ResetBoot", 2048, NULL, 1, NULL);
}

void loop() {
    if (isRecoveryMode) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return;
    }
    // The dual-core FreeRTOS task loops handle execution. The main Arduino loop remains idle.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
