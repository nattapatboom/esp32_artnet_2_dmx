#ifndef I2C_DAC_H
#define I2C_DAC_H

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ── I2C DAC Drivers ─────────────────────────
// MCP4725 (source=5), DAC7571 (source=6),
// DAC7573 (source=7)
//
// All write functions are inline and require
// the caller to hold i2cMutex or pass a mutex.

extern SemaphoreHandle_t i2cMutex;

namespace I2cDac {

inline void writeMcp4725(uint8_t addr, uint8_t value) {
    uint16_t dac = ((uint16_t)value << 4) | (value >> 4);
    if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    Wire.beginTransmission(addr);
    Wire.write(0x40);
    Wire.write((dac >> 4) & 0xFF);
    Wire.write((dac & 0x0F) << 4);
    Wire.endTransmission();
    if (i2cMutex) xSemaphoreGive(i2cMutex);
}

inline void writeDac7571(uint8_t addr, uint8_t value) {
    uint16_t dac = ((uint16_t)value << 4) | (value >> 4);
    uint8_t ctrl = (dac >> 8) & 0x0F;
    if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    Wire.beginTransmission(addr);
    Wire.write(ctrl);
    Wire.write(dac & 0xFF);
    Wire.endTransmission();
    if (i2cMutex) xSemaphoreGive(i2cMutex);
}

inline void writeDac7573(uint8_t addr, uint8_t channel, uint8_t value) {
    uint16_t dac = ((uint16_t)value << 4) | (value >> 4);
    uint8_t cmd = (channel & 0x03) << 1;
    if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    Wire.beginTransmission(addr);
    Wire.write(cmd);
    Wire.write((dac >> 4) & 0xFF);
    Wire.write((dac & 0x0F) << 4);
    Wire.endTransmission();
    if (i2cMutex) xSemaphoreGive(i2cMutex);
}

}  // namespace I2cDac

#endif
