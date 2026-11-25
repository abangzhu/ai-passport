#pragma once

#include <stdbool.h>
#include <stdint.h>

enum { MARVY_LEVEL_COUNT = 6 };

typedef struct {
    uint16_t best_score[MARVY_LEVEL_COUNT];
    uint8_t best_stars[MARVY_LEVEL_COUNT];
    uint8_t unlocked_count; /* Count in 1..6, not a zero-based level index. */
} marvy_progress_t;

static inline void marvy_progress_init(marvy_progress_t *progress)
{
    for (unsigned i = 0; i < MARVY_LEVEL_COUNT; ++i) {
        progress->best_score[i] = 0;
        progress->best_stars[i] = 0;
    }
    progress->unlocked_count = 1;
}

static inline bool marvy_progress_valid(const marvy_progress_t *progress)
{
    if (!progress || progress->unlocked_count < 1 ||
        progress->unlocked_count > MARVY_LEVEL_COUNT) return false;
    for (unsigned i = 0; i < MARVY_LEVEL_COUNT; ++i) {
        if (progress->best_stars[i] > 3) return false;
    }
    return true;
}

/* Merge only valid snapshots. Records and unlocks are monotonic; invalid
 * persistence must never become a new high score or unlock arbitrary levels. */
static inline bool marvy_progress_merge(marvy_progress_t *into,
                                        const marvy_progress_t *snapshot)
{
    if (!marvy_progress_valid(into) || !marvy_progress_valid(snapshot)) return false;
    bool changed = false;
    if (snapshot->unlocked_count > into->unlocked_count) {
        into->unlocked_count = snapshot->unlocked_count;
        changed = true;
    }
    for (unsigned i = 0; i < MARVY_LEVEL_COUNT; ++i) {
        if (snapshot->best_score[i] > into->best_score[i]) {
            into->best_score[i] = snapshot->best_score[i];
            changed = true;
        }
        if (snapshot->best_stars[i] > into->best_stars[i]) {
            into->best_stars[i] = snapshot->best_stars[i];
            changed = true;
        }
    }
    return changed;
}
