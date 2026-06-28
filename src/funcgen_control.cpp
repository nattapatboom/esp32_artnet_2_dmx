#include "output_devices/funcgen_control.h"

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
        sqTable[i] = i < 128 ? 128 : -128;
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
    if (type == 4) { amp = 0; }
    if (type == 4 && freq > 50000) freq = 50000;

    if (type == lastType && freq == lastFreq && amp == lastAmp && offset == lastOffset) return;

    lastType = type;
    lastFreq = freq;
    lastAmp = amp;
    lastOffset = offset;
    rebuild();
}

void FuncGenController::stop() {
    timerRunning = false;
    if (timer) {
        esp_timer_stop(timer);
        esp_timer_delete(timer);
        timer = nullptr;
    }
    if (ledcChan != 255) ledcWrite(ledcChan, 0);
    sampleIdx = 0;
}

void FuncGenController::rebuild() {
    timerRunning = false;
    if (timer) {
        esp_timer_stop(timer);
        esp_timer_delete(timer);
        timer = nullptr;
    }

    if (type == 4) {
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
    esp_err_t err = esp_timer_create(&args, &timer);
    if (err != ESP_OK) {
        Serial.printf("[funcgen] Failed to create timer: %d\n", err);
        return;
    }
    err = esp_timer_start_periodic(timer, period_us);
    if (err != ESP_OK) {
        Serial.printf("[funcgen] Failed to start timer: %d\n", err);
        esp_timer_delete(timer);
        timer = nullptr;
        return;
    }
    timerRunning = true;
}

void FuncGenController::sampleISR() {
    if (!timerRunning) return;
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
