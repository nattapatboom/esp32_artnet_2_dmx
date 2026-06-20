#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

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

    bool begin(uint8_t type, uint8_t addr) {
        end();
        _type = type;
        _addr = addr;
        if (type == DISPLAY_OFF) return false;

        if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;

        Wire.beginTransmission(addr);
        if (Wire.endTransmission() != 0) {
            if (i2cMutex) xSemaphoreGive(i2cMutex);
            return false;
        }

        if (type == DISPLAY_SSD1306) {
            _oled = new SSD1306AsciiWire(Wire);
            _oled->begin(&Adafruit128x64, addr);
            _oled->setFont(System5x7);
            _oled->clear();
            _active = true;
        } else if (type == DISPLAY_SH1106) {
            _oled = new SSD1306AsciiWire(Wire);
            _oled->begin(&SH1106_128x64, addr);
            _oled->setFont(System5x7);
            _oled->clear();
            _active = true;
        } else if (type == DISPLAY_PCF8574) {
            _lcd = new LiquidCrystal_I2C(addr, 20, 4);
            _lcd->init();
            _lcd->backlight();
            _lcd->clear();
            _active = true;
        }

        if (i2cMutex) xSemaphoreGive(i2cMutex);
        return _active;
    }

    void update(const String& line1, const String& line2, const String& line3, const String& line4) {
        if (!_active) return;

        if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

        Wire.beginTransmission(_addr);
        if (Wire.endTransmission() != 0) {
            _errorCount++;
            if (_errorCount >= DISPLAY_MAX_ERRORS) {
                _active = false;
                if (_oled) { delete _oled; _oled = nullptr; }
                if (_lcd) { delete _lcd; _lcd = nullptr; }
            }
            if (i2cMutex) xSemaphoreGive(i2cMutex);
            return;
        }
        _errorCount = 0;

        if (_oled) {
            _oled->clear();
            _oled->setCursor(0, 0);
            _oled->println(line1.c_str());
            _oled->println(line2.c_str());
            _oled->println(line3.c_str());
            _oled->println(line4.c_str());
        } else if (_lcd) {
            _lcd->clear();
            _lcd->setCursor(0, 0);
            _lcd->print(line1.c_str());
            _lcd->setCursor(0, 1);
            _lcd->print(line2.c_str());
            _lcd->setCursor(0, 2);
            _lcd->print(line3.c_str());
            _lcd->setCursor(0, 3);
            _lcd->print(line4.c_str());
        }

        if (i2cMutex) xSemaphoreGive(i2cMutex);
    }

    void setBrightness(uint8_t level) {
        if (!_active) return;
        _brightness = level;

        if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

        Wire.beginTransmission(_addr);
        if (Wire.endTransmission() == 0) {
            if (_oled) {
                _oled->setContrast(level);
            } else if (_lcd) {
                if (level > 0) _lcd->backlight();
                else _lcd->noBacklight();
            }
        }

        if (i2cMutex) xSemaphoreGive(i2cMutex);
    }

    void end() {
        if (!_active) return;

        if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

        Wire.beginTransmission(_addr);
        if (Wire.endTransmission() == 0) {
            if (_oled) {
                _oled->clear();
            } else if (_lcd) {
                _lcd->clear();
                _lcd->noBacklight();
            }
        }

        if (i2cMutex) xSemaphoreGive(i2cMutex);

        if (_oled) { delete _oled; _oled = nullptr; }
        if (_lcd) { delete _lcd; _lcd = nullptr; }
        _active = false;
        _errorCount = 0;
    }

    bool isActive() const { return _active; }

private:
    bool _active = false;
    uint8_t _type = 0;
    uint8_t _addr = 0x3C;
    uint8_t _brightness = 128;
    uint8_t _errorCount = 0;
    SSD1306AsciiWire* _oled = nullptr;
    LiquidCrystal_I2C* _lcd = nullptr;
};

extern DisplayDriver display;

#endif
