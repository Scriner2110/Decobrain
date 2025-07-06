#include "dive_computer.h"

// Instance globale
static DiveComputer g_dive_computer;

void DiveComputer_Init(DiveComputer* dc) {
    // Initialisation matérielle
    HAL_InitHardware();
    
    // Chargement configuration
    // TODO: Charger depuis Flash
    
    // Initialisation des modules
    float surface_pressure;
    HAL_ReadPressureTemp(&surface_pressure, &dc->hw.temperature_c);
    surface_pressure /= 1000.0; // mbar vers bar
    
    ZHL16_Init(&dc->zhl16, surface_pressure, false); // ZHL-16B par défaut
    DiveManager_Init(&dc->dive);
    CCR_Init(&dc->ccr);
    UI_Init();
    
    // Configuration des gaz par défaut
    ZHL16_AddGas(&dc->zhl16, 0, "Air", 0.21, 0.79, 0.0, false);
    ZHL16_AddGas(&dc->zhl16, 1, "EAN32", 0.32, 0.68, 0.0, false);
    ZHL16_AddGas(&dc->zhl16, 2, "EAN50", 0.50, 0.50, 0.0, false);
    ZHL16_AddGas(&dc->zhl16, 3, "Oxygen", 1.00, 0.00, 0.0, false);
    
    dc->mode = MODE_SURFACE;
    dc->in_dive = false;
    dc->emergency_mode = false;
}

void DiveComputer_Update(DiveComputer* dc) {
    // Lecture des capteurs
    float pressure_mbar, temperature_c;
    if (!HAL_ReadPressureTemp(&pressure_mbar, &temperature_c)) {
        dc->emergency_mode = true;
        return;
    }
    
    dc->hw.pressure_mbar = pressure_mbar;
    dc->hw.temperature_c = temperature_c;
    
    // Calcul profondeur
    float depth = (pressure_mbar - dc->zhl16.surface_pressure * 1000) / 100.0;
    if (depth < 0) depth = 0;
    
    // Mode CCR : lecture cellules
    if (dc->mode == MODE_CCR || dc->mode == MODE_SCR) {
        HAL_ReadO2Cells(&dc->hw.cell_mv[0], &dc->hw.cell_mv[1], &dc->hw.cell_mv[2]);
        CCR_UpdateCellReadings(&dc->ccr, dc->hw.cell_mv[0], dc->hw.cell_mv[1], dc->hw.cell_mv[2]);
        CCR_Update(&dc->ccr, dc->zhl16.ambient_pressure, temperature_c);
        
        // Mise à jour ppO2 pour décompression
        if (dc->mode == MODE_CCR) {
            ZHL16_UpdateCCRppO2(&dc->zhl16, dc->ccr.voted_ppO2);
        } else {
            // Mode SCR
            float inspired_ppO2 = ZHL16_GetPartialPressure(
                dc->zhl16.ambient_pressure, 
                dc->zhl16.gases[dc->zhl16.current_gas].fO2
            );
            float scr_ppO2 = CCR_CalculateSCRppO2(&dc->ccr, inspired_ppO2);
            ZHL16_UpdateCCRppO2(&dc->zhl16, scr_ppO2);
        }
    }
    
    // Mise à jour modèle décompression
    ZHL16_UpdateDepth(&dc->zhl16, depth);
    
    // Mise à jour gestionnaire de plongée
    DiveManager_Update(&dc->dive, depth, temperature_c, &dc->zhl16);
    
    // Mise à jour interface
    UI_Update(dc);
    
    // Batterie
    dc->hw.battery_voltage = HAL_GetBatteryVoltage();
    dc->hw.battery_percent = HAL_GetBatteryPercent();
    
    // Watchdog
    HAL_WatchdogFeed();
}

void DiveComputer_1HzTasks(DiveComputer* dc) {
    // Tâches exécutées chaque seconde
    static uint32_t last_second = 0;
    uint32_t now = HAL_GetSysTick() / 1000;
    
    if (now == last_second) return;
    last_second = now;
    
    // Mise à jour des tissus (1 seconde)
    if (dc->dive.is_diving) {
        ZHL16_UpdateTissues(&dc->zhl16, 1.0);
        ZHL16_UpdateCNS(&dc->zhl16, 1.0);
        
        // Calcul du plafond
        ZHL16_GetCeiling(&dc->zhl16);
        
        // Calcul NDL ou plan de remontée
        if (dc->zhl16.ceiling > 0) {
            ZHL16_CalculateAscendPlan(&dc->zhl16);
        } else {
            ZHL16_GetNDL(&dc->zhl16);
        }
    }
    
    // Auto setpoint CCR
    if (dc->mode == MODE_CCR && dc->ccr.mode == CCR_MODE_AUTO_SETPOINT) {
        CCR_UpdateAutoSetpoint(&dc->ccr, dc->zhl16.current_depth);
    }
    
    // Vérification alarmes
    CCR_CheckAlarms(&dc->ccr);
}

void DiveComputer_10HzTasks(DiveComputer* dc) {
    // Tâches rapides (100ms)
    
    // Lecture boutons
    ButtonEvent button = HAL_GetButtonEvent();
    if (button != BUTTON_NONE) {
        DiveComputer_HandleButton(dc, button);
    }
}

void DiveComputer_HandleButton(DiveComputer* dc, ButtonEvent event) {
    switch (event) {
        case BUTTON_MENU:
            if (dc->mode == MODE_SURFACE) {
                UI_SwitchScreen(SCREEN_MENU_MAIN);
            }
            break;
            
        case BUTTON_UP:
            // Navigation ou changement de gaz
            if (dc->dive.is_diving) {
                uint8_t next_gas = (dc->zhl16.current_gas + 1) % dc->zhl16.num_gases;
                ZHL16_SwitchGas(&dc->zhl16, next_gas);
            }
            break;
            
        case BUTTON_DOWN:
            // Navigation
            break;
            
        case BUTTON_ENTER:
            // Validation
            break;
            
        case BUTTON_MENU_LONG:
            // Changement de mode
            if (dc->mode == MODE_CCR) {
                DiveComputer_SwitchMode(dc, MODE_BAILOUT);
            }
            break;
            
        case BUTTON_ENTER_LONG:
            // Reset alarme ou marqueur
            if (ui_state.alarm_message[0]) {
                UI_ClearAlarm();
            }
            break;
    }
}

void DiveComputer_SwitchMode(DiveComputer* dc, DiveMode new_mode) {
    dc->previous_mode = dc->mode;
    dc->mode = new_mode;
    
    switch (new_mode) {
        case MODE_CCR:
            ZHL16_SetCCRMode(&dc->zhl16, true, dc->ccr.current_setpoint);
            break;
            
        case MODE_BAILOUT:
            CCR_SwitchToBailout(&dc->ccr, 0); // Premier gaz bailout
            ZHL16_SwitchToBailout(&dc->zhl16);
            UI_ShowAlarm("BAILOUT!", 2);
            break;
            
        case MODE_GAUGE:
            // Mode profondimètre simple
            break;
            
        default:
            break;
    }
    
    UI_ForceRedraw();
}

// ============================================================================
// POINT D'ENTRÉE PRINCIPAL
// ============================================================================
int main(void) {
    // Initialisation HAL STM32
    HAL_Init();
    
    // Initialisation ordinateur de plongée
    DiveComputer_Init(&g_dive_computer);
    
    // Configuration des timers
    // Timer 1ms pour tâches système
    // Timer 100ms pour mise à jour principale
    // Timer 1s pour calculs décompression
    
    uint32_t last_update = 0;
    uint32_t last_100ms = 0;
    uint32_t last_1s = 0;
    
    // Boucle principale
    while (1) {
        uint32_t now = HAL_GetSysTick();
        
        // Mise à jour principale (50Hz)
        if (now - last_update >= 20) {
            last_update = now;
            DiveComputer_Update(&g_dive_computer);
        }
        
        // Tâches 10Hz
        if (now - last_100ms >= 100) {
            last_100ms = now;
            DiveComputer_10HzTasks(&g_dive_computer);
        }
        
        // Tâches 1Hz
        if (now - last_1s >= 1000) {
            last_1s = now;
            DiveComputer_1HzTasks(&g_dive_computer);
        }
        
        // Mode économie d'énergie en surface
        if (!g_dive_computer.dive.is_diving && 
            g_dive_computer.mode == MODE_SURFACE) {
            __WFI(); // Wait For Interrupt
        }
    }
}