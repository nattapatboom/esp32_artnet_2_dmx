#include <Arduino.h>
#include <atomic>
#include <map>
#include <new>
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
#include "i2c_devices/i2c_bus.h"
#include "i2c_devices/pwm_expander.h"
#include "i2c_devices/i2c_gpio_expander.h"
#include "output_control.h"
#include "output_impl.h"
#include "output_devices/dimmer.h"
#include "motion_control.h"
#include "espnow_control.h"
#include "lighting_protocols/artnet_control.h"
#include "lighting_protocols/sacn_control.h"
#include "i2c_devices/display_driver.h"
#include "scoring.h"
#include "source_rules.h"
#include "display_protocol.h"
#include "gpio_control.h"
#include "network_protocol.h"
#include "ota_control.h"
#include "recovery_control.h"

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
std::atomic<unsigned long> lastDmxUpdateTime{0};
std::atomic<bool> systemActive{false};
std::atomic<bool> networkFramePending(false);

OutputControl outputCtrl;
ArtNetControl artNetCtrl;
SACNControl sacnCtrl;
EspNowControl espNowCtrl;
MotionControl motionCtrl;
DimmerControl dimmerCtrl;
hw_timer_t *dimmerTimer = NULL;
volatile uint32_t dimmer_tick = 0;
volatile DimmerCh dimmerChannels[MAX_DIMMER_CHANNELS];
volatile uint8_t numDimmerChannels = 0;
volatile bool dimmerEnabled = false;

PwmExpanderManager pwmExpanderManager;
DigitalExpanderManager digitalExpanderManager;

void updateMotionControl() {
    motionCtrl.update();
}

// Static member definitions (out-of-class, C++14 compatible)
unsigned long ArtNetControl::packetCount = 0;
uint8_t EspNowControl::rxDmxBuffer[512] = {0};
uint16_t EspNowControl::rxDmxLength = 0;
unsigned long EspNowControl::lastRxTime = 0;
std::atomic<bool> EspNowControl::newRxData(false);

AsyncWebServer* serverPtr = nullptr;

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
SemaphoreHandle_t swapMutex = NULL;
String i2cScanJson = "[]";
volatile bool i2cScanRequested = false;
volatile bool i2cScanPending = false;
QueueHandle_t espNowQueue = NULL;
DisplayDriver display;
std::atomic<bool> outputTestActive{false};
std::atomic<bool> outputTestClearRequested{false};
std::atomic<int> outputTestIndex{-1};
std::atomic<unsigned long> outputTestUntil{0};

// Network functions forward declarations
void startAP();
void stopAP();
void setupNetwork();
void setupWebServer();
void startWiFiClient(bool keepApActive = false);


void configureWiFiRadioForBoot() {
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
}

void resetEthernetPhy() {
    pinMode(ETH_PHY_POWER, OUTPUT);
    digitalWrite(ETH_PHY_POWER, LOW);
    vTaskDelay(pdMS_TO_TICKS(150));
    digitalWrite(ETH_PHY_POWER, HIGH);
    vTaskDelay(pdMS_TO_TICKS(300));
}

void powerDownEthernetPhy() {
    pinMode(ETH_PHY_POWER, OUTPUT);
    digitalWrite(ETH_PHY_POWER, LOW);
    vTaskDelay(pdMS_TO_TICKS(150));
}

void blinkStatusLed(uint8_t times, uint16_t onMs, uint16_t offMs) {
    if (sysCfg.status_led_pin == 255) return;
    pinMode(sysCfg.status_led_pin, OUTPUT);
    for (uint8_t i = 0; i < times; i++) {
        digitalWrite(sysCfg.status_led_pin, HIGH);
        vTaskDelay(pdMS_TO_TICKS(onMs));
        digitalWrite(sysCfg.status_led_pin, LOW);
        vTaskDelay(pdMS_TO_TICKS(offMs));
    }
}

void clearOutputTest() {
    for (auto& ch : outputCtrl.getChannels()) {
        if (ch.shadowBuffer != nullptr && ch.bufferSize > 0) {
            memset(ch.shadowBuffer, 0, ch.bufferSize);
        }
    }
    outputCtrl.swapBuffers();
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
    if (!force && now - lastWifiReconnectAttempt < sysCfg.wifi_reconnect_interval) return;
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
        String* buffer = new (std::nothrow) String();
        if (buffer == nullptr || (total > 0 && !buffer->reserve(total))) {
            delete buffer;
            request->_tempObject = nullptr;
            request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to allocate request buffer\"}");
            return false;
        }
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

template <typename F>
bool forEachOutputGpioPin(JsonObjectConst output, F&& callback) {
    uint8_t source = output["source"] | 0;
    uint8_t type = output["type"] | 0;
    uint8_t mcMode = output["mc_mode"] | 0;
    const OutputDefs::OutputModeDef* def = OutputDefs::modeDef(type, mcMode);
    if (def == nullptr) return false;

    JsonArrayConst segPins = output["seg_pins"].as<JsonArrayConst>();
    JsonArrayConst segSources = output["seg_sources"].as<JsonArrayConst>();
    bool hasSegPins = !segPins.isNull();
    bool sevenSegCommonDim = def->pins == OutputDefs::PINS_7SEG_COMMON_DIM;
    bool sevenSeg = def->pins == OutputDefs::PINS_7SEG_DIRECT ||
                    def->pins == OutputDefs::PINS_7SEG_DIMMED ||
                    sevenSegCommonDim;
    int basePin = output["pin"] | 255;

    auto routeSourceForSlot = [&](uint8_t slotIndex) -> uint8_t {
        if (slotIndex == 0) return source;
        if (sevenSeg && hasSegPins) {
            uint8_t segIndex = sevenSegCommonDim ? (uint8_t)(slotIndex - 1) : slotIndex;
            if (segIndex < segSources.size()) return segSources[segIndex] | 0;
            return 0;
        }
        if (slotIndex == 1) return output["pin2_source"] | 0;
        if (slotIndex == 2) return output["pin3_source"] | 0;
        if (slotIndex == 3) return output["pin4_source"] | 0;
        return 255;
    };

    auto pinForSlot = [&](uint8_t slotIndex) -> int {
        if (sevenSeg) {
            uint8_t segIndex = sevenSegCommonDim ? (slotIndex == 0 ? 255 : (uint8_t)(slotIndex - 1)) : slotIndex;
            if (hasSegPins && segIndex != 255 && segIndex < segPins.size()) return segPins[segIndex] | 255;
            if (slotIndex == 0) return basePin;
            if (slotIndex == 1) return output["pin2"] | ((basePin != 255) ? basePin + 1 : 255);
            if (slotIndex == 2) return output["pin3"] | ((basePin != 255) ? basePin + 2 : 255);
            if (slotIndex == 3) return output["pin4"] | ((basePin != 255) ? basePin + 3 : 255);
            return (basePin != 255) ? basePin + slotIndex : 255;
        }
        if (slotIndex == 0) return output["pin"] | 255;
        if (slotIndex == 1) return output["pin2"] | 255;
        if (slotIndex == 2) return output["pin3"] | 255;
        if (slotIndex == 3) return output["pin4"] | 255;
        return 255;
    };

    for (uint8_t slot = 0; slot < def->pinCount; slot++) {
        uint8_t routeSource = routeSourceForSlot(slot);
        if (!OutputDefs::pinSlotUsesGpio(type, mcMode, slot, routeSource)) continue;
        int pin = pinForSlot(slot);
        if (pin == 255 || pin < 0) continue;
        if (callback(pin, def->pins[slot].label)) return true;
    }
    return false;
}

bool isHexDigitChar(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

char upperHexChar(char c) {
    if (c >= 'a' && c <= 'f') return (char)(c - 'a' + 'A');
    return c;
}

bool normalizeEspNowPeerMac(const char* input, char* output, size_t outputSize) {
    if (input == nullptr || output == nullptr || outputSize < 18) return false;
    if (strlen(input) != 17) return false;
    const char sep = input[2];
    if (sep != ':' && sep != '-') return false;
    for (uint8_t i = 0; i < 17; i++) {
        if ((i % 3) == 2) {
            if (input[i] != sep) return false;
            output[i] = ':';
        } else {
            if (!isHexDigitChar(input[i])) return false;
            output[i] = upperHexChar(input[i]);
        }
    }
    output[17] = '\0';
    return true;
}

bool outputJsonUsesPin(JsonObjectConst output, uint8_t reservedPin) {
    if (reservedPin == 255) return false;
    return forEachOutputGpioPin(output, [&](int pin, const char*) {
        return pin == reservedPin;
    });
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

// Check for GPIO34,35,36,39 (input-only) and Ethernet/system reserved pins.
// GPIO12 (MTDI bootstrap) is warning-only in the Web UI because existing field hardware may use it.
bool outputsUseForbiddenGpio(JsonArray outputs, String& message) {
    uint8_t channel = 1;
    for (JsonObject output : outputs) {
        auto forbid = [&](int rawPin, const char* label) -> bool {
            if (rawPin == 255 || rawPin < 0) return false;
            uint8_t p = (uint8_t)rawPin;
            if (GpioControl::isReservedEthernetPin(p)) {
                message = "GPIO " + String(p) + " is reserved for Ethernet and cannot be used as an output on channel " + String(channel) + label;
                return true;
            }
            if (GpioControl::isInputOnlyPin(p)) {
                message = "GPIO " + String(p) + " is input-only and cannot be used as an output on channel " + String(channel) + label;
                return true;
            }
            return false;
        };

        if (forEachOutputGpioPin(output, [&](int pin, const char* label) {
            return forbid(pin, (String(" ") + label).c_str());
        })) return true;
        channel++;
    }
    return false;
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
        if (forEachOutputGpioPin(output, [&](int pin, const char*) {
            return addPin(pin, channel);
        })) return true;
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
        if (source == 0 || rawChannel == 255 || rawChannel < 0 || rawChannel > 15) return false;
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
        const OutputDefs::OutputModeDef* def = OutputDefs::modeDef(type, mcMode);
        bool primaryIsSevenSegA = def != nullptr && def->primaryRouteIsSegment;
        if (source != 0 && !primaryIsSevenSegA) {
            static const char* const pcaChanKeys[] = {"pca_channel","pca_channel2","pca_channel3","pca_channel4"};
            static const char* const pinSrcKeys[]  = {nullptr,"pin2_source","pin3_source","pin4_source"};
            static const char* const pinAddrKeys[] = {nullptr,"pin2_addr","pin3_addr","pin4_addr"};
            uint8_t pinCount = def != nullptr ? def->pinCount : 1;
            for (uint8_t slot = 0; slot < pinCount && slot < 4; slot++) {
                const OutputDefs::PinRule* pr = (def != nullptr) ? &def->pins[slot] : nullptr;
                if (pr == nullptr || !(pr->sources & (OutputDefs::SRC_PWM_EXPANDER | OutputDefs::SRC_DIGITAL_EXPANDER))) continue;
                uint8_t slotSrc  = (slot == 0) ? source : (uint8_t)(output[pinSrcKeys[slot]] | 0);
                if (slotSrc == 0) continue;
                uint8_t slotAddr = (slot == 0) ? address : (uint8_t)(output[pinAddrKeys[slot]] | (slotSrc == 1 ? 0x40 : 0x20));
                int rawCh = output[pcaChanKeys[slot]] | (slot == 0 ? 0 : 255);
                if (addChannel(slotSrc, slotAddr, rawCh, outputIndex)) return true;
            }
        }

        uint8_t pin2Source = output["pin2_source"] | 0;
        uint8_t pin3Source = output["pin3_source"] | 0;
        uint8_t pin4Source = output["pin4_source"] | 0;

        uint8_t segmentCount = def != nullptr ? def->segmentCount : 0;
        bool is7SegDD = segmentCount > 0;
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
                uint8_t numSeg = segmentCount;
                for (uint8_t s = 0; s < numSeg; s++) {
                    if (addChannel(pin2Source, pin2Addr, baseCh != 255 ? baseCh + s : 255, outputIndex)) return true;
                }
            } else if (output.containsKey("seg_sources")) {
                JsonArrayConst segSources = output["seg_sources"].as<JsonArrayConst>();
                JsonArrayConst segAddrs = output["seg_addrs"].as<JsonArrayConst>();
                JsonArrayConst segChannels = output["seg_channels"].as<JsonArrayConst>();
                uint8_t numSeg = segmentCount;
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

uint8_t defaultI2cAddressForSource(uint8_t source) {
    if (source == 1) return 0x40;
    if (source == 8) return 0x10;
    if (source == 9) return 0x54;
    if (source == 10) return 0x58;
    if (source == 5) return 0x60;
    if (source == 6 || source == 7) return 0x4C;
    return 0x20;
}

bool outputsHaveI2cAddressConflict(JsonArray outputs, uint8_t displayType, uint8_t displayAddr, String& message) {
    struct UsedAddress {
        uint8_t address;
        uint8_t source;
        uint8_t outputIndex;
    };
    UsedAddress used[32];
    uint8_t usedCount = 0;

    auto addAddress = [&](uint8_t source, uint8_t address, uint8_t outputIndex) -> bool {
        if (source == 0) return false;
        if (displayType != DisplayProtocol::DTYPE_NONE && address == displayAddr) {
            message = "I2C address 0x" + String(address, HEX) + " on output channel " +
                      String(outputIndex) + " conflicts with the configured display address";
            return true;
        }
        for (uint8_t i = 0; i < usedCount; i++) {
            if (used[i].address == address && used[i].source != source) {
                message = "I2C address 0x" + String(address, HEX) + " is used by incompatible source " +
                          String(used[i].source) + " on output channel " + String(used[i].outputIndex) +
                          " and source " + String(source) + " on output channel " + String(outputIndex);
                return true;
            }
        }
        if (usedCount < 32) used[usedCount++] = {address, source, outputIndex};
        return false;
    };

    uint8_t outputIndex = 1;
    for (JsonObject output : outputs) {
        uint8_t type = output["type"] | 0;
        uint8_t mcMode = output["mc_mode"] | 0;
        const OutputDefs::OutputModeDef* def = OutputDefs::modeDef(type, mcMode);
        uint8_t source = output["source"] | 0;
        if (source != 0 && addAddress(source, output["pca_addr"] | defaultI2cAddressForSource(source), outputIndex)) return true;

        uint8_t pin2Source = output["pin2_source"] | 0;
        uint8_t pin3Source = output["pin3_source"] | 0;
        uint8_t pin4Source = output["pin4_source"] | 0;
        if (pin2Source != 0 && addAddress(pin2Source, output["pin2_addr"] | defaultI2cAddressForSource(pin2Source), outputIndex)) return true;
        if (pin3Source != 0 && addAddress(pin3Source, output["pin3_addr"] | defaultI2cAddressForSource(pin3Source), outputIndex)) return true;
        if (pin4Source != 0 && addAddress(pin4Source, output["pin4_addr"] | defaultI2cAddressForSource(pin4Source), outputIndex)) return true;

        if (def != nullptr && def->segmentCount > 0 && pin2Source == 0 && output.containsKey("seg_sources")) {
            JsonArrayConst segSources = output["seg_sources"].as<JsonArrayConst>();
            JsonArrayConst segAddrs = output["seg_addrs"].as<JsonArrayConst>();
            for (uint8_t s = 0; s < def->segmentCount && s < segSources.size(); s++) {
                uint8_t segSource = segSources[s] | 0;
                if (segSource != 0) {
                    uint8_t segAddr = s < segAddrs.size() ? (uint8_t)(segAddrs[s] | defaultI2cAddressForSource(segSource)) : defaultI2cAddressForSource(segSource);
                    if (addAddress(segSource, segAddr, outputIndex)) return true;
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
    target["espnow_channel"] = sysCfg.espnow_channel;
    target["espnow_chunk_size"] = sysCfg.espnow_chunk_size;
    target["artnet_enabled"] = sysCfg.artnet_enabled;
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
    target["display_refresh_ms"] = sysCfg.display_refresh_ms;
    target["display_recover_ms"] = sysCfg.display_recover_ms;
    target["display_cols"] = sysCfg.display_cols;
    target["display_rows"] = sysCfg.display_rows;
    target["web_port"] = sysCfg.web_port;
    target["espnow_queue_depth"] = sysCfg.espnow_queue_depth;
    target["wifi_reconnect_interval"] = sysCfg.wifi_reconnect_interval;
    target["default_output_type"] = sysCfg.default_output_type;
    target["default_output_pin"] = sysCfg.default_output_pin;
    target["default_led_count"] = sysCfg.default_led_count;
    target["artnet_short_name"] = sysCfg.artnet_short_name;
    target["artnet_long_name"] = sysCfg.artnet_long_name;
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
    if (doc["espnow_channel"].is<int>()) sysCfg.espnow_channel = doc["espnow_channel"];
    if (doc["espnow_chunk_size"].is<int>()) {
        sysCfg.espnow_chunk_size = doc["espnow_chunk_size"];
        if (sysCfg.espnow_chunk_size < 16 || sysCfg.espnow_chunk_size > 230) sysCfg.espnow_chunk_size = 200;
    }
    if (doc["artnet_enabled"].is<bool>()) sysCfg.artnet_enabled = doc["artnet_enabled"];
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
    if (doc["display_refresh_ms"].is<int>()) {
        int v = doc["display_refresh_ms"];
        if (v >= 100 && v <= 10000) sysCfg.display_refresh_ms = v;
    }
    if (doc["display_recover_ms"].is<int>()) {
        int v = doc["display_recover_ms"];
        if (v >= 1000 && v <= 60000) sysCfg.display_recover_ms = v;
    }
    if (doc["display_cols"].is<int>()) {
        int v = doc["display_cols"];
        if (v >= 16 && v <= 40) sysCfg.display_cols = v;
    }
    if (doc["display_rows"].is<int>()) {
        int v = doc["display_rows"];
        if (v >= 2 && v <= 4) sysCfg.display_rows = v;
    }
    if (doc["web_port"].is<int>()) {
        int v = doc["web_port"];
        if (v >= 1 && v <= 65535) sysCfg.web_port = v;
    }
    if (doc["espnow_queue_depth"].is<int>()) {
        int v = doc["espnow_queue_depth"];
        if (v >= 4 && v <= 64) sysCfg.espnow_queue_depth = v;
    }
    if (doc["wifi_reconnect_interval"].is<int>()) {
        int v = doc["wifi_reconnect_interval"];
        if (v >= 1000 && v <= 60000) sysCfg.wifi_reconnect_interval = v;
    }
    if (doc["default_output_type"].is<int>()) sysCfg.default_output_type = doc["default_output_type"];
    if (doc["default_output_pin"].is<int>()) sysCfg.default_output_pin = doc["default_output_pin"];
    if (doc["default_led_count"].is<int>()) {
        int v = doc["default_led_count"];
        if (v >= 1 && v <= 1000) sysCfg.default_led_count = v;
    }
    if (doc["artnet_short_name"].is<const char*>()) copyConfigString(sysCfg.artnet_short_name, sizeof(sysCfg.artnet_short_name), doc["artnet_short_name"]);
    if (doc["artnet_long_name"].is<const char*>()) copyConfigString(sysCfg.artnet_long_name, sizeof(sysCfg.artnet_long_name), doc["artnet_long_name"]);
}

bool validateSettingsAndOutputs(JsonObjectConst settings, JsonArray outputs, String& message) {
    if (settings.containsKey(NetworkProtocol::KEY_OUTPUT_FPS)) {
        int fps = settings[NetworkProtocol::KEY_OUTPUT_FPS] | 0;
        if (fps < NetworkProtocol::OUTPUT_FPS_MIN || fps > NetworkProtocol::OUTPUT_FPS_MAX) {
            message = "Global Output FPS must be between " +
                      String(NetworkProtocol::OUTPUT_FPS_MIN) + " and " +
                      String(NetworkProtocol::OUTPUT_FPS_MAX);
            return false;
        }
    }
    auto validateIpField = [&](const char* key) -> bool {
        if (settings[key].is<const char*>()) {
            if (!NetworkProtocol::ip4Valid(settings[key] | "")) {
                message = String(key) + " is not a valid IPv4 address";
                return false;
            }
        }
        return true;
    };
    if (!validateIpField(NetworkProtocol::KEY_ETH_IP)) return false;
    if (!validateIpField(NetworkProtocol::KEY_ETH_NETMASK)) return false;
    if (!validateIpField(NetworkProtocol::KEY_ETH_GATEWAY)) return false;
    if (!validateIpField(NetworkProtocol::KEY_ETH_DNS)) return false;
    if (!validateIpField(NetworkProtocol::KEY_WIFI_IP)) return false;
    if (!validateIpField(NetworkProtocol::KEY_WIFI_NETMASK)) return false;
    if (!validateIpField(NetworkProtocol::KEY_WIFI_GATEWAY)) return false;
    if (!validateIpField(NetworkProtocol::KEY_WIFI_DNS)) return false;
    uint8_t statusPin = settings[NetworkProtocol::KEY_STATUS_LED_PIN].is<int>() ? (uint8_t)(int)settings[NetworkProtocol::KEY_STATUS_LED_PIN] : sysCfg.status_led_pin;
    uint8_t zcPin = settings[NetworkProtocol::KEY_ZC_PIN].is<int>() ? (uint8_t)(int)settings[NetworkProtocol::KEY_ZC_PIN] : sysCfg.zc_pin;
    uint8_t sdaPin = settings[NetworkProtocol::KEY_I2C_SDA].is<int>() ? (uint8_t)(int)settings[NetworkProtocol::KEY_I2C_SDA] : sysCfg.i2c_sda;
    uint8_t sclPin = settings[NetworkProtocol::KEY_I2C_SCL].is<int>() ? (uint8_t)(int)settings[NetworkProtocol::KEY_I2C_SCL] : sysCfg.i2c_scl;
    uint8_t displayType = settings[NetworkProtocol::KEY_DISPLAY_ENABLED].is<int>() ? (uint8_t)(int)settings[NetworkProtocol::KEY_DISPLAY_ENABLED] : sysCfg.display_enabled;
    uint8_t displayAddr = settings[NetworkProtocol::KEY_DISPLAY_I2C_ADDR].is<int>() ? (uint8_t)(int)settings[NetworkProtocol::KEY_DISPLAY_I2C_ADDR] : sysCfg.display_i2c_addr;

    auto validateSystemOutputPin = [&](uint8_t pin, const char* label) -> bool {
        if (pin == 255) return true;
        if (GpioControl::isReservedEthernetPin(pin)) {
            message = String(label) + " GPIO " + String(pin) + " is reserved for Ethernet";
            return false;
        }
        if (GpioControl::isInputOnlyPin(pin)) {
            message = String(label) + " GPIO " + String(pin) + " is input-only";
            return false;
        }
        return true;
    };
    auto validateSystemInputPin = [&](uint8_t pin, const char* label) -> bool {
        if (pin == 255) return true;
        if (GpioControl::isReservedEthernetPin(pin)) {
            message = String(label) + " GPIO " + String(pin) + " is reserved for Ethernet";
            return false;
        }
        return true;
    };

    if (!validateSystemOutputPin(statusPin, "Status LED")) return false;
    if (!validateSystemInputPin(zcPin, "Zero-Crossing")) return false;
    if (!validateSystemOutputPin(sdaPin, "I2C SDA")) return false;
    if (!validateSystemOutputPin(sclPin, "I2C SCL")) return false;

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
    if (!DisplayProtocol::addressValid(displayType, displayAddr)) {
        message = "I2C display address is invalid for the selected display type";
        return false;
    }

    if (settings.containsKey(NetworkProtocol::KEY_I2C_SPEED)) {
        int i2cSpeed = settings[NetworkProtocol::KEY_I2C_SPEED] | 0;
        if (!NetworkProtocol::i2cSpeedValid((uint32_t)i2cSpeed)) {
            message = "I2C speed must be 100000, 400000, or 1000000";
            return false;
        }
    }

    uint8_t deviceMode = settings[NetworkProtocol::KEY_DEVICE_MODE].is<int>() ? (uint8_t)(int)settings[NetworkProtocol::KEY_DEVICE_MODE] : sysCfg.device_mode;
    uint8_t espnowChan = settings[NetworkProtocol::KEY_ESPNOW_CHANNEL].is<int>() ? (uint8_t)(int)settings[NetworkProtocol::KEY_ESPNOW_CHANNEL] : sysCfg.espnow_channel;
    if (deviceMode == MODE_ESPNOW_MASTER && espnowChan == 0) {
        message = "ESP-NOW Master cannot use Auto-Scan channel mode";
        return false;
    }

    bool artnetEnabled = settings.containsKey(NetworkProtocol::KEY_ARTNET_ENABLED) ? settings[NetworkProtocol::KEY_ARTNET_ENABLED].as<bool>() : sysCfg.artnet_enabled;
    bool sacnEnabled = settings.containsKey(NetworkProtocol::KEY_SACN_ENABLED) ? settings[NetworkProtocol::KEY_SACN_ENABLED].as<bool>() : sysCfg.sacn_enabled;
    if (!artnetEnabled && !sacnEnabled) {
        message = "Cannot disable both Art-Net and sACN protocols";
        return false;
    }

    if (settings.containsKey(NetworkProtocol::KEY_MDNS_NAME)) {
        const char* mdns = settings[NetworkProtocol::KEY_MDNS_NAME];
        size_t len = strlen(mdns);
        if (len < 1 || len > NetworkProtocol::MDNS_NAME_MAX_LEN) {
            message = "mDNS hostname must be 1-" + String(NetworkProtocol::MDNS_NAME_MAX_LEN) + " characters";
            return false;
        }
    }

    if (outputsUseReservedPin(outputs, statusPin, "Status LED", message)) return false;
    if (outputsUseReservedPin(outputs, zcPin, "Zero-Crossing", message)) return false;
    if (outputsUseReservedPin(outputs, sdaPin, "I2C SDA", message)) return false;
    if (outputsUseReservedPin(outputs, sclPin, "I2C SCL", message)) return false;
    if (outputsUseForbiddenGpio(outputs, message)) return false;
    if (outputsHaveDuplicateGpio(outputs, message)) return false;
    if (outputsHaveDuplicateExpanderChannel(outputs, message)) return false;
    if (outputsHaveI2cAddressConflict(outputs, displayType, displayAddr, message)) return false;
    for (JsonObjectConst output : outputs) {
        if ((uint8_t)(output["type"] | 0) == OutputDefs::TYPE_DIMMER && zcPin == 255) {
            message = "Zero-Crossing pin is not configured (GPIO 255). AC Dimmer outputs require a ZC pin to operate.";
            return false;
        }
    }
    return true;
}

bool validateScoresForSettingsAndOutputs(JsonObjectConst settings, JsonArray outputs, String& message) {
    int fpsRaw = settings[NetworkProtocol::KEY_OUTPUT_FPS].is<int>() ? (int)settings[NetworkProtocol::KEY_OUTPUT_FPS] : sysCfg.output_fps;
    if (fpsRaw < NetworkProtocol::OUTPUT_FPS_MIN || fpsRaw > NetworkProtocol::OUTPUT_FPS_MAX) {
        message = "Global Output FPS must be between " +
                  String(NetworkProtocol::OUTPUT_FPS_MIN) + " and " +
                  String(NetworkProtocol::OUTPUT_FPS_MAX);
        return false;
    }
    uint8_t fps = (uint8_t)fpsRaw;
    uint8_t oldMode = sysCfg.device_mode;
    uint16_t oldChunkSize = sysCfg.espnow_chunk_size;

    if (settings[NetworkProtocol::KEY_DEVICE_MODE].is<int>()) sysCfg.device_mode = settings[NetworkProtocol::KEY_DEVICE_MODE];
    if (settings[NetworkProtocol::KEY_ESPNOW_CHUNK_SIZE].is<int>()) {
        int chunkSize = settings[NetworkProtocol::KEY_ESPNOW_CHUNK_SIZE];
        sysCfg.espnow_chunk_size = NetworkProtocol::espnowChunkSizeValid((uint16_t)chunkSize) ? (uint16_t)chunkSize : NetworkProtocol::ESPNOW_CHUNK_SIZE_DEFAULT;
    }

    ScoreBlocker blocker = checkScoresFromJson(outputs, fps);

    sysCfg.device_mode = oldMode;
    sysCfg.espnow_chunk_size = oldChunkSize;

    if (blocker == ScoreBlocker::None) return true;
    char msg[96];
    snprintf(msg, sizeof(msg), "%s at %d FPS. Reduce channels or FPS.", scoreBlockerName(blocker), fps);
    message = msg;
    return false;
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
        if (type == OutputDefs::TYPE_DMX && source == 0) { // DMX on local GPIO
            dmxCount++;
        } else if (type == OutputDefs::TYPE_DFPLAYER) { // DFPlayer
            dfPlayerCount++;
        } else if (type == OutputDefs::TYPE_LED_STRIP) { // LED Strip
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
    if (outputsUseForbiddenGpio(outputs, message)) return false;

    uint8_t channelNumber = 1;
    for (JsonObject output : outputs) {
        uint8_t type = output["type"] | 0;
        uint8_t source = output["source"] | 0;
        if (type > OutputDefs::TYPE_SMOKE) {
            message = "Invalid output type on channel " + String(channelNumber) + ".";
            return false;
        }
        uint8_t mcMode = output["mc_mode"] | 0;
        const OutputDefs::OutputModeDef* def = OutputDefs::modeDef(type, mcMode);
        if (type == OutputDefs::TYPE_DAC && source == 0) {
            message = "DAC output does not support ESP32 GPIO (source 0) on WT32-ETH01; use I2C DAC (source 5-7) on channel " + String(channelNumber);
            return false;
        }
        auto sourceForSlot = [&](uint8_t slotIndex) -> uint8_t {
            if (def != nullptr && def->segmentCount > 0) {
                if (def->pinCount > def->segmentCount && slotIndex == 0) return source;
                uint8_t baseSource = output["pin2_source"] | 0;
                if (baseSource != 0) return baseSource;
                uint8_t segIdx = (def->pinCount > def->segmentCount) ? slotIndex - 1 : slotIndex;
                JsonArrayConst segSources = output["seg_sources"].as<JsonArrayConst>();
                return (!segSources.isNull() && segIdx < segSources.size()) ? (uint8_t)(segSources[segIdx] | 0) : 0;
            }
            if (slotIndex == 0) return source;
            if (slotIndex == 1) return output["pin2_source"] | 0;
            if (slotIndex == 2) return output["pin3_source"] | 0;
            if (slotIndex == 3) return output["pin4_source"] | 0;
            return 0;
        };
        if (def == nullptr) {
            message = "Invalid output mode on channel " + String(channelNumber);
            return false;
        }
        for (uint8_t slot = 0; slot < def->pinCount; slot++) {
            if (!OutputDefs::sourceAllowedForSlot(type, mcMode, slot, sourceForSlot(slot))) {
                message = "Unsupported source for " + String(def->pins[slot].label) + " on channel " + String(channelNumber);
                return false;
            }
        }
        if (source != 0 && !(source >= 5 && source <= 7)) {
            uint8_t addr = output["pca_addr"] | defaultI2cAddressForSource(source);
            if (!SourceRules::validateAddress(source, addr, "Primary I2C source on channel " + String(channelNumber), message)) return false;
            uint8_t pcaChan = output["pca_channel"] | 255;
            if (pcaChan != 255 && pcaChan > 15) {
                message = "Primary I2C channel must be 0-15 on channel " + String(channelNumber);
                return false;
            }
        }
        auto checkHybridChan = [&](const String& label, const char* srcKey, const char* chanKey) -> bool {
            uint8_t src = output[srcKey] | 0;
            if (src >= 1 && src <= 4) {
                uint8_t ch = output[chanKey] | 255;
                if (ch != 255 && ch > 15) {
                    message = label + " channel must be 0-15 on channel " + String(channelNumber);
                    return false;
                }
            }
            return true;
        };
        if (!checkHybridChan("Pin 2", "pin2_source", "pin2_channel")) return false;
        if (!checkHybridChan("Pin 3", "pin3_source", "pin3_channel")) return false;
        if (!checkHybridChan("Pin 4", "pin4_source", "pin4_channel")) return false;
        if (type == OutputDefs::TYPE_DAC && source >= 5 && source <= 7) {
            uint8_t addr = output["pca_addr"] | defaultI2cAddressForSource(source);
            uint8_t dacChannel = output["pca_channel"] | 0;
            if (!SourceRules::addressValid(source, addr)) {
                message = "I2C DAC source on channel " + String(channelNumber) + " has invalid address 0x" + String(addr, HEX) + ". " + SourceRules::addressRangeLabel(source);
                return false;
            }
            if (source != 7 && dacChannel != 0) {
                message = "Only DAC7573 supports DAC channels B-D on channel " + String(channelNumber);
                return false;
            }
            if (source == 7 && dacChannel > 3) {
                message = "DAC7573 channel must be 0-3 on channel " + String(channelNumber);
                return false;
            }
        }
        if (def != nullptr && def->segmentCount > 0) {
            uint8_t pin2Source = output["pin2_source"] | 0;
            if (pin2Source != 0 && pin2Source > 4) {
                message = "Unsupported segment expander source on channel " + String(channelNumber);
                return false;
            }
            if (pin2Source != 0) {
                uint8_t pin2Addr = output["pin2_addr"] | defaultI2cAddressForSource(pin2Source);
                if (!SourceRules::validateAddress(pin2Source, pin2Addr, "Segment base I2C source on channel " + String(channelNumber), message)) return false;
            }
            if ((mcMode == 4 || mcMode == 5) && pin2Source >= 2 && pin2Source <= 4) {
                message = "7-Segment Direct Dim requires ESP32 GPIO or PCA9685 segment source on channel " + String(channelNumber);
                return false;
            }
            if (pin2Source != 0 && (int)(output["pin2_channel"] | 255) == 255) {
                message = "Segment expander base channel is missing on channel " + String(channelNumber);
                return false;
            }
            if (pin2Source >= 1 && pin2Source <= 4) {
                uint8_t baseCh = output["pin2_channel"] | 255;
                uint8_t numSeg = def->segmentCount;
                if (baseCh != 255 && (uint8_t)(baseCh + numSeg - 1) > 15) {
                    message = "7-Segment base channel " + String(baseCh) + " with " + String(numSeg) + " segments exceeds channel 15 on channel " + String(channelNumber);
                    return false;
                }
            }
            if (output.containsKey("seg_sources")) {
                JsonArrayConst segSources = output["seg_sources"].as<JsonArrayConst>();
                JsonArrayConst segAddrs = output["seg_addrs"].as<JsonArrayConst>();
                JsonArrayConst segChannels = output["seg_channels"].as<JsonArrayConst>();
                uint8_t numSeg = def->segmentCount;
                for (int s = 0; s < segSources.size(); s++) {
                    uint8_t sSrc = segSources[s] | 0;
                    if (sSrc != 0 && sSrc > 4) {
                        message = "Unsupported segment expander source on channel " + String(channelNumber);
                        return false;
                    }
                    if (sSrc != 0) {
                        uint8_t sAddr = (s < segAddrs.size()) ? (uint8_t)(segAddrs[s] | defaultI2cAddressForSource(sSrc)) : defaultI2cAddressForSource(sSrc);
                        if (!SourceRules::validateAddress(sSrc, sAddr, "Segment " + String(s + 1) + " I2C source on channel " + String(channelNumber), message)) return false;
                    }
                    if ((mcMode == 4 || mcMode == 5) && sSrc >= 2 && sSrc <= 4) {
                        message = "7-Segment Direct Dim requires ESP32 GPIO or PCA9685 segment source on channel " + String(channelNumber);
                        return false;
                    }
                    if (s < numSeg && sSrc != 0 && (s >= segChannels.size() || (int)(segChannels[s] | 255) == 255)) {
                        message = "Segment expander channel is missing on channel " + String(channelNumber);
                        return false;
                    }
                    if (s < numSeg && sSrc != 0 && (int)(segChannels[s] | 255) != 255 && ((int)(segChannels[s] | 0) > 15)) {
                        message = "Segment expander channel must be 0-15 on channel " + String(channelNumber);
                        return false;
                    }
                }
            }
        }
        if (type == OutputDefs::TYPE_STEPPER) {
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
                uint8_t pin2Addr = output["pin2_addr"] | defaultI2cAddressForSource(pin2Source);
                if (!SourceRules::validateAddress(pin2Source, pin2Addr, "Stepper DIR I2C source on channel " + String(channelNumber), message)) return false;
            }
            if (pin3Source != 0) {
                uint8_t pin3Addr = output["pin3_addr"] | defaultI2cAddressForSource(pin3Source);
                if (!SourceRules::validateAddress(pin3Source, pin3Addr, "Stepper ENABLE I2C source on channel " + String(channelNumber), message)) return false;
            }
            if (homingMode == 0 && pin4Source != 0) {
                uint8_t pin4Addr = output["pin4_addr"] | defaultI2cAddressForSource(pin4Source);
                if (!SourceRules::validateAddress(pin4Source, pin4Addr, "Stepper HOME I2C source on channel " + String(channelNumber), message)) return false;
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
        if (type == OutputDefs::TYPE_ANALOG_RGB) {
            uint8_t pin2Source = output["pin2_source"] | 0;
            uint8_t pin3Source = output["pin3_source"] | 0;
            uint8_t pin4Source = output["pin4_source"] | 0;
            uint8_t colorOrder = output["color_order"] | 0;
            if (source > 1 || pin2Source > 1 || pin3Source > 1 || pin4Source > 1) {
                message = "Analog RGB/RGBW channels only support ESP32 GPIO or PCA9685 sources on channel " + String(channelNumber);
                return false;
            }
            if (pin2Source != 0) {
                uint8_t pin2Addr = output["pin2_addr"] | defaultI2cAddressForSource(pin2Source);
                if (!SourceRules::validateAddress(pin2Source, pin2Addr, "Analog RGB/RGBW Green I2C source on channel " + String(channelNumber), message)) return false;
            }
            if (pin3Source != 0) {
                uint8_t pin3Addr = output["pin3_addr"] | defaultI2cAddressForSource(pin3Source);
                if (!SourceRules::validateAddress(pin3Source, pin3Addr, "Analog RGB/RGBW Blue I2C source on channel " + String(channelNumber), message)) return false;
            }
            if (colorOrder >= 4 && pin4Source != 0) {
                uint8_t pin4Addr = output["pin4_addr"] | defaultI2cAddressForSource(pin4Source);
                if (!SourceRules::validateAddress(pin4Source, pin4Addr, "Analog RGB/RGBW White I2C source on channel " + String(channelNumber), message)) return false;
            }
            if ((OutputDefs::isPwmExpanderSource(source) && (int)(output["pca_channel"] | 255) == 255) ||
                (OutputDefs::isPwmExpanderSource(pin2Source) && (int)(output["pin2_channel"] | 255) == 255) ||
                (OutputDefs::isPwmExpanderSource(pin3Source) && (int)(output["pin3_channel"] | 255) == 255) ||
                (colorOrder >= 4 && OutputDefs::isPwmExpanderSource(pin4Source) && (int)(output["pin4_channel"] | 255) == 255)) {
                message = "Analog RGB/RGBW PWM Expander channel is missing on channel " + String(channelNumber);
                return false;
            }
        }
        if (type == OutputDefs::TYPE_SMOKE) {
            uint8_t pin2Source = output["pin2_source"] | 0;
            if (pin2Source != 0 && !OutputDefs::isDigitalExpanderSource(pin2Source)) {
                message = "Unsupported smoke shooter hybrid pin source on channel " + String(channelNumber);
                return false;
            }
            if (pin2Source != 0) {
                uint8_t pin2Addr = output["pin2_addr"] | defaultI2cAddressForSource(pin2Source);
                if (!SourceRules::validateAddress(pin2Source, pin2Addr, "Smoke Shooter shoot I2C source on channel " + String(channelNumber), message)) return false;
            }
            if (pin2Source != 0 && (int)(output["pin2_channel"] | 255) == 255) {
                message = "Smoke Shooter shoot expander channel is missing on channel " + String(channelNumber);
                return false;
            }
        }
        if (type == OutputDefs::TYPE_MOTOR) {
            uint8_t pin2Source = output["pin2_source"] | 0;
            uint8_t pin3Source = output["pin3_source"] | 0;
            if ((pin2Source != 0 && !OutputDefs::isDigitalExpanderSource(pin2Source)) ||
                (pin3Source != 0 && !OutputDefs::isPwmExpanderSource(pin3Source))) {
                message = "Unsupported motor hybrid pin source on channel " + String(channelNumber);
                return false;
            }
            if (pin2Source != 0) {
                uint8_t pin2Addr = output["pin2_addr"] | defaultI2cAddressForSource(pin2Source);
                if (!SourceRules::validateAddress(pin2Source, pin2Addr, "Motor IN2/DIR I2C source on channel " + String(channelNumber), message)) return false;
            }
            if (pin3Source != 0) {
                uint8_t pin3Addr = output["pin3_addr"] | defaultI2cAddressForSource(pin3Source);
                if (!SourceRules::validateAddress(pin3Source, pin3Addr, "Motor EN I2C source on channel " + String(channelNumber), message)) return false;
            }
        }
        if (type == OutputDefs::TYPE_MOTOR && (uint8_t)(output["mc_mode"] | 0) == 2) {
            uint8_t enSource = output["pin3_source"] | 0;
            if (enSource != 0 && !OutputDefs::isPwmExpanderSource(enSource)) {
                message = "Motor EN pin needs PWM; use ESP32 GPIO or PWM Expander on channel " + String(channelNumber);
                return false;
            }
            if (OutputDefs::isPwmExpanderSource(enSource) && (int)(output["pin3_channel"] | 255) == 255) {
                message = "Motor EN PWM Expander channel is missing on channel " + String(channelNumber);
                return false;
            }
        }
        if (type == OutputDefs::TYPE_PWM_DAC) {
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
        {
            uint16_t uni = output["start_universe"] | 0;
            if (uni > 32767) {
                message = "start_universe must be 0-32767 on channel " + String(channelNumber);
                return false;
            }
        }
        if (type == OutputDefs::TYPE_LED_STRIP) {
            uint16_t lc = output["led_count"] | 0;
            if (lc < 1 || lc > 1360) {
                message = "LED count must be 1-1360 on channel " + String(channelNumber);
                return false;
            }
        }
        {
            uint16_t sa = output["start_address"] | 1;
            if (sa < 1 || sa > 512) {
                message = "start_address must be 1-512 on channel " + String(channelNumber);
                return false;
            }
            if (!OutputDefs::startsAtFirstUniverse(type, mcMode)) {
                OutputChannel temp;
                temp.type = type;
                temp.led_count = output["led_count"] | 0;
                temp.color_order = output["color_order"] | 0;
                temp.mc_resolution = output["mc_resolution"] | 8;
                temp.mc_mode = mcMode;
                uint16_t byteCount = outputDmxByteCount(temp);
                if (byteCount > 0 && (uint32_t)sa + byteCount - 1U > 512U) {
                    message = "DMX address range exceeds universe 512 on channel " + String(channelNumber);
                    return false;
                }
            }
        }
        channelNumber++;
    }
    // PCA9685 shared frequency conflict detection (non-blocking warning)
    {
        struct PcaRequirement { uint16_t freq; bool isServo; };
        std::map<uint8_t, PcaRequirement> pcaFreqMap;
        for (JsonObject output : outputs) {
            uint8_t type = output["type"] | 0;
            uint8_t source = output["source"] | 0;
            if (source != 1) continue;
            uint8_t addr = output["pca_addr"] | 0x40;
            auto it = pcaFreqMap.find(addr);
            if (type == OutputDefs::TYPE_SERVO) { // servo forces 50 Hz
                if (it != pcaFreqMap.end() && !it->second.isServo) {
                    Serial.printf("[WARN] PCA9685 at 0x%02X: servo (50 Hz) conflicts with other PWM types on same chip\n", addr);
                }
                pcaFreqMap[addr] = {50, true};
            } else {
                if (it != pcaFreqMap.end() && it->second.isServo) {
                    Serial.printf("[WARN] PCA9685 at 0x%02X: non-servo channel conflicts with servo (50 Hz) on same chip\n", addr);
                }
                pcaFreqMap[addr] = {200, false};
            }
        }
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

    String activeSsid = String(sysCfg.ap_ssid);
    if (activeSsid == "ESP32-ArtNet-Setup") {
        String mac = WiFi.macAddress();
        mac.replace(":", "");
        String suffix = mac.substring(mac.length() - 4);
        activeSsid = activeSsid + "-" + suffix;
    }

    bool keepSta = wifiClientConfigured() || WiFi.isConnected();
    WiFi.mode(keepSta ? WIFI_AP_STA : WIFI_AP);
    configureWiFiRadioForBoot();
    WiFi.enableAP(true);
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    
    int apChannel = (sysCfg.espnow_channel > 0) ? sysCfg.espnow_channel : 1;
    if (strlen(sysCfg.ap_pass) >= 8) {
        WiFi.softAP(activeSsid.c_str(), sysCfg.ap_pass, apChannel);
    } else {
        WiFi.softAP(activeSsid.c_str(), NULL, apChannel);
    }
    
    Serial.printf("Access Point started: SSID = %s, IP = %s\n", activeSsid.c_str(), WiFi.softAPIP().toString().c_str());
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
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Determine whether to launch wireless services immediately or stay wired-only.
    if (sysCfg.device_mode == MODE_ESPNOW_SLAVE) {
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
                vTaskDelay(pdMS_TO_TICKS(100));
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

void startRecoveryMode() {
    WiFi.persistent(false);
    configureWiFiRadioForBoot();
    WiFi.mode(WIFI_AP_STA);
    String recoverySsid = "ESP32-ArtNet-Recovery";
    {
        String mac = WiFi.macAddress();
        mac.replace(":", "");
        String suffix = mac.substring(mac.length() - 4);
        recoverySsid = recoverySsid + "-" + suffix;
    }
    WiFi.softAP(recoverySsid.c_str(), NULL, 1);
    Serial.printf("Recovery AP started: %s\n", recoverySsid.c_str());

    WiFi.onEvent(onWiFiEvent);
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
    Serial.println("Recovery Mode ready on Ethernet + AP. Connect to " + recoverySsid);
}

void setupWebServer() {
    if (!serverPtr) {
        serverPtr = new AsyncWebServer(sysCfg.web_port);
    }
    // Serve Setup UI Page
    serverPtr->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse* response = request->beginResponse_P(200, "text/html; charset=utf-8", CONFIG_HTML_GZ, CONFIG_HTML_GZ_LEN);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    });

    // API Route: Get Config settings
    serverPtr->on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["device_mode"] = sysCfg.device_mode;
        doc["eth_dhcp"] = sysCfg.eth_dhcp;
        doc["eth_ip"] = sysCfg.eth_ip;
        doc["eth_netmask"] = sysCfg.eth_netmask;
        doc["eth_gateway"] = sysCfg.eth_gateway;
        doc["eth_dns"] = sysCfg.eth_dns;
        doc["ap_ssid"] = sysCfg.ap_ssid;
        doc["ap_pass"] = sysCfg.ap_pass;
        doc["espnow_channel"] = sysCfg.espnow_channel;
        doc["artnet_enabled"] = sysCfg.artnet_enabled;
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
    serverPtr->on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, 
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body;
            if (!collectRequestBody(request, data, len, index, total, body)) return;

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, body);
            if (error) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
                return;
            }

            String validationMessage;
            JsonDocument outputsDoc;
            JsonArray savedOutputs = outputsDoc["outputs"].to<JsonArray>();
            if (LittleFS.exists("/outputs.json")) {
                File outputsFile = LittleFS.open("/outputs.json", "r");
                DeserializationError outputsError = deserializeJson(outputsDoc, outputsFile);
                outputsFile.close();
                if (outputsError || !outputsDoc["outputs"].is<JsonArray>()) {
                    request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Saved outputs file is invalid\"}");
                    return;
                }
                savedOutputs = outputsDoc["outputs"].as<JsonArray>();
            }
            if (!validateSettingsAndOutputs(doc.as<JsonObjectConst>(), savedOutputs, validationMessage) ||
                !validateScoresForSettingsAndOutputs(doc.as<JsonObjectConst>(), savedOutputs, validationMessage)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }

            applySettingsFromJson(doc.as<JsonObjectConst>());

            if (!saveConfig(sysCfg)) {
                request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save config to NVS\"}");
                return;
            }
            request->send(200, "application/json", "{\"status\":\"ok\"}");

            // Perform reboot to apply pin layouts and network assignments
            xTaskCreate([](void*){
                vTaskDelay(pdMS_TO_TICKS(1500));
                ESP.restart();
            }, "rebootTask", 2048, NULL, 1, NULL);
        }
    );

    // API Route: Get Dynamic Output Channels
    serverPtr->on("/api/outputs", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!LittleFS.exists("/outputs.json")) {
            request->send(200, "application/json", "{\"outputs\":[{\"type\":3,\"pin\":4,\"start_universe\":0,\"start_address\":1,\"led_count\":170,\"color_order\":0}]}");
            return;
        }
        request->send(LittleFS, "/outputs.json", "application/json");
    });

    // API Route: Export Settings + Outputs as one backup JSON file
    serverPtr->on("/api/config/backup", HTTP_GET, [](AsyncWebServerRequest *request) {
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
    serverPtr->on("/api/config/import", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
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
                !validateSettingsAndOutputs(settings, outputs, validationMessage) ||
                !validateScoresForSettingsAndOutputs(settings, outputs, validationMessage)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                return;
            }

            applySettingsFromJson(settings);
            if (!saveConfig(sysCfg)) {
                request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save config to NVS\"}");
                return;
            }

            JsonDocument outDoc;
            outDoc["version"] = 3;
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
    serverPtr->on("/api/outputs", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
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

            {
                // Run full pin-conflict validation + score check against current settings
                JsonDocument settingsDoc;
                settingsDoc["output_fps"] = sysCfg.output_fps;
                settingsDoc["status_led_pin"] = sysCfg.status_led_pin;
                settingsDoc["zc_pin"] = sysCfg.zc_pin;
                settingsDoc["i2c_sda"] = sysCfg.i2c_sda;
                settingsDoc["i2c_scl"] = sysCfg.i2c_scl;
                settingsDoc["display_enabled"] = sysCfg.display_enabled;
                settingsDoc["display_i2c_addr"] = sysCfg.display_i2c_addr;
                if (!validateSettingsAndOutputs(settingsDoc.as<JsonObjectConst>(), outputs, validationMessage)) {
                    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                    return;
                }
                if (!validateScoresForSettingsAndOutputs(settingsDoc.as<JsonObjectConst>(), outputs, validationMessage)) {
                    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationMessage + "\"}");
                    return;
                }
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

    // API Route: Calculate resource scores/budgets dynamically
    serverPtr->on("/api/outputs/score", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body;
            if (!collectRequestBody(request, data, len, index, total, body)) return;

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, body);
            if (error || !doc["outputs"].is<JsonArray>()) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid request JSON\"}");
                return;
            }

            JsonArrayConst arr = doc["outputs"].as<JsonArrayConst>();
            int fpsRaw = doc["fps"].is<int>() ? (int)doc["fps"] : (int)sysCfg.output_fps;
            uint8_t fps = (uint8_t)constrain(fpsRaw, (int)NetworkProtocol::OUTPUT_FPS_MIN, (int)NetworkProtocol::OUTPUT_FPS_MAX);

            // Temporarily swap device mode and chunk size for peer cost calculation
            uint8_t oldMode = sysCfg.device_mode;
            uint16_t oldChunkSize = sysCfg.espnow_chunk_size;
            if (doc["device_mode"].is<int>()) sysCfg.device_mode = doc["device_mode"];
            if (doc["espnow_chunk_size"].is<int>()) sysCfg.espnow_chunk_size = doc["espnow_chunk_size"];

            HardwareResource hw = totalHardwareFromJson(arr);
            CpuBudget cpu = totalCpuFromJson(arr, fps);
            RamBudget ram = totalRamFromJson(arr);

            sysCfg.device_mode = oldMode;
            sysCfg.espnow_chunk_size = oldChunkSize;

            uint32_t cpuLimit = CpuBudget::limit(fps);
            uint32_t ramLimit = RamBudget::limit();

            bool hwBad = !hardwareWithinLimit(hw);
            bool cpuBad = cpu.usPerFrame > cpuLimit;
            bool ramBad = ram.bytes > ramLimit;

            float cpuPct = cpuLimit > 0 ? ((float)cpu.usPerFrame / cpuLimit) * 100.0f : 0.0f;
            float ramPct = ramLimit > 0 ? ((float)ram.bytes / ramLimit) * 100.0f : 0.0f;

            JsonDocument responseDoc;
            responseDoc["status"] = "ok";
            responseDoc["cpu_us"] = cpu.usPerFrame;
            responseDoc["cpu_limit"] = cpuLimit;
            responseDoc["cpu_pct"] = cpuPct;
            responseDoc["cpu_bad"] = cpuBad;
            
            responseDoc["ram_bytes"] = ram.bytes;
            responseDoc["ram_limit"] = ramLimit;
            responseDoc["ram_pct"] = ramPct;
            responseDoc["ram_bad"] = ramBad;

            JsonObject hwObj = responseDoc["hw"].to<JsonObject>();
            hwObj["ledc"] = hw.ledc;
            hwObj["ledc_limit"] = ScoringLimits::MAX_LEDC;
            hwObj["rmt"] = hw.rmt;
            hwObj["rmt_limit"] = ScoringLimits::MAX_RMT;
            hwObj["uart"] = hw.uart;
            hwObj["uart_limit"] = ScoringLimits::MAX_UART;
            hwObj["dac"] = hw.dac;
            hwObj["dac_limit"] = ScoringLimits::MAX_DAC;
            hwObj["timer"] = hw.timer;
            hwObj["timer_limit"] = ScoringLimits::MAX_TIMER;
            hwObj["hw_bad"] = hwBad;

            String responseBody;
            serializeJson(responseDoc, responseBody);
            request->send(200, "application/json", responseBody);
        }
    );

    // API Route: Manual Output Test
    serverPtr->on("/api/output-test", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
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
            if (ch.shadowBuffer == nullptr || ch.bufferSize == 0) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Output channel is not initialized\"}");
                return;
            }

            memset(ch.shadowBuffer, 0, ch.bufferSize);

            if (doc.containsKey("params")) {
                uint8_t cmdVal = doc["cmd"] | 0;
                JsonObjectConst params = doc["params"].as<JsonObjectConst>();
                
                switch (ch.type) {
                    case OutputDefs::TYPE_DIMMER:
                    case OutputDefs::TYPE_SINGLE_LED:
                    case OutputDefs::TYPE_SERVO:
                    case OutputDefs::TYPE_BUZZER:
                    case OutputDefs::TYPE_DAC:
                    case OutputDefs::TYPE_PWM_DAC:
                    case OutputDefs::TYPE_FUNC_GEN:
                    case OutputDefs::TYPE_SOLENOID:
                    case OutputDefs::TYPE_SMOKE: {
                        uint8_t bytes = getValueByteCount(ch.mc_resolution);
                        uint32_t val = params["test_level_num"] | 128;
                        uint32_t max_val = getMaxValue(ch.mc_resolution);
                        
                        if (ch.type == OutputDefs::TYPE_BUZZER && cmdVal == 1) {
                            val = 0;
                        }
                        if (ch.type == OutputDefs::TYPE_FUNC_GEN) {
                            if (cmdVal == 1) val = 0;
                            else if (cmdVal >= 2 && cmdVal <= 5) {
                                uint8_t wave = cmdVal - 2;
                                if (ch.bufferSize >= 2) {
                                    ch.shadowBuffer[1] = wave;
                                }
                            }
                        }
                        if (ch.type == OutputDefs::TYPE_SOLENOID) {
                            if (cmdVal == 2) val = 0;
                            else if (cmdVal == 0 || cmdVal == 1) val = 255;
                        }
                        if (ch.type == OutputDefs::TYPE_SMOKE) {
                            if (cmdVal == 0) val = 255;
                            else if (cmdVal == 1) { ch.shadowBuffer[0] = 255; val = 0; }
                            else if (cmdVal == 2) { ch.shadowBuffer[0] = 0; val = 0; }
                            else if (cmdVal == 3) { ch.shadowBuffer[1] = 255; val = 0; }
                            else if (cmdVal == 4) { ch.shadowBuffer[1] = 0; val = 0; }
                        }
                        
                        val = constrain(val * max_val / 255, 0UL, max_val);
                        for (int i = bytes - 1; i >= 0; i--) {
                            ch.shadowBuffer[i] = (val >> (i * 8)) & 255;
                        }
                        break;
                    }
                    case OutputDefs::TYPE_DMX: {
                        int targetCh = params["test_dmx_ch"] | 1;
                        uint8_t level = params["test_level_num"] | 128;
                        if (cmdVal == 1) level = 0;
                        else if (cmdVal == 2) level = 255;
                        if (targetCh >= 1 && targetCh <= 512) {
                            ch.shadowBuffer[targetCh - 1] = level;
                        }
                        break;
                    }
                    case OutputDefs::TYPE_LED_STRIP:
                    case OutputDefs::TYPE_ANALOG_RGB: {
                        uint8_t r = params["r"] | 0;
                        uint8_t g = params["g"] | 0;
                        uint8_t b = params["b"] | 0;
                        uint8_t w = params["w"] | 0;
                        
                        if (cmdVal == 1) { r = 255; g = 0; b = 0; }
                        else if (cmdVal == 2) { r = 0; g = 255; b = 0; }
                        else if (cmdVal == 3) { r = 0; g = 0; b = 255; }
                        else if (cmdVal == 5) { r = 255; g = 0; b = 0; }
                        else if (cmdVal == 6) { r = 0; g = 255; b = 0; }
                        else if (cmdVal == 7) { r = 0; g = 0; b = 255; }
                        else if (cmdVal == 8) { r = 0; g = 0; b = 0; w = 0; }
                        
                        int targetPixel = (params["test_pixel"] | 1) - 1;
                        bool isPixelMode = (cmdVal >= 4 && cmdVal <= 7);
                        
                        uint8_t bpp = ch.color_order >= 4 ? 4 : 3;
                        uint16_t ledCount = ch.led_count > 0 ? ch.led_count : 170;
                        for (uint16_t i = 0; i < ledCount; i++) {
                            uint16_t offset = i * bpp;
                            if (offset + bpp > ch.bufferSize) break;
                            bool active = !isPixelMode || (i == targetPixel);
                            ch.shadowBuffer[offset] = active ? r : 0;
                            ch.shadowBuffer[offset + 1] = active ? g : 0;
                            ch.shadowBuffer[offset + 2] = active ? b : 0;
                            if (bpp == 4) ch.shadowBuffer[offset + 3] = active ? w : 0;
                        }
                        break;
                    }
                    case OutputDefs::TYPE_MOTOR: {
                        uint8_t bytes = getValueByteCount(ch.mc_resolution);
                        uint32_t max_val = getMaxValue(ch.mc_resolution);
                        uint32_t center = max_val / 2;
                        uint32_t speed = params["test_motor_num"] | 128;
                        int32_t dir = 0;
                        if (cmdVal == 1) dir = 1;
                        else if (cmdVal == 2) dir = -1;
                        
                        uint32_t mv = center;
                        if (dir > 0) mv = center + (speed * (max_val - center)) / 255;
                        else if (dir < 0) mv = center - (speed * center) / 255;
                        
                        for (int i = bytes - 1; i >= 0; i--) {
                            ch.shadowBuffer[i] = (mv >> (i * 8)) & 255;
                        }
                        break;
                    }
                    case OutputDefs::TYPE_STEPPER: {
                        uint8_t pos_bytes = getValueByteCount(ch.mc_resolution);
                        uint32_t pos = params["test_step_pos"] | 128;
                        uint8_t speed = params["test_step_speed"] | 180;
                        uint8_t rawCmd = cmdVal;
                        
                        uint32_t max_val = getMaxValue(ch.mc_resolution);
                        pos = constrain(pos, 0UL, max_val);
                        for (int i = pos_bytes - 1; i >= 0; i--) {
                            ch.shadowBuffer[i] = (pos >> (i * 8)) & 255;
                        }
                        ch.shadowBuffer[pos_bytes] = speed;
                        ch.shadowBuffer[pos_bytes + 1] = rawCmd;
                        break;
                    }
                    case OutputDefs::TYPE_DFPLAYER: {
                        uint8_t track = params["test_mp3_track"] | 1;
                        uint8_t vol = params["test_mp3_vol"] | 200;
                        ch.shadowBuffer[0] = track;
                        ch.shadowBuffer[1] = vol;
                        ch.shadowBuffer[2] = cmdVal;
                        break;
                    }
                    case OutputDefs::TYPE_TM1637:
                    case OutputDefs::TYPE_7SEG_7PIN:
                    case OutputDefs::TYPE_7SEG_8PIN: {
                        if (cmdVal == 1) {
                            const char* text = params["test_7seg_text"] | "";
                            for (int i = 0; i < 4 && i < (int)ch.bufferSize; i++) {
                                ch.shadowBuffer[i] = text[i] ? text[i] : ' ';
                            }
                        } else if (cmdVal == 0) {
                            uint16_t num = params["test_7seg_num"] | 1234;
                            if (ch.bufferSize >= 2) {
                                ch.shadowBuffer[0] = (num >> 8) & 255;
                                ch.shadowBuffer[1] = num & 255;
                            }
                        } else if (cmdVal == 2) {
                            memset(ch.shadowBuffer, 0xFF, ch.bufferSize);
                        }
                        break;
                    }
                }
            } else if (doc.containsKey("values")) {
                JsonArray values = doc["values"].as<JsonArray>();
                uint16_t pos = 0;
                for (JsonVariant value : values) {
                    if (pos >= ch.bufferSize) break;
                    int v = value.as<int>();
                    ch.shadowBuffer[pos++] = constrain(v, 0, 255);
                }
            }

            outputCtrl.swapBuffers();

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
    serverPtr->on("/api/wifi-scan", HTTP_GET, [](AsyncWebServerRequest *request) {
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
    serverPtr->on("/api/outputs/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["version"] = 3;
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
    serverPtr->on("/api/config/factory-reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        // Clear all preferences
        Preferences prefs;
        if (!prefs.begin("system", false)) {
            request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to open NVS for factory reset\"}");
            return;
        }
        prefs.clear();
        prefs.end();

        // Reset outputs to default
        JsonDocument doc;
        doc["version"] = 3;
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

        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Factory reset complete. Rebooting...\"}");

        xTaskCreate([](void*){
            vTaskDelay(pdMS_TO_TICKS(1500));
            ESP.restart();
        }, "factoryResetTask", 2048, NULL, 1, NULL);
    });

    // API Route: Get ESP-NOW Peers
    serverPtr->on("/api/espnow-peers", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!LittleFS.exists("/espnow_peers.json")) {
            request->send(200, "application/json", "{\"peers\":[]}");
            return;
        }
        request->send(LittleFS, "/espnow_peers.json", "application/json");
    });

    // API Route: Save ESP-NOW Peers
    serverPtr->on("/api/espnow-peers", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
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
                char normalizedMac[18];
                if (!normalizeEspNowPeerMac(obj["mac"].as<const char*>(), normalizedMac, sizeof(normalizedMac))) {
                    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid peer MAC address\"}");
                    return;
                }
                obj["mac"] = normalizedMac;
                uint32_t suRaw = obj["start_universe"] | 0;
                uint16_t sa = obj["start_address"] | 1;
                uint32_t euRaw = obj["end_universe"] | suRaw;
                uint16_t ea = obj["end_address"] | 512;
                if (suRaw > 32767 || euRaw > 32767) {
                    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"ESP-NOW peer universe must be 0-32767\"}");
                    return;
                }
                uint16_t su = (uint16_t)suRaw;
                uint16_t eu = (uint16_t)euRaw;
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
            
            // Queue peer reload for network task (avoids race with sendDmx)
            espNowCtrl.reloadPeersPending = true;

            request->send(200, "application/json", "{\"status\":\"ok\"}");
        }
    );

    // API Route: Get Telemetry Live Status
    serverPtr->on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["device_mode"] = sysCfg.device_mode;
        
        // Network connectivity status
        bool wifiUp = WiFi.isConnected();
        doc["eth_up"]  = ethConnected;
        doc["wifi_up"] = wifiUp;
        doc["ap_up"]   = apActive;
        doc["wifi_ssid_active"] = wifiUp ? WiFi.SSID().c_str() : "";

        // Per-interface IPs
        doc["eth_ip"] = ethConnected ? ETH.localIP().toString() : "";
        doc["wifi_ip"] = wifiUp ? WiFi.localIP().toString() : "";
        doc["ap_ip"] = apActive ? WiFi.softAPIP().toString() : "";

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
        doc["active"]   = systemActive.load();

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
    serverPtr->on("/api/sacn/status", HTTP_GET, [](AsyncWebServerRequest *request) {
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
    serverPtr->on("/api/i2c-scan", HTTP_GET, [](AsyncWebServerRequest *request) {
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

    // API Route: Hardware + CPU + RAM budget report
    serverPtr->on("/api/scoring", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        std::vector<OutputChannel>& chs = outputCtrl.getChannels();
        uint8_t fps = sysCfg.output_fps > 0 ? sysCfg.output_fps : 40;

        HardwareResource hw = totalHardware(chs);
        CpuBudget cpu = totalCpu(chs, fps);
        RamBudget ram = totalRam(chs);
        uint32_t cpuLimit = CpuBudget::limit(fps);
        uint32_t ramLimit = RamBudget::limit();

        doc["ledc"] = hw.ledc;   doc["ledc_max"] = MAX_LEDC_RESOURCE;
        doc["rmt"]  = hw.rmt;    doc["rmt_max"]  = MAX_RMT_RESOURCE;
        doc["uart"] = hw.uart;   doc["uart_max"] = MAX_UART_RESOURCE;
        doc["dac"]  = hw.dac;    doc["dac_max"]  = MAX_DAC_RESOURCE;
        doc["timer"] = hw.timer; doc["timer_max"] = MAX_TIMER_RESOURCE;
        doc["cpu_us"] = cpu.usPerFrame;   doc["cpu_limit"] = cpuLimit;
        doc["ram_bytes"]  = ram.bytes;    doc["ram_limit"]  = ramLimit;
        doc["fps"] = fps;

        ScoreBlocker blocker = checkScores(hw, cpu, ram, fps);
        doc["blocked"] = (blocker != ScoreBlocker::None);
        if (blocker != ScoreBlocker::None) doc["blocker"] = scoreBlockerName(blocker);

        JsonArray arr = doc["channels"].to<JsonArray>();
        for (size_t i = 0; i < chs.size(); i++) {
            JsonObject item = arr.add<JsonObject>();
            item["index"] = (uint8_t)i;
            item["type"] = chs[i].type;
            item["name"] = outputTypeName(chs[i].type);
            PerChannelCost cc = estimateChannelCost(chs[i]);
            item["cpu_us"] = cc.cpuUs;
            item["ram_bytes"]  = cc.ramBytes;
            HardwareResource chHw = estimateHardware(chs[i]);
            item["ledc"] = chHw.ledc; item["rmt"] = chHw.rmt;
            item["uart"] = chHw.uart; item["dac"] = chHw.dac; item["timer"] = chHw.timer;
        }
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API Route: OTA Web Update Post Handler
    serverPtr->on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
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
    serverPtr->begin();
    Serial.println("Async Web Server started");
}

// Helper: compute active universe range from output channels
uint16_t activeUniverseMin() {
    uint16_t minU = 0xFFFF;
    bool found = false;
    for (const auto& ch : outputCtrl.getChannels()) {
        uint16_t u = ch.start_universe;
        if (u < minU) { minU = u; found = true; }
    }
    return found ? minU : 0;
}
uint16_t activeUniverseMax() {
    uint16_t maxU = 0;
    for (const auto& ch : outputCtrl.getChannels()) {
        uint16_t u = ch.start_universe;
        if (u > maxU) maxU = u;
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
        if (sysCfg.artnet_enabled) {
            artNetCtrl.begin();
        }
        
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

        // Deferred ESP-NOW peer reload (avoids race with sendDmx iteration)
        if (espNowCtrl.reloadPeersPending) {
            espNowCtrl.reloadPeersPending = false;
            espNowCtrl.loadPeers();
        }

        // Run network stack loops
        if (sysCfg.artnet_enabled) {
            artNetCtrl.loop();
        }
        reconnectWiFiClient();
        
        // Process sACN packets if enabled
        if (sysCfg.sacn_enabled && (sysCfg.device_mode == MODE_ARTNET_ETHERNET || sysCfg.device_mode == MODE_ESPNOW_MASTER)) {
            sacnCtrl.process();
        }
        
        // Process ESP-NOW auto-channel scanning
        if (sysCfg.device_mode == MODE_ESPNOW_SLAVE) {
            espNowCtrl.loop();
        }
        
        // ESP-NOW Slave keeps AP active so it can always be configured in the field.

        // Update I2C Display (non-blocking, ~2 Hz)
        if (display.isActive()) {
            static unsigned long lastDisplayUpdate = 0;
            unsigned long now = millis();
            if (now - lastDisplayUpdate >= sysCfg.display_refresh_ms) {
                lastDisplayUpdate = now;

                // Build display lines
                IPAddress ip = ETH.localIP();
                String line1 = "IP " + (ethConnected ? ip.toString() : String("--.---.-.--"));
                String line2 = ethConnected ? "ETH Link Up" : (WiFi.isConnected() ? "Wi-Fi Connected" : "No Link");
                String line3 = "U " + String(activeUniverseMin()) + "-" + String(activeUniverseMax());
                String line4 = "FPS " + String(sysCfg.output_fps) + "  Heap " + String(ESP.getFreeHeap() / 1024) + "K";

                display.update(line1, line2, line3, line4);
            }
        } else if (sysCfg.display_enabled) {
            // Attempt periodic recovery if display was deactivated by I2C errors (ADR004)
            static unsigned long lastDisplayRecover = 0;
            unsigned long now = millis();
            if (now - lastDisplayRecover >= sysCfg.display_recover_ms) {
                lastDisplayRecover = now;
                display.tryRecover();
            }
        }

        // Process queued I2C scan request (runs on Core 0, non-blocking for HTTP)
        if (i2cScanRequested && !i2cScanPending) {
            i2cScanRequested = false;
            i2cScanPending = true;

            JsonDocument doc;
            JsonArray arr = doc.to<JsonArray>();
            for (uint8_t addr = 1; addr < 128; addr++) {
                if ((addr & 0x07) == 0 && addr > 1) vTaskDelay(pdMS_TO_TICKS(1));
                bool found = I2cBus::probe(addr);
                if (found) {
                    JsonObject obj = arr.add<JsonObject>();
                    obj["address"] = addr;
                    char hex[6];
                    snprintf(hex, sizeof(hex), "0x%02X", addr);
                    obj["hex"] = hex;
                    String usedBy = "";
                    int idx = 0;
                    for (const auto& ch : outputCtrl.getChannels()) {
                        idx++;
                        const auto* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
                        uint8_t pinCount = def != nullptr ? def->pinCount : 1;
                        if (pinCount > 9) pinCount = 9;
                        for (uint8_t r = 0; r < pinCount; r++) {
                            uint8_t src = ch.routes[r].source;
                            uint8_t r_addr = ch.routes[r].addr;
                            if (src >= 1 && src <= 4 && r_addr == addr) {
                                if (usedBy.length() > 0) usedBy += ", ";
                                usedBy += "CH" + String(idx) + "(P" + String(r + 1) + ")";
                            }
                        }
                    }
                    if (usedBy.length() > 0) obj["used_by"] = usedBy;
                }
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
            if ((int32_t)(millis() - outputTestUntil.load()) >= 0) {
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
                if (packet.universe == 0 && packet.offset + dmxLen <= sizeof(EspNowControl::rxDmxBuffer)) {
                    memcpy(EspNowControl::rxDmxBuffer + packet.offset, packet.data, dmxLen);
                    EspNowControl::rxDmxLength = min((uint16_t)512, packet.totalLength);
                    matched = true;
                }

                if (matched) {
                    EspNowControl::lastRxTime = millis();
                    EspNowControl::newRxData = true;
                }
            }
            if (EspNowControl::newRxData) {
                outputCtrl.swapBuffers();
            }
        }

        // If in ESP-NOW Slave mode, check for incoming wireless packages
        if (sysCfg.device_mode == MODE_ESPNOW_SLAVE) {
            if (EspNowControl::newRxData) {
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
        }

        // DMX frame timeout detection: if no network DMX/ESP-NOW data
        // has been received for the timeout period, mark system inactive.
        const unsigned long DMX_FRAME_TIMEOUT_MS = 5000;
        static unsigned long lastDmxTimeoutLog = 0;
        unsigned long now = millis();
        unsigned long lastUpdate = lastDmxUpdateTime.load();
        if (lastUpdate != 0 && now - lastUpdate > DMX_FRAME_TIMEOUT_MS) {
            if (systemActive.exchange(false)) {
                Serial.println("[WARN] DMX frame timeout — no data received for 5 seconds");
            }
            if (now - lastDmxTimeoutLog > 30000) {
                lastDmxTimeoutLog = now;
                Serial.println("[WARN] DMX timeout persists, outputs holding last state");
            }
        }

        updateStatusLedPattern();

        // Minor yield to prevent core hogging
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

// Recovery Mode tracking
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
    vTaskDelay(pdMS_TO_TICKS(100));

    bootCount++;
    Serial.printf("Boot count: %d\n", bootCount);

    if (bootCount >= 5) {
        Serial.println("Recovery Mode Triggered! (5 consecutive resets)");
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
        startRecoveryMode();
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
    swapMutex = xSemaphoreCreateMutex();

    // Initialize I2C Display
    display.begin(sysCfg.display_enabled, sysCfg.display_i2c_addr, sysCfg.display_cols, sysCfg.display_rows);
    if (display.isActive()) {
        display.setBrightness(sysCfg.display_brightness);
        Serial.printf("Display initialized: type=%d, addr=0x%02X\n", sysCfg.display_enabled, sysCfg.display_i2c_addr);
    }

    // Initialize Outputs System after network startup current settles.
    vTaskDelay(pdMS_TO_TICKS(250));
    outputCtrl.begin();
    dimmerCtrl.begin();
    motionCtrl.begin();

    // Initialize ESP-NOW wireless transceiver if device mode requires it.
    if (sysCfg.device_mode == MODE_ESPNOW_MASTER || sysCfg.device_mode == MODE_ESPNOW_SLAVE) {
        espNowQueue = xQueueCreate(sysCfg.espnow_queue_depth, sizeof(EspNowDmxPacket));
        espNowCtrl.begin();
    }

    // Configure mDNS Responder
    String mdnsHost = String(sysCfg.mdns_name);
    if (isRecoveryMode) {
        mdnsHost = mdnsHost + "-recovery";
    } else {
        String mac = WiFi.macAddress();
        mac.replace(":", "");
        String suffix = mac.substring(mac.length() - 4);
        suffix.toLowerCase();
        mdnsHost = mdnsHost + "-" + suffix;
    }
    if (MDNS.begin(mdnsHost.c_str())) {
        Serial.printf("mDNS responder started: http://%s.local\n", mdnsHost.c_str());
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
