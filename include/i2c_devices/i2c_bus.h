#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "source_rules.h"

extern SemaphoreHandle_t i2cMutex;

namespace I2cBus {

class Lock {
public:
    explicit Lock(TickType_t timeout = pdMS_TO_TICKS(100)) {
        _locked = (i2cMutex == nullptr) || (xSemaphoreTake(i2cMutex, timeout) == pdTRUE);
    }

    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;

    ~Lock() {
        if (_locked && i2cMutex != nullptr) xSemaphoreGive(i2cMutex);
    }

    bool locked() const { return _locked; }

private:
    bool _locked = false;
};

inline bool addressValid(uint8_t source, uint8_t address) {
    return SourceRules::addressValid(source, address);
}

inline bool writeBytes(uint8_t address, const uint8_t* data, uint8_t length) {
    if (data == nullptr || length == 0) return false;
    Lock lock;
    if (!lock.locked()) return false;
    Wire.beginTransmission(address);
    for (uint8_t i = 0; i < length; i++) Wire.write(data[i]);
    return Wire.endTransmission() == 0;
}

inline bool writeReg8(uint8_t address, uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    return writeBytes(address, data, sizeof(data));
}

inline bool writeReg16(uint8_t address, uint8_t reg, uint16_t value) {
    uint8_t data[3] = {reg, (uint8_t)(value & 0xFF), (uint8_t)((value >> 8) & 0xFF)};
    return writeBytes(address, data, sizeof(data));
}

inline uint8_t readReg8(uint8_t address, uint8_t reg, uint8_t fallback = 0xFF) {
    Lock lock;
    if (!lock.locked()) return fallback;
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return fallback;
    Wire.requestFrom(address, (uint8_t)1);
    return Wire.available() ? Wire.read() : fallback;
}

inline uint16_t readReg16(uint8_t address, uint8_t reg, uint16_t fallback = 0xFFFF) {
    Lock lock;
    if (!lock.locked()) return fallback;
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return fallback;
    Wire.requestFrom(address, (uint8_t)2);
    uint16_t val = 0;
    if (!Wire.available()) return fallback;
    val = Wire.read();
    if (!Wire.available()) return fallback;
    val |= (uint16_t)Wire.read() << 8;
    return val;
}

}  // namespace I2cBus

#endif
