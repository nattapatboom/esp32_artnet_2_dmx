#ifndef OUTPUT_DEVICES_SMOKE_SHOOTER_H
#define OUTPUT_DEVICES_SMOKE_SHOOTER_H

#include <Arduino.h>
#include "output_control.h"

inline void smokeShooterSetup(OutputChannel& ch) {
    if (ch.pin == 255) return;
    if (ch.source == 0) {
        pinMode(ch.pin, OUTPUT);
        pinMode(ch.pin2, OUTPUT);
        digitalWrite(ch.pin, ch.pin_invert ? HIGH : LOW);
        digitalWrite(ch.pin2, ch.pin2_invert ? HIGH : LOW);
    }
    ch.smoke_state = 0;
    ch.smoke_prev_trigger = false;
}

inline void smokeShooterUpdate() {
    unsigned long now = millis();
    for (auto& ch : outputCtrl.getChannels()) {
        if (ch.type != 18) continue;
        if (ch.dmxBuffer == nullptr) continue;
        bool trigger_active = (ch.dmxBuffer[0] > ch.solenoid_threshold);
        if (ch.smoke_state == 0) {
            writeOutputPin(ch, 1, false);
            writeOutputPin(ch, 2, false);
            if (trigger_active && !ch.smoke_prev_trigger) {
                ch.smoke_state = 1;
                ch.smoke_timer = now;
                writeOutputPin(ch, 1, true);
            }
        } else if (ch.smoke_state == 1) {
            writeOutputPin(ch, 1, true);
            writeOutputPin(ch, 2, false);
            if (now - ch.smoke_timer >= ch.smoke_duration_ms) {
                ch.smoke_state = 2;
                ch.smoke_timer = now;
                writeOutputPin(ch, 1, false);
            }
        } else if (ch.smoke_state == 2) {
            writeOutputPin(ch, 1, false);
            writeOutputPin(ch, 2, false);
            if (now - ch.smoke_timer >= ch.settle_delay_ms) {
                ch.smoke_state = 3;
                ch.smoke_timer = now;
                writeOutputPin(ch, 2, true);
            }
        } else if (ch.smoke_state == 3) {
            writeOutputPin(ch, 1, false);
            writeOutputPin(ch, 2, true);
            if (now - ch.smoke_timer >= ch.shoot_duration_ms) {
                ch.smoke_state = 4;
                ch.smoke_timer = now;
                writeOutputPin(ch, 2, false);
            }
        } else if (ch.smoke_state == 4) {
            writeOutputPin(ch, 1, false);
            writeOutputPin(ch, 2, false);
            if (now - ch.smoke_timer >= ch.smoke_lockout_ms) {
                ch.smoke_state = 0;
            }
        }
        ch.smoke_prev_trigger = trigger_active;
    }
}

#endif
