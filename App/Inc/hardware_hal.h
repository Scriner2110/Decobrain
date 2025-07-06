#ifndef HARDWARE_HAL_H
#define HARDWARE_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

// Configuration matérielle
#define PRESSURE_SENSOR_I2C_ADDR 0x76
#define EXTERNAL_ADC_I2C_ADDR 0x48
#define DISPLAY_SPI_CS_PIN GPIO_PIN_4
#define BUZZER_PWM_CHANNEL TIM_CHANNEL_1

// Types de boutons
typedef enum {
    BUTTON_NONE = 0,
    BUTTON_MENU = 1,
    BUTTON_UP = 2,
    BUTTON_DOWN = 4,
    BUTTON_ENTER = 8,
    BUTTON_MENU_LONG = 16,
    BUTTON_ENTER_LONG = 32
} ButtonEvent;

// État du matériel
typedef struct {
    // Capteurs
    float pressure_mbar;
    float temperature_c;
    float battery_voltage;
    uint8_t battery_percent;
    
    // État des périphériques
    bool pressure_sensor_ok;
    bool display_ok;
    bool flash_ok;
    bool rtc_ok;
    
    // Mesures CCR
    float cell_mv[3];
    bool cells_connected;
    
    // Système
    uint32_t uptime_seconds;
    uint32_t last_error_code;
} HardwareStatus;

// Configuration système
typedef struct {
    // Unités
    bool metric_units;          // true = métrique, false = impérial
    bool celsius;               // true = Celsius, false = Fahrenheit
    
    // Affichage
    uint8_t brightness;         // 0-100%
    uint8_t contrast;
    bool auto_dim;
    uint16_t backlight_timeout;
    
    // Alarmes
    bool audible_alarms;
    uint8_t alarm_volume;       // 0-100%
    bool vibration_alerts;
    
    // Échantillonnage
    uint8_t log_rate;           // Secondes entre échantillons
    bool high_resolution_log;
    
    // Étalonnage
    float pressure_offset;
    float temperature_offset;
    float cell_calibration[3];
} SystemConfig;

// Initialisation matérielle
void HAL_InitHardware(void);
void HAL_InitPressureSensor(void);
void HAL_InitDisplay(void);
void HAL_InitButtons(void);
void HAL_InitPower(void);
void HAL_InitStorage(void);
void HAL_InitRTC(void);
void HAL_InitADC(void);

// Capteur de pression MS5837
bool HAL_ReadPressureTemp(float* pressure_mbar, float* temperature_c);
bool HAL_CompensatePressure(float raw_pressure, float temperature);
void HAL_CalibratePressureSensor(float reference_pressure);

// Cellules O2 (ADC)
void HAL_ReadO2Cells(float* cell1_mv, float* cell2_mv, float* cell3_mv);
bool HAL_IsO2CellConnected(uint8_t cell_num);
void HAL_SetADCGain(uint8_t gain);

// Affichage
void HAL_DisplayInit(void);
void HAL_DisplayClear(void);
void HAL_DisplayUpdate(void);
void HAL_DisplaySetBrightness(uint8_t percent);
void HAL_DisplayDrawPixel(uint16_t x, uint16_t y, uint16_t color);
void HAL_DisplayDrawText(uint16_t x, uint16_t y, const char* text, uint16_t color);
void HAL_DisplayDrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void HAL_DisplayDrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

// Boutons
ButtonEvent HAL_GetButtonEvent(void);
void HAL_ResetButtonState(void);
bool HAL_IsButtonPressed(ButtonEvent button);

// Buzzer et vibration
void HAL_BuzzerBeep(uint16_t frequency, uint16_t duration_ms);
void HAL_BuzzerAlarm(uint8_t pattern);
void HAL_VibrationPulse(uint16_t duration_ms);

// Alimentation et batterie
float HAL_GetBatteryVoltage(void);
uint8_t HAL_GetBatteryPercent(void);
bool HAL_IsCharging(void);
void HAL_EnterSleepMode(void);
void HAL_EnterDeepSleepMode(void);

// Stockage Flash
bool HAL_FlashWrite(uint32_t address, uint8_t* data, uint32_t size);
bool HAL_FlashRead(uint32_t address, uint8_t* data, uint32_t size);
bool HAL_FlashEraseSector(uint32_t sector);
uint32_t HAL_FlashGetFreeSpace(void);

// RTC (Real Time Clock)
void HAL_RTCGetTime(uint8_t* hour, uint8_t* min, uint8_t* sec);
void HAL_RTCSetTime(uint8_t hour, uint8_t min, uint8_t sec);
void HAL_RTCGetDate(uint8_t* day, uint8_t* month, uint16_t* year);
void HAL_RTCSetDate(uint8_t day, uint8_t month, uint16_t year);
uint32_t HAL_RTCGetUnixTime(void);

// Watchdog
void HAL_WatchdogInit(uint16_t timeout_ms);
void HAL_WatchdogFeed(void);

// Utilitaires système
uint32_t HAL_GetSysTick(void);
void HAL_Delay(uint32_t ms);
void HAL_GetUID(uint8_t* uid);
float HAL_GetCPUTemperature(void);

#endif