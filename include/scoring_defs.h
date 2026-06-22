#ifndef SCORING_DEFS_H
#define SCORING_DEFS_H

#include <Arduino.h>

namespace ScoringDefs {
constexpr uint8_t MAX_LEDC_RESOURCE = 16;
constexpr uint8_t MAX_RMT_RESOURCE = 8;
constexpr uint8_t MAX_UART_RESOURCE = 2;
constexpr uint8_t MAX_DAC_RESOURCE = 2;
constexpr uint8_t MAX_TIMER_RESOURCE = 4;

constexpr uint32_t CPU_BASE_OVERHEAD_US = 500;
constexpr uint32_t CPU_SAFETY_RESERVE_US = 1500;
constexpr uint32_t RAM_KEEP_FREE = 150 * 1024;
constexpr uint32_t RAM_LIMIT_CAP = 65535;

constexpr uint16_t I2C_WRITE_US = 180;
constexpr uint32_t BASE_CHANNEL_RAM = 224;
constexpr uint32_t DMX_BUFFER_RAM = 512;
constexpr uint32_t PIXEL_STRIP_OBJECT_RAM = 256;
constexpr uint32_t DFPLAYER_OBJECT_RAM = 160;
constexpr uint32_t STEPPER_RUNTIME_RAM = 512;
constexpr uint32_t FUNCGEN_OBJECT_RAM = 1120;
constexpr uint32_t I2C_ROUTE_RAM = 32;
constexpr uint32_t DMX_OUTPUT_SERVICE_US = 250;
}

#endif
