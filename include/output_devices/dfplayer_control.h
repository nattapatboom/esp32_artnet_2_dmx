#ifndef OUTPUT_DEVICES_DFPLAYER_CONTROL_H
#define OUTPUT_DEVICES_DFPLAYER_CONTROL_H

#include <Arduino.h>

#define DF_CMD_NEXT       0x01
#define DF_CMD_PREV       0x02
#define DF_CMD_TRACK      0x03
#define DF_CMD_VOL_UP     0x04
#define DF_CMD_VOL_DOWN   0x05
#define DF_CMD_VOL        0x06
#define DF_CMD_EQ         0x07
#define DF_CMD_PLAYBACK   0x08
#define DF_CMD_PLAY_SRC   0x09
#define DF_CMD_STANDBY    0x0A
#define DF_CMD_NORMAL     0x0B
#define DF_CMD_RESET      0x0C
#define DF_CMD_PLAY       0x0D
#define DF_CMD_PAUSE      0x0E
#define DF_CMD_FOLDER     0x0F
#define DF_CMD_LOOP_ALL   0x11
#define DF_CMD_PLAY_MP3   0x12
#define DF_CMD_ADVERT     0x13
#define DF_CMD_STOP_ADVERT 0x15
#define DF_CMD_STOP       0x16
#define DF_CMD_LOOP_FOLDER 0x17
#define DF_CMD_RANDOM     0x18
#define DF_CMD_LOOP_SINGLE 0x19
#define DF_CMD_DAC        0x1A

#define DF_ACK_LEN 10
#define DF_CMD_DELAY 50

class DFPlayerController {
private:
    HardwareSerial* serial = nullptr;
    bool initialized = false;
    unsigned long lastCmdTime = 0;
    uint8_t lastTrack = 0;
    uint16_t lastVolume = 65535;

    void sendCmd(uint8_t cmd, uint16_t param) {
        if (!serial) return;
        uint8_t buf[DF_ACK_LEN] = {
            0x7E, 0xFF, 0x06, cmd,
            (uint8_t)(param >> 8), (uint8_t)(param & 0xFF),
            0x00, 0x00, 0x00, 0xEF
        };
        uint16_t sum = 0;
        for (int i = 1; i < 7; i++) sum += buf[i];
        buf[7] = (~(sum >> 8)) & 0xFF;
        buf[8] = (~sum) & 0xFF;
        serial->write(buf, DF_ACK_LEN);
        serial->flush();
        lastCmdTime = millis();
    }

public:
    DFPlayerController() {}

    void begin(HardwareSerial& s, uint8_t txPin, uint8_t rxPin) {
        serial = &s;
        serial->begin(9600, SERIAL_8N1, rxPin, txPin);
        delay(600);
        sendCmd(DF_CMD_RESET, 0);
        delay(200);
        sendCmd(DF_CMD_PLAY_SRC, 1);
        delay(100);
        sendCmd(DF_CMD_VOL, 15);
        delay(100);
        initialized = true;
        lastVolume = 15;
    }

    bool isInitialized() const { return initialized; }

    void playTrack(uint8_t track) {
        if (!initialized) return;
        if (track == 0) { stop(); return; }
        if (track == lastTrack) return;
        if (millis() - lastCmdTime < DF_CMD_DELAY) return;
        sendCmd(DF_CMD_PLAY_MP3, track);
        lastTrack = track;
    }

    void setVolume(uint8_t vol) {
        if (!initialized) return;
        uint16_t dfVol = map(constrain(vol, 0, 255), 0, 255, 0, 30);
        if (dfVol == lastVolume) return;
        if (millis() - lastCmdTime < DF_CMD_DELAY) return;
        sendCmd(DF_CMD_VOL, (uint16_t)dfVol);
        lastVolume = dfVol;
    }

    void stop() {
        if (!initialized) return;
        if (lastTrack == 0) return;
        if (millis() - lastCmdTime < DF_CMD_DELAY) return;
        sendCmd(DF_CMD_STOP, 0);
        lastTrack = 0;
    }

    void pause() {
        if (!initialized) return;
        if (millis() - lastCmdTime < DF_CMD_DELAY) return;
        sendCmd(DF_CMD_PAUSE, 0);
    }

    void resume() {
        if (!initialized) return;
        if (millis() - lastCmdTime < DF_CMD_DELAY) return;
        sendCmd(DF_CMD_PLAY, 0);
    }

    void next() {
        if (!initialized) return;
        if (millis() - lastCmdTime < DF_CMD_DELAY) return;
        sendCmd(DF_CMD_NEXT, 0);
    }

    void previous() {
        if (!initialized) return;
        if (millis() - lastCmdTime < DF_CMD_DELAY) return;
        sendCmd(DF_CMD_PREV, 0);
    }

    void loopSingle() {
        if (!initialized) return;
        if (millis() - lastCmdTime < DF_CMD_DELAY) return;
        sendCmd(DF_CMD_LOOP_SINGLE, 0);
    }

    void loopAll() {
        if (!initialized) return;
        if (millis() - lastCmdTime < DF_CMD_DELAY) return;
        sendCmd(DF_CMD_LOOP_ALL, 0);
    }

    // Returns true if a command was sent
    bool update(uint8_t track, uint8_t volume, uint8_t control) {
        if (!initialized) return false;

        // Control commands (priority)
        if (control == 1) { next(); return true; }
        if (control == 2) { previous(); return true; }
        if (control == 3) { pause(); return true; }
        if (control == 4) { resume(); return true; }
        if (control == 5) { loopSingle(); return true; }
        if (control == 6) { loopAll(); return true; }

        setVolume(volume);

        if (track != lastTrack) {
            playTrack(track);
        }

        return false;
    }

    uint8_t getLastTrack() const { return lastTrack; }
    uint16_t getLastVolume() const { return lastVolume; }
};

#endif
