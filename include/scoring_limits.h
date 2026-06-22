#ifndef SCORING_LIMITS_H
#define SCORING_LIMITS_H

#include <stdint.h>

// ─────────────────────────────────────────────
//  Scoring Limits
//  Hardware resource limits, CPU/RAM budget
//  constants, and score blocker enum.
//  Single source of truth shared between C++
//  and Web UI (mirror of HW/COST objects in
//  web/js/_gpio.js).
// ─────────────────────────────────────────────

namespace ScoringLimits {

// ── Hardware resource caps ──
constexpr uint8_t MAX_LEDC   = 16;
constexpr uint8_t MAX_RMT    = 8;
constexpr uint8_t MAX_UART   = 2;   // UART0 is console
constexpr uint8_t MAX_DAC    = 2;
constexpr uint8_t MAX_TIMER  = 4;

// ── CPU budget constants ──
constexpr uint32_t BASE_OVERHEAD_US  = 500;   // output loop overhead per frame
constexpr uint32_t SAFETY_RESERVE_US = 1500;  // reserve per frame
constexpr uint16_t I2C_WRITE_US      = 180;   // per active I2C transaction
constexpr uint16_t DMX_SERVICE_US    = 250;   // DMX UART/RMT enqueue

// ── RAM budget constants ──
constexpr uint32_t KEEP_FREE_BYTES  = 150 * 1024;  // keep free for network/system
constexpr uint32_t LIMIT_CAP_BYTES  = 65535;       // output allocation cap
constexpr uint32_t BASE_CHANNEL_BYTES = 224;       // OutputChannel slot
constexpr uint32_t DMX_BUFFER_BYTES = 512;         // per DMX universe
constexpr uint32_t I2C_ROUTE_BYTES  = 32;          // per I2C route bookkeeping

}  // namespace ScoringLimits

#endif
