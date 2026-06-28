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
        const auto* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
        uint8_t pinCount = def != nullptr ? def->pinCount : 1;
        
        for (uint8_t r = 0; r < pinCount; r++) {
            uint8_t src = ch.routes[r].source;
            uint8_t addr = ch.routes[r].addr;
            if (OutputDefs::isPwmExpanderSource(src)) {
                bool alreadyInitialized = false;
                for (uint8_t i = 0; i < pcaAddrCount; i++) {
                    if (pcaAddrs[i] == addr) { alreadyInitialized = true; break; }
                }
                if (!alreadyInitialized) {
                    uint16_t freq = getPcaSharedFrequency(addr);
                    pcaManager.getOrCreateDriver(addr, src);
                    pcaManager.setFrequency(addr, freq, src);
                    if (pcaAddrCount < 8) pcaAddrs[pcaAddrCount++] = addr;
                    Serial.printf("%s initialized at 0x%02X: freq=%dHz type=%d\n", src == 8 ? "PCA9635" : "PCA9685", addr, freq, ch.type);
                }
            } else if (src >= 2 && src <= 4) {
                writeOutputPin(ch, r + 1, false);
            }
        }
    }



    for (auto& ch : channels) {
        if (ch.type == OutputDefs::TYPE_LED_STRIP) {
            ledStripSetup(ch, rmtIdx);
        } else if (ch.type == OutputDefs::TYPE_RELAY) {
            relaySetup(ch);
            Serial.printf("Relay init: GPIO%d U%d\n", ch.routes[0].pin, ch.start_universe);
        } else if (ch.type == OutputDefs::TYPE_SOLENOID) {
            solenoidSetup(ch);
            Serial.printf("Solenoid init: GPIO%d U%d pulse=%dms thresh=%d\n",
                ch.routes[0].pin, ch.start_universe, ch.solenoid_pulse_ms, ch.solenoid_threshold);
        } else if (ch.type == OutputDefs::TYPE_DMX) {
            dmxSetup(ch, uart2Used, uart1Used, rmtIdx);
        } else if (ch.type == OutputDefs::TYPE_SMOKE) {
            smokeShooterSetup(ch);
            Serial.printf("Smoke Shooter init: pin=%d pin2=%d thresh=%d\n", ch.routes[0].pin, ch.routes[1].pin, ch.solenoid_threshold);
        } else if (ch.type == OutputDefs::TYPE_DFPLAYER) {
            ch.dfPlayer = new (std::nothrow) DFPlayerController();
            if (!ch.dfPlayer) {
                Serial.println("Error: DFPlayer malloc failed!");
                continue;
            }
            if (dfPlayerCount == 0) {
                ch.dfPlayer->begin(Serial2, ch.routes[0].pin, ch.routes[1].pin);
                Serial.printf("DFPlayer (UART2): TX=%d RX=%d\n", ch.routes[0].pin, ch.routes[1].pin);
                dfPlayerCount++;
            } else if (dfPlayerCount == 1) {
                ch.dfPlayer->begin(Serial1, ch.routes[0].pin, ch.routes[1].pin);
                Serial.printf("DFPlayer (UART1): TX=%d RX=%d\n", ch.routes[0].pin, ch.routes[1].pin);
                dfPlayerCount++;
            } else {
                Serial.println("Error: No UART for DFPlayer!");
            }
        }
    }
}

#endif // OUTPUT_IMPL_H
