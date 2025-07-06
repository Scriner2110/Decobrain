#ifndef DIVE_MANAGER_H
#define DIVE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "zhl16_core.h"

#define DIVE_START_DEPTH 1.2        // Mètres
#define DIVE_END_DEPTH 0.8          // Mètres
#define DIVE_END_TIME 300           // 5 minutes
#define MAX_DIVE_SAMPLES 3600       // 1 heure à 1Hz
#define DIVE_LOG_MAX_ENTRIES 100

// Phase de plongée
typedef enum {
    PHASE_SURFACE,
    PHASE_DESCENT,
    PHASE_BOTTOM,
    PHASE_ASCENT,
    PHASE_DECO_STOP,
    PHASE_SAFETY_STOP,
    PHASE_SURFACE_INTERVAL
} DivePhase;

// Échantillon de plongée
typedef struct {
    uint16_t time;          // Secondes depuis début
    int16_t depth;          // Profondeur en cm
    int16_t temperature;    // Température en 0.1°C
    uint8_t gas_idx;
    uint8_t deco_time;      // Minutes de déco
    uint8_t cns;            // % CNS
    uint16_t events;        // Flags d'événements
} DiveSample;

// Profil de plongée
typedef struct {
    // Identification
    uint32_t dive_number;
    uint32_t start_timestamp;
    uint32_t end_timestamp;
    
    // Statistiques
    float max_depth;
    float avg_depth;
    uint32_t duration;
    float min_temperature;
    uint16_t surface_interval;  // Minutes depuis dernière plongée
    
    // Décompression
    uint8_t deco_violations;
    uint16_t max_deco_time;
    float max_gf;
    float max_cns;
    float max_otu;
    
    // Gaz
    uint8_t gases_used;
    float sac_rate;         // Surface Air Consumption
    
    // Échantillons
    DiveSample samples[MAX_DIVE_SAMPLES];
    uint16_t num_samples;
} DiveProfile;

// Gestionnaire de plongée
typedef struct {
    // État actuel
    DivePhase phase;
    bool is_diving;
    uint32_t dive_start_time;
    uint32_t phase_start_time;
    
    // Profil en cours
    DiveProfile current_dive;
    uint16_t sample_counter;
    
    // Statistiques temps réel
    float ascent_rate;          // m/min
    float descent_rate;         // m/min
    float avg_depth_sum;
    uint32_t avg_depth_samples;
    
    // Violations et alarmes
    bool ascent_rate_alarm;
    bool deco_ceiling_alarm;
    bool ppO2_alarm;
    uint8_t missed_deco_stops;
    
    // Safety stop
    bool safety_stop_required;
    bool safety_stop_completed;
    uint16_t safety_stop_timer;
    
    // Surface interval
    uint32_t surface_interval_start;
    uint16_t surface_interval_mins;
    
    // Configuration
    float max_ascent_rate;      // m/min
    float fast_ascent_rate;     // m/min pour alarme
    bool auto_start_dive;
    bool safety_stop_enforce;
} DiveManager;

// Fonctions principales
void DiveManager_Init(DiveManager* dm);
void DiveManager_Update(DiveManager* dm, float depth, float temperature, ZHL16Model* model);
void DiveManager_StartDive(DiveManager* dm);
void DiveManager_EndDive(DiveManager* dm);
bool DiveManager_CheckDiveStart(DiveManager* dm, float depth);
bool DiveManager_CheckDiveEnd(DiveManager* dm, float depth);

// Échantillonnage
void DiveManager_RecordSample(DiveManager* dm, float depth, float temp, uint8_t gas, uint8_t deco);
void DiveManager_CompressSamples(DiveManager* dm);

// Taux de remontée/descente
void DiveManager_UpdateRates(DiveManager* dm, float depth);
bool DiveManager_CheckAscentRate(DiveManager* dm);
float DiveManager_GetInstantRate(DiveManager* dm, float depth);

// Safety stop
void DiveManager_UpdateSafetyStop(DiveManager* dm, float depth);
bool DiveManager_IsSafetyStopRequired(DiveManager* dm);
void DiveManager_ResetSafetyStop(DiveManager* dm);

// Journal de plongée
bool DiveManager_SaveDive(DiveManager* dm);
bool DiveManager_LoadDive(uint32_t dive_number, DiveProfile* profile);
uint32_t DiveManager_GetLastDiveNumber(void);
void DiveManager_GetDiveList(uint32_t* dive_numbers, uint8_t max_count);

// Statistiques
float DiveManager_CalculateSAC(DiveManager* dm, float start_pressure, float end_pressure);
void DiveManager_UpdateTissueGraph(DiveManager* dm, ZHL16Model* model);

// Export
void DiveManager_ExportDive(DiveProfile* profile, char* buffer, uint32_t buffer_size);
void DiveManager_FormatLogbook(DiveProfile* profile, char* buffer, uint32_t buffer_size);

#endif