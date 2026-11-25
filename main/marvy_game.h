#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "marvy_progress.h"

enum {
    MARVY_WIDTH = 240,
    MARVY_HEIGHT = 320,
    MARVY_AIM_X = 120,
    MARVY_SHEEP_BODY_Y = 194,
    MARVY_SHEEP_COUNT = 4,
    MARVY_HIT_RADIUS = 16,
    MARVY_CHARGE_MS = 1400,
    MARVY_DEFAULT_CHARGE_MS = MARVY_CHARGE_MS / 2,
    MARVY_TARGET_MIN_X = 32,
    MARVY_TARGET_MAX_X = 208,
    MARVY_FLIGHT_MS = 600,
    MARVY_FEEDBACK_MS = 200,
    MARVY_MARVY_FEEDBACK_MS = 1200,
    MARVY_COMBO_MS = 900,
    MARVY_MAX_STEP_MS = 60000,
};

typedef enum {
    MARVY_PHASE_INTRO,
    MARVY_PHASE_PLAYING,
    MARVY_PHASE_RESULTS,
    MARVY_PHASE_PAUSED,
    MARVY_PHASE_BRIEF,
} marvy_phase_t;

typedef enum { MARVY_PAUSE_CONTINUE, MARVY_PAUSE_EXIT } marvy_pause_selection_t;
typedef enum {
    MARVY_RESULT_NEXT,
    MARVY_RESULT_RETRY,
    MARVY_RESULT_WELCOME,
} marvy_result_selection_t;
typedef enum {
    MARVY_RING_READY,
    MARVY_RING_CHARGING,
    MARVY_RING_FLYING,
    MARVY_RING_FEEDBACK,
} marvy_ring_state_t;

typedef enum {
    MARVY_EVENT_THROW = 1u << 0,
    MARVY_EVENT_HIT = 1u << 1,
    MARVY_EVENT_MARVY_HIT = 1u << 2,
    MARVY_EVENT_MISS = 1u << 3,
    MARVY_EVENT_FINISH = 1u << 4,
    MARVY_EVENT_NEW_BEST = 1u << 5,
    MARVY_EVENT_START = 1u << 6,
    MARVY_EVENT_MARVY_APPEAR = 1u << 7,
    MARVY_EVENT_LEVEL_CLEAR = 1u << 8,
    MARVY_EVENT_LEVEL_FAIL = 1u << 9,
    MARVY_EVENT_PROGRESS_CHANGED = 1u << 10,
    MARVY_EVENT_COMBO_3 = 1u << 11,
    MARVY_EVENT_COMBO_5 = 1u << 12,
} marvy_event_t;

typedef struct {
    uint32_t duration_ms;
    uint16_t target_catches;
    uint16_t turn_limit_ms; /* Zero disables the per-throw deadline. */
    uint16_t speed_percent;
    uint8_t marvy_percent;
    uint8_t marvy_cap;
    bool show_target;
} marvy_level_config_t;

typedef struct {
    float x;
    float speed;
    bool marvy;
    bool announced;
    uint32_t hidden_ms;
} marvy_sheep_t;

typedef struct {
    marvy_phase_t phase;
    marvy_pause_selection_t pause_selection;
    marvy_result_selection_t result_selection;
    marvy_ring_state_t ring_state;
    marvy_progress_t progress;
    uint8_t level_index;
    uint16_t score;
    uint16_t best_score; /* Legacy overall record, separate from per-level data. */
    uint16_t captures;
    uint16_t throws;
    uint16_t hit_streak;
    uint16_t best_streak;
    uint8_t stars;
    bool passed;
    bool time_expired;
    uint32_t phase_ms;
    uint32_t stage_elapsed_ms;
    uint32_t ring_ms;
    uint32_t turn_ms;
    float ring_target_x;
    uint8_t last_points;
    float last_hit_x;
    bool last_hit_marvy;
    int8_t last_hit_sheep;
    uint32_t celebration_ms; /* Remaining milliseconds; zero means inactive. */
    float celebration_x;
    uint8_t celebration_points;
    uint32_t combo_ms;       /* Remaining milliseconds; zero means inactive. */
    uint16_t combo_count;
    marvy_sheep_t sheep[MARVY_SHEEP_COUNT];
    uint8_t spawn_bag[10];
    uint8_t spawn_bag_size;
    uint8_t spawn_bag_next;
    uint32_t rng_state;
} marvy_game_t;

/* Pure state, one owning task. Level indices are zero-based. */
const marvy_level_config_t *marvy_game_level_config(unsigned level_index);
uint32_t marvy_game_remaining_ms(const marvy_game_t *game);
void marvy_game_init(marvy_game_t *game, uint32_t seed, uint16_t legacy_best);
void marvy_game_apply_progress(marvy_game_t *game, const marvy_progress_t *progress);
/* Intro goes to the untimed brief. Brief confirm starts; results confirm applies
 * the selected menu action. Only active READY presses start a charge. */
uint32_t marvy_game_press(marvy_game_t *game);
void marvy_game_brief_move(marvy_game_t *game, int direction);
bool marvy_game_can_next(const marvy_game_t *game);
void marvy_game_result_move(marvy_game_t *game, int direction);
uint32_t marvy_game_result_confirm(marvy_game_t *game);
void marvy_game_pause(marvy_game_t *game);
void marvy_game_pause_select(marvy_game_t *game, marvy_pause_selection_t selection);
void marvy_game_pause_confirm(marvy_game_t *game);
float marvy_game_charge_target_x(uint32_t held_ms);
uint32_t marvy_game_release(marvy_game_t *game);
void marvy_game_cancel_charge(marvy_game_t *game);
/* Final three seconds of a finite throw deadline. Hidden in other states and
 * during the stage's final five seconds, when the overall clock takes priority. */
unsigned marvy_game_countdown(const marvy_game_t *game);
/* Identical input lock for every landing; celebration has its own timer. */
uint32_t marvy_game_feedback_ms(const marvy_game_t *game);
/* At the stage boundary, no new throw is accepted. An already airborne ring
 * settles before results. Stage deadline wins over a simultaneous throw limit. */
uint32_t marvy_game_step(marvy_game_t *game, uint32_t elapsed_ms);
