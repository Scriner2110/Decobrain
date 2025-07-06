#include "hardware_hal.h"
#include "ms5837.h"
#include <string.h>

// Variables globales HAL
static I2C_HandleTypeDef hi2c1;
static SPI_HandleTypeDef hspi1;
static ADC_HandleTypeDef hadc1;
static TIM_HandleTypeDef htim1;
static TIM_HandleTypeDef htim2;
static RTC_HandleTypeDef hrtc;
static UART_HandleTypeDef huart1;

// États internes
static MS5837_Handle pressure_sensor;
static uint32_t button_state = 0;
static uint32_t button_time[5] = {0};
static float battery_voltage_filtered = 0;

// Initialisation matérielle globale
void HAL_InitHardware(void) {
    // Configuration des horloges
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.LSEState = RCC_LSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                 RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
    
    // Initialisation des périphériques
    HAL_InitPressureSensor();
    HAL_InitDisplay();
    HAL_InitButtons();
    HAL_InitPower();
    HAL_InitStorage();
    HAL_InitRTC();
    HAL_InitADC();
    HAL_WatchdogInit(5000);
}

// Capteur de pression MS5837
void HAL_InitPressureSensor(void) {
    // Configuration I2C1
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
    
    // Initialisation MS5837
    MS5837_Init(&pressure_sensor, &hi2c1, MS5837_ADDR_76);
    MS5837_SetResolution(&pressure_sensor, MS5837_OSR_8192);
}

bool HAL_ReadPressureTemp(float* pressure_mbar, float* temperature_c) {
    if (MS5837_Read(&pressure_sensor) != MS5837_OK) {
        return false;
    }
    
    *pressure_mbar = MS5837_GetPressure(&pressure_sensor);
    *temperature_c = MS5837_GetTemperature(&pressure_sensor);
    
    return true;
}

// ADC pour cellules O2
void HAL_InitADC(void) {
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    // Configuration des pins ADC (PC0, PC1, PC2)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    // Configuration ADC
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = ENABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 3;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    HAL_ADC_Init(&hadc1);
}

void HAL_ReadO2Cells(float* cell1_mv, float* cell2_mv, float* cell3_mv) {
    ADC_ChannelConfTypeDef sConfig = {0};
    uint32_t adc_values[3];
    
    // Lecture des 3 canaux
    for (int i = 0; i < 3; i++) {
        sConfig.Channel = ADC_CHANNEL_10 + i;
        sConfig.Rank = 1;
        sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
        HAL_ADC_ConfigChannel(&hadc1, &sConfig);
        
        HAL_ADC_Start(&hadc1);
        HAL_ADC_PollForConversion(&hadc1, 10);
        adc_values[i] = HAL_ADC_GetValue(&hadc1);
    }
    
    // Conversion en millivolts (3.3V ref, 12 bits)
    *cell1_mv = (adc_values[0] * 3300.0) / 4096.0;
    *cell2_mv = (adc_values[1] * 3300.0) / 4096.0;
    *cell3_mv = (adc_values[2] * 3300.0) / 4096.0;
}

// Affichage TFT
void HAL_DisplayInit(void) {
    // Configuration SPI1 pour écran TFT
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    // SPI pins
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // CS pin
    GPIO_InitStruct.Pin = DISPLAY_SPI_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // Configuration SPI
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    HAL_SPI_Init(&hspi1);
    
    // Initialisation écran ILI9341 ou similaire
    // ... (séquence d'initialisation spécifique à l'écran)
}

// Boutons
void HAL_InitButtons(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

ButtonEvent HAL_GetButtonEvent(void) {
    static uint32_t last_scan = 0;
    uint32_t now = HAL_GetSysTick();
    ButtonEvent event = BUTTON_NONE;
    
    if (now - last_scan < 20) return BUTTON_NONE; // Debounce 20ms
    last_scan = now;
    
    // Scan des boutons
    uint32_t current_state = 0;
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET) current_state |= BUTTON_MENU;
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET) current_state |= BUTTON_UP;
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2) == GPIO_PIN_RESET) current_state |= BUTTON_DOWN;
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) == GPIO_PIN_RESET) current_state |= BUTTON_ENTER;
    
    // Détection des changements
    uint32_t pressed = current_state & ~button_state;
    uint32_t released = ~current_state & button_state;
    
    // Mise à jour de l'état
    button_state = current_state;
    
    // Gestion des appuis longs
    if (current_state & BUTTON_MENU) {
        if (button_time[0] == 0) button_time[0] = now;
        else if (now - button_time[0] > 1000) event = BUTTON_MENU_LONG;
    } else {
        button_time[0] = 0;
    }
    
    if (current_state & BUTTON_ENTER) {
        if (button_time[3] == 0) button_time[3] = now;
        else if (now - button_time[3] > 1000) event = BUTTON_ENTER_LONG;
    } else {
        button_time[3] = 0;
    }
    
    // Retour des événements simples si pas d'appui long
    if (event == BUTTON_NONE) {
        if (pressed & BUTTON_MENU) event = BUTTON_MENU;
        else if (pressed & BUTTON_UP) event = BUTTON_UP;
        else if (pressed & BUTTON_DOWN) event = BUTTON_DOWN;
        else if (pressed & BUTTON_ENTER) event = BUTTON_ENTER;
    }
    
    return event;
}

// RTC
void HAL_InitRTC(void) {
    __HAL_RCC_RTC_ENABLE();
    
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    HAL_RTC_Init(&hrtc);
}

uint32_t HAL_RTCGetUnixTime(void) {
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;
    
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    
    // Conversion simplifiée en timestamp Unix
    // (implémentation complète nécessiterait une vraie fonction de conversion)
    uint32_t timestamp = sDate.Year * 365 * 24 * 3600;
    timestamp += sDate.Month * 30 * 24 * 3600;
    timestamp += sDate.Date * 24 * 3600;
    timestamp += sTime.Hours * 3600;
    timestamp += sTime.Minutes * 60;
    timestamp += sTime.Seconds;
    
    return timestamp;
}

// Watchdog
void HAL_WatchdogInit(uint16_t timeout_ms) {
    IWDG_HandleTypeDef hiwdg;
    
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
    hiwdg.Init.Reload = (timeout_ms * 40000) / (32 * 1000); // LSI ~40kHz
    HAL_IWDG_Init(&hiwdg);
}

void HAL_WatchdogFeed(void) {
    IWDG->KR = 0xAAAA;
}