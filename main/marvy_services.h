#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MARVY_SOUND_INTRO = 0,
    MARVY_SOUND_THROW,
    MARVY_SOUND_HIT,
    MARVY_SOUND_MARVY_APPEAR,
    MARVY_SOUND_MARVY_HIT,
    MARVY_SOUND_MISS,
    MARVY_SOUND_FINISH,
} marvy_sound_t;

/* Call once during application startup, after attempting nvs_flash_init().
 * Loads the best score, then creates a process-lifetime worker. A true result
 * means the worker exists, not that storage or audio hardware is available.
 * Failures leave the game usable with a RAM-only record and/or no sound.
 * Repeated calls are harmless; startup calls must come from one task. */
bool marvy_services_start(void);

/* RAM-only, synchronized read; never accesses Flash or audio. */
uint16_t marvy_services_best_score(void);

/* Non-blocking. Coalesces pending records to the highest value. The worker
 * retries failed writes at most once per second; it never erases NVS. */
void marvy_services_save_best(uint16_t score);

/* Non-blocking, latest cue wins. Audio may be dropped to avoid a backlog.
 * Cues are original synthesized placeholders, not the final Marvy sound logo.
 * Both enqueue functions are safe from task callbacks, not interrupt handlers. */
void marvy_services_play(marvy_sound_t sound);
