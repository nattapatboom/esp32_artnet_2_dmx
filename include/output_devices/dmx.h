#ifndef OUTPUT_DEVICES_DMX_H
#define OUTPUT_DEVICES_DMX_H

#include <Arduino.h>
#include <esp_dmx.h>
#include <new>
#include "output_control.h"
#include "output_devices/rmt_dmx.h"

inline void dmxSetup(OutputChannel& ch, bool& uart2Used, bool& uart1Used, uint8_t& rmtIdx) {
    if (ch.pin == 255) return;
    if (ch.dmxPort != 255) {
        dmx_driver_delete((dmx_port_t)ch.dmxPort);
        ch.dmxPort = 255;
    }
    if (ch.rmtDmx != nullptr) {
        delete ch.rmtDmx;
        ch.rmtDmx = nullptr;
    }

    esp_err_t err;
    if (!uart2Used) {
        ch.dmxPort = (uint8_t)DMX_NUM_2;
        uart2Used = true;
        dmx_config_t dmxConfig = DMX_CONFIG_DEFAULT;
        err = dmx_driver_install(DMX_NUM_2, &dmxConfig, DMX_INTR_FLAGS_DEFAULT);
        if (err != ESP_OK) {
            Serial.printf("DMX: dmx_driver_install UART2 failed err=%d\n", err);
            ch.dmxPort = 255;
            uart2Used = false;
            return;
        }
        err = dmx_set_pin(DMX_NUM_2, ch.pin, DMX_PIN_NO_CHANGE, DMX_PIN_NO_CHANGE);
        if (err != ESP_OK) {
            Serial.printf("DMX: dmx_set_pin UART2 failed err=%d\n", err);
        }
    } else if (!uart1Used) {
        ch.dmxPort = (uint8_t)DMX_NUM_1;
        uart1Used = true;
        dmx_config_t dmxConfig = DMX_CONFIG_DEFAULT;
        err = dmx_driver_install(DMX_NUM_1, &dmxConfig, DMX_INTR_FLAGS_DEFAULT);
        if (err != ESP_OK) {
            Serial.printf("DMX: dmx_driver_install UART1 failed err=%d\n", err);
            ch.dmxPort = 255;
            uart1Used = false;
            return;
        }
        err = dmx_set_pin(DMX_NUM_1, ch.pin, DMX_PIN_NO_CHANGE, DMX_PIN_NO_CHANGE);
        if (err != ESP_OK) {
            Serial.printf("DMX: dmx_set_pin UART1 failed err=%d\n", err);
        }
    } else if (rmtIdx < 8) {
        ch.rmtDmx = new (std::nothrow) RmtDmxDriver(rmtIdx, ch.pin);
        if (ch.rmtDmx != nullptr) ch.rmtDmx->begin();
        if (ch.rmtDmx == nullptr || !ch.rmtDmx->ready()) {
            Serial.printf("DMX: RMT fallback allocation failed ch=%d gpio=%d\n", rmtIdx, ch.pin);
            delete ch.rmtDmx;
            ch.rmtDmx = nullptr;
            return;
        }
        rmtIdx++;
    }
}

inline void dmxUpdate() {
    static uint8_t dmxTxBuffer[513];
    for (auto& ch : outputCtrl.getChannels()) {
        if (ch.type == OutputDefs::TYPE_DMX && ch.dmxBuffer != nullptr) {
            if (ch.dmxPort != 255) {
                dmx_port_t port = (dmx_port_t)ch.dmxPort;
                if (!dmx_wait_sent(port, 0)) continue;
                dmxTxBuffer[0] = 0x00;
                memcpy(dmxTxBuffer + 1, ch.dmxBuffer, 512);
                dmx_write(port, dmxTxBuffer, DMX_PACKET_SIZE);
                dmx_send(port, DMX_PACKET_SIZE);
            } else if (ch.rmtDmx != nullptr) {
                ch.rmtDmx->send(ch.dmxBuffer);
            }
        }
    }
}

#endif
