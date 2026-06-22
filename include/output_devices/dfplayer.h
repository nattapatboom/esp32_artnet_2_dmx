#ifndef OUTPUT_DEVICES_DFPLAYER_H
#define OUTPUT_DEVICES_DFPLAYER_H

#include <Arduino.h>
#include "output_control.h"

inline void dfPlayerUpdate(OutputChannel& ch) {
    if (ch.dfPlayer != nullptr && ch.dmxBuffer != nullptr) {
        ch.dfPlayer->update(ch.dmxBuffer[0], ch.dmxBuffer[1], ch.dmxBuffer[2]);
    }
}

#endif
