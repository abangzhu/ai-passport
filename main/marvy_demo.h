#pragma once

#include "marvy_game.h"

typedef enum {
    MARVY_DEMO_READY,
    MARVY_DEMO_CHARGE,
    MARVY_DEMO_THROW,
    MARVY_DEMO_HIT,
    MARVY_DEMO_BONUS,
    MARVY_DEMO_DONE,
} marvy_demo_step_t;

typedef struct {
    marvy_game_t scene;
    marvy_demo_step_t step;
    bool key_held;
} marvy_demo_frame_t;

/* Independent presentation data only: no input, audio, progress or state
 * mutation. elapsed_ms loops every six seconds; null output is inert. */
void marvy_demo_frame(uint32_t elapsed_ms, marvy_demo_frame_t *frame);
