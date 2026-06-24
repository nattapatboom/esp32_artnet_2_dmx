#ifndef I2C_GPIO_EXPANDER_H
#define I2C_GPIO_EXPANDER_H

#include <Arduino.h>
#include "i2c_devices/i2c_bus.h"

enum DigitalExpanderKind : uint8_t {
    DIG_EXP_MCP23017 = 2,
    DIG_EXP_TCA9555 = 3,
    DIG_EXP_PCF857X = 4
};

class DigitalExpanderDevice {
private:
    uint8_t _kind;
    uint8_t _address;
    uint16_t _state;
    bool _started;

    void writeReg8(uint8_t reg, uint8_t value) {
        I2cBus::writeReg8(_address, reg, value);
    }

    void writeReg16(uint8_t reg, uint16_t value) {
        I2cBus::writeReg16(_address, reg, value);
    }

    void writePcf(uint16_t value) {
        uint8_t data[2] = {(uint8_t)(value & 0xFF), (uint8_t)((value >> 8) & 0xFF)};
        I2cBus::writeBytes(_address, data, value > 0xFF ? 2 : 1);
    }

public:
    DigitalExpanderDevice(uint8_t kind, uint8_t address)
        : _kind(kind), _address(address), _state(0), _started(false) {}

    uint8_t getKind() const { return _kind; }
    uint8_t getAddress() const { return _address; }

    void begin() {
        if (_started) return;
        _started = true;
        _state = 0;

        if (_kind == DIG_EXP_MCP23017) {
            writeReg8(0x00, 0x00); // IODIRA: all output
            writeReg8(0x01, 0x00); // IODIRB: all output
            writeReg8(0x14, 0x00); // OLATA
            writeReg8(0x15, 0x00); // OLATB
        } else if (_kind == DIG_EXP_TCA9555) {
            writeReg16(0x06, 0x0000); // CONFIG ports: all output
            writeReg16(0x02, 0x0000); // OUTPUT ports
        } else if (_kind == DIG_EXP_PCF857X) {
            writePcf(0x0000);
        }
    }

    void writeChannel(uint8_t channel, bool state, bool force = false) {
        if (channel > 15) return;
        uint16_t mask = (uint16_t)1 << channel;
        uint16_t next = state ? (_state | mask) : (_state & ~mask);
        if (!force && next == _state) return;
        _state = next;

        if (_kind == DIG_EXP_MCP23017) {
            writeReg8(0x14, _state & 0xFF);
            writeReg8(0x15, (_state >> 8) & 0xFF);
        } else if (_kind == DIG_EXP_TCA9555) {
            writeReg16(0x02, _state);
        } else if (_kind == DIG_EXP_PCF857X) {
            writePcf(_state);
        }
    }

    uint16_t readInputs() {
        if (_kind == DIG_EXP_MCP23017) {
            uint8_t lo = regRead8(0x12); // GPIOA
            uint8_t hi = regRead8(0x13); // GPIOB
            return ((uint16_t)hi << 8) | lo;
        } else if (_kind == DIG_EXP_TCA9555) {
            return regRead16(0x00); // INPUT port
        } else if (_kind == DIG_EXP_PCF857X) {
            uint8_t tx[2] = {(uint8_t)(_state & 0xFF), (uint8_t)((_state >> 8) & 0xFF)};
            uint8_t rx[2] = {0, 0};
            uint8_t width = (_state > 0xFF) ? 2 : 1;
            if (!I2cBus::writeThenRead(_address, tx, width, rx, width)) return 0xFFFF;
            uint16_t val = rx[0];
            if (width > 1) val |= (uint16_t)rx[1] << 8;
            return val;
        }
        return 0xFFFF;
    }

    void configureInput(uint8_t channel, bool pullUp = true) {
        if (channel > 15) return;
        if (_kind == DIG_EXP_MCP23017) {
            uint8_t iodirReg = (channel < 8) ? 0x00 : 0x01;
            uint8_t portBit = (uint8_t)1 << (channel & 7);
            uint8_t iodir = regRead8(iodirReg);
            if (iodir == 0xFF) return; // I2C read error, skip to avoid corruption
            iodir |= portBit;
            writeReg8(iodirReg, iodir);
            if (pullUp) {
                uint8_t gppuReg = (channel < 8) ? 0x06 : 0x07;
                uint8_t gppu = regRead8(gppuReg);
                if (gppu == 0xFF) return;
                gppu |= portBit;
                writeReg8(gppuReg, gppu);
            }
        } else if (_kind == DIG_EXP_TCA9555) {
            uint16_t config = regRead16(0x06);
            if (config == 0xFFFF) return; // I2C read error
            config |= (uint16_t)1 << channel;
            writeReg16(0x06, config);
        } else if (_kind == DIG_EXP_PCF857X) {
            writeChannel(channel, true, true);
        }
    }

private:
    uint8_t regRead8(uint8_t reg) {
        return I2cBus::readReg8(_address, reg, 0xFF);
    }

    uint16_t regRead16(uint8_t reg) {
        return I2cBus::readReg16(_address, reg, 0xFFFF);
    }
};

class DigitalExpanderManager {
private:
    DigitalExpanderDevice* _devices[12];
    uint8_t _deviceCount;

public:
    DigitalExpanderManager() : _deviceCount(0) {
        for (uint8_t i = 0; i < 12; i++) _devices[i] = nullptr;
    }

    ~DigitalExpanderManager() {
        clear();
    }

    void clear() {
        for (uint8_t i = 0; i < 12; i++) {
            if (_devices[i]) {
                delete _devices[i];
                _devices[i] = nullptr;
            }
        }
        _deviceCount = 0;
    }

    DigitalExpanderDevice* getOrCreate(uint8_t kind, uint8_t address) {
        for (uint8_t i = 0; i < _deviceCount; i++) {
            if (_devices[i] && _devices[i]->getKind() == kind && _devices[i]->getAddress() == address) {
                return _devices[i];
            }
        }
        if (_deviceCount >= 12) return nullptr;

        DigitalExpanderDevice* dev = new (std::nothrow) DigitalExpanderDevice(kind, address);
        if (!dev) return nullptr;
        dev->begin();
        _devices[_deviceCount++] = dev;
        return dev;
    }

    void write(uint8_t kind, uint8_t address, uint8_t channel, bool state, bool force = false) {
        DigitalExpanderDevice* dev = getOrCreate(kind, address);
        if (dev) dev->writeChannel(channel, state, force);
    }

    bool digitalRead(uint8_t kind, uint8_t address, uint8_t channel) {
        DigitalExpanderDevice* dev = getOrCreate(kind, address);
        if (!dev || channel > 15) return true;
        uint16_t port = dev->readInputs();
        return (port >> channel) & 1;
    }

    void configureInput(uint8_t kind, uint8_t address, uint8_t channel, bool pullUp = true) {
        DigitalExpanderDevice* dev = getOrCreate(kind, address);
        if (dev) dev->configureInput(channel, pullUp);
    }
};

extern DigitalExpanderManager digitalExpanderManager;

#endif // I2C_GPIO_EXPANDER_H
