#pragma once

#include "marvy_game.h"

enum {
    MARVY_SESSION_IDLE_MS = 45000,
    MARVY_SESSION_EVENT_DIM = 1u << 16,
    MARVY_SESSION_EVENT_WAKE = 1u << 17,
};

typedef enum {
    MARVY_BUTTON_UP,
    MARVY_BUTTON_DOWN,
    MARVY_BUTTON_OK,
    MARVY_BUTTON_COUNT,
} marvy_button_t;

/* Physical right-side keys from top to bottom. Keep logical menu identities
 * stable; the board adapter translates ADC slots before session dispatch. */
#define MARVY_KEY_1 MARVY_BUTTON_OK
#define MARVY_KEY_2 MARVY_BUTTON_UP
#define MARVY_KEY_3 MARVY_BUTTON_DOWN

typedef struct {
    marvy_game_t game;
    uint64_t now_ms;
    uint64_t last_input_ms;
    bool dimmed;
    bool button_held;
    bool release_throws; /* This #2/#3 gesture actually began a charge. */
} marvy_session_t;

/* A session owns its game and accepts monotonic, absolute millisecond times.
 * Every function requires a valid pointer and the same owning task. */
void marvy_session_init(marvy_session_t *session, uint32_t seed, uint16_t best,
                        uint64_t now_ms);

/* Run queued inputs in timestamp order before advancing to the frame end.
 * Playing stays awake until the fixed stage timer and final airborne ring settle.
 * Intro/controls/brief/results/paused stop at their
 * 45-second idle deadline while wall time keeps moving; newly observed results
 * start a fresh idle interval at target_ms. Large frames use bounded core steps.
 * Game event bits are preserved alongside DIM/WAKE session event bits. */
uint32_t marvy_session_advance(marvy_session_t *session, uint64_t target_ms);

/* Convenience #1 confirmation/return at the captured time. An event older
 * than the session clock is ignored. The first press after idle only wakes the
 * display and never skips the intro, throws a ring, or restarts the results. */
uint32_t marvy_session_press(marvy_session_t *session, uint64_t pressed_at_ms);

/* Menus: #1 confirms, #2 moves up and #3 moves down. During play #1 opens the
 * pause menu immediately; #2/#3 both charge and throw on their matching release.
 * Controls-page #2/#3 are inert. A press
 * whose time advance completes a stage is consumed before results navigation.
 * Every press still requires its matching release;
 * the first press after idle only wakes, including in the paused menu. */
uint32_t marvy_session_press_button(marvy_session_t *session, marvy_button_t button,
                                   uint64_t pressed_at_ms);

/* Apply the matching release at its captured timestamp. Unpaired/old releases
 * cannot spend a ring or wake the display. Repeat presses require a release. */
uint32_t marvy_session_release(marvy_session_t *session, uint64_t released_at_ms);

/* Recover from lost/overflowed input without inventing a throw. The caller
 * should also discard any queued edges whose pairing is no longer known. */
void marvy_session_cancel_input(marvy_session_t *session);
