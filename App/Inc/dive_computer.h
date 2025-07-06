
#ifndef DIVE_COMPUTER_H
#define DIVE_COMPUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "zhl16_core.h"
#include "hardware_hal.h"
#include "dive_manager.h"
#include "ccr_manager.h"

// Version du firmware
#define FIRMWARE_VERSION "1.0.0"
#define HARDWARE_VERSION "STM32-DC-v1"

// Modes de fonctionnement
typedef enum {
    MODE_SURFACE,
    MODE_DIVE,
    MODE_GAUGE,
    MODE_APNEA,
    MODE_CCR,
    MODE_SCR,
    MODE_BAILOUT
} DiveMode;

// Structure principale de l'ordinateur
typedef struct {
    DiveMode mode;
    DiveMode previous_mode;
    ZHL16Model zhl16;
    DiveManager dive;
    CCRManager ccr;
    HardwareStatus hw;
    SystemConfig config;
    bool in_dive;
    bool emergency_mode;
} DiveComputer;

// Fonctions principales
void DiveComputer_Init(DiveComputer* dc);
void DiveComputer_Update(DiveComputer* dc);
void DiveComputer_1HzTasks(DiveComputer* dc);
void DiveComputer_10HzTasks(DiveComputer* dc);
void DiveComputer_HandleButton(DiveComputer* dc, ButtonEvent event);
void DiveComputer_SwitchMode(DiveComputer* dc, DiveMode new_mode);

#endif