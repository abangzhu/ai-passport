#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    MARVY_WIDTH = 240,
    MARVY_HEIGHT = 320,
    MARVY_AIM_X = 120,
    MARVY_SHEEP_BODY_Y = 194,
    MARVY_SHEEP_COUNT = 4,
    MARVY_RING_COUNT = 10,
    MARVY_HIT_RADIUS = 20,
    MARVY_INTRO_MS = 2000,
    MARVY_FLIGHT_MS = 600,
    MARVY_FEEDBACK_MS = 400,
    /* Bound work after a corrupt timestamp or a very long suspended frame. */
    MARVY_MAX_STEP_MS = 60000,
};

typedef enum {
    MARVY_PHASE_INTRO,
    MARVY_PHASE_PLAYING,
    MARVY_PHASE_RESULTS,
} marvy_phase_t;

typedef enum {
    MARVY_RING_READY,
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
} marvy_event_t;

typedef struct {
    float x;             /* Body centre, in logical screen pixels. */
    float speed;         /* Signed pixels per second. */
    bool marvy;
    bool announced;      /* Appearance event already sent for this traversal. */
    uint32_t hidden_ms;  /* Captured sheep cannot be drawn or caught again. */
} marvy_sheep_t;

typedef struct {
    marvy_phase_t phase;
    marvy_ring_state_t ring_state;
    uint16_t score;
    uint16_t best_score;
    uint8_t rings_left;
    uint32_t phase_ms;
    uint32_t ring_ms;
    uint8_t last_points;
    float last_hit_x;
    bool last_hit_marvy;
    int8_t last_hit_sheep;
    marvy_sheep_t sheep[MARVY_SHEEP_COUNT];
    uint32_t rng_state;
} marvy_game_t;

/* All functions require a valid game pointer and a single owning task. */
void marvy_game_init(marvy_game_t *game, uint32_t seed, uint16_t best);

/* Intro press skips the splash; results press restarts without spending a ring.
 * During flight or feedback a press is ignored. Events are returned once, not
 * retained for a later read; callers may dispatch them to their audio worker. */
uint32_t marvy_game_press(marvy_game_t *game);

/* Advances sheep before resolving each landing, including when a call spans
 * multiple phase deadlines. At most MARVY_MAX_STEP_MS is accepted per call. */
uint32_t marvy_game_step(marvy_game_t *game, uint32_t elapsed_ms);
