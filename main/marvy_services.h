#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "marvy_music.h"
#include "marvy_progress.h"

/* Call once during application startup, after attempting nvs_flash_init().
 * Loads legacy and per-level records, then creates a process-lifetime worker. A true result
 * means the worker exists, not that storage or audio hardware is available.
 * Failures leave the game usable with a RAM-only record and/or no sound.
 * Repeated calls are harmless; startup calls must come from one task. */
bool marvy_services_start(void);

/* Legacy ten-ring record. It is retained separately and is never interpreted
 * as a timed-level score. These APIs never access Flash or audio directly. */
uint16_t marvy_services_best_score(void);

/* Non-blocking. Coalesces pending records to the highest value. The worker
 * retries failed writes at most once per second; it never erases NVS. */
void marvy_services_save_best(uint16_t score);

/* RAM-only synchronized snapshot and monotonic merge. Invalid snapshots are
 * ignored; null pointers are harmless. Saving never blocks on Flash or audio.
 * The worker saves a versioned encoding and retries at most once per second,
 * re-reading persisted data before writing to prevent record regression. */
void marvy_services_get_progress(marvy_progress_t *out);
void marvy_services_save_progress(const marvy_progress_t *progress);

/* Non-blocking, latest cue wins. Feedback is mixed over ducked background
 * music by the same worker, never by a second codec owner. Ordinary and Marvy
 * hits, level results and combos use licensed samples with synthesized fallback. */
void marvy_services_play(marvy_sound_t sound);

/* Non-blocking music gate. Enable for active gameplay; disable on welcome,
 * level brief, pause, results or idle. Rising edges restart the loop; repeated values do not.
 * Cues can finish after disable. Requests are safe from task callbacks, not ISR. */
void marvy_services_set_music(bool enabled);
