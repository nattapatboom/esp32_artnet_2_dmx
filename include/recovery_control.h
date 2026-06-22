#ifndef RECOVERY_CONTROL_H
#define RECOVERY_CONTROL_H

#include <Arduino.h>
#include <ETH.h>
#include <WiFi.h>

#define RECOVERY_BUTTON_PIN 0
#define RECOVERY_BOOT_THRESHOLD 5

extern uint32_t bootCount;
extern bool isRecoveryMode;

bool checkRecoveryMode();
void startRecoveryMode();
void resetBootCountTask(void* pvParameters);

#endif
