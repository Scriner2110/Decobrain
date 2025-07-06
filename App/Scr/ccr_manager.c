#include "ccr_manager.h"
#include <string.h>
#include <math.h>

void CCR_Init(CCRManager* ccr) {
    memset(ccr, 0, sizeof(CCRManager));
    
    // Configuration par défaut
    ccr->setpoint_low = 0.7;
    ccr->setpoint_high = 1.3;
    ccr->setpoint_deco = 1.4;
    ccr->auto_sp_switch_depth = 6.0;
    ccr->current_setpoint = ccr->setpoint_low;
    ccr->mode = CCR_MODE_FIXED_SETPOINT;
    
    // SCR par défaut
    ccr->scr_ratio = 10.0;  // 1:10
    ccr->scr_drop = 0.05;   // 0.05 bar métabolique
    
    // Initialisation des cellules
    for (int i = 0; i < NUM_O2_CELLS; i++) {
        ccr->cells[i].status = CELL_NOT_CALIBRATED;
        ccr->cells[i].calibration_factor = 47.6; // ~10mV pour 0.21 bar
        ccr->cells[i].is_voting = false;
    }
}

void CCR_UpdateCellReadings(CCRManager* ccr, float cell1_mv, float cell2_mv, float cell3_mv) {
    float cell_mv[3] = {cell1_mv, cell2_mv, cell3_mv};
    
    // Mise à jour des lectures
    for (int i = 0; i < NUM_O2_CELLS; i++) {
        O2Cell* cell = &ccr->cells[i];
        
        // Historique pour filtrage
        cell->mv_history[cell->history_idx] = cell_mv[i];
        cell->history_idx = (cell->history_idx + 1) % CELL_MV_WINDOW_SIZE;
        
        // Moyenne glissante
        float sum = 0;
        for (int j = 0; j < CELL_MV_WINDOW_SIZE; j++) {
            sum += cell->mv_history[j];
        }
        cell->mv = sum / CELL_MV_WINDOW_SIZE;
        
        // Calcul ppO2
        cell->ppO2 = cell->mv / cell->calibration_factor;
        
        // Vérification limites
        if (cell->ppO2 < 0.05 || cell->ppO2 > 2.0) {
            cell->status = CELL_FAIL;
            cell->is_voting = false;
        } else if (cell->status != CELL_FAIL) {
            cell->status = CELL_OK;
            cell->is_voting = true;
        }
    }
    
    // Validation et vote
    CCR_ValidateCells(ccr);
}

bool CCR_ValidateCells(CCRManager* ccr) {
    float sum = 0;
    int valid_cells = 0;
    
    // Calculer la moyenne des cellules valides
    for (int i = 0; i < NUM_O2_CELLS; i++) {
        if (ccr->cells[i].is_voting) {
            sum += ccr->cells[i].ppO2;
            valid_cells++;
        }
    }
    
    if (valid_cells < 2) {
        ccr->alarm_cells_failed = true;
        return false;
    }
    
    float average = sum / valid_cells;
    
    // Vérifier la divergence
    for (int i = 0; i < NUM_O2_CELLS; i++) {
        if (ccr->cells[i].is_voting) {
            ccr->cells[i].deviation = fabsf(ccr->cells[i].ppO2 - average);
            
            // Exclusion si trop divergent (>0.1 bar ou >10%)
            if (ccr->cells[i].deviation > 0.1 || 
                ccr->cells[i].deviation > average * 0.1) {
                ccr->cells[i].is_voting = false;
                ccr->cells[i].status = CELL_DRIFT;
            }
        }
    }
    
    // Recalculer avec les cellules restantes
    sum = 0;
    valid_cells = 0;
    for (int i = 0; i < NUM_O2_CELLS; i++) {
        if (ccr->cells[i].is_voting) {
            sum += ccr->cells[i].ppO2;
            valid_cells++;
        }
    }
    
    if (valid_cells >= 2) {
        ccr->voted_ppO2 = sum / valid_cells;
        ccr->voting_cells = valid_cells;
        ccr->alarm_cells_divergent = (valid_cells < 3);
        return true;
    }
    
    ccr->alarm_cells_failed = true;
    return false;
}

void CCR_CalibrateCell(CCRManager* ccr, uint8_t cell_idx, float reference_ppO2) {
    if (cell_idx >= NUM_O2_CELLS) return;
    
    O2Cell* cell = &ccr->cells[cell_idx];
    
    if (cell->mv > 0 && reference_ppO2 > 0) {
        cell->calibration_factor = cell->mv / reference_ppO2;
        cell->calibration_ppO2 = reference_ppO2;
        cell->calibration_timestamp = HAL_GetSysTick() / 1000;
        cell->status = CELL_OK;
        cell->is_voting = true;
    }
}

void CCR_UpdateAutoSetpoint(CCRManager* ccr, float depth) {
    if (ccr->mode != CCR_MODE_AUTO_SETPOINT) return;
    
    float new_setpoint = ccr->current_setpoint;
    
    if (depth < 3.0) {
        new_setpoint = ccr->setpoint_low;
    } else if (depth < ccr->auto_sp_switch_depth) {
        // Interpolation linéaire
        float ratio = (depth - 3.0) / (ccr->auto_sp_switch_depth - 3.0);
        new_setpoint = ccr->setpoint_low + 
                      (ccr->setpoint_high - ccr->setpoint_low) * ratio;
    } else {
        new_setpoint = ccr->setpoint_high;
    }
    
    // Changement progressif
    float delta = new_setpoint - ccr->current_setpoint;
    if (fabsf(delta) > 0.01) {
        ccr->current_setpoint += delta * 0.1; // 10% par mise à jour
    }
}

void CCR_CheckAlarms(CCRManager* ccr) {
    // Alarmes ppO2
    ccr->alarm_ppO2_high = (ccr->voted_ppO2 > 1.6);
    ccr->alarm_ppO2_low = (ccr->voted_ppO2 < 0.4);
    
    // Mise à jour min/max
    if (ccr->voted_ppO2 > ccr->ppO2_max) {
        ccr->ppO2_max = ccr->voted_ppO2;
    }
    if (ccr->voted_ppO2 < ccr->ppO2_min || ccr->ppO2_min == 0) {
        ccr->ppO2_min = ccr->voted_ppO2;
    }
}

void CCR_SwitchToBailout(CCRManager* ccr, uint8_t bailout_gas) {
    ccr->is_bailout = true;
    ccr->bailout_gas_idx = bailout_gas;
}

void CCR_ReturnToLoop(CCRManager* ccr) {
    ccr->is_bailout = false;
}

// Mode SCR
float CCR_CalculateSCRppO2(CCRManager* ccr, float inspired_ppO2) {
    // ppO2 = (FiO2 * Pamb * (1 - 1/ratio)) - drop métabolique
    float scr_ppO2 = inspired_ppO2 * (1.0 - 1.0 / ccr->scr_ratio) - ccr->scr_drop;
    if (scr_ppO2 < 0.16) scr_ppO2 = 0.16; // Sécurité minimum
    return scr_ppO2;
}