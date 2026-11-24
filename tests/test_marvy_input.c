#include "marvy_input.h"
#include <assert.h>
#include <stdio.h>

static uint32_t send(marvy_input_t *input, marvy_session_t *session,
                      unsigned button, bool pressed, uint64_t at_ms)
{
    const marvy_input_event_t event = {button, pressed, at_ms};
    return marvy_input_handle(input, session, &event);
}

int main(void)
{
    marvy_input_t input;
    marvy_session_t session;
    marvy_input_init(&input);
    marvy_session_init(&session, 1, 0, 0);
    assert(send(&input, &session, 0, true, 10) & MARVY_EVENT_START);
    assert(send(&input, &session, 0, true, 11) == 0);
    assert(send(&input, &session, 1, false, 12) == 0);
    assert(input.held_button == 0);
    assert((send(&input, &session, 0, false, 20) & MARVY_EVENT_THROW) == 0);
    assert(session.game.rings_left == 10 && session.game.ring_state == MARVY_RING_READY);

    send(&input, &session, 2, true, 50);
    assert(session.game.ring_state == MARVY_RING_CHARGING);
    assert(send(&input, &session, 0, true, 60) == 0);
    assert(send(&input, &session, 0, false, 70) == 0);
    assert(send(&input, &session, 2, false, 750) & MARVY_EVENT_THROW);
    assert(session.game.rings_left == 9);
    assert(session.game.ring_target_x == marvy_game_charge_target_x(700));
    assert(send(&input, &session, 2, false, 751) == 0);

    marvy_session_init(&session, 1, 0, 0);
    marvy_input_init(&input);
    send(&input, &session, 0, true, 0);
    send(&input, &session, 0, false, 1);
    send(&input, &session, 1, true, 10);
    marvy_session_advance(&session, 200);
    marvy_input_cancel(&input, &session);
    assert(input.held_button == -1 && !session.button_held);
    assert(session.game.ring_state == MARVY_RING_READY && session.game.rings_left == 10);
    assert(send(&input, &session, 1, false, 201) == 0);
    send(&input, &session, 2, true, 210);
    assert(session.game.ring_state == MARVY_RING_CHARGING);
    marvy_session_advance(&session, 300);
    assert(send(&input, &session, 2, false, 299) == 0);
    assert(session.game.ring_state == MARVY_RING_READY && session.game.rings_left == 10);
    assert(!session.button_held && input.held_button == -1);
    assert(send(&input, &session, 9, true, 301) == 0);
    assert(send(&input, &session, 0, false, 302) == 0);
    assert(send(&input, &session, 0, true, 299) == 0);
    assert(input.held_button == -1);

    send(&input, &session, 1, true, 400);
    assert(marvy_session_advance(&session, 45400) & MARVY_SESSION_EVENT_DIM);
    marvy_input_cancel(&input, &session);
    assert(send(&input, &session, 1, false, 45401) == 0);
    assert(send(&input, &session, 2, true, 45402) & MARVY_SESSION_EVENT_WAKE);
    assert((send(&input, &session, 2, false, 45403) & MARVY_EVENT_THROW) == 0);
    assert(session.game.ring_state == MARVY_RING_READY && session.game.rings_left == 10);
    send(&input, &session, 0, true, 45500);
    assert(send(&input, &session, 0, false, 45501) & MARVY_EVENT_THROW);
    assert(session.game.rings_left == 9);
    puts("Marvy input ownership, stale release and cancellation tests: PASS");
    return 0;
}
