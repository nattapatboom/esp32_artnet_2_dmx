#ifndef OUTPUT_DEVICES_RELAY_H
#define OUTPUT_DEVICES_RELAY_H

#include <Arduino.h>
#include "output_control.h"

inline void relaySetup(OutputChannel& ch) {
    if (ch.pin == 255) return;
    pinMode(ch.pin, OUTPUT);
    digitalWrite(ch.pin, ch.pin_invert ? HIGH : LOW);
}

inline void relayUpdate() {
    for (auto& ch : outputCtrl.getChannels()) {
        if (ch.type == OutputDefs::TYPE_RELAY && ch.dmxBuffer != nullptr) {
            bool state = ch.dmxBuffer[0] > 127;
            writeOutputPin(ch, 1, state);
        }
    }
}

#endif
