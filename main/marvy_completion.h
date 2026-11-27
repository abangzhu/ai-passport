#pragma once

#include "marvy_game.h"

enum {
    MARVY_COMPLETION_PARTICLES = 24,
    MARVY_COMPLETION_MS = 1800,
    MARVY_COMPLETION_FINALE_MS = 3000,
};

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t opacity;
    bool visible;
} marvy_completion_particle_t;

typedef struct {
    bool active;
    bool finale;
    uint8_t lit_stars;
    int8_t star_lift[3];
    int8_t heading_lift;
    int8_t sheep_lift;
    int8_t trophy_lift;
    marvy_completion_particle_t particles[MARVY_COMPLETION_PARTICLES];
} marvy_completion_frame_t;

/* Pure result-page animation, driven by phase_ms. Positive lifts move upward.
 * Menus remain interactive: this model never changes game state or input.
 * A null/invalid game produces an empty frame; a null frame is ignored. */
void marvy_completion_frame(const marvy_game_t *game,
                             marvy_completion_frame_t *frame);
