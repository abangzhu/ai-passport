#include "marvy_session.h"

void marvy_session_init(marvy_session_t *session, uint32_t seed, uint16_t best,
                        uint64_t now_ms)
{
    marvy_game_init(&session->game, seed, best);
    session->now_ms = now_ms;
    session->last_input_ms = now_ms;
    session->dimmed = false;
    session->button_held = false;
}

uint32_t marvy_session_advance(marvy_session_t *session, uint64_t target_ms)
{
    if (target_ms <= session->now_ms) {
        return 0;
    }
    uint32_t events = 0;
    if (!session->dimmed) {
        uint64_t active_until = target_ms;
        /* last_input_ms <= now_ms < target_ms, so subtraction cannot wrap.
         * Add the deadline only after proving it lies at/before target_ms. */
        bool idle = target_ms - session->last_input_ms >= MARVY_SESSION_IDLE_MS;
        if (idle) {
            active_until = session->last_input_ms + MARVY_SESSION_IDLE_MS;
        }
        /* Active time is bounded by the idle interval and fits in uint32_t. */
        uint32_t elapsed = (uint32_t)(active_until - session->now_ms);
        events = marvy_game_step(&session->game, elapsed);
        if (idle) {
            marvy_session_cancel_input(session);
            session->dimmed = true;
            events |= MARVY_SESSION_EVENT_DIM;
        }
    }
    session->now_ms = target_ms;
    return events;
}

uint32_t marvy_session_press(marvy_session_t *session, uint64_t pressed_at_ms)
{
    if (pressed_at_ms < session->now_ms) {
        return 0;
    }
    uint32_t events = marvy_session_advance(session, pressed_at_ms);
    if (session->button_held) return events;
    session->button_held = true;
    session->last_input_ms = pressed_at_ms;
    if (session->dimmed) {
        session->dimmed = false;
        return events | MARVY_SESSION_EVENT_WAKE;
    }
    return events | marvy_game_press(&session->game);
}

uint32_t marvy_session_release(marvy_session_t *session, uint64_t released_at_ms)
{
    if (released_at_ms < session->now_ms) return 0;
    uint32_t events = marvy_session_advance(session, released_at_ms);
    if (!session->button_held) return events;
    session->button_held = false;
    session->last_input_ms = released_at_ms;
    return events | marvy_game_release(&session->game);
}

void marvy_session_cancel_input(marvy_session_t *session)
{
    session->button_held = false;
    marvy_game_cancel_charge(&session->game);
}
