#ifndef PCA9685_CONTROL_H
#define PCA9685_CONTROL_H

#include <Arduino.h>
#include <Wire.h>

class PCA9685Driver {
private:
    uint8_t _address;
    uint16_t _lastDuty[16]; // Keep track of the last duty cycle written to each channel (dirty-state check)

    void writeRegister(uint8_t reg, uint8_t value) {
        if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
        Wire.beginTransmission(_address);
        Wire.write(reg);
        Wire.write(value);
        Wire.endTransmission();
        if (i2cMutex) xSemaphoreGive(i2cMutex);
    }

    uint8_t readRegister(uint8_t reg) {
        if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) return 0;
        Wire.beginTransmission(_address);
        Wire.write(reg);
        Wire.endTransmission(false);
        Wire.requestFrom(_address, (uint8_t)1);
        uint8_t val = 0;
        if (Wire.available()) val = Wire.read();
        if (i2cMutex) xSemaphoreGive(i2cMutex);
        return val;
    }

public:
    PCA9685Driver(uint8_t address = 0x40) : _address(address) {
        for (int i = 0; i < 16; i++) {
            _lastDuty[i] = 9999; // Force update on first write
        }
    }

    uint8_t getAddress() const { return _address; }

    void begin() {
        // Wake PCA9685, enable register Auto-Increment for multi-byte writes
        writeRegister(0x00, 0x20); // MODE1 with AI bit set
        delay(5);
    }

    void setFrequency(uint16_t freq) {
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
        if (channel > 15) return;
        if (duty > 4095) duty = 4095;

        // Dirty-state check to minimize I2C traffic
        if (!force && _lastDuty[channel] == duty) {
            return;
        }

        if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
        Wire.beginTransmission(_address);
        Wire.write(0x06 + 4 * channel); // LEDn_ON_L register

        if (duty == 4095) {
            // Full ON: set ON_H to 0x10, OFF_H to 0x00
            Wire.write(0x00); // ON_L
            Wire.write(0x10); // ON_H
            Wire.write(0x00); // OFF_L
            Wire.write(0x00); // OFF_H
        } else if (duty == 0) {
            // Full OFF: set ON_H to 0x00, OFF_H to 0x10
            Wire.write(0x00); // ON_L
            Wire.write(0x00); // ON_H
            Wire.write(0x00); // OFF_L
            Wire.write(0x10); // OFF_H
        } else {
            // Standard duty
            Wire.write(0x00); // ON_L
            Wire.write(0x00); // ON_H
            Wire.write(duty & 0xFF); // OFF_L
            Wire.write((duty >> 8) & 0x0F); // OFF_H
        }
        uint8_t err = Wire.endTransmission();
        if (i2cMutex) xSemaphoreGive(i2cMutex);

        if (err == 0) _lastDuty[channel] = duty;
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

    PCA9685Driver* getOrCreateDriver(uint8_t address) {
        for (int i = 0; i < _driverCount; i++) {
            if (_drivers[i] && _drivers[i]->getAddress() == address) {
                return _drivers[i];
            }
        }

        if (_driverCount >= 8) return nullptr;

        PCA9685Driver* drv = new PCA9685Driver(address);
        drv->begin();
        _drivers[_driverCount++] = drv;
        return drv;
    }

    void write(uint8_t address, uint8_t channel, uint16_t duty, bool force = false) {
        PCA9685Driver* drv = getOrCreateDriver(address);
        if (drv) {
            drv->writeChannel(channel, duty, force);
        }
    }

    void setFrequency(uint8_t address, uint16_t freq) {
        PCA9685Driver* drv = getOrCreateDriver(address);
        if (drv) {
            drv->setFrequency(freq);
        }
    }
};

extern PCA9685Manager pcaManager;

#endif // PCA9685_CONTROL_H
