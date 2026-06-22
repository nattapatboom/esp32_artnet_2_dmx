#ifndef OUTPUT_DEVICES_TM1637_H
#define OUTPUT_DEVICES_TM1637_H

#include <Arduino.h>

extern const uint8_t SEG_DIGITS[10];

class TM1637Driver {
private:
    uint8_t clk_pin;
    uint8_t dio_pin;

    void start() {
        pinMode(dio_pin, OUTPUT);
        digitalWrite(dio_pin, LOW);
        delayMicroseconds(5);
        digitalWrite(clk_pin, LOW);
        delayMicroseconds(5);
    }

    void stop() {
        pinMode(dio_pin, OUTPUT);
        digitalWrite(dio_pin, LOW);
        delayMicroseconds(5);
        digitalWrite(clk_pin, HIGH);
        delayMicroseconds(5);
        digitalWrite(dio_pin, HIGH);
        delayMicroseconds(5);
    }

    bool writeByte(uint8_t b) {
        for (uint8_t i = 0; i < 8; i++) {
            digitalWrite(clk_pin, LOW);
            pinMode(dio_pin, OUTPUT);
            digitalWrite(dio_pin, (b & 0x01) ? HIGH : LOW);
            delayMicroseconds(5);
            digitalWrite(clk_pin, HIGH);
            delayMicroseconds(5);
            b >>= 1;
        }
        digitalWrite(clk_pin, LOW);
        pinMode(dio_pin, INPUT_PULLUP);
        delayMicroseconds(5);
        digitalWrite(clk_pin, HIGH);
        delayMicroseconds(5);
        bool ack = (digitalRead(dio_pin) == LOW);
        digitalWrite(clk_pin, LOW);
        return ack;
    }

public:
    TM1637Driver(uint8_t clk, uint8_t dio) : clk_pin(clk), dio_pin(dio) {}

    void begin() {
        pinMode(clk_pin, OUTPUT);
        pinMode(dio_pin, OUTPUT);
        digitalWrite(clk_pin, HIGH);
        digitalWrite(dio_pin, HIGH);
    }

    void displayNum(uint16_t num) {
        if (num > 9999) num = 9999;
        uint8_t data[4];
        data[0] = SEG_DIGITS[num / 1000];
        data[1] = SEG_DIGITS[(num % 1000) / 100];
        data[2] = SEG_DIGITS[(num % 100) / 10];
        data[3] = SEG_DIGITS[num % 10];

        start();
        writeByte(0x40);
        stop();

        start();
        writeByte(0xC0);
        for (int i = 0; i < 4; i++) {
            writeByte(data[i]);
        }
        stop();

        start();
        writeByte(0x88 + 7);
        stop();
    }

    void displayRaw(const uint8_t* segments) {
        if (segments == nullptr) return;

        start();
        writeByte(0x40);
        stop();

        start();
        writeByte(0xC0);
        for (int i = 0; i < 4; i++) {
            writeByte(segments[i]);
        }
        stop();

        start();
        writeByte(0x88 + 7);
        stop();
    }
};

#endif
