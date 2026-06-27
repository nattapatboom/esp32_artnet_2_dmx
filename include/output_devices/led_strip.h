#ifndef OUTPUT_DEVICES_LED_STRIP_H
#define OUTPUT_DEVICES_LED_STRIP_H

#include <Arduino.h>
#include <NeoPixelBus.h>
#include <new>
#include "output_control.h"

template <typename T_Method>
class PixelStripRmt : public PixelStripWrapper {
private:
    NeoPixelBus<NeoRgbFeature, T_Method>* strip = nullptr;
public:
    PixelStripRmt(uint16_t count, uint8_t pin) {
        strip = new (std::nothrow) NeoPixelBus<NeoRgbFeature, T_Method>(count, pin);
    }
    ~PixelStripRmt() override { if (strip) delete strip; }
    void Begin() override { if (strip) strip->Begin(); }
    bool CanShow() const override { return strip && strip->CanShow(); }
    void Show() override { if (strip) strip->Show(); }
    void SetPixelColor(uint16_t index, RgbColor color) override {
        if (strip) strip->SetPixelColor(index, color);
    }
    bool IsRgbw() const override { return false; }
};

template <typename T_Method>
class PixelStripRmtRgbw : public PixelStripWrapper {
private:
    NeoPixelBus<NeoRgbwFeature, T_Method>* strip = nullptr;
public:
    PixelStripRmtRgbw(uint16_t count, uint8_t pin) {
        strip = new (std::nothrow) NeoPixelBus<NeoRgbwFeature, T_Method>(count, pin);
    }
    ~PixelStripRmtRgbw() override { if (strip) delete strip; }
    void Begin() override { if (strip) strip->Begin(); }
    bool CanShow() const override { return strip && strip->CanShow(); }
    void Show() override { if (strip) strip->Show(); }
    void SetPixelColor(uint16_t index, RgbColor color) override {
        if (strip) strip->SetPixelColor(index, RgbwColor(color.R, color.G, color.B, 0));
    }
    void SetPixelColorRgbw(uint16_t index, RgbwColor color) override {
        if (strip) strip->SetPixelColor(index, color);
    }
    bool IsRgbw() const override { return true; }
};

inline void ledStripSetup(OutputChannel& ch, uint8_t& rmtIdx) {
    if (ch.pin == 255) return;
    if (ch.pixelStrip != nullptr) {
        delete ch.pixelStrip;
        ch.pixelStrip = nullptr;
    }
    if (rmtIdx < 8) {
        auto createStrip = [&]() -> PixelStripWrapper* {
            if (ch.color_order >= 4) {
                switch (rmtIdx) {
                    case 0: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt0Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt0Ws2812xMethod>(ch.led_count, ch.pin);
                    case 1: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt1Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt1Ws2812xMethod>(ch.led_count, ch.pin);
                    case 2: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt2Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt2Ws2812xMethod>(ch.led_count, ch.pin);
                    case 3: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt3Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt3Ws2812xMethod>(ch.led_count, ch.pin);
                    case 4: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt4Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt4Ws2812xMethod>(ch.led_count, ch.pin);
                    case 5: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt5Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt5Ws2812xMethod>(ch.led_count, ch.pin);
                    case 6: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt6Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt6Ws2812xMethod>(ch.led_count, ch.pin);
                    case 7: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt7Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmtRgbw<NeoEsp32Rmt7Ws2812xMethod>(ch.led_count, ch.pin);
                    default: return nullptr;
                }
            } else {
                switch (rmtIdx) {
                    case 0: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt0Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt0Ws2812xMethod>(ch.led_count, ch.pin);
                    case 1: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt1Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt1Ws2812xMethod>(ch.led_count, ch.pin);
                    case 2: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt2Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt2Ws2812xMethod>(ch.led_count, ch.pin);
                    case 3: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt3Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt3Ws2812xMethod>(ch.led_count, ch.pin);
                    case 4: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt4Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt4Ws2812xMethod>(ch.led_count, ch.pin);
                    case 5: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt5Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt5Ws2812xMethod>(ch.led_count, ch.pin);
                    case 6: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt6Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt6Ws2812xMethod>(ch.led_count, ch.pin);
                    case 7: return ch.led_protocol == 1 ? (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt7Ws2811Method>(ch.led_count, ch.pin) : (PixelStripWrapper*)new (std::nothrow) PixelStripRmt<NeoEsp32Rmt7Ws2812xMethod>(ch.led_count, ch.pin);
                    default: return nullptr;
                }
            }
        };
        ch.pixelStrip = createStrip();
        if (ch.pixelStrip != nullptr) {
            ch.pixelStrip->Begin();
            ch.pixelStrip->Show();
        }
        rmtIdx++;
    }
}

inline void ledStripUpdate() {
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    uint8_t fps = sysCfg.output_fps > 0 ? sysCfg.output_fps : 40;
    if (now - lastUpdate < (1000 / fps)) return;
    lastUpdate = now;

    for (auto& ch : outputCtrl.getChannels()) {
        if (ch.type != 3) continue;
        if (ch.pixelStrip == nullptr || ch.dmxBuffer == nullptr) continue;
        if (!ch.pixelStrip->CanShow()) continue;

        const uint8_t colorOrder = ch.color_order;
        const uint16_t ledCount = ch.led_count;
        const uint16_t bufSize = ch.bufferSize;
        const uint8_t* buf = ch.dmxBuffer;
        const bool isRgbw = ch.pixelStrip->IsRgbw();
        const uint8_t bytesPerPixel = isRgbw ? 4 : 3;
        const uint16_t pixelsPerUniverse = 512 / bytesPerPixel;

        uint16_t universe = 0;
        uint16_t posInUniverse = 0;
        uint16_t bufOffset = 0;

        for (uint16_t i = 0; i < ledCount; i++) {
            if (posInUniverse == pixelsPerUniverse) {
                universe++;
                posInUniverse = 0;
                bufOffset = universe * 512;
            }
            uint16_t idx = bufOffset + posInUniverse * bytesPerPixel;
            posInUniverse++;
            if (idx + bytesPerPixel > bufSize) continue;

            uint8_t r = buf[idx];
            uint8_t g = buf[idx + 1];
            uint8_t b = buf[idx + 2];
            uint8_t w = isRgbw ? buf[idx + 3] : 0;

            if (isRgbw) {
                RgbwColor color;
                switch (colorOrder) {
                    case COLOR_RGBW: color = RgbwColor(r, g, b, w); break;
                    case COLOR_GRBW: color = RgbwColor(g, r, b, w); break;
                    case COLOR_BRGW: color = RgbwColor(b, r, g, w); break;
                    case COLOR_WRGB: color = RgbwColor(w, r, g, b); break;
                    default:         color = RgbwColor(r, g, b, w); break;
                }
                ch.pixelStrip->SetPixelColorRgbw(i, color);
            } else {
                RgbColor color;
                switch (colorOrder) {
                    case COLOR_GRB: color = RgbColor(g, r, b); break;
                    case COLOR_BRG: color = RgbColor(b, r, g); break;
                    case COLOR_RBG: color = RgbColor(r, b, g); break;
                    case COLOR_RGB:
                    default:        color = RgbColor(r, g, b); break;
                }
                ch.pixelStrip->SetPixelColor(i, color);
            }
        }
        ch.pixelStrip->Show();
    }
}

#endif
