#pragma once

#include "marvy_session.h"

enum { MARVY_INPUT_BUTTON_COUNT = MARVY_BUTTON_COUNT };

typedef struct {
    uint8_t button;
    bool pressed;
    uint64_t at_ms;
} marvy_input_event_t;

typedef struct {
    int8_t held_button;
} marvy_input_t;

/* Per the latest user device test, raw slots 0/1/2 are physical #1/#2/#3:
 * right-upper OK, right-middle UP, right-lower DOWN. Apply to both edges before
 * queueing; event.button and held_button always contain logical actions.
 * Invalid slots or a null output return false without modifying the output. */
bool marvy_input_from_board(uint8_t board_button, bool pressed, uint64_t at_ms,
                            marvy_input_event_t *event);

/* Pure adapter for timestamped board events. Only the button that began a
 * gesture can end it; overflow/stale release cancellation never spends a ring. */
void marvy_input_init(marvy_input_t *input);
void marvy_input_cancel(marvy_input_t *input, marvy_session_t *session);
uint32_t marvy_input_handle(marvy_input_t *input, marvy_session_t *session,
                            const marvy_input_event_t *event);
