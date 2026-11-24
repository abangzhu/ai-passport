#pragma once

#include "marvy_game.h"

enum {
    MARVY_SESSION_IDLE_MS = 45000,
    MARVY_SESSION_EVENT_DIM = 1u << 16,
    MARVY_SESSION_EVENT_WAKE = 1u << 17,
};

typedef struct {
    marvy_game_t game;
    uint64_t now_ms;
    uint64_t last_input_ms;
    bool dimmed;
} marvy_session_t;

/* A session owns its game and accepts monotonic, absolute millisecond times.
 * Every function requires a valid pointer and the same owning task. */
void marvy_session_init(marvy_session_t *session, uint32_t seed, uint16_t best,
                        uint64_t now_ms);

/* Run queued inputs in timestamp order before advancing to the frame end.
 * Game time stops at the 45-second idle deadline while wall time keeps moving.
 * Game event bits are preserved alongside DIM/WAKE session event bits. */
uint32_t marvy_session_advance(marvy_session_t *session, uint64_t target_ms);

/* Advance to the captured event time before applying the action. An event older
 * than the session clock is ignored. The first press after idle only wakes the
 * display and never skips the intro, throws a ring, or restarts the results. */
uint32_t marvy_session_press(marvy_session_t *session, uint64_t pressed_at_ms);
