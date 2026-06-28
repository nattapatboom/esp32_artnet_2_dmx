#ifndef I2C_DEVICES_DISPLAY_DRIVER_H
#define I2C_DEVICES_DISPLAY_DRIVER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "i2c_devices/i2c_bus.h"

#define DISPLAY_OFF      0
#define DISPLAY_SSD1306  1
#define DISPLAY_SH1106   2
#define DISPLAY_PCF8574  3

#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>
#include <LiquidCrystal_I2C.h>

#define DISPLAY_MAX_ERRORS 3

class DisplayDriver {
public:
    DisplayDriver() {}

    bool begin(uint8_t type, uint8_t addr, uint8_t cols = 20, uint8_t rows = 4) {
        end();
        _type = type;
        _addr = addr;
        _cols = cols;
        _rows = rows;
        if (type == DISPLAY_OFF) return false;

        if (!I2cBus::probe(addr)) return false;

        I2cBus::Lock lock;
        if (!lock.locked()) return false;

        if (type == DISPLAY_SSD1306) {
            _oled = new (std::nothrow) SSD1306AsciiWire(Wire);
            if (!_oled) return false;
            _oled->begin(&Adafruit128x64, addr);
            _oled->setFont(System5x7);
            _oled->clear();
            _cols = 21;
            _active = true;
        } else if (type == DISPLAY_SH1106) {
            _oled = new (std::nothrow) SSD1306AsciiWire(Wire);
            if (!_oled) return false;
            _oled->begin(&SH1106_128x64, addr);
            _oled->setFont(System5x7);
            _oled->clear();
            _cols = 21;
            _active = true;
        } else if (type == DISPLAY_PCF8574) {
            _lcd = new (std::nothrow) LiquidCrystal_I2C(addr, cols, rows);
            if (!_lcd) return false;
            _lcd->init();
            _lcd->backlight();
            _lcd->clear();
            _active = true;
        }

        return _active;
    }

    uint8_t displayCols() const { return _cols; }
    uint8_t displayRows() const { return _rows; }
    bool isOled() const { return _oled != nullptr; }

    void showBitmap(const uint8_t* bitmap, uint16_t len) {
        if (!_active || !_oled) return;
        I2cBus::Lock lock;
        if (!lock.locked()) return;
        Wire.beginTransmission(_addr);
        Wire.write((uint8_t)0x00);
        Wire.write((uint8_t)0x21); Wire.write(0); Wire.write(127);
        Wire.write((uint8_t)0x22); Wire.write(0); Wire.write(7);
        Wire.endTransmission();
        for (uint16_t i = 0; i < len; i += 16) {
            Wire.beginTransmission(_addr);
            Wire.write((uint8_t)0x40);
            for (uint8_t j = 0; j < 16; j++) {
                Wire.write(pgm_read_byte(&bitmap[i + j]));
            }
            Wire.endTransmission();
        }
    }

    void update(const String& line1, const String& line2, const String& line3, const String& line4) {
        if (!_active) return;

        I2cBus::Lock lock;
        if (!lock.locked()) return;

        Wire.beginTransmission(_addr);
        if (Wire.endTransmission() != 0) {
            _errorCount++;
            if (_errorCount >= DISPLAY_MAX_ERRORS) {
                _active = false;
                if (_oled) { delete _oled; _oled = nullptr; }
                if (_lcd) { delete _lcd; _lcd = nullptr; }
            }
            return;
        }
        _errorCount = 0;

        String raw[4] = {line1, line2, line3, line4};
        uint8_t n = _oled ? 4 : (_rows < 4 ? _rows : 4);
        for (uint8_t i = 0; i < n; i++) {
            if ((uint8_t)raw[i].length() > _cols) {
                raw[i] = raw[i].substring(0, _cols);
            }
        }

        if (_oled) {
            _oled->clear();
            for (uint8_t i = 0; i < n; i++) {
                _oled->setCursor(0, i);
                _oled->print(raw[i].c_str());
            }
        } else if (_lcd) {
            _lcd->clear();
            for (uint8_t i = 0; i < n; i++) {
                _lcd->setCursor(0, i);
                _lcd->print(raw[i].c_str());
            }
        }

    }

    void setBrightness(uint8_t level) {
        if (!_active) return;
        _brightness = level;

        I2cBus::Lock lock;
        if (!lock.locked()) return;

        Wire.beginTransmission(_addr);
        if (Wire.endTransmission() == 0) {
            if (_oled) {
                _oled->setContrast(level);
            } else if (_lcd) {
                if (level > 0) _lcd->backlight();
                else _lcd->noBacklight();
            }
        }

    }

    void end() {
        if (!_active) return;

        {
            I2cBus::Lock lock;
            if (!lock.locked()) return;

            Wire.beginTransmission(_addr);
            if (Wire.endTransmission() == 0) {
                if (_oled) {
                    _oled->clear();
                } else if (_lcd) {
                    _lcd->clear();
                    _lcd->noBacklight();
                }
            }
        }

        if (_oled) { delete _oled; _oled = nullptr; }
        if (_lcd) { delete _lcd; _lcd = nullptr; }
        _active = false;
        _errorCount = 0;
    }

    bool isActive() const { return _active; }

    bool tryRecover() {
        if (_active) return true;
        if (_type == DISPLAY_OFF) return false;

        bool alive = I2cBus::probe(_addr);

        if (!alive) return false;

        return begin(_type, _addr);
    }

private:
    bool _active = false;
    uint8_t _type = 0;
    uint8_t _addr = 0x3C;
    uint8_t _cols = 20;
    uint8_t _rows = 4;
    uint8_t _brightness = 128;
    uint8_t _errorCount = 0;
    SSD1306AsciiWire* _oled = nullptr;
    LiquidCrystal_I2C* _lcd = nullptr;
};

extern DisplayDriver display;

#endif
