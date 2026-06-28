#ifndef I2C_DEVICES_PCA9685_H
#define I2C_DEVICES_PCA9685_H

#include <Arduino.h>
#include "i2c_devices/i2c_bus.h"

class PCA9685Driver {
private:
    uint8_t _address;
    uint8_t _source;
    uint16_t _lastDuty[16]; // Keep track of the last duty cycle written to each channel (dirty-state check)

    void writeRegister(uint8_t reg, uint8_t value) {
        I2cBus::writeReg8(_address, reg, value);
    }

    uint8_t readRegister(uint8_t reg) {
        return I2cBus::readReg8(_address, reg, 0);
    }

public:
    PCA9685Driver(uint8_t address = 0x40, uint8_t source = 1) : _address(address), _source(source) {
        for (int i = 0; i < 16; i++) {
            _lastDuty[i] = 9999; // Force update on first write
        }
    }

    uint8_t getAddress() const { return _address; }
    uint8_t getSource() const { return _source; }

    void begin() {
        if (_source == 8) {
            // Wake PCA9635 (MODE1 sleep bit is bit 4, wake is bit 4 = 0)
            writeRegister(0x00, 0x80); // Register 0x00 is MODE1, bit 7 is ALLCALL
            delay(5);
            // Set all 16 pins to PWM mode: LEDOUT0 to LEDOUT3 (registers 0x14-0x17)
            writeRegister(0x14, 0xAA);
            writeRegister(0x15, 0xAA);
            writeRegister(0x16, 0xAA);
            writeRegister(0x17, 0xAA);
        } else if (_source == 9) {
            // Wake SN3218 (Register 0x00 = 0x01)
            writeRegister(0x00, 0x01);
            delay(5);
            // Enable all 18 channels (LED Control registers 0x13, 0x14, 0x15 = 0x3F each)
            writeRegister(0x13, 0x3F);
            writeRegister(0x14, 0x3F);
            writeRegister(0x15, 0x3F);
            writeRegister(0x16, 0xFF); // Initial update
        } else if (_source == 10) {
            // Configure AW9523: set all 16 pins to LED mode (PWM control)
            writeRegister(0x11, 0x00); // LEDOUT Port 0
            writeRegister(0x12, 0x00); // LEDOUT Port 1
            writeRegister(0x02, 0x00); // Config Port 0: all output
            writeRegister(0x03, 0x00); // Config Port 1: all output
            delay(5);
        } else if (_source == 11) {
            // TLC5940 is SPI-based. Stub only.
        } else {
            // Wake PCA9685, enable register Auto-Increment for multi-byte writes
            writeRegister(0x00, 0x20); // MODE1 with AI bit set
            delay(5);
        }
    }

    void setFrequency(uint16_t freq) {
        if (_source == 8 || _source == 9 || _source == 10 || _source == 11) return;

        if (freq < 24) freq = 24;
        if (freq > 1526) freq = 1526;
        
        // Calculate prescale: round(25,000,000 / (4096 * freq)) - 1
        float prescale_val = (25000000.0f / (4096.0f * freq)) - 0.5f;
        uint8_t prescale = (uint8_t)prescale_val;

        uint8_t oldmode = readRegister(0x00); // MODE1
        uint8_t newmode = (oldmode & 0x7F) | 0x10; // Go to sleep to set prescale

        writeRegister(0x00, newmode); // Set sleep mode
        writeRegister(0xFE, prescale); // Write prescale value
        writeRegister(0x00, oldmode); // Restore previous mode

        delay(5);
        writeRegister(0x00, oldmode | 0x80); // Auto-increment and restart
    }

    void writeChannel(uint8_t channel, uint16_t duty, bool force = false) {
        if (channel > 17) return;

        if (_source == 8) {
            // Map 12-bit duty (0..4095) to PCA9635 8-bit duty (0..255)
            uint8_t duty8 = (uint8_t)((duty * 255) / 4095);

            if (!force && _lastDuty[channel] == duty8) {
                return;
            }
            _lastDuty[channel] = duty8;

            // PWM register on PCA9635 is 0x02 + channel
            writeRegister(0x02 + channel, duty8);
            return;
        } else if (_source == 9) {
            // Map 12-bit duty (0..4095) to SN3218 8-bit duty (0..255)
            uint8_t duty8 = (uint8_t)((duty * 255) / 4095);

            if (!force && _lastDuty[channel] == duty8) {
                return;
            }
            _lastDuty[channel] = duty8;

            // PWM register on SN3218 is 0x01 + channel
            writeRegister(0x01 + channel, duty8);
            writeRegister(0x16, 0xFF); // Update PWM data
            return;
        } else if (_source == 10) {
            // Map 12-bit duty (0..4095) to AW9523 8-bit duty (0..255)
            uint8_t duty8 = (uint8_t)((duty * 255) / 4095);

            if (!force && _lastDuty[channel] == duty8) {
                return;
            }
            _lastDuty[channel] = duty8;

            // PWM register on AW9523 is 0x20 + channel
            writeRegister(0x20 + channel, duty8);
            return;
        } else if (_source == 11) {
            // TLC5940 (SPI) stub write
            return;
        }

        if (duty > 4095) duty = 4095;

        // Dirty-state check to minimize I2C traffic
        if (!force && _lastDuty[channel] == duty) {
            return;
        }

        uint8_t data[5];
        data[0] = 0x06 + 4 * channel; // LEDn_ON_L register

        if (duty == 4095) {
            data[1] = 0x00; // ON_L
            data[2] = 0x10; // ON_H
            data[3] = 0x00; // OFF_L
            data[4] = 0x00; // OFF_H
        } else if (duty == 0) {
            data[1] = 0x00; // ON_L
            data[2] = 0x00; // ON_H
            data[3] = 0x00; // OFF_L
            data[4] = 0x10; // OFF_H
        } else {
            data[1] = 0x00; // ON_L
            data[2] = 0x00; // ON_H
            data[3] = duty & 0xFF; // OFF_L
            data[4] = (duty >> 8) & 0x0F; // OFF_H
        }

        if (I2cBus::writeBytes(_address, data, sizeof(data))) _lastDuty[channel] = duty;
    }
};

// Global management for PCA9685 devices
class PCA9685Manager {
private:
    PCA9685Driver* _drivers[8];
    uint8_t _driverCount;

public:
    PCA9685Manager() : _driverCount(0) {
        for (int i = 0; i < 8; i++) {
            _drivers[i] = nullptr;
        }
    }

    ~PCA9685Manager() {
        clear();
    }

    void clear() {
        for (int i = 0; i < 8; i++) {
            if (_drivers[i]) {
                delete _drivers[i];
                _drivers[i] = nullptr;
            }
        }
        _driverCount = 0;
    }

    PCA9685Driver* getOrCreateDriver(uint8_t address, uint8_t source = 1) {
        for (int i = 0; i < _driverCount; i++) {
            if (_drivers[i] && _drivers[i]->getAddress() == address) {
                return _drivers[i];
            }
        }

        if (_driverCount >= 8) return nullptr;

        PCA9685Driver* drv = new (std::nothrow) PCA9685Driver(address, source);
        if (!drv) return nullptr;
        drv->begin();
        _drivers[_driverCount++] = drv;
        return drv;
    }

    void write(uint8_t address, uint8_t channel, uint16_t duty, bool force = false, uint8_t source = 1) {
        PCA9685Driver* drv = getOrCreateDriver(address, source);
        if (drv) {
            drv->writeChannel(channel, duty, force);
        }
    }

    void setFrequency(uint8_t address, uint16_t freq, uint8_t source = 1) {
        PCA9685Driver* drv = getOrCreateDriver(address, source);
        if (drv) {
            drv->setFrequency(freq);
        }
    }
};

extern PCA9685Manager pcaManager;

#endif // I2C_DEVICES_PCA9685_H
