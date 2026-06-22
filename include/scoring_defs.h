#ifndef SCORING_DEFS_H
#define SCORING_DEFS_H

#include <Arduino.h>
#include "output_defs.h"
#include "scoring_limits.h"

namespace ScoringDefs {

// Runtime constants specific to scoring logic (not hardware limits)
// Hardware limits moved to ScoringLimits in scoring_limits.h

constexpr uint8_t MAX_LEDC_RESOURCE = ScoringLimits::MAX_LEDC;
constexpr uint8_t MAX_RMT_RESOURCE = ScoringLimits::MAX_RMT;
constexpr uint8_t MAX_UART_RESOURCE = ScoringLimits::MAX_UART;
constexpr uint8_t MAX_DAC_RESOURCE = ScoringLimits::MAX_DAC;
constexpr uint8_t MAX_TIMER_RESOURCE = ScoringLimits::MAX_TIMER;

constexpr uint32_t CPU_BASE_OVERHEAD_US = ScoringLimits::BASE_OVERHEAD_US;
constexpr uint32_t CPU_SAFETY_RESERVE_US = ScoringLimits::SAFETY_RESERVE_US;
constexpr uint32_t RAM_KEEP_FREE = ScoringLimits::KEEP_FREE_BYTES;
constexpr uint32_t RAM_LIMIT_CAP = ScoringLimits::LIMIT_CAP_BYTES;

constexpr uint16_t I2C_WRITE_US = ScoringLimits::I2C_WRITE_US;
constexpr uint32_t BASE_CHANNEL_RAM = ScoringLimits::BASE_CHANNEL_BYTES;
constexpr uint32_t DMX_BUFFER_RAM = ScoringLimits::DMX_BUFFER_BYTES;
constexpr uint32_t I2C_ROUTE_RAM = ScoringLimits::I2C_ROUTE_BYTES;
constexpr uint32_t DMX_OUTPUT_SERVICE_US = ScoringLimits::DMX_SERVICE_US;
}

#endif
