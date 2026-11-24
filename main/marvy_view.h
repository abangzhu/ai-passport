#pragma once

#include "marvy_game.h"

/* These functions require the LVGL task or the BSP LVGL lock. The view owns
 * one process-lifetime screen and installs no callbacks or timers. */
void marvy_view_create(void);
void marvy_view_render(const marvy_game_t *game, int battery_percent,
                       bool input_available);
