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

        String lines[4] = {line1, line2, line3, line4};
        for (int i = 0; i < 4; i++) {
            if ((uint8_t)lines[i].length() > _cols) {
                lines[i] = lines[i].substring(0, _cols);
            }
        }

        if (_oled) {
            _oled->clear();
            _oled->setCursor(0, 0);
            _oled->print(lines[0].c_str());
            _oled->setCursor(0, 1);
            _oled->print(lines[1].c_str());
            _oled->setCursor(0, 2);
            _oled->print(lines[2].c_str());
            _oled->setCursor(0, 3);
            _oled->print(lines[3].c_str());
        } else if (_lcd) {
            _lcd->clear();
            _lcd->setCursor(0, 0);
            _lcd->print(lines[0].c_str());
            _lcd->setCursor(0, 1);
            _lcd->print(lines[1].c_str());
            _lcd->setCursor(0, 2);
            _lcd->print(lines[2].c_str());
            _lcd->setCursor(0, 3);
            _lcd->print(lines[3].c_str());
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
