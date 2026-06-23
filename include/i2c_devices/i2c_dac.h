#ifndef I2C_DAC_H
#define I2C_DAC_H

#include <Arduino.h>
#include "i2c_devices/i2c_bus.h"

// ── I2C DAC Drivers ─────────────────────────
// MCP4725 (source=5), DAC7571 (source=6),
// DAC7573 (source=7)
// All write functions use the shared I2cBus transaction helpers.

namespace I2cDac {

inline void writeMcp4725(uint8_t addr, uint8_t value) {
    uint16_t dac = ((uint16_t)value << 4) | (value >> 4);
    uint8_t data[3] = {0x40, (uint8_t)((dac >> 4) & 0xFF), (uint8_t)((dac & 0x0F) << 4)};
    I2cBus::writeBytes(addr, data, sizeof(data));
}

inline void writeDac7571(uint8_t addr, uint8_t value) {
    uint16_t dac = ((uint16_t)value << 4) | (value >> 4);
    uint8_t ctrl = (dac >> 8) & 0x0F;
    uint8_t data[2] = {ctrl, (uint8_t)(dac & 0xFF)};
    I2cBus::writeBytes(addr, data, sizeof(data));
}

inline void writeDac7573(uint8_t addr, uint8_t channel, uint8_t value) {
    uint16_t dac = ((uint16_t)value << 4) | (value >> 4);
    uint8_t cmd = (channel & 0x03) << 1;
    uint8_t data[3] = {cmd, (uint8_t)((dac >> 4) & 0xFF), (uint8_t)((dac & 0x0F) << 4)};
    I2cBus::writeBytes(addr, data, sizeof(data));
}

}  // namespace I2cDac

#endif
