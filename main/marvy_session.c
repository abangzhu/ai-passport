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
        if (session->game.phase == MARVY_PHASE_PLAYING) {
            /* The finite stage and its final airborne ring must settle even
             * without input. Chunk before narrowing an absolute time delta. */
            uint64_t remaining = target_ms - session->now_ms;
            while (remaining > 0 && session->game.phase == MARVY_PHASE_PLAYING) {
                uint32_t elapsed = remaining > MARVY_MAX_STEP_MS
                                   ? MARVY_MAX_STEP_MS : (uint32_t)remaining;
                events |= marvy_game_step(&session->game, elapsed);
                remaining -= elapsed;
            }
            if (session->game.phase == MARVY_PHASE_RESULTS) {
                /* Results gets a fresh idle interval from the time this frame
                 * observes completion, even after a very long suspended frame. */
                session->last_input_ms = target_ms;
            }
        } else {
            uint64_t active_until = target_ms;
            /* last_input_ms <= now_ms < target_ms, so subtraction cannot wrap.
             * Only add the deadline after proving it lies before target_ms. */
            bool idle = target_ms - session->last_input_ms >= MARVY_SESSION_IDLE_MS;
            if (idle) active_until = session->last_input_ms + MARVY_SESSION_IDLE_MS;
            uint32_t elapsed = (uint32_t)(active_until - session->now_ms);
            events = marvy_game_step(&session->game, elapsed);
            if (idle) {
                marvy_session_cancel_input(session);
                session->dimmed = true;
                events |= MARVY_SESSION_EVENT_DIM;
            }
        }
    }
    session->now_ms = target_ms;
    return events;
}

uint32_t marvy_session_press(marvy_session_t *session, uint64_t pressed_at_ms)
{
    return marvy_session_press_button(session, MARVY_BUTTON_UP, pressed_at_ms);
}

uint32_t marvy_session_press_button(marvy_session_t *session, marvy_button_t button,
                                   uint64_t pressed_at_ms)
{
    if ((unsigned)button >= MARVY_BUTTON_COUNT || pressed_at_ms < session->now_ms) {
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
    /* An input captured at the finishing boundary belongs to the old stage.
     * Consume its full gesture before accepting results-menu navigation. */
    if (events & MARVY_EVENT_FINISH) return events;
    if (session->game.phase == MARVY_PHASE_BRIEF) {
        if (button == MARVY_BUTTON_OK) events |= marvy_game_press(&session->game);
        else marvy_game_brief_move(&session->game, button == MARVY_BUTTON_UP ? -1 : 1);
        return events;
    }
    if (session->game.phase == MARVY_PHASE_RESULTS) {
        if (button == MARVY_BUTTON_OK) events |= marvy_game_result_confirm(&session->game);
        else marvy_game_result_move(&session->game, button == MARVY_BUTTON_UP ? -1 : 1);
        return events;
    }
    if (session->game.phase == MARVY_PHASE_PAUSED) {
        if (button == MARVY_BUTTON_OK) marvy_game_pause_confirm(&session->game);
        else marvy_game_pause_select(&session->game, button == MARVY_BUTTON_UP
                                     ? MARVY_PAUSE_CONTINUE : MARVY_PAUSE_EXIT);
    } else if (session->game.phase == MARVY_PHASE_PLAYING && button == MARVY_BUTTON_OK) {
        marvy_game_pause(&session->game);
    } else {
        events |= marvy_game_press(&session->game);
    }
    return events;
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
