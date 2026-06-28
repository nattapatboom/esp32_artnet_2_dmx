#ifndef OUTPUT_DEVICES_FUNCGEN_CONTROL_H
#define OUTPUT_DEVICES_FUNCGEN_CONTROL_H

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
    volatile bool timerRunning = false;
    volatile uint8_t ledcChan = 255;
    uint8_t pin = 255;

    uint16_t freq = 100;
    volatile uint8_t type = 0;
    volatile uint8_t amp = 128;
    volatile uint8_t offset = 128;

    volatile uint32_t sampleIdx = 0;
    volatile uint8_t numSamples = 128;

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

#endif
