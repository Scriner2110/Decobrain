#include "dive_manager.h"
#include <string.h>

void DiveManager_Init(DiveManager* dm) {
    memset(dm, 0, sizeof(DiveManager));
    
    dm->phase = PHASE_SURFACE;
    dm->is_diving = false;
    dm->max_ascent_rate = 10.0; // m/min
    dm->fast_ascent_rate = 18.0; // m/min alarme
    dm->auto_start_dive = true;
    dm->safety_stop_enforce = true;
}

void DiveManager_Update(DiveManager* dm, float depth, float temperature, ZHL16Model* model) {
    uint32_t now = HAL_GetSysTick() / 1000;
    
    // Détection automatique début/fin de plongée
    if (dm->auto_start_dive) {
        if (!dm->is_diving && DiveManager_CheckDiveStart(dm, depth)) {
            DiveManager_StartDive(dm);
        } else if (dm->is_diving && DiveManager_CheckDiveEnd(dm, depth)) {
            DiveManager_EndDive(dm);
        }
    }
    
    if (!dm->is_diving) {
        // Mise à jour surface interval
        if (dm->surface_interval_start > 0) {
            dm->surface_interval_mins = (now - dm->surface_interval_start) / 60;
        }
        return;
    }
    
    // Mise à jour des taux
    DiveManager_UpdateRates(dm, depth);
    
    // Mise à jour profondeur moyenne
    dm->avg_depth_sum += depth;
    dm->avg_depth_samples++;
    
    // Vérification vitesse de remontée
    if (DiveManager_CheckAscentRate(dm)) {
        dm->ascent_rate_alarm = true;
    } else {
        dm->ascent_rate_alarm = false;
    }
    
    // Vérification plafond déco
    if (depth < model->ceiling) {
        dm->deco_ceiling_alarm = true;
        dm->missed_deco_stops++;
    } else {
        dm->deco_ceiling_alarm = false;
    }
    
    // Mise à jour phase de plongée
    DivePhase old_phase = dm->phase;
    
    if (dm->ascent_rate < -5.0) {
        dm->phase = PHASE_DESCENT;
    } else if (dm->ascent_rate > 3.0) {
        if (model->ceiling > 0 && depth <= model->ceiling + 3.0) {
            dm->phase = PHASE_DECO_STOP;
        } else {
            dm->phase = PHASE_ASCENT;
        }
    } else if (depth >= 3.0 && depth <= 6.0 && dm->safety_stop_required) {
        dm->phase = PHASE_SAFETY_STOP;
    } else {
        dm->phase = PHASE_BOTTOM;
    }
    
    // Gestion safety stop
    if (dm->safety_stop_enforce) {
        DiveManager_UpdateSafetyStop(dm, depth);
    }
    
    // Enregistrement échantillon
    if (now - dm->current_dive.start_timestamp >= dm->sample_counter) {
        DiveManager_RecordSample(dm, depth, temperature, 
                               model->current_gas, model->ceiling > 0 ? model->ascend_plan.tts : 0);
        dm->sample_counter++;
    }
}

void DiveManager_StartDive(DiveManager* dm) {
    uint32_t now = HAL_GetSysTick() / 1000;
    
    dm->is_diving = true;
    dm->dive_start_time = now;
    dm->phase = PHASE_DESCENT;
    
    // Initialisation profil
    memset(&dm->current_dive, 0, sizeof(DiveProfile));
    dm->current_dive.dive_number = DiveManager_GetLastDiveNumber() + 1;
    dm->current_dive.start_timestamp = now;
    dm->current_dive.surface_interval = dm->surface_interval_mins;
    
    dm->sample_counter = 0;
    dm->avg_depth_sum = 0;
    dm->avg_depth_samples = 0;
    dm->safety_stop_required = false;
    dm->safety_stop_completed = false;
    dm->safety_stop_timer = 0;
}

void DiveManager_EndDive(DiveManager* dm) {
    uint32_t now = HAL_GetSysTick() / 1000;
    
    dm->is_diving = false;
    dm->phase = PHASE_SURFACE_INTERVAL;
    dm->surface_interval_start = now;
    
    // Finalisation profil
    dm->current_dive.end_timestamp = now;
    dm->current_dive.duration = now - dm->current_dive.start_timestamp;
    dm->current_dive.avg_depth = dm->avg_depth_sum / dm->avg_depth_samples;
    
    // Compression et sauvegarde
    DiveManager_CompressSamples(dm);
    DiveManager_SaveDive(dm);
}

bool DiveManager_CheckDiveStart(DiveManager* dm, float depth) {
    static uint32_t start_time = 0;
    
    if (depth >= DIVE_START_DEPTH) {
        if (start_time == 0) {
            start_time = HAL_GetSysTick() / 1000;
        } else if ((HAL_GetSysTick() / 1000) - start_time >= 20) {
            start_time = 0;
            return true;
        }
    } else {
        start_time = 0;
    }
    
    return false;
}

bool DiveManager_CheckDiveEnd(DiveManager* dm, float depth) {
    static uint32_t surface_time = 0;
    
    if (depth <= DIVE_END_DEPTH) {
        if (surface_time == 0) {
            surface_time = HAL_GetSysTick() / 1000;
        } else if ((HAL_GetSysTick() / 1000) - surface_time >= DIVE_END_TIME) {
            surface_time = 0;
            return true;
        }
    } else {
        surface_time = 0;
    }
    
    return false;
}

void DiveManager_UpdateRates(DiveManager* dm, float depth) {
    static float last_depth = 0;
    static uint32_t last_time = 0;
    uint32_t now = HAL_GetSysTick() / 1000;
    
    if (last_time == 0) {
        last_time = now;
        last_depth = depth;
        return;
    }
    
    float time_delta = (now - last_time) / 60.0; // Minutes
    if (time_delta > 0) {
        float depth_delta = depth - last_depth;
        float rate = depth_delta / time_delta;
        
        // Filtrage exponentiel
        dm->ascent_rate = dm->ascent_rate * 0.7 + rate * 0.3;
        
        if (rate < 0) {
            dm->descent_rate = -rate;
        }
    }
    
    last_depth = depth;
    last_time = now;
}

bool DiveManager_CheckAscentRate(DiveManager* dm) {
    return (dm->ascent_rate > dm->fast_ascent_rate);
}

void DiveManager_UpdateSafetyStop(DiveManager* dm, float depth) {
    // Déclencher safety stop si remontée depuis >10m
    if (dm->current_dive.max_depth > 10.0 && !dm->safety_stop_completed) {
        dm->safety_stop_required = true;
    }
    
    // Gestion du timer
    if (dm->phase == PHASE_SAFETY_STOP && dm->safety_stop_required) {
        if (depth >= 4.5 && depth <= 5.5) {
            dm->safety_stop_timer++;
            if (dm->safety_stop_timer >= dm->config.safety_stop_time) {
                dm->safety_stop_completed = true;
                dm->safety_stop_required = false;
            }
        } else {
            dm->safety_stop_timer = 0; // Reset si hors zone
        }
    }
}

void DiveManager_RecordSample(DiveManager* dm, float depth, float temp, uint8_t gas, uint8_t deco) {
    if (dm->current_dive.num_samples >= MAX_DIVE_SAMPLES) {
        DiveManager_CompressSamples(dm);
    }
    
    DiveSample* sample = &dm->current_dive.samples[dm->current_dive.num_samples];
    sample->time = dm->sample_counter;
    sample->depth = (int16_t)(depth * 100);
    sample->temperature = (int16_t)(temp * 10);
    sample->gas_idx = gas;
    sample->deco_time = deco;
    sample->cns = 0; // À implémenter
    sample->events = 0;
    
    // Mise à jour des statistiques
    if (depth > dm->current_dive.max_depth) {
        dm->current_dive.max_depth = depth;
    }
    if (temp < dm->current_dive.min_temperature || dm->current_dive.min_temperature == 0) {
        dm->current_dive.min_temperature = temp;
    }
    
    dm->current_dive.num_samples++;
}

void DiveManager_CompressSamples(DiveManager* dm) {
    // Compression simple : garder 1 échantillon sur 2
    uint16_t j = 0;
    for (uint16_t i = 0; i < dm->current_dive.num_samples; i += 2) {
        dm->current_dive.samples[j] = dm->current_dive.samples[i];
        j++;
    }
    dm->current_dive.num_samples = j;
}