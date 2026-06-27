#ifndef OUTPUT_COMMON_H
#define OUTPUT_COMMON_H

#define SENTINEL_NONE 255
#define DMX_BUFFER_SIZE 512

inline uint8_t getValueByteCount(uint8_t resolution) {
    if (resolution <= 8) return 1;
    if (resolution <= 16) return 2;
    if (resolution <= 24) return 3;
    return 4;
}

inline uint32_t getMaxValue(uint8_t resolution) {
    if (resolution >= 32) return 0xFFFFFFFFUL;
    return ((uint32_t)1 << resolution) - 1;
}

#endif
