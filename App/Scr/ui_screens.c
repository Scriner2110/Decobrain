#include "ui_screens.h"
#include <stdio.h>

// État global UI
static struct {
    ScreenType current_screen;
    Screen screens[16];
    bool needs_full_redraw;
    uint32_t last_alarm_time;
    char alarm_message[64];
} ui_state;

void UI_Init(void) {
    memset(&ui_state, 0, sizeof(ui_state));
    ui_state.current_screen = SCREEN_MAIN_DIVE;
    ui_state.needs_full_redraw = true;
    
    // Configuration des écrans
    ui_state.screens[SCREEN_MAIN_DIVE].draw = UI_DrawMainDiveScreen;
    ui_state.screens[SCREEN_CCR_MONITOR].draw = UI_DrawCCRMonitorScreen;
    ui_state.screens[SCREEN_DECO_INFO].draw = UI_DrawDecoInfoScreen;
    // ... autres écrans
}

void UI_Update(DiveComputer* dc) {
    Screen* screen = &ui_state.screens[ui_state.current_screen];
    
    if (ui_state.needs_full_redraw || screen->needs_redraw) {
        HAL_DisplayClear();
        if (screen->draw) {
            screen->draw(dc);
        }
        HAL_DisplayUpdate();
        screen->needs_redraw = false;
        ui_state.needs_full_redraw = false;
    } else if (screen->update) {
        screen->update(dc);
        HAL_DisplayUpdate();
    }
}

void UI_DrawMainDiveScreen(DiveComputer* dc) {
    char buffer[32];
    
    // Profondeur
    UI_DrawDepth(10, 20, dc->zhl16.current_depth, dc->config.metric_units);
    
    // Temps de plongée
    UI_DrawTime(200, 20, dc->dive.current_dive.duration);
    
    // NDL ou Déco
    if (dc->zhl16.ceiling > 0) {
        UI_DrawDeco(10, 80, dc->zhl16.ceiling, dc->zhl16.ascend_plan.tts);
    } else {
        UI_DrawNDL(10, 80, dc->zhl16.ndl);
    }
    
    // Gaz actuel
    UI_DrawGas(200, 80, &dc->zhl16.gases[dc->zhl16.current_gas]);
    
    // Mode CCR : affichage ppO2
    if (dc->mode == MODE_CCR || dc->mode == MODE_SCR) {
        UI_DrawPPO2(10, 140, dc->ccr.voted_ppO2, 
                    dc->ccr.alarm_ppO2_high || dc->ccr.alarm_ppO2_low);
        
        // Setpoint
        sprintf(buffer, "SP:%.2f", dc->ccr.current_setpoint);
        UI_DrawText(100, 140, buffer, COLOR_CYAN, 1);
    }
    
    // Vitesse de remontée
    UI_DrawAscendRate(280, 100, dc->dive.ascent_rate);
    
    // CNS et batterie
    UI_DrawCNS(10, 200, dc->zhl16.cns);
    UI_DrawBattery(260, 200, dc->hw.battery_percent);
    
    // Température
    sprintf(buffer, "%.1f°C", dc->hw.temperature_c);
    UI_DrawText(150, 200, buffer, COLOR_WHITE, 1);
    
    // Alarmes visuelles
    if (dc->dive.ascent_rate_alarm) {
        UI_ShowAlarm("SLOW DOWN!", 2);
    } else if (dc->dive.deco_ceiling_alarm) {
        UI_ShowAlarm("DECO VIOLATION!", 3);
    }
}

void UI_DrawCCRMonitorScreen(DiveComputer* dc) {
    char buffer[32];
    
    // Titre
    UI_DrawText(100, 10, "CCR MONITOR", COLOR_CYAN, 2);
    
    // Cellules O2
    for (int i = 0; i < 3; i++) {
        O2Cell* cell = &dc->ccr.cells[i];
        uint16_t color = COLOR_GREEN;
        
        if (cell->status == CELL_FAIL) color = COLOR_RED;
        else if (cell->status == CELL_DRIFT) color = COLOR_YELLOW;
        else if (!cell->is_voting) color = COLOR_GRAY;
        
        sprintf(buffer, "Cell %d: %.2f bar", i+1, cell->ppO2);
        UI_DrawText(20, 50 + i*30, buffer, color, 1);
        
        sprintf(buffer, "%.1f mV", cell->mv);
        UI_DrawText(200, 50 + i*30, buffer, color, 1);
    }
    
    // ppO2 votée
    sprintf(buffer, "Voted ppO2: %.2f", dc->ccr.voted_ppO2);
    UI_DrawText(20, 160, buffer, COLOR_WHITE, 2);
    
    // Setpoint
    sprintf(buffer, "Setpoint: %.2f", dc->ccr.current_setpoint);
    UI_DrawText(20, 190, buffer, COLOR_CYAN, 1);
    
    // Mode
    const char* mode_str = "Unknown";
    switch (dc->ccr.mode) {
        case CCR_MODE_FIXED_SETPOINT: mode_str = "Fixed SP"; break;
        case CCR_MODE_AUTO_SETPOINT: mode_str = "Auto SP"; break;
        case SCR_MODE_PASSIVE: mode_str = "SCR"; break;
    }
    sprintf(buffer, "Mode: %s", mode_str);
    UI_DrawText(200, 190, buffer, COLOR_WHITE, 1);
    
    // Alarmes
    if (dc->ccr.alarm_ppO2_high) {
        UI_DrawText(20, 220, "HIGH PPO2!", COLOR_RED, 1);
    } else if (dc->ccr.alarm_ppO2_low) {
        UI_DrawText(20, 220, "LOW PPO2!", COLOR_RED, 1);
    }
}

void UI_DrawDecoInfoScreen(DiveComputer* dc) {
    char buffer[32];
    
    // Titre
    UI_DrawText(100, 10, "DECO INFO", COLOR_YELLOW, 2);
    
    // Plan de remontée
    AscendPlan* plan = &dc->zhl16.ascend_plan;
    
    if (plan->num_stops == 0) {
        UI_DrawText(80, 100, "NO DECO REQUIRED", COLOR_GREEN, 2);
        sprintf(buffer, "Direct ascent: %d min", plan->tts);
        UI_DrawText(60, 130, buffer, COLOR_WHITE, 1);
    } else {
        // Affichage des paliers
        UI_DrawText(20, 40, "Depth  Time  Gas", COLOR_GRAY, 1);
        
        for (int i = 0; i < plan->num_stops && i < 6; i++) {
            DecoStop* stop = &plan->stops[i];
            GasMix* gas = &dc->zhl16.gases[stop->gas_idx];
            
            sprintf(buffer, "%3.0fm  %3d'  %s", 
                    stop->depth, stop->time/60, gas->name);
            UI_DrawText(20, 60 + i*20, buffer, COLOR_WHITE, 1);
        }
        
        // TTS
        sprintf(buffer, "TTS: %d min", plan->tts);
        UI_DrawText(20, 200, buffer, COLOR_YELLOW, 2);
    }
    
    // GF actuel
    sprintf(buffer, "GF: %.0f%%", dc->zhl16.gf_current);
    UI_DrawText(200, 200, buffer, COLOR_CYAN, 1);
}

// Éléments d'interface
void UI_DrawDepth(uint16_t x, uint16_t y, float depth, bool metric) {
    char buffer[16];
    
    if (metric) {
        sprintf(buffer, "%.1f", depth);
        UI_DrawText(x, y, buffer, COLOR_WHITE, 3);
        UI_DrawText(x + 80, y + 20, "m", COLOR_GRAY, 1);
    } else {
        sprintf(buffer, "%.0f", depth * 3.28084);
        UI_DrawText(x, y, buffer, COLOR_WHITE, 3);
        UI_DrawText(x + 80, y + 20, "ft", COLOR_GRAY, 1);
    }
}

void UI_DrawTime(uint16_t x, uint16_t y, uint32_t seconds) {
    char buffer[16];
    uint32_t minutes = seconds / 60;
    
    if (minutes < 100) {
        sprintf(buffer, "%d:%02d", minutes, seconds % 60);
    } else {
        sprintf(buffer, "%d'", minutes);
    }
    
    UI_DrawText(x, y, buffer, COLOR_WHITE, 2);
}

void UI_DrawPPO2(uint16_t x, uint16_t y, float ppO2, bool alarm) {
    char buffer[16];
    uint16_t color = COLOR_GREEN;
    
    if (ppO2 < 0.18) color = COLOR_RED;
    else if (ppO2 < 0.4) color = COLOR_YELLOW;
    else if (ppO2 > 1.4) color = COLOR_RED;
    else if (ppO2 > 1.2) color = COLOR_YELLOW;
    
    if (alarm) {
        // Clignotement
        if ((HAL_GetSysTick() / 500) % 2) {
            color = COLOR_RED;
        }
    }
    
    sprintf(buffer, "%.2f", ppO2);
    UI_DrawText(x, y, buffer, color, 2);
    UI_DrawText(x + 50, y + 10, "ppO2", COLOR_GRAY, 1);
}

void UI_DrawAscendRate(uint16_t x, uint16_t y, float rate) {
    // Barre graphique verticale
    int16_t bar_height = (int16_t)(rate * 2); // 2 pixels par m/min
    uint16_t color = COLOR_GREEN;
    
    if (fabsf(rate) > 18.0) color = COLOR_RED;
    else if (fabsf(rate) > 10.0) color = COLOR_YELLOW;
    
    // Fond de l'échelle
    UI_DrawRect(x, y - 40, 20, 80, COLOR_DARK_GRAY);
    
    // Barre de vitesse
    if (bar_height > 0) {
        // Remontée
        UI_DrawRect(x + 2, y - bar_height, 16, bar_height, color);
    } else {
        // Descente
        UI_DrawRect(x + 2, y, 16, -bar_height, color);
    }
    
    // Ligne de référence
    UI_DrawLine(x - 5, y, x + 25, y, COLOR_WHITE);
}

// Utilitaires d'affichage
void UI_DrawText(uint16_t x, uint16_t y, const char* text, uint16_t color, uint8_t size) {
    // Implémentation dépendante de la bibliothèque graphique utilisée
    // Exemple avec une police bitmap simple
    HAL_DisplayDrawText(x, y, text, color);
}

void UI_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    for (uint16_t i = 0; i < h; i++) {
        HAL_DisplayDrawLine(x, y + i, x + w - 1, y + i, color);
    }
}

void UI_ShowAlarm(const char* message, uint8_t severity) {
    uint32_t now = HAL_GetSysTick();
    
    // Copier le message
    strncpy(ui_state.alarm_message, message, sizeof(ui_state.alarm_message) - 1);
    ui_state.last_alarm_time = now;
    
    // Flash écran selon sévérité
    if (severity >= 3) {
        UI_FlashScreen(COLOR_RED);
        HAL_BuzzerAlarm(3); // Pattern alarme critique
    } else if (severity >= 2) {
        UI_FlashScreen(COLOR_YELLOW);
        HAL_BuzzerAlarm(2);
    } else {
        HAL_BuzzerBeep(1000, 100);
    }
    
    // Affichage du message
    uint16_t bg_color = (severity >= 3) ? COLOR_RED : COLOR_YELLOW;
    UI_DrawRect(40, 100, 240, 40, bg_color);
    UI_DrawText(50, 110, ui_state.alarm_message, COLOR_WHITE, 2);
}