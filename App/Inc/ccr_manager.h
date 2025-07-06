#ifndef CCR_MANAGER_H
#define CCR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define NUM_O2_CELLS 3
#define CELL_MV_WINDOW_SIZE 10

// États des cellules O2
typedef enum {
    CELL_OK,
    CELL_CURRENT_LIMITED,
    CELL_DRIFT,
    CELL_FAIL,
    CELL_NOT_CALIBRATED
} CellStatus;

// Structure cellule O2
typedef struct {
    float mv;                           // Millivolts actuels
    float ppO2;                         // ppO2 calculée
    float mv_history[CELL_MV_WINDOW_SIZE];
    uint8_t history_idx;
    float calibration_factor;           // mV/ppO2
    float calibration_ppO2;             // ppO2 lors de la calibration
    uint32_t calibration_timestamp;
    float temperature_comp;             // Compensation température
    CellStatus status;
    bool is_voting;                     // Participe au vote
    float deviation;                    // Écart par rapport à la moyenne
} O2Cell;

// Modes de recycleur
typedef enum {
    CCR_MODE_FIXED_SETPOINT,
    CCR_MODE_AUTO_SETPOINT,
    SCR_MODE_PASSIVE,
    SCR_MODE_ACTIVE,
    PSCR_MODE
} RecyclerMode;

// Structure de gestion CCR
typedef struct {
    // Cellules O2
    O2Cell cells[NUM_O2_CELLS];
    float voted_ppO2;                   // ppO2 après vote
    uint8_t voting_cells;               // Nombre de cellules votantes
    
    // Setpoints
    float setpoint_low;                 // Setpoint bas (descente)
    float setpoint_high;                // Setpoint haut (fond)
    float setpoint_deco;                // Setpoint déco
    float current_setpoint;
    float auto_sp_switch_depth;
    
    // Mode et état
    RecyclerMode mode;
    bool is_bailout;
    uint8_t diluent_idx;
    uint8_t bailout_gas_idx;
    
    // SCR spécifique
    float scr_ratio;                    // Ratio 1:X pour SCR
    float scr_drop;                     // Chute métabolique ppO2
    
    // Alarmes
    bool alarm_ppO2_high;
    bool alarm_ppO2_low;
    bool alarm_cells_divergent;
    bool alarm_cells_failed;
    
    // Statistiques
    float ppO2_average_1min;
    float ppO2_max;
    float ppO2_min;
    uint32_t time_on_loop;              // Temps sur la boucle
} CCRManager;

// Fonctions principales CCR
void CCR_Init(CCRManager* ccr);
void CCR_Reset(CCRManager* ccr);
void CCR_Update(CCRManager* ccr, float ambient_pressure, float temperature);
void CCR_UpdateCellReadings(CCRManager* ccr, float cell1_mv, float cell2_mv, float cell3_mv);
float CCR_GetVotedPPO2(CCRManager* ccr);
void CCR_CalibrateCell(CCRManager* ccr, uint8_t cell_idx, float reference_ppO2);
void CCR_CalibrateAllCells(CCRManager* ccr, float reference_ppO2);

// Gestion des setpoints
void CCR_SetFixedSetpoint(CCRManager* ccr, float setpoint);
void CCR_SetAutoSetpoints(CCRManager* ccr, float low, float high, float deco, float switch_depth);
void CCR_UpdateAutoSetpoint(CCRManager* ccr, float depth);
void CCR_SwitchToBailout(CCRManager* ccr, uint8_t bailout_gas);
void CCR_ReturnToLoop(CCRManager* ccr);

// Mode SCR
void CCR_SetSCRMode(CCRManager* ccr, float ratio, float metabolic_drop);
float CCR_CalculateSCRppO2(CCRManager* ccr, float inspired_ppO2);

// Validation et alarmes
bool CCR_ValidateCells(CCRManager* ccr);
void CCR_CheckAlarms(CCRManager* ccr);
bool CCR_IsCellHealthy(O2Cell* cell);
float CCR_GetCellDeviation(CCRManager* ccr, uint8_t cell_idx);

#endif