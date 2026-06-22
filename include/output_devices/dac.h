#ifndef OUTPUT_DEVICES_DAC_H
#define OUTPUT_DEVICES_DAC_H

#include <Arduino.h>
#include "output_control.h"
#include "i2c_devices/i2c_dac.h"

inline void dacUpdate(OutputChannel& ch) {
    if (ch.source == 5) {
        I2cDac::writeMcp4725(ch.pca_addr, ch.dmxBuffer[0]);
    } else if (ch.source == 6) {
        I2cDac::writeDac7571(ch.pca_addr, ch.dmxBuffer[0]);
    } else if (ch.source == 7) {
        I2cDac::writeDac7573(ch.pca_addr, ch.pca_channel, ch.dmxBuffer[0]);
    }
}

#endif
