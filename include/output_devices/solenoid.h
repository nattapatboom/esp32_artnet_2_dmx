#ifndef OUTPUT_DEVICES_SOLENOID_H
#define OUTPUT_DEVICES_SOLENOID_H

#include <Arduino.h>
#include "output_control.h"

inline void solenoidSetup(OutputChannel& ch) {
    pinMode(ch.pin, OUTPUT);
    digitalWrite(ch.pin, ch.pin_invert ? HIGH : LOW);
    if (ch.solenoid_pulse_ms == 0) ch.solenoid_pulse_ms = 50;
    if (ch.solenoid_threshold == 0) ch.solenoid_threshold = 127;
}

inline void solenoidUpdate() {
    unsigned long now = millis();
    for (auto& ch : outputCtrl.getChannels()) {
        if (ch.type != 17) continue;
        if (ch.dmxBuffer == nullptr) continue;
        bool should_trigger = (ch.dmxBuffer[0] > ch.solenoid_threshold);
        if (now - ch.solenoid_last_trigger < ch.solenoid_post_delay) should_trigger = false;
        if (should_trigger && !ch.solenoid_pulse_active) {
            if (now >= ch.solenoid_last_trigger + ch.solenoid_post_delay + ch.solenoid_pre_delay) {
                ch.solenoid_pulse_active = true;
                ch.solenoid_pulse_start = now;
                writeOutputPin(ch, 1, true);
                ch.solenoid_last_trigger = now;
            }
        }
        if (ch.solenoid_pulse_active && (now - ch.solenoid_pulse_start >= ch.solenoid_pulse_ms)) {
            writeOutputPin(ch, 1, false);
            ch.solenoid_pulse_active = false;
        }
    }
}

#endif
