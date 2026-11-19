// Copyright 2024 Lukas Hrazky
//
// This file is part of the Refloat VESC package.
//
// Refloat VESC package is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.
//
// Refloat VESC package is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <http://www.gnu.org/licenses/>.

#include "haptic_feedback.h"

#include "conf/datatypes.h"
#include "utils.h"
#include "vesc_c_if.h"

#include <math.h>

#define TONE_LENGTH 0.1f
// Re-RinkFloat horn safety limits (seconds)
// - stops this long after the last keepalive from the app (app disconnects).
//   A keepalive never starts a new press (only an explicit start does), so a
//   Bluetooth hiccup can cut a press short but never extend it.
#define HORN_TIMEOUT 0.5f
// - a tap plays at least this long, so it's noticeable
#define HORN_MIN_TIME 0.2f
// - one press never plays longer than this; holding longer is silent
#define HORN_MAX_TIME_RIDING 3.0f
#define HORN_MAX_TIME_IDLE 2.0f
// - heat budget instead of a fixed cooldown: up to HORN_BUDGET seconds of
//   horn are available; sounding uses it up (faster when idle, since the
//   tone then runs at standstill with no airflow cooling), silence refills
//   it 1 s per s. Tapping or holding is free as long as the long-run duty
//   stays <= ~50% riding / ~40% idle; when it runs dry the horn stops until
//   HORN_MIN_TIME has been earned back.
#define HORN_BUDGET 3.0f
#define HORN_DRAIN_RIDING 1.0f
#define HORN_DRAIN_IDLE 1.5f
#define HORN_REFILL 1.0f
// - in ALT mode, beep and buzz swap this often
#define HORN_ALT_STEP 0.125f
// rf75 safety: while riding the horn is the audible beep only (the same
// foc_play_tone upstream uses for its duty warning), never an extra motor
// current, and only below this duty
#define HORN_MAX_RIDING_DUTY 0.6f
// - the horn works out of the box: if the rider's haptic strengths are lower
//   (Refloat's default Vibrate Strength is 0), these minimums are used for the
//   horn only. Other haptic alerts keep using the configured values.
#define HORN_MIN_BEEP_STRENGTH 2.5f
// rf81: the horn's beep never goes above 4 V (the app's 10/10)
#define HORN_MAX_BEEP_STRENGTH 4.0f
#define HORN_MIN_VIBRATE_STRENGTH 8.0f
#define HORN_DEFAULT_BEEP_FREQUENCY 495
#define HORN_DEFAULT_VIBRATE_FREQUENCY 70
// Re-Float Rink-Mixed (rf68): the default boot melody is the rider's own
// ("Melody 2"), in the custom melody format: (pitch level 1-10 = E4..G5, 0 =
// rest; length in 20 ms units). 3.46 s.
static const uint8_t default_melody[] = {
    1, 17, 0, 5, 4, 6,  0, 5, 4, 4, 0, 9, 4, 14, 0,  11,
    4, 16, 0, 6, 6, 20, 0, 3, 9, 7, 0, 3, 7, 11, 10, 36,
};

void haptic_feedback_init(HapticFeedback *hf) {
    hf->type_playing = HAPTIC_FEEDBACK_NONE;
    hf->tone_timer = 0;
    hf->is_playing = false;
    hf->can_change_type = true;
    hf->horn_timer = 0;
    hf->horn_start = 0;
    hf->horn_budget = HORN_BUDGET;
    hf->tone_budget = TONE_BUDGET;
    hf->tone_budget_timer = 0;
    hf->tone_rest = false;
    hf->horn_budget_timer = 0;
    hf->horn_on = false;
    hf->horn_used = false;
    hf->horn_mode = HORN_MODE_BOTH;
    hf->horn_custom = false;
    hf->melody_active = false;
    hf->live_hz = 0;
    hf->live_start = 0;
    hf->live_playing = false;
}

uint16_t melody_level_hz(uint8_t level) {
    if (level == 0) {
        return 0;
    }
    if (level > 10) {
        level = 10;
    }
    // real musical notes (equal temperament, A4 = 440 Hz), the white keys
    // E4 F4 G4 A4 B4 C5 D5 E5 F5 G5 - all inside the safe 300-1000 Hz range
    static const uint16_t note_hz[10] = {330, 349, 392, 440, 494, 523, 587, 659, 698, 784};
    return note_hz[level - 1];
}

void haptic_feedback_play_melody(
    HapticFeedback *hf, const Time *time, float volume, const uint8_t *notes, uint8_t count
) {
    hf->melody_active = false;
    float total = 0.0f;
    uint8_t n = 0;
    hf->melody_gaps = true;
    if (count == 0) {
        notes = default_melody;
        count = sizeof(default_melody) / 2;
    }
    for (uint8_t i = 0; i < count && n < MELODY_MAX_NOTES; ++i) {
        uint8_t level = notes[2 * i];
        float len = notes[2 * i + 1] * MELODY_UNIT;
        if (len <= 0.0f) {
            break;
        }
        if (level > 0) {
            len = fminf(len, MELODY_MAX_NOTE);
        }
        len = fminf(len, MELODY_MAX_LENGTH - total);
        if (len < 0.04f) {
            break;
        }
        hf->melody_freq[n] = melody_level_hz(level);
        hf->melody_len[n] = len;
        total += len;
        ++n;
    }
    if (n == 0) {
        return;
    }
    hf->melody_count = n;
    // boot melody volume goes down to 0.25 V (app level 1)
    hf->melody_volume = clampf(volume, 0.2f, 6.0f);
    hf->melody_start = time->now;
    hf->melody_active = true;
}

void haptic_feedback_live_tone(HapticFeedback *hf, const Time *time, uint8_t level, float volume) {
    uint16_t hz = melody_level_hz(level);
    // a new note (different tile, or after a stop / timeout) restarts the 2 s cap
    if (hz != hf->live_hz || timer_older(time, hf->live_timer, 0.3f)) {
        hf->live_start = time->now;
    }
    hf->live_volume = clampf(volume, 0.2f, 6.0f);
    hf->live_hz = hz;
    hf->live_timer = time->now;
}

void haptic_feedback_horn_settings(
    HapticFeedback *hf,
    float beep_strength,
    uint16_t beep_frequency,
    float buzz_strength,
    uint16_t buzz_frequency
) {
    hf->horn_beep_strength = clampf(beep_strength, 0.4f, HORN_MAX_BEEP_STRENGTH);
    hf->horn_beep_frequency = (uint16_t) clampf(beep_frequency, 300, 1000);
    hf->horn_buzz_strength = clampf(buzz_strength, 0.0f, 15.0f);
    hf->horn_buzz_frequency = (uint16_t) clampf(buzz_frequency, 30, 150);
    hf->horn_custom = true;
}

void haptic_feedback_horn(HapticFeedback *hf, const Time *time, uint8_t req, uint8_t mode) {
    // A press whose keepalives stopped arriving counts as released now.
    if (hf->horn_on && timer_older(time, hf->horn_timer, HORN_TIMEOUT)) {
        hf->horn_on = false;
    }

    if (req == HORN_REQ_OFF) {
        hf->horn_on = false;
        return;
    }

    if (req == HORN_REQ_KEEPALIVE) {
        // keeps the current press alive (does not extend the max time);
        // ignored if the press already ended
        if (hf->horn_on) {
            hf->horn_timer = time->now;
        }
        return;
    }

    // HORN_REQ_START: a new press

    // new press: needs enough budget left for at least a short tap
    if (hf->horn_budget < HORN_MIN_TIME) {
        return;
    }

    hf->horn_mode = mode <= HORN_MODE_BOTH ? mode : HORN_MODE_BOTH;
    hf->horn_used = true;
    hf->horn_start = time->now;
    hf->horn_timer = time->now;
    hf->horn_on = true;
}

static inline bool horn_active(const HapticFeedback *hf, const Time *time, float max_time) {
    if (!hf->horn_used || hf->horn_budget <= 0.0f || timer_older(time, hf->horn_start, max_time)) {
        return false;
    }

    // minimum length for short taps
    if (!timer_older(time, hf->horn_start, HORN_MIN_TIME)) {
        return true;
    }

    return hf->horn_on && !timer_older(time, hf->horn_timer, HORN_TIMEOUT);
}

void haptic_feedback_configure(HapticFeedback *hf, const RefloatConfig *cfg) {
    hf->cfg = &cfg->haptic;
    hf->duty_solid_threshold = cfg->tiltback_duty + hf->cfg->duty_solid_offset;

    // pre-calculate the coefficients of the polynomial given the configured max strength speed
    float m = hf->cfg->max_strength_speed > 0 ? hf->cfg->max_strength_speed : 1;
    float a = hf->cfg->min_strength;
    hf->str_poly_b = (1 - hf->cfg->strength_curvature) * (1 - a) / m;
    hf->str_poly_c = (1 - a - hf->str_poly_b * m) / (m * m);
}

static HapticFeedbackType haptic_feedback_get_type(
    const HapticFeedback *hf,
    const State *state,
    const MotorData *md,
    const AlertTracker *at,
    const Time *time
) {
    // Re-RinkFloat: the horn also works while the board is idle (READY).
    // update() then plays only the foc_play_tone part, which runs the motor
    // at 0 A with an audio signal on top, so the wheel isn't pushed.
    if (state->state == STATE_READY && horn_active(hf, time, HORN_MAX_TIME_IDLE)) {
        return HAPTIC_FEEDBACK_HORN;
    }

    // TODO: Ideally we don't even do pushback in handtest, as it can be confusing
    if (state->state != STATE_RUNNING || state->mode == MODE_HANDTEST) {
        return HAPTIC_FEEDBACK_NONE;
    }

    if (at->fatal_error) {
        return HAPTIC_FEEDBACK_ERROR_FATAL;
    }

    switch (state->sat) {
    case SAT_PB_DUTY:
        if (md->duty_cycle > hf->duty_solid_threshold) {
            return HAPTIC_FEEDBACK_DUTY_CONTINUOUS;
        } else {
            return HAPTIC_FEEDBACK_DUTY_SPEED;
        }
    case SAT_PB_SPEED:
        return HAPTIC_FEEDBACK_DUTY_SPEED;
    case SAT_PB_TEMPERATURE:
        return HAPTIC_FEEDBACK_ERROR_TEMPERATURE;
    case SAT_PB_LOW_VOLTAGE:
    case SAT_PB_HIGH_VOLTAGE:
    case SAT_PB_ERROR:
        return HAPTIC_FEEDBACK_ERROR_VOLTAGE;
    default:
        break;
    }

    if (hf->cfg->current_threshold > 0.0f &&
        motor_data_get_current_saturation(md) > hf->cfg->current_threshold) {
        return HAPTIC_FEEDBACK_DUTY_CONTINUOUS;
    }

    // Re-RinkFloat: the horn has the lowest priority, so real pushback and
    // error alerts are never masked by it. rf75: and never above 60% duty,
    // where the motor needs all its voltage headroom.
    if (fabsf(md->duty_cycle) < HORN_MAX_RIDING_DUTY &&
        horn_active(hf, time, HORN_MAX_TIME_RIDING)) {
        return HAPTIC_FEEDBACK_HORN;
    }

    return HAPTIC_FEEDBACK_NONE;
}

// Returns the number of "beats" per period of a given tone. Tones are played
// on even beats and if there are more than two beats, the last beat is
// skipped, giving a certain number of "beeps" followed by a pause.
static uint8_t get_beats(HapticFeedbackType type) {
    switch (type) {
    case HAPTIC_FEEDBACK_DUTY_SPEED:
        return 2;
    case HAPTIC_FEEDBACK_DUTY_CONTINUOUS:
    case HAPTIC_FEEDBACK_HORN:
        return 0;
    case HAPTIC_FEEDBACK_ERROR_TEMPERATURE:
        return 6;
    case HAPTIC_FEEDBACK_ERROR_VOLTAGE:
        return 8;
    case HAPTIC_FEEDBACK_ERROR_FATAL:
        return 10;
    case HAPTIC_FEEDBACK_NONE:
        break;
    }

    return 0;
}

static const CfgHapticTone *get_haptic_tone(const HapticFeedback *hf) {
    switch (hf->type_playing) {
    case HAPTIC_FEEDBACK_DUTY_SPEED:
    case HAPTIC_FEEDBACK_DUTY_CONTINUOUS:
    case HAPTIC_FEEDBACK_HORN:
        return &hf->cfg->duty;
    case HAPTIC_FEEDBACK_ERROR_TEMPERATURE:
    case HAPTIC_FEEDBACK_ERROR_VOLTAGE:
    case HAPTIC_FEEDBACK_ERROR_FATAL:
        return &hf->cfg->error;
    case HAPTIC_FEEDBACK_NONE:
        break;
    }

    return 0;
}

static inline float strength_scale(const HapticFeedback *hf, float speed) {
    return min(hf->cfg->min_strength + hf->str_poly_b * speed + hf->str_poly_c * speed * speed, 1);
}

static inline void foc_play_tone(int channel, float freq, float voltage) {
    if (!VESC_IF->foc_play_tone) {
        return;
    }

    VESC_IF->foc_play_tone(channel, freq, voltage);
}

void haptic_feedback_update(
    HapticFeedback *hf,
    MotorControl *mc,
    const State *state,
    const MotorData *md,
    const AlertTracker *at,
    const Time *time
) {
    // Re-RinkFloat: tone budget for melodies + live tiles
    float tdt = fminf(timer_age(time, hf->tone_budget_timer), 0.1f);
    hf->tone_budget_timer = time->now;
    if (hf->melody_active || hf->live_playing) {
        hf->tone_budget -= tdt;
    } else {
        hf->tone_budget = fminf(hf->tone_budget + TONE_BUDGET_REFILL * tdt, TONE_BUDGET);
    }
    if (hf->tone_budget <= 0.0f) {
        hf->tone_budget = 0.0f;
        hf->tone_rest = true;
    } else if (hf->tone_rest && hf->tone_budget >= TONE_BUDGET_RESUME) {
        hf->tone_rest = false;
    }

    // Re-RinkFloat: boot melody takes over the tone while it plays (READY only)
    if (hf->melody_active) {
        float age = timer_age(time, hf->melody_start);
        uint8_t step = 0;
        while (step < hf->melody_count && age >= hf->melody_len[step]) {
            age -= hf->melody_len[step];
            ++step;
        }
        if (state->state != STATE_READY || step >= hf->melody_count || hf->tone_rest) {
            hf->melody_active = false;
            foc_play_tone(0, 1, 0.0f);
            hf->is_playing = false;
            hf->type_playing = HAPTIC_FEEDBACK_NONE;
            hf->can_change_type = true;
        } else {
            uint16_t hz = hf->melody_freq[step];
            // short gap at the end of each note so repeated pitches are heard
            bool gap = hf->melody_gaps && age > hf->melody_len[step] - 0.03f;
            if (hz == 0 || gap) {
                foc_play_tone(0, 1, 0.0f);
            } else {
                foc_play_tone(0, hz, hf->melody_volume);
            }
            return;
        }
    }

    // Re-RinkFloat: live tone from the melody tiles (READY only)
    // (a single held tile sounds for at most MELODY_MAX_NOTE)
    bool live = hf->live_hz > 0 && state->state == STATE_READY && !hf->tone_rest &&
        !timer_older(time, hf->live_timer, 0.3f) &&
        !timer_older(time, hf->live_start, MELODY_MAX_NOTE);
    if (live) {
        foc_play_tone(0, hf->live_hz, hf->live_volume);
        hf->live_playing = true;
        return;
    } else if (hf->live_playing) {
        // live_hz is kept so further keepalives of the same held tile
        // stay silent after the 2 s cap; a stop or a new tile clears it
        hf->live_playing = false;
        foc_play_tone(0, 1, 0.0f);
        hf->is_playing = false;
        hf->type_playing = HAPTIC_FEEDBACK_NONE;
        hf->can_change_type = true;
    }

    // Re-RinkFloat: horn heat budget (main loop only)
    float dt = timer_age(time, hf->horn_budget_timer);
    hf->horn_budget_timer = time->now;
    if (dt > 0.1f || dt < 0.0f) {
        dt = 0.1f;
    }
    if (haptic_feedback_horn_playing(hf)) {
        float drain = state->state == STATE_RUNNING ? HORN_DRAIN_RIDING : HORN_DRAIN_IDLE;
        hf->horn_budget = fmaxf(hf->horn_budget - drain * dt, 0.0f);
        if (hf->horn_budget <= 0.0f) {
            // ran dry: end this press so it doesn't sputter back on
            hf->horn_on = false;
        }
    } else {
        hf->horn_budget = fminf(hf->horn_budget + HORN_REFILL * dt, HORN_BUDGET);
    }

    HapticFeedbackType type_to_play = haptic_feedback_get_type(hf, state, md, at, time);

    if (type_to_play != hf->type_playing && hf->can_change_type) {
        hf->type_playing = type_to_play;
        timer_refresh(time, &hf->tone_timer);
    }

    bool should_be_playing = false;
    if (hf->type_playing != HAPTIC_FEEDBACK_NONE) {
        uint8_t beats = get_beats(hf->type_playing);
        if (beats == 0) {
            should_be_playing = true;
            hf->can_change_type = true;
        } else {
            float period = TONE_LENGTH * beats;
            float tone_time = fmodf(timer_age(time, hf->tone_timer), period);
            uint8_t beat = floorf(tone_time / TONE_LENGTH);
            uint8_t off_beat = beats > 2 ? beats - 2 : 0;

            should_be_playing = beat % 2 == 0 && (off_beat == 0 || beat != off_beat);
            // Only allow changing type (another pattern or stop alerting)
            // if we just finished a period
            hf->can_change_type = !hf->is_playing && beat == 0;
        }
    } else {
        hf->can_change_type = true;
    }

    if (hf->is_playing && !should_be_playing) {
        foc_play_tone(0, 1, 0.0f);
        motor_control_stop_tone(mc);
        hf->is_playing = false;
    } else if (should_be_playing) {
        const CfgHapticTone *tone = get_haptic_tone(hf);
        // Re-RinkFloat: the horn always plays at the configured maximum
        // strength (the strength reached at Max Strength Speed), regardless of
        // the current speed.
        float scale =
            hf->type_playing == HAPTIC_FEEDBACK_HORN ? 1.0f : strength_scale(hf, fabsf(md->speed));

        float beep_strength = tone->strength;
        uint16_t beep_frequency = tone->frequency;
        float buzz_strength = hf->cfg->vibrate.strength;
        uint16_t buzz_frequency = hf->cfg->vibrate.frequency;
        if (hf->type_playing == HAPTIC_FEEDBACK_HORN && hf->horn_custom) {
            // Re-RinkFloat: the horn's own settings from the app
            beep_strength = hf->horn_beep_strength;
            beep_frequency = hf->horn_beep_frequency;
            buzz_strength = hf->horn_buzz_strength;
            buzz_frequency = hf->horn_buzz_frequency;
        } else if (hf->type_playing == HAPTIC_FEEDBACK_HORN) {
            beep_strength =
                fminf(fmaxf(beep_strength, HORN_MIN_BEEP_STRENGTH), HORN_MAX_BEEP_STRENGTH);
            buzz_strength = fmaxf(buzz_strength, HORN_MIN_VIBRATE_STRENGTH);
            if (beep_frequency == 0) {
                beep_frequency = HORN_DEFAULT_BEEP_FREQUENCY;
            }
            if (buzz_frequency == 0) {
                buzz_frequency = HORN_DEFAULT_VIBRATE_FREQUENCY;
            }
        }

        bool play_beep = beep_strength > 0.0f;
        bool play_buzz = buzz_strength > 0.0f;

        if (hf->type_playing == HAPTIC_FEEDBACK_HORN) {
            // rf75 safety: the horn never adds current to the motor. Idle, a
            // current tone would move the wheel; riding, it would add a square
            // wave on top of the balance current. Buzz modes fall back to the
            // beep.
            bool can_buzz = false;
            (void) play_buzz;
            switch (hf->horn_mode) {
            case HORN_MODE_BEEP:
                play_buzz = false;
                break;
            case HORN_MODE_BUZZ:
                // fall back to the beep if buzzing isn't possible
                play_buzz = can_buzz;
                play_beep = play_beep && !can_buzz;
                break;
            case HORN_MODE_ALT: {
                uint32_t step = (uint32_t) (timer_age(time, hf->horn_start) / HORN_ALT_STEP);
                bool beep_step = (step % 2) == 0;
                // idle / no vibrate: beep, pause, beep, ...
                play_buzz = can_buzz && !beep_step;
                play_beep = play_beep && beep_step;
                break;
            }
            default:
                play_buzz = can_buzz;
                break;
            }
        }

        bool horn = hf->type_playing == HAPTIC_FEEDBACK_HORN;

        if (play_beep) {
            foc_play_tone(0, beep_frequency, beep_strength * scale);
        } else if (horn) {
            foc_play_tone(0, 1, 0.0f);
        }

        if (play_buzz) {
            motor_control_play_tone(mc, buzz_frequency, buzz_strength * scale);
        } else if (horn) {
            motor_control_stop_tone(mc);
        }

        hf->is_playing = true;
    }
}

bool haptic_feedback_horn_playing(const HapticFeedback *hf) {
    return hf->type_playing == HAPTIC_FEEDBACK_HORN && hf->is_playing;
}
