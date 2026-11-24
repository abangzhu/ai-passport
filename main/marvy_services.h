#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "marvy_music.h"

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

/* Non-blocking, latest cue wins. Feedback is mixed over ducked background
 * music by the same worker, never by a second codec owner. Ordinary and Marvy
 * hits use distinct licensed samples; other cues retain synthesized motifs. */
void marvy_services_play(marvy_sound_t sound);

/* Non-blocking music gate. Enable for active gameplay; disable on welcome,
 * results or idle. Rising edges restart the loop; repeated values do not.
 * Cues can finish after disable. Requests are safe from task callbacks, not ISR. */
void marvy_services_set_music(bool enabled);
