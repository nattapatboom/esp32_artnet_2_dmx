#ifndef SCORING_DEFS_H
#define SCORING_DEFS_H

#include <Arduino.h>

namespace ScoringDefs {
struct HardwareLimits {
    uint8_t ledc;
    uint8_t rmt;
    uint8_t uart;
    uint8_t dac;
    uint8_t timer;
};

struct CpuRuntimeDef {
    uint32_t baseOverheadUs;
    uint32_t safetyReserveUs;
    uint16_t i2cWriteUs;
    uint16_t dmxOutputServiceUs;
};

struct RamRuntimeDef {
    uint32_t keepFree;
    uint32_t limitCap;
    uint32_t baseChannel;
    uint32_t dmxBuffer;
    uint32_t pixelStripObject;
    uint32_t dfPlayerObject;
    uint32_t stepperRuntime;
    uint32_t funcGenObject;
    uint32_t i2cRoute;
};

constexpr HardwareLimits HARDWARE_LIMITS = {
    16, // LEDC
    8,  // RMT
    2,  // UART usable by outputs (UART0 is console)
    2,  // internal DAC count; WT32-ETH01 blocks GPIO25/26
    4   // timer/runtime slots
};

constexpr CpuRuntimeDef CPU_RUNTIME = {
    500,  // output loop base overhead per frame
    1500, // safety reserve per frame
    180,  // active I2C route write
    250   // DMX UART/RMT enqueue service time
};

constexpr RamRuntimeDef RAM_RUNTIME = {
    150 * 1024, // keep free for network/system
    65535,      // output allocation scoring cap
    224,        // OutputChannel vector slot + allocator/header slack
    512,        // DMX universe buffer
    256,        // NeoPixel wrapper/object overhead
    160,        // DFPlayer object overhead
    512,        // Stepper runtime object/slack
    1120,       // Function generator waveform tables + esp_timer slack
    32          // I2C route bookkeeping estimate
};

constexpr uint16_t CHANNEL_CPU_US[19] = {
    5,   // 0 AC Dimmer
    250, // 1 DMX Output
    5,   // 2 Relay
    80,  // 3 RGB LED base; led count is added dynamically
    6,   // 4 Single LED
    18,  // 5 Analog RGB/RGBW
    35,  // 6 DC Motor
    80,  // 7 Stepper
    12,  // 8 Servo
    35,  // 9 Buzzer
    30,  // 10 DFPlayer
    900, // 11 TM1637
    30,  // 12 7-seg 7-pin DD
    35,  // 13 7-seg 8-pin DD
    10,  // 14 DAC
    6,   // 15 PWM DAC
    120, // 16 Function Generator
    10,  // 17 Solenoid
    25   // 18 Smoke Shooter
};

constexpr uint8_t MAX_LEDC_RESOURCE = HARDWARE_LIMITS.ledc;
constexpr uint8_t MAX_RMT_RESOURCE = HARDWARE_LIMITS.rmt;
constexpr uint8_t MAX_UART_RESOURCE = HARDWARE_LIMITS.uart;
constexpr uint8_t MAX_DAC_RESOURCE = HARDWARE_LIMITS.dac;
constexpr uint8_t MAX_TIMER_RESOURCE = HARDWARE_LIMITS.timer;

constexpr uint32_t CPU_BASE_OVERHEAD_US = CPU_RUNTIME.baseOverheadUs;
constexpr uint32_t CPU_SAFETY_RESERVE_US = CPU_RUNTIME.safetyReserveUs;
constexpr uint32_t RAM_KEEP_FREE = RAM_RUNTIME.keepFree;
constexpr uint32_t RAM_LIMIT_CAP = RAM_RUNTIME.limitCap;

constexpr uint16_t I2C_WRITE_US = CPU_RUNTIME.i2cWriteUs;
constexpr uint32_t BASE_CHANNEL_RAM = RAM_RUNTIME.baseChannel;
constexpr uint32_t DMX_BUFFER_RAM = RAM_RUNTIME.dmxBuffer;
constexpr uint32_t PIXEL_STRIP_OBJECT_RAM = RAM_RUNTIME.pixelStripObject;
constexpr uint32_t DFPLAYER_OBJECT_RAM = RAM_RUNTIME.dfPlayerObject;
constexpr uint32_t STEPPER_RUNTIME_RAM = RAM_RUNTIME.stepperRuntime;
constexpr uint32_t FUNCGEN_OBJECT_RAM = RAM_RUNTIME.funcGenObject;
constexpr uint32_t I2C_ROUTE_RAM = RAM_RUNTIME.i2cRoute;
constexpr uint32_t DMX_OUTPUT_SERVICE_US = CPU_RUNTIME.dmxOutputServiceUs;
}

#endif
