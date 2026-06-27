#ifndef OUTPUT_IMPL_H
#define OUTPUT_IMPL_H

// Inline implementations of OutputControl::loop(), updateLeds(), setupChannels().
// Included once by src/main.cpp after all device headers are available.

#include "output_control.h"
#include "output_devices/relay.h"
#include "output_devices/solenoid.h"
#include "output_devices/smoke_shooter.h"
#include "output_devices/dmx.h"
#include "output_devices/led_strip.h"

inline void OutputControl::loop() {
    static unsigned long lastDmxSend = 0;
    unsigned long now = millis();
    uint8_t fps = sysCfg.output_fps > 0 ? sysCfg.output_fps : 40;
    unsigned long interval = 1000 / fps;
    if (now - lastDmxSend >= interval) {
        lastDmxSend = now;
        dmxUpdate();
        relayUpdate();
        solenoidUpdate();
        smokeShooterUpdate();
    }
}

inline void OutputControl::updateLeds() {
    ledStripUpdate();
    extern void updateMotionControl();
    updateMotionControl();
}

inline void OutputControl::setupChannels() {
    uint8_t rmtIdx = 0;
    uint8_t dmxIdx = 0;
    pcaManager.clear();
    digitalExpanderManager.clear();
    bool uart2Used = false;
    bool uart1Used = false;
    for (const auto& ch : channels) {
        if (ch.type == OutputDefs::TYPE_DFPLAYER) {
            if (!uart2Used) uart2Used = true;
            else if (!uart1Used) uart1Used = true;
        }
    }

    uint8_t dfPlayerCount = 0;
    uint8_t pcaAddrs[8];
    uint8_t pcaAddrCount = 0;

    for (auto& ch : channels) {
        if (ch.source == 1) {
            bool alreadyInitialized = false;
            for (uint8_t i = 0; i < pcaAddrCount; i++) {
                if (pcaAddrs[i] == ch.pca_addr) { alreadyInitialized = true; break; }
            }
            if (!alreadyInitialized) {
                uint16_t freq = getPcaSharedFrequency(ch.pca_addr);
                pcaManager.getOrCreateDriver(ch.pca_addr);
                pcaManager.setFrequency(ch.pca_addr, freq);
                if (pcaAddrCount < 8) pcaAddrs[pcaAddrCount++] = ch.pca_addr;
                Serial.printf("PCA9685 initialized at 0x%02X: freq=%dHz type=%d\n", ch.pca_addr, freq, ch.type);
            } else {
                pcaManager.getOrCreateDriver(ch.pca_addr);
                uint16_t freq = getPcaSharedFrequency(ch.pca_addr);
                Serial.printf("PCA9685 at 0x%02X already initialized (shared), type=%d uses freq=%dHz\n", ch.pca_addr, ch.type, freq);
            }
            continue;
        } else if (ch.source >= 2 && ch.source <= 4) {
            writeOutputPin(ch, 1, false);
            if ((ch.type == OutputDefs::TYPE_SMOKE || ch.type == OutputDefs::TYPE_MOTOR || ch.type == OutputDefs::TYPE_STEPPER) && ch.pca_channel2 != 255) {
                writeOutputPin(ch, 2, false);
            }
            if ((ch.type == OutputDefs::TYPE_MOTOR || ch.type == OutputDefs::TYPE_STEPPER) && ch.pca_channel3 != 255) {
                writeOutputPin(ch, 3, false);
            }
            Serial.printf("Expander initialized: src=%d addr=0x%02X type=%d\n", ch.source, ch.pca_addr, ch.type);
            continue;
        }

        if (ch.type == OutputDefs::TYPE_LED_STRIP) {
            ledStripSetup(ch, rmtIdx);
        } else if (ch.type == OutputDefs::TYPE_RELAY) {
            relaySetup(ch);
            Serial.printf("Relay init: GPIO%d U%d\n", ch.pin, ch.start_universe);
        } else if (ch.type == OutputDefs::TYPE_SOLENOID) {
            solenoidSetup(ch);
            Serial.printf("Solenoid init: GPIO%d U%d pulse=%dms thresh=%d\n",
                ch.pin, ch.start_universe, ch.solenoid_pulse_ms, ch.solenoid_threshold);
        } else if (ch.type == OutputDefs::TYPE_DMX) {
            dmxSetup(ch, uart2Used, uart1Used, rmtIdx);
        } else if (ch.type == OutputDefs::TYPE_SMOKE) {
            smokeShooterSetup(ch);
            Serial.printf("Smoke Shooter init: pin=%d pin2=%d thresh=%d\n", ch.pin, ch.pin2, ch.solenoid_threshold);
        } else if (ch.type == OutputDefs::TYPE_DFPLAYER) {
            ch.dfPlayer = new (std::nothrow) DFPlayerController();
            if (!ch.dfPlayer) {
                Serial.println("Error: DFPlayer malloc failed!");
                continue;
            }
            if (dfPlayerCount == 0) {
                ch.dfPlayer->begin(Serial2, ch.pin, ch.pin2);
                Serial.printf("DFPlayer (UART2): TX=%d RX=%d\n", ch.pin, ch.pin2);
                dfPlayerCount++;
            } else if (dfPlayerCount == 1) {
                ch.dfPlayer->begin(Serial1, ch.pin, ch.pin2);
                Serial.printf("DFPlayer (UART1): TX=%d RX=%d\n", ch.pin, ch.pin2);
                dfPlayerCount++;
            } else {
                Serial.println("Error: No UART for DFPlayer!");
            }
        }
    }
}

#endif // OUTPUT_IMPL_H
