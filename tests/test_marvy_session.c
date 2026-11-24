#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "marvy_session.h"

static void start(marvy_session_t *session, uint64_t now_ms)
{
    marvy_session_init(session, 1, 7, now_ms);
    assert(session->now_ms == now_ms && session->last_input_ms == now_ms);
    assert(!session->dimmed);
    assert(marvy_session_press(session, now_ms) == MARVY_EVENT_START);
}

static void clear_lane(marvy_session_t *session)
{
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        session->game.sheep[i].hidden_ms = 100000;
    }
}

static void test_event_time_before_landing_and_feedback_boundaries(void)
{
    marvy_session_t session;
    start(&session, 0);
    clear_lane(&session);
    assert(marvy_session_press(&session, 100) == MARVY_EVENT_THROW);
    assert(marvy_session_press(&session, 699) == 0);
    assert(session.game.ring_state == MARVY_RING_FLYING);
    assert(session.game.ring_ms == MARVY_FLIGHT_MS - 1);
    assert(session.game.rings_left == 9);
    assert(marvy_session_advance(&session, 700) == MARVY_EVENT_MISS);
    assert(marvy_session_press(&session, 1099) == 0);
    assert(session.game.ring_state == MARVY_RING_FEEDBACK);
    assert(session.game.ring_ms == MARVY_FEEDBACK_MS - 1);
    assert(session.game.rings_left == 9);
    assert(marvy_session_advance(&session, 1100) == 0);
    assert(session.game.ring_state == MARVY_RING_READY);
    assert(marvy_session_press(&session, 1101) == MARVY_EVENT_THROW);
    assert(session.game.rings_left == 8);
}

static void test_last_feedback_press_does_not_restart_but_new_press_does(void)
{
    marvy_session_t session;
    start(&session, 0);
    clear_lane(&session);
    session.game.rings_left = 1;
    assert(marvy_session_press(&session, 100) == MARVY_EVENT_THROW);
    uint32_t events = marvy_session_press(&session, 1099);
    assert(events == MARVY_EVENT_MISS);
    assert(session.game.phase == MARVY_PHASE_PLAYING);
    assert(session.game.rings_left == 0);
    assert(marvy_session_advance(&session, 1100) == MARVY_EVENT_FINISH);
    assert(session.game.phase == MARVY_PHASE_RESULTS);
    assert(marvy_session_press(&session, 1101) == MARVY_EVENT_START);
    assert(session.game.phase == MARVY_PHASE_PLAYING);
    assert(session.game.rings_left == MARVY_RING_COUNT);
}

static void test_press_can_complete_a_landing_before_its_action(void)
{
    marvy_session_t session;
    start(&session, 0);
    clear_lane(&session);
    marvy_session_press(&session, 10);
    uint32_t events = marvy_session_press(&session, 1010);
    assert(events == (MARVY_EVENT_MISS | MARVY_EVENT_THROW));
    assert(session.game.ring_state == MARVY_RING_FLYING);
    assert(session.game.ring_ms == 0 && session.game.rings_left == 8);
}

static void test_idle_deadline_is_exact_and_sheep_pause(void)
{
    marvy_session_t session, reference;
    start(&session, 1000);
    reference = session;
    uint32_t events = marvy_session_advance(&session, 45999);
    assert((events & MARVY_SESSION_EVENT_DIM) == 0 && !session.dimmed);
    events = marvy_session_advance(&session, 46000);
    assert((events & MARVY_SESSION_EVENT_DIM) != 0 && session.dimmed);
    marvy_game_step(&reference.game, MARVY_SESSION_IDLE_MS);
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        assert(fabsf(reference.game.sheep[i].x - session.game.sheep[i].x) < 0.05f);
    }
    marvy_game_t paused = session.game;
    assert(marvy_session_advance(&session, 5000000) == 0);
    assert(session.now_ms == 5000000 && session.dimmed);
    assert(session.game.phase_ms == paused.phase_ms);
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        assert(session.game.sheep[i].x == paused.sheep[i].x);
    }
}

static void test_first_idle_press_wakes_without_throwing(void)
{
    marvy_session_t session;
    start(&session, 0);
    marvy_session_advance(&session, MARVY_SESSION_IDLE_MS);
    assert(session.dimmed);
    uint32_t events = marvy_session_press(&session, 60000);
    assert(events == MARVY_SESSION_EVENT_WAKE);
    assert(!session.dimmed && session.last_input_ms == 60000);
    assert(session.game.ring_state == MARVY_RING_READY);
    assert(session.game.rings_left == MARVY_RING_COUNT);
    assert(marvy_session_press(&session, 60001) == MARVY_EVENT_THROW);
    assert(session.game.rings_left == MARVY_RING_COUNT - 1);
}

static void test_a_press_at_idle_cutoff_only_wakes(void)
{
    marvy_session_t session;
    start(&session, 0);
    uint32_t events = marvy_session_press(&session, MARVY_SESSION_IDLE_MS);
    assert((events & MARVY_SESSION_EVENT_WAKE) != 0);
    assert((events & MARVY_EVENT_THROW) == 0);
    assert(!session.dimmed && session.game.rings_left == MARVY_RING_COUNT);
}

static void test_first_idle_press_does_not_restart_results(void)
{
    marvy_session_t session;
    start(&session, 0);
    clear_lane(&session);
    session.game.rings_left = 1;
    marvy_session_press(&session, 10);
    marvy_session_advance(&session, 1010);
    assert(session.game.phase == MARVY_PHASE_RESULTS);
    marvy_session_advance(&session, 50000);
    assert(session.dimmed);
    assert(marvy_session_press(&session, 50001) == MARVY_SESSION_EVENT_WAKE);
    assert(session.game.phase == MARVY_PHASE_RESULTS);
    assert(session.game.rings_left == 0);
    assert(marvy_session_press(&session, 50002) == MARVY_EVENT_START);
    assert(session.game.phase == MARVY_PHASE_PLAYING);
}

static void test_ignored_game_press_still_resets_idle_timer(void)
{
    marvy_session_t session;
    start(&session, 0);
    clear_lane(&session);
    marvy_session_press(&session, 44000);
    assert(marvy_session_press(&session, 44001) == 0);
    assert(session.last_input_ms == 44001);
    marvy_session_advance(&session, 89000);
    assert(!session.dimmed);
    assert((marvy_session_advance(&session, 89001) & MARVY_SESSION_EVENT_DIM) != 0);
}

static void test_clock_rollback_and_stale_press_are_ignored(void)
{
    marvy_session_t session;
    start(&session, 100);
    clear_lane(&session);
    marvy_session_advance(&session, 1000);
    marvy_game_t before = session.game;
    assert(marvy_session_advance(&session, 999) == 0);
    assert(marvy_session_press(&session, 999) == 0);
    assert(session.now_ms == 1000 && session.last_input_ms == 100);
    assert(session.game.phase_ms == before.phase_ms);
    assert(session.game.rings_left == before.rings_left);
    assert(marvy_session_press(&session, 1000) == MARVY_EVENT_THROW);
    assert(session.last_input_ms == 1000);
}

static void test_uint64_timestamps_do_not_wrap_or_overflow(void)
{
    marvy_session_t session;
    start(&session, UINT64_MAX - 40000);
    marvy_session_advance(&session, UINT64_MAX);
    assert(!session.dimmed && session.now_ms == UINT64_MAX);
    assert(marvy_session_press(&session, 0) == 0);
    assert(session.now_ms == UINT64_MAX);
    start(&session, UINT64_MAX - MARVY_SESSION_IDLE_MS);
    assert((marvy_session_advance(&session, UINT64_MAX) & MARVY_SESSION_EVENT_DIM) != 0);
    assert(session.dimmed);
    assert(marvy_session_press(&session, UINT64_MAX) == MARVY_SESSION_EVENT_WAKE);
    assert(!session.dimmed);
}

static void test_huge_frame_advances_only_to_idle_cutoff(void)
{
    marvy_session_t session;
    marvy_session_init(&session, 1, 7, 0);
    uint32_t events = marvy_session_advance(&session, UINT64_MAX);
    assert((events & MARVY_EVENT_START) != 0);
    assert((events & MARVY_SESSION_EVENT_DIM) != 0);
    assert(session.game.phase_ms == MARVY_SESSION_IDLE_MS - MARVY_INTRO_MS);
    assert(session.now_ms == UINT64_MAX && session.dimmed);
    assert(marvy_session_advance(&session, UINT64_MAX) == 0);
}

int main(void)
{
    test_event_time_before_landing_and_feedback_boundaries();
    test_last_feedback_press_does_not_restart_but_new_press_does();
    test_press_can_complete_a_landing_before_its_action();
    test_idle_deadline_is_exact_and_sheep_pause();
    test_first_idle_press_wakes_without_throwing();
    test_a_press_at_idle_cutoff_only_wakes();
    test_first_idle_press_does_not_restart_results();
    test_ignored_game_press_still_resets_idle_timer();
    test_clock_rollback_and_stale_press_are_ignored();
    test_uint64_timestamps_do_not_wrap_or_overflow();
    test_huge_frame_advances_only_to_idle_cutoff();
    puts("Marvy session tests: PASS");
    return 0;
}
