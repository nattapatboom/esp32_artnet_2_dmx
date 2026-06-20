#ifndef DIMMER_CONTROL_H
#define DIMMER_CONTROL_H

#include <Arduino.h>
#include "output_control.h"
#include "config.h"

#define ZC_PULSE_WIDTH_US 20
#define DIMMER_TICK_US 39 // 10000us / 255 steps = 39.2us for 50Hz

static hw_timer_t *dimmerTimer = NULL;
static volatile uint32_t dimmer_tick = 0;

// Maximum number of dimmer channels for quick ISR access
#define MAX_DIMMER_CHANNELS 8
struct DimmerCh {
    uint8_t pin;
    volatile uint8_t* dmxVal;
};

static DimmerCh dimmerChannels[MAX_DIMMER_CHANNELS];
static volatile uint8_t numDimmerChannels = 0;
static volatile bool dimmerEnabled = false;

// IRAM_ATTR is required for hardware interrupt handlers
void IRAM_ATTR onDimmerTimer() {
    if (!dimmerEnabled) return;
    dimmer_tick++;
    
    // Check all registered dimmer channels
    for (uint8_t i = 0; i < numDimmerChannels; i++) {
        uint8_t val = *(dimmerChannels[i].dmxVal);
        if (val > 0) {
            uint32_t targetTick = 255 - val;
            if (dimmer_tick == targetTick) {
                digitalWrite(dimmerChannels[i].pin, HIGH);
            }
        }
    }
}

void IRAM_ATTR onZeroCross() {
    if (!dimmerEnabled) return;
    
    // Reset tick
    dimmer_tick = 0;
    
    // Turn off all gates at zero cross
    for (uint8_t i = 0; i < numDimmerChannels; i++) {
        digitalWrite(dimmerChannels[i].pin, LOW);
        
        // If 100% full brightness, turn it back on immediately
        if (*(dimmerChannels[i].dmxVal) == 255) {
            digitalWrite(dimmerChannels[i].pin, HIGH);
        }
    }
    
    // Reset timer to 0 so we count exactly from the ZC
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
        
        // Find all Dimmer channels
        std::vector<OutputChannel>& chs = outputCtrl.getChannels();
        for (auto& ch : chs) {
            if (ch.type == 3 && ch.dmxBuffer != nullptr) { // CHAN_TYPE_DIMMER
                if (numDimmerChannels < MAX_DIMMER_CHANNELS) {
                    dimmerChannels[numDimmerChannels].pin = ch.pin;
                    dimmerChannels[numDimmerChannels].dmxVal = &ch.dmxBuffer[0];
                    pinMode(ch.pin, OUTPUT);
                    digitalWrite(ch.pin, LOW);
                    numDimmerChannels++;
                    Serial.printf("Dimmer Channel initialized: GPIO %d, Start Universe %d\n", ch.pin, ch.start_universe);
                }
            }
        }

        if (numDimmerChannels > 0) {
            pinMode(sysCfg.zc_pin, INPUT_PULLUP);
            
            // Setup Timer 0, divider 80 = 1MHz = 1 tick per us
            dimmerTimer = timerBegin(0, 80, true);
            timerAttachInterrupt(dimmerTimer, &onDimmerTimer, true);
            // Trigger every 39us, autoreload = true
            timerAlarmWrite(dimmerTimer, DIMMER_TICK_US, true);
            timerAlarmEnable(dimmerTimer);

            // Attach ZC interrupt
            attachInterrupt(digitalPinToInterrupt(sysCfg.zc_pin), onZeroCross, RISING);
            
            dimmerEnabled = true;
            Serial.printf("Dimmer Control started with %d channels on ZC pin %d\n", numDimmerChannels, sysCfg.zc_pin);
        } else {
            Serial.println("Dimmer: No dimmer channels configured.");
        }
    }
};

extern DimmerControl dimmerCtrl;
// NOTE: DimmerControl dimmerCtrl is defined in main.cpp

#endif // DIMMER_CONTROL_H
