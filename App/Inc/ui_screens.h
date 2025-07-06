#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include <stdint.h>
#include <stdbool.h>
#include "dive_computer.h"

// Types d'écrans
typedef enum {
    SCREEN_MAIN_DIVE,
    SCREEN_COMPASS,
    SCREEN_DECO_INFO,
    SCREEN_GAS_LIST,
    SCREEN_CCR_MONITOR,
    SCREEN_DIVE_PROFILE,
    SCREEN_TISSUE_GRAPH,
    SCREEN_MENU_MAIN,
    SCREEN_MENU_GAS,
    SCREEN_MENU_DECO,
    SCREEN_MENU_SYSTEM,
    SCREEN_LOGBOOK,
    SCREEN_INFO
} ScreenType;

// Couleurs (RGB565)
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_ORANGE    0xFD20
#define COLOR_GRAY      0x8410
#define COLOR_DARK_GRAY 0x4208

// Dimensions écran
#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   240

// Structure écran
typedef struct {
    ScreenType type;
    void (*draw)(DiveComputer* dc);
    void (*update)(DiveComputer* dc);
    void (*handle_button)(DiveComputer* dc, ButtonEvent event);
    uint32_t last_update;
    bool needs_redraw;
} Screen;

// Fonctions d'écran principales
void UI_Init(void);
void UI_Update(DiveComputer* dc);
void UI_HandleButton(DiveComputer* dc, ButtonEvent event);
void UI_SwitchScreen(ScreenType screen);
void UI_ForceRedraw(void);

// Écrans de plongée
void UI_DrawMainDiveScreen(DiveComputer* dc);
void UI_DrawCompassScreen(DiveComputer* dc);
void UI_DrawDecoInfoScreen(DiveComputer* dc);
void UI_DrawGasListScreen(DiveComputer* dc);
void UI_DrawCCRMonitorScreen(DiveComputer* dc);
void UI_DrawDiveProfileScreen(DiveComputer* dc);
void UI_DrawTissueGraphScreen(DiveComputer* dc);

// Écrans de menu
void UI_DrawMainMenu(DiveComputer* dc);
void UI_DrawGasMenu(DiveComputer* dc);
void UI_DrawDecoMenu(DiveComputer* dc);
void UI_DrawSystemMenu(DiveComputer* dc);
void UI_DrawLogbook(DiveComputer* dc);
void UI_DrawInfoScreen(DiveComputer* dc);

// Éléments d'interface
void UI_DrawDepth(uint16_t x, uint16_t y, float depth, bool metric);
void UI_DrawTime(uint16_t x, uint16_t y, uint32_t seconds);
void UI_DrawDeco(uint16_t x, uint16_t y, float ceiling, uint16_t tts);
void UI_DrawNDL(uint16_t x, uint16_t y, float ndl);
void UI_DrawGas(uint16_t x, uint16_t y, GasMix* gas);
void UI_DrawPPO2(uint16_t x, uint16_t y, float ppO2, bool alarm);
void UI_DrawBattery(uint16_t x, uint16_t y, uint8_t percent);
void UI_DrawCompass(uint16_t x, uint16_t y, uint16_t heading);
void UI_DrawAscendRate(uint16_t x, uint16_t y, float rate);
void UI_DrawCNS(uint16_t x, uint16_t y, float cns);
void UI_DrawGF(uint16_t x, uint16_t y, float gf);

// Graphiques
void UI_DrawTissueBar(uint16_t x, uint16_t y, uint8_t compartment, float loading);
void UI_DrawDecoProfile(uint16_t x, uint16_t y, AscendPlan* plan);
void UI_DrawDiveProfile(uint16_t x, uint16_t y, DiveProfile* profile);
void UI_DrawPressureGraph(uint16_t x, uint16_t y, float* data, uint16_t count);

// Alarmes visuelles
void UI_ShowAlarm(const char* message, uint8_t severity);
void UI_ClearAlarm(void);
void UI_FlashScreen(uint16_t color);

// Utilitaires
void UI_DrawText(uint16_t x, uint16_t y, const char* text, uint16_t color, uint8_t size);
void UI_DrawNumber(uint16_t x, uint16_t y, int32_t value, uint16_t color, uint8_t size);
void UI_DrawFloat(uint16_t x, uint16_t y, float value, uint8_t decimals, uint16_t color, uint8_t size);
void UI_DrawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t percent, uint16_t color);
void UI_DrawIcon(uint16_t x, uint16_t y, uint8_t icon_id);

#endif