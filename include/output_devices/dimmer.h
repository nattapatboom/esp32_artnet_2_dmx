#ifndef OUTPUT_DEVICES_DIMMER_H
#define OUTPUT_DEVICES_DIMMER_H

#include <Arduino.h>
#include "output_control.h"
#include "config.h"

#define ZC_PULSE_WIDTH_US 20
#define DIMMER_TICK_US 39
#define DIMMER_TICK_MAX 512

#include <driver/gpio.h>

extern hw_timer_t *dimmerTimer;
extern volatile uint32_t dimmer_tick;

#define MAX_DIMMER_CHANNELS 8
struct DimmerCh {
    uint8_t pin;
    uint8_t* volatile* dmxVal;
};

extern volatile DimmerCh dimmerChannels[MAX_DIMMER_CHANNELS];
extern volatile uint8_t numDimmerChannels;
extern volatile bool dimmerEnabled;

void IRAM_ATTR onDimmerTimer() {
    if (!dimmerEnabled) return;
    dimmer_tick++;
    if (dimmer_tick >= DIMMER_TICK_MAX) {
        dimmer_tick = DIMMER_TICK_MAX;
        return;
    }
    for (uint8_t i = 0; i < numDimmerChannels; i++) {
        uint8_t val = *(*(dimmerChannels[i].dmxVal));
        if (val > 0) {
            uint32_t targetTick = 255 - val;
            if (dimmer_tick == targetTick) {
                gpio_set_level((gpio_num_t)dimmerChannels[i].pin, HIGH);
            }
        }
    }
}

void IRAM_ATTR onZeroCross() {
    if (!dimmerEnabled) return;
    static volatile uint64_t lastZcTime = 0;
    uint64_t now = esp_timer_get_time();
    if (now - lastZcTime < 5000) return;
    lastZcTime = now;
    dimmer_tick = 0;
    for (uint8_t i = 0; i < numDimmerChannels; i++) {
        gpio_set_level((gpio_num_t)dimmerChannels[i].pin, LOW);
        if (*(*(dimmerChannels[i].dmxVal)) == 255) {
            gpio_set_level((gpio_num_t)dimmerChannels[i].pin, HIGH);
        }
    }
    timerWrite(dimmerTimer, 0);
}

class DimmerControl {
public:
    void begin() {
        if (sysCfg.zc_pin == 255) {
            Serial.println("Dimmer: ZC pin not configured.");
            return;
        }

        numDimmerChannels = 0;

        std::vector<OutputChannel>& chs = outputCtrl.getChannels();
        for (auto& ch : chs) {
            if (ch.type == OutputDefs::TYPE_DIMMER && ch.dmxBuffer != nullptr && ch.pin != 255) {
                if (numDimmerChannels < MAX_DIMMER_CHANNELS) {
                    dimmerChannels[numDimmerChannels].pin = ch.pin;
                    dimmerChannels[numDimmerChannels].dmxVal = &ch.dmxBuffer;
                    pinMode(ch.pin, OUTPUT);
                    digitalWrite(ch.pin, LOW);
                    numDimmerChannels++;
                }
            }
        }

        if (numDimmerChannels > 0) {
            pinMode(sysCfg.zc_pin, INPUT_PULLUP);
            if (dimmerTimer) {
                timerEnd(dimmerTimer);
                dimmerTimer = nullptr;
            }
            dimmerTimer = timerBegin(0, 80, true);
            timerAttachInterrupt(dimmerTimer, &onDimmerTimer, true);
            timerAlarmWrite(dimmerTimer, DIMMER_TICK_US, true);
            timerAlarmEnable(dimmerTimer);
            attachInterrupt(digitalPinToInterrupt(sysCfg.zc_pin), onZeroCross, RISING);
            dimmerEnabled = true;
        }
    }
};

extern DimmerControl dimmerCtrl;

#endif
