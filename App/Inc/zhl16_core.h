#ifndef ZHL16_CORE_H
#define ZHL16_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define NUM_COMPARTMENTS 16
#define MAX_GASES 10
#define MAX_DECO_STOPS 20

// Structure compartiment tissulaire
typedef struct {
    float half_time_N2;
    float half_time_He;
    float a_N2;
    float b_N2;
    float a_He;
    float b_He;
    float pressure_N2;
    float pressure_He;
    float loading;          // % de saturation
} Compartment;

// Structure mélange gazeux
typedef struct {
    char name[16];
    float fO2;
    float fN2;
    float fHe;
    float ppO2_max;
    float ppO2_min;
    float mod;              // Maximum Operating Depth
    bool is_diluent;
    bool is_enabled;
    bool is_bailout;
} GasMix;

// Structure palier de décompression
typedef struct {
    float depth;
    uint16_t time;          // Secondes
    uint8_t gas_idx;
} DecoStop;

// Structure plan de remontée
typedef struct {
    DecoStop stops[MAX_DECO_STOPS];
    uint8_t num_stops;
    uint16_t tts;           // Time To Surface en minutes
    uint16_t tts_at_surface;
    float first_stop_depth;
    bool is_valid;
} AscendPlan;

// Configuration décompression
typedef struct {
    float gf_low;
    float gf_high;
    float last_stop_depth;
    float ascent_rate;
    float descent_rate;
    bool conservatism;      // Mode conservateur
    uint8_t altitude_level; // 0-4 (sea level to 3000m+)
    bool safety_stop_required;
    float safety_stop_depth;
    uint16_t safety_stop_time;
} DecoConfig;

// Structure principale ZHL-16
typedef struct {
    Compartment compartments[NUM_COMPARTMENTS];
    GasMix gases[MAX_GASES];
    uint8_t current_gas;
    uint8_t num_gases;
    DecoConfig config;
    
    // État de plongée
    float current_depth;
    float max_depth;
    float average_depth;
    uint32_t dive_time_seconds;
    float ambient_pressure;
    float surface_pressure;
    float water_vapor_pressure;
    
    // Résultats de calcul
    float ceiling;
    float ndl;              // No Deco Limit
    float cns;              // CNS O2 toxicity
    float otu;              // OTU O2 toxicity
    AscendPlan ascend_plan;
    
    // Mode recycleur
    bool ccr_mode;
    float setpoint;
    float actual_ppO2;      // ppO2 réelle mesurée
    
    // Statistiques
    float gf_current;
    float gf_surface;
    uint8_t leading_compartment;
    float saturation_percent;
} ZHL16Model;

// Tables ZHL-16B/C
extern const float ZHL16B_N2_halftimes[NUM_COMPARTMENTS];
extern const float ZHL16B_He_halftimes[NUM_COMPARTMENTS];
extern const float ZHL16B_N2_a[NUM_COMPARTMENTS];
extern const float ZHL16B_N2_b[NUM_COMPARTMENTS];
extern const float ZHL16C_N2_halftimes[NUM_COMPARTMENTS];
extern const float ZHL16C_N2_a[NUM_COMPARTMENTS];
extern const float ZHL16C_N2_b[NUM_COMPARTMENTS];

// Fonctions principales
void ZHL16_Init(ZHL16Model* model, float surface_pressure, bool use_zhl16c);
void ZHL16_Reset(ZHL16Model* model);
void ZHL16_UpdateTissues(ZHL16Model* model, float time_seconds);
void ZHL16_UpdateDepth(ZHL16Model* model, float depth_meters);
float ZHL16_GetCeiling(ZHL16Model* model);
float ZHL16_GetNDL(ZHL16Model* model);
void ZHL16_CalculateAscendPlan(ZHL16Model* model);
bool ZHL16_NeedsDecoStop(ZHL16Model* model);

// Gestion des gaz
void ZHL16_AddGas(ZHL16Model* model, uint8_t idx, const char* name, 
                  float fO2, float fN2, float fHe, bool is_diluent);
bool ZHL16_SwitchGas(ZHL16Model* model, uint8_t gas_idx);
uint8_t ZHL16_GetBestGas(ZHL16Model* model, float depth);
float ZHL16_CalculateMOD(float fO2, float ppO2_max);
float ZHL16_CalculateEND(float depth, float fN2);

// Mode CCR/SCR
void ZHL16_SetCCRMode(ZHL16Model* model, bool enable, float setpoint);
void ZHL16_UpdateCCRppO2(ZHL16Model* model, float measured_ppO2);
void ZHL16_SwitchToBailout(ZHL16Model* model);

// Toxicité O2
void ZHL16_UpdateCNS(ZHL16Model* model, float time_seconds);
void ZHL16_UpdateOTU(ZHL16Model* model, float time_seconds);
float ZHL16_GetCNSAtDepth(float ppO2);

// Gradient factors
void ZHL16_SetGradientFactors(ZHL16Model* model, float gf_low, float gf_high);
float ZHL16_GetCurrentGF(ZHL16Model* model);

// Utilitaires
float ZHL16_GetAmbientPressure(float depth, float surface_pressure);
float ZHL16_GetPartialPressure(float ambient_pressure, float fraction);
void ZHL16_GetTissueLoadings(ZHL16Model* model, float* loadings);

#endif