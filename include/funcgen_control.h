#ifndef FUNCGEN_CONTROL_H
#define FUNCGEN_CONTROL_H

#include <Arduino.h>
#include <esp_timer.h>

class FuncGenController {
public:
    FuncGenController() { computeTables(); }
    ~FuncGenController() { stop(); }

    void begin(uint8_t pin, uint8_t ledcChan, uint16_t pwmFreq = 50000);
    void update(uint8_t type, uint16_t freq, uint8_t amp, uint8_t offset);
    void stop();

private:
    esp_timer_handle_t timer = nullptr;
    uint8_t ledcChan = 255;
    uint8_t pin = 255;

    uint16_t freq = 100;
    uint8_t type = 0;
    uint8_t amp = 128;
    uint8_t offset = 128;

    uint32_t sampleIdx = 0;
    uint8_t numSamples = 128;

    uint16_t lastFreq = 0;
    uint8_t lastType = 0;
    uint8_t lastAmp = 0;
    uint8_t lastOffset = 0;

    int8_t sineTable[256];
    int8_t triTable[256];
    int8_t sawTable[256];
    int8_t sqTable[256];

    void computeTables();
    void rebuild();
    void sampleISR();
    static void timerCallback(void* arg) { static_cast<FuncGenController*>(arg)->sampleISR(); }
};

void FuncGenController::computeTables() {
    for (int i = 0; i < 256; i++) {
        float phase = i / 256.0f * 2.0f * M_PI;
        sineTable[i] = (int8_t)(sinf(phase) * 127.0f);
    }
    for (int i = 0; i < 256; i++) {
        if (i < 128) triTable[i] = i * 254 / 127 - 127;
        else triTable[i] = 254 - i * 254 / 127 + 127;
    }
    for (int i = 0; i < 256; i++) {
        sawTable[i] = i * 254 / 255 - 127;
    }
    for (int i = 0; i < 256; i++) {
        sqTable[i] = i < 128 ? 127 : -127;
    }
}

void FuncGenController::begin(uint8_t pin, uint8_t ledcChan, uint16_t pwmFreq) {
    this->pin = pin;
    this->ledcChan = ledcChan;
    ledcSetup(ledcChan, pwmFreq, 8);
    ledcAttachPin(pin, ledcChan);
    ledcWrite(ledcChan, 0);
    lastFreq = 0;
    sampleIdx = 0;
}

void FuncGenController::update(uint8_t newType, uint16_t newFreq, uint8_t newAmp, uint8_t newOffset) {
    type = newType;
    freq = newFreq;
    amp = newAmp;
    offset = newOffset;
    if (freq < 1) freq = 1;
    if (freq > 10000) freq = 10000;
    if (type > 4) type = 0;
    if (type == 4) { amp = 0; } // PWM mode: amp ignored
    if (type == 4 && freq > 50000) freq = 50000; // PWM can go higher

    if (type == lastType && freq == lastFreq && amp == lastAmp && offset == lastOffset) return;

    lastType = type;
    lastFreq = freq;
    lastAmp = amp;
    lastOffset = offset;
    rebuild();
}

void FuncGenController::stop() {
    if (timer) {
        esp_timer_stop(timer);
        esp_timer_delete(timer);
        timer = nullptr;
    }
    if (ledcChan != 255) ledcWrite(ledcChan, 0);
    sampleIdx = 0;
}

void FuncGenController::rebuild() {
    if (timer) {
        esp_timer_stop(timer);
        esp_timer_delete(timer);
        timer = nullptr;
    }

    if (type == 4) { // PWM mode: constant duty, no timer needed
        ledcWrite(ledcChan, offset);
        return;
    }

    if (freq <= 100) numSamples = 128;
    else if (freq <= 1000) numSamples = 64;
    else numSamples = 32;

    uint32_t period_us = 1000000UL / freq / numSamples;
    if (period_us < 50) period_us = 50;

    sampleIdx = 0;

    esp_timer_create_args_t args = {};
    args.callback = timerCallback;
    args.arg = this;
    args.name = "funcGen";
    esp_timer_create(&args, &timer);
    esp_timer_start_periodic(timer, period_us);
}

void FuncGenController::sampleISR() {
    int8_t* table;
    switch (type) {
        case 0: table = sineTable; break;
        case 1: table = triTable; break;
        case 2: table = sawTable; break;
        case 3: table = sqTable; break;
        default: return;
    }

    uint8_t idx = (sampleIdx * 256) / numSamples;
    int32_t val = table[idx] * amp / 255 + offset;
    ledcWrite(ledcChan, (uint32_t)constrain(val, 0, 255));

    sampleIdx++;
    if (sampleIdx >= numSamples) sampleIdx = 0;
}

#endif
