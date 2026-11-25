#include "marvy_input.h"

void marvy_input_init(marvy_input_t *input)
{
    input->held_button = -1;
}

void marvy_input_cancel(marvy_input_t *input, marvy_session_t *session)
{
    input->held_button = -1;
    marvy_session_cancel_input(session);
}

uint32_t marvy_input_handle(marvy_input_t *input, marvy_session_t *session,
                            const marvy_input_event_t *event)
{
    if (event->button >= MARVY_INPUT_BUTTON_COUNT) return 0;
    if (event->at_ms < session->now_ms) {
        /* A stale end of the current gesture cannot spend a ring later or
         * leave the session believing a released button is still held. */
        if (!event->pressed && input->held_button == event->button) {
            marvy_input_cancel(input, session);
        }
        return 0;
    }
    if (event->pressed) {
        if (input->held_button >= 0) return 0;
        input->held_button = (int8_t)event->button;
        return marvy_session_press_button(session, (marvy_button_t)event->button, event->at_ms);
    }
    if (input->held_button != event->button) return 0;
    input->held_button = -1;
    return marvy_session_release(session, event->at_ms);
}
