#include "zhl16_core.h"
#include <string.h>

// Tables ZHL-16B
const float ZHL16B_N2_halftimes[NUM_COMPARTMENTS] = {
    4.0, 8.0, 12.5, 18.5, 27.0, 38.3, 54.3, 77.0,
    109.0, 146.0, 187.0, 239.0, 305.0, 390.0, 498.0, 635.0
};

const float ZHL16B_He_halftimes[NUM_COMPARTMENTS] = {
    1.51, 3.02, 4.72, 6.99, 10.21, 14.48, 20.53, 29.11,
    41.20, 55.19, 70.69, 90.34, 115.29, 147.42, 188.24, 240.03
};

const float ZHL16B_N2_a[NUM_COMPARTMENTS] = {
    1.2599, 1.0000, 0.8618, 0.7562, 0.6667, 0.5933,
    0.5282, 0.4701, 0.4187, 0.3798, 0.3497, 0.3223,
    0.2971, 0.2737, 0.2523, 0.2327
};

const float ZHL16B_N2_b[NUM_COMPARTMENTS] = {
    0.5050, 0.6514, 0.7222, 0.7825, 0.8126, 0.8434,
    0.8693, 0.8910, 0.9092, 0.9222, 0.9319, 0.9403,
    0.9477, 0.9544, 0.9602, 0.9653
};

// Tables ZHL-16C (plus conservatrices)
const float ZHL16C_N2_halftimes[NUM_COMPARTMENTS] = {
    4.0, 8.0, 12.5, 18.5, 27.0, 38.3, 54.3, 77.0,
    109.0, 146.0, 187.0, 239.0, 305.0, 390.0, 498.0, 635.0
};

const float ZHL16C_N2_a[NUM_COMPARTMENTS] = {
    1.2599, 1.0000, 0.8618, 0.7562, 0.6200, 0.5043,
    0.4410, 0.4000, 0.3750, 0.3500, 0.3295, 0.3065,
    0.2835, 0.2610, 0.2480, 0.2327
};

const float ZHL16C_N2_b[NUM_COMPARTMENTS] = {
    0.5050, 0.6514, 0.7222, 0.7825, 0.8126, 0.8434,
    0.8693, 0.8910, 0.9092, 0.9222, 0.9319, 0.9403,
    0.9477, 0.9544, 0.9602, 0.9653
};

// Initialisation du modèle
void ZHL16_Init(ZHL16Model* model, float surface_pressure, bool use_zhl16c) {
    memset(model, 0, sizeof(ZHL16Model));
    
    model->surface_pressure = surface_pressure;
    model->water_vapor_pressure = 0.0627; // bar à 37°C
    model->ambient_pressure = surface_pressure;
    
    // Configuration par défaut
    model->config.gf_low = 30.0;
    model->config.gf_high = 85.0;
    model->config.last_stop_depth = 3.0;
    model->config.ascent_rate = 10.0;
    model->config.descent_rate = 20.0;
    model->config.safety_stop_required = true;
    model->config.safety_stop_depth = 5.0;
    model->config.safety_stop_time = 180; // 3 minutes
    
    // Initialisation des compartiments
    for (int i = 0; i < NUM_COMPARTMENTS; i++) {
        Compartment* comp = &model->compartments[i];
        
        if (use_zhl16c) {
            comp->half_time_N2 = ZHL16C_N2_halftimes[i];
            comp->a_N2 = ZHL16C_N2_a[i];
            comp->b_N2 = ZHL16C_N2_b[i];
        } else {
            comp->half_time_N2 = ZHL16B_N2_halftimes[i];
            comp->a_N2 = ZHL16B_N2_a[i];
            comp->b_N2 = ZHL16B_N2_b[i];
        }
        
        comp->half_time_He = ZHL16B_He_halftimes[i];
        comp->a_He = comp->a_N2 * 1.5;
        comp->b_He = comp->b_N2 * 0.9;
        
        // Saturation initiale avec air
        float air_pressure = (surface_pressure - model->water_vapor_pressure) * 0.79;
        comp->pressure_N2 = air_pressure;
        comp->pressure_He = 0.0;
        comp->loading = 0.0;
    }
}

// Mise à jour des tissus
void ZHL16_UpdateTissues(ZHL16Model* model, float time_seconds) {
    float time_minutes = time_seconds / 60.0;
    GasMix* gas = &model->gases[model->current_gas];
    
    // Variables pour tracking
    float max_loading = 0.0;
    model->leading_compartment = 0;
    
    for (int i = 0; i < NUM_COMPARTMENTS; i++) {
        Compartment* comp = &model->compartments[i];
        
        // Calcul des pressions inspirées
        float inspired_N2, inspired_He;
        
        if (model->ccr_mode) {
            // Mode CCR : utilise la ppO2 mesurée
            float diluent_pressure = model->ambient_pressure - model->actual_ppO2;
            float total_inert = gas->fN2 + gas->fHe;
            
            if (total_inert > 0) {
                inspired_N2 = diluent_pressure * (gas->fN2 / total_inert);
                inspired_He = diluent_pressure * (gas->fHe / total_inert);
            } else {
                inspired_N2 = 0;
                inspired_He = 0;
            }
        } else {
            // Circuit ouvert
            inspired_N2 = (model->ambient_pressure - model->water_vapor_pressure) * gas->fN2;
            inspired_He = (model->ambient_pressure - model->water_vapor_pressure) * gas->fHe;
        }
        
        // Équation de Schreiner
        float k_N2 = 0.693147 / comp->half_time_N2;
        float k_He = 0.693147 / comp->half_time_He;
        
        comp->pressure_N2 = inspired_N2 + (comp->pressure_N2 - inspired_N2) * expf(-k_N2 * time_minutes);
        comp->pressure_He = inspired_He + (comp->pressure_He - inspired_He) * expf(-k_He * time_minutes);
        
        // Calcul du pourcentage de saturation
        float p_total = comp->pressure_N2 + comp->pressure_He;
        float a = (comp->a_N2 * comp->pressure_N2 + comp->a_He * comp->pressure_He) / p_total;
        float b = (comp->b_N2 * comp->pressure_N2 + comp->b_He * comp->pressure_He) / p_total;
        
        float m_value = a + model->ambient_pressure / b;
        comp->loading = (p_total / m_value) * 100.0;
        
        if (comp->loading > max_loading) {
            max_loading = comp->loading;
            model->leading_compartment = i;
        }
    }
    
    model->saturation_percent = max_loading;
    model->dive_time_seconds += time_seconds;
}

// Calcul du plafond
float ZHL16_GetCeiling(ZHL16Model* model) {
    float ceiling = 0.0;
    
    for (int i = 0; i < NUM_COMPARTMENTS; i++) {
        Compartment* comp = &model->compartments[i];
        
        float p_total = comp->pressure_N2 + comp->pressure_He;
        if (p_total <= 0) continue;
        
        // Coefficients moyens pondérés
        float a = (comp->a_N2 * comp->pressure_N2 + comp->a_He * comp->pressure_He) / p_total;
        float b = (comp->b_N2 * comp->pressure_N2 + comp->b_He * comp->pressure_He) / p_total;
        
        // Gradient factor actuel
        float gf;
        if (model->current_depth <= 0) {
            gf = model->config.gf_high;
        } else {
            float gf_slope = (model->config.gf_high - model->config.gf_low) / model->max_depth;
            gf = model->config.gf_low + gf_slope * (model->max_depth - model->current_depth);
        }
        
        // Pression ambiante tolérée avec GF
        float p_tolerated = (p_total - a * (gf / 100.0)) / (1.0 / b - (gf / 100.0) + 1.0);
        
        // Conversion en profondeur
        float comp_ceiling = (p_tolerated - model->surface_pressure) * 10.0;
        
        if (comp_ceiling > ceiling) {
            ceiling = comp_ceiling;
        }
    }
    
    // Arrondir au palier supérieur
    if (ceiling > 0) {
        ceiling = ceilf(ceiling / model->config.last_stop_depth) * model->config.last_stop_depth;
    }
    
    model->ceiling = ceiling;
    return ceiling;
}

// Calcul du plan de remontée
void ZHL16_CalculateAscendPlan(ZHL16Model* model) {
    AscendPlan* plan = &model->ascend_plan;
    memset(plan, 0, sizeof(AscendPlan));
    
    if (!ZHL16_NeedsDecoStop(model)) {
        plan->is_valid = true;
        plan->tts = (uint16_t)(model->current_depth / model->config.ascent_rate);
        return;
    }
    
    // Copie temporaire du modèle pour simulation
    ZHL16Model sim_model = *model;
    float current_depth = model->current_depth;
    uint16_t total_time = 0;
    uint8_t stop_idx = 0;
    
    // Remontée jusqu'au premier palier
    float first_stop = ZHL16_GetCeiling(&sim_model);
    if (first_stop > 0) {
        float ascent_time = (current_depth - first_stop) / model->config.ascent_rate;
        ZHL16_UpdateTissues(&sim_model, ascent_time * 60);
        total_time += ascent_time;
        current_depth = first_stop;
        plan->first_stop_depth = first_stop;
    }
    
    // Calcul des paliers
    while (current_depth > 0 && stop_idx < MAX_DECO_STOPS) {
        DecoStop* stop = &plan->stops[stop_idx];
        stop->depth = current_depth;
        stop->time = 0;
        stop->gas_idx = ZHL16_GetBestGas(&sim_model, current_depth);
        
        // Changer de gaz si nécessaire
        if (stop->gas_idx != sim_model.current_gas) {
            sim_model.current_gas = stop->gas_idx;
        }
        
        // Attendre à ce palier jusqu'à pouvoir monter
        while (ZHL16_GetCeiling(&sim_model) > (current_depth - model->config.last_stop_depth)) {
            ZHL16_UpdateTissues(&sim_model, 60); // 1 minute
            stop->time += 60;
            total_time++;
            
            if (stop->time > 3600) { // Sécurité : max 1h par palier
                break;
            }
        }
        
        if (stop->time > 0) {
            stop_idx++;
        }
        
        // Monter au palier suivant
        current_depth -= model->config.last_stop_depth;
        if (current_depth > 0) {
            float ascent_time = model->config.last_stop_depth / model->config.ascent_rate;
            ZHL16_UpdateTissues(&sim_model, ascent_time * 60);
            total_time += ascent_time;
        }
    }
    
    // Remontée finale
    if (current_depth > 0) {
        total_time += current_depth / model->config.ascent_rate;
    }
    
    plan->num_stops = stop_idx;
    plan->tts = total_time;
    plan->is_valid = true;
}

// Calcul du NDL
float ZHL16_GetNDL(ZHL16Model* model) {
    if (ZHL16_NeedsDecoStop(model)) {
        return 0.0;
    }
    
    float ndl = 999.0;
    GasMix* gas = &model->gases[model->current_gas];
    
    for (int i = 0; i < NUM_COMPARTMENTS; i++) {
        Compartment* comp = &model->compartments[i];
        
        float inspired_N2, inspired_He;
        if (model->ccr_mode) {
            float diluent_pressure = model->ambient_pressure - model->actual_ppO2;
            float total_inert = gas->fN2 + gas->fHe;
            inspired_N2 = diluent_pressure * (gas->fN2 / total_inert);
            inspired_He = diluent_pressure * (gas->fHe / total_inert);
        } else {
            inspired_N2 = (model->ambient_pressure - model->water_vapor_pressure) * gas->fN2;
            inspired_He = (model->ambient_pressure - model->water_vapor_pressure) * gas->fHe;
        }
        
        // M-value à la surface avec GF high
        float gf = model->config.gf_high / 100.0;
        float m_value_surface = (model->surface_pressure - comp->a_N2 * gf) / 
                               (comp->b_N2 - gf + 1.0);
        
        // Temps restant pour N2
        if (inspired_N2 > comp->pressure_N2 && comp->pressure_N2 < m_value_surface) {
            float k = 0.693147 / comp->half_time_N2;
            float remaining = -logf((m_value_surface - inspired_N2) / 
                                   (comp->pressure_N2 - inspired_N2)) / k;
            if (remaining < ndl) ndl = remaining;
        }
        
        // Temps restant pour He
        if (gas->fHe > 0 && inspired_He > comp->pressure_He) {
            float m_value_He = (model->surface_pressure - comp->a_He * gf) / 
                              (comp->b_He - gf + 1.0);
            if (comp->pressure_He < m_value_He) {
                float k = 0.693147 / comp->half_time_He;
                float remaining = -logf((m_value_He - inspired_He) / 
                                       (comp->pressure_He - inspired_He)) / k;
                if (remaining < ndl) ndl = remaining;
            }
        }
    }
    
    model->ndl = ndl;
    return ndl;
}

// Gestion des gaz
void ZHL16_AddGas(ZHL16Model* model, uint8_t idx, const char* name, 
                  float fO2, float fN2, float fHe, bool is_diluent) {
    if (idx >= MAX_GASES) return;
    
    GasMix* gas = &model->gases[idx];
    strncpy(gas->name, name, sizeof(gas->name) - 1);
    gas->fO2 = fO2;
    gas->fN2 = fN2;
    gas->fHe = fHe;
    gas->ppO2_max = 1.4;
    gas->ppO2_min = 0.16;
    gas->mod = ZHL16_CalculateMOD(fO2, gas->ppO2_max);
    gas->is_diluent = is_diluent;
    gas->is_enabled = true;
    gas->is_bailout = false;
    
    if (idx >= model->num_gases) {
        model->num_gases = idx + 1;
    }
}

// Calcul du meilleur gaz
uint8_t ZHL16_GetBestGas(ZHL16Model* model, float depth) {
    uint8_t best_gas = model->current_gas;
    float best_ppO2 = 0;
    
    for (uint8_t i = 0; i < model->num_gases; i++) {
        GasMix* gas = &model->gases[i];
        if (!gas->is_enabled) continue;
        
        float ppO2 = ZHL16_GetPartialPressure(
            ZHL16_GetAmbientPressure(depth, model->surface_pressure), 
            gas->fO2
        );
        
        // Vérifier les limites
        if (ppO2 > gas->ppO2_max || ppO2 < gas->ppO2_min) continue;
        
        // Préférer le gaz avec la ppO2 la plus élevée (dans les limites)
        if (ppO2 > best_ppO2) {
            best_ppO2 = ppO2;
            best_gas = i;
        }
    }
    
    return best_gas;
}

// Calcul MOD
float ZHL16_CalculateMOD(float fO2, float ppO2_max) {
    return (ppO2_max / fO2 - 1.0) * 10.0;
}

// Calcul END
float ZHL16_CalculateEND(float depth, float fN2) {
    float narcotic_pressure = ZHL16_GetPartialPressure(
        ZHL16_GetAmbientPressure(depth, 1.013), 
        fN2
    );
    return (narcotic_pressure / 0.79 - 1.0) * 10.0;
}

// Toxicité O2 - CNS
void ZHL16_UpdateCNS(ZHL16Model* model, float time_seconds) {
    float ppO2;
    
    if (model->ccr_mode) {
        ppO2 = model->actual_ppO2;
    } else {
        GasMix* gas = &model->gases[model->current_gas];
        ppO2 = ZHL16_GetPartialPressure(model->ambient_pressure, gas->fO2);
    }
    
    float cns_rate = ZHL16_GetCNSAtDepth(ppO2);
    model->cns += (cns_rate * time_seconds / 60.0);
    
    // Décroissance en surface
    if (ppO2 < 0.5) {
        float half_time = 90.0; // minutes
        model->cns *= expf(-0.693147 * time_seconds / (half_time * 60));
    }
    
    if (model->cns > 100.0) model->cns = 100.0;
    if (model->cns < 0.0) model->cns = 0.0;
}

// Table NOAA CNS
float ZHL16_GetCNSAtDepth(float ppO2) {
    if (ppO2 <= 0.5) return 0.0;
    else if (ppO2 <= 0.6) return 100.0 / 720.0;  // 0.14%/min
    else if (ppO2 <= 0.7) return 100.0 / 570.0;  // 0.18%/min
    else if (ppO2 <= 0.8) return 100.0 / 450.0;  // 0.22%/min
    else if (ppO2 <= 0.9) return 100.0 / 360.0;  // 0.28%/min
    else if (ppO2 <= 1.0) return 100.0 / 300.0;  // 0.33%/min
    else if (ppO2 <= 1.1) return 100.0 / 240.0;  // 0.42%/min
    else if (ppO2 <= 1.2) return 100.0 / 210.0;  // 0.48%/min
    else if (ppO2 <= 1.3) return 100.0 / 180.0;  // 0.56%/min
    else if (ppO2 <= 1.4) return 100.0 / 150.0;  // 0.67%/min
    else if (ppO2 <= 1.5) return 100.0 / 120.0;  // 0.83%/min
    else if (ppO2 <= 1.6) return 100.0 / 45.0;   // 2.22%/min
    else return 100.0 / 6.0;                      // 16.67%/min
}

// Mode CCR
void ZHL16_SetCCRMode(ZHL16Model* model, bool enable, float setpoint) {
    model->ccr_mode = enable;
    model->setpoint = setpoint;
    if (!enable) {
        model->actual_ppO2 = 0;
    }
}

void ZHL16_UpdateCCRppO2(ZHL16Model* model, float measured_ppO2) {
    model->actual_ppO2 = measured_ppO2;
}

void ZHL16_SwitchToBailout(ZHL16Model* model) {
    model->ccr_mode = false;
    model->actual_ppO2 = 0;
    // Trouver le premier gaz bailout
    for (uint8_t i = 0; i < model->num_gases; i++) {
        if (model->gases[i].is_bailout && model->gases[i].is_enabled) {
            model->current_gas = i;
            break;
        }
    }
}