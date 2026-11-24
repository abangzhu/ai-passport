#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "marvy_session.h"

static void start(marvy_session_t *session, uint64_t now_ms)
{
    marvy_session_init(session, 1, 7, now_ms);
    assert(marvy_session_press(session, now_ms) == MARVY_EVENT_START);
    assert(marvy_session_release(session, now_ms) == 0);
    assert(!session->button_held && !session->dimmed);
}

static void clear_lane(marvy_session_t *session)
{
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) session->game.sheep[i].hidden_ms = 100000;
}

static void test_welcome_requires_press_and_release_cannot_throw(void)
{
    marvy_session_t s;
    marvy_session_init(&s, 1, 7, 0);
    assert(marvy_session_advance(&s, 44000) == 0);
    assert(s.game.phase == MARVY_PHASE_INTRO);
    assert(marvy_session_release(&s, 44000) == 0 && s.game.phase == MARVY_PHASE_INTRO);
    assert(marvy_session_press(&s, 44001) == MARVY_EVENT_START);
    assert(s.button_held && s.game.phase == MARVY_PHASE_PLAYING);
    clear_lane(&s);
    assert(marvy_session_press(&s, 44002) == 0);
    assert(marvy_session_release(&s, 44003) == 0);
    assert(s.game.rings_left == 10 && s.game.ring_state == MARVY_RING_READY);
}

static void test_press_release_timestamps_control_distance(void)
{
    marvy_session_t s;
    start(&s, 0); clear_lane(&s);
    assert(marvy_session_press(&s, 100) == 0);
    assert(s.game.ring_state == MARVY_RING_CHARGING && s.game.rings_left == 10);
    assert(marvy_session_release(&s, 800) == MARVY_EVENT_THROW);
    assert(s.game.ring_target_x == 120 && s.game.rings_left == 9 && !s.button_held);
    assert(marvy_session_release(&s, 801) == 0);
    assert(marvy_session_advance(&s, 1399) == 0);
    assert(s.game.ring_state == MARVY_RING_FLYING && s.game.ring_ms == 599);
    assert(marvy_session_advance(&s, 1400) == MARVY_EVENT_MISS);
    assert(marvy_session_press(&s, 1799) == 0);
    assert(s.game.ring_state == MARVY_RING_FEEDBACK);
    assert(marvy_session_release(&s, 1801) == 0);
    assert(s.game.ring_state == MARVY_RING_READY && s.game.rings_left == 9);
    assert(marvy_session_press(&s, 1802) == 0);
    assert(marvy_session_release(&s, 1802) == MARVY_EVENT_THROW);
    assert(s.game.ring_target_x == 32 && s.game.rings_left == 8);
}

static void test_repeated_press_does_not_restart_charge(void)
{
    marvy_session_t s;
    start(&s, 0); clear_lane(&s);
    marvy_session_press(&s, 10);
    assert(marvy_session_press(&s, 710) == 0);
    assert(s.game.ring_ms == 700 && s.last_input_ms == 10);
    assert(marvy_session_release(&s, 1410) == MARVY_EVENT_THROW);
    assert(s.game.ring_target_x == 208 && s.game.rings_left == 9);
}

static void test_last_feedback_input_does_not_restart_or_accidentally_throw(void)
{
    marvy_session_t s;
    start(&s, 0); clear_lane(&s); s.game.rings_left = 1;
    marvy_session_press(&s, 100);
    assert(marvy_session_release(&s, 100) == MARVY_EVENT_THROW);
    assert(marvy_session_press(&s, 1099) == MARVY_EVENT_MISS);
    assert(s.game.phase == MARVY_PHASE_PLAYING && s.game.rings_left == 0);
    assert(marvy_session_release(&s, 1100) == MARVY_EVENT_FINISH);
    assert(s.game.phase == MARVY_PHASE_RESULTS);
    assert(marvy_session_press(&s, 1101) == MARVY_EVENT_START);
    assert(marvy_session_release(&s, 1200) == 0);
    assert(s.game.phase == MARVY_PHASE_PLAYING && s.game.rings_left == 10);
    assert(s.game.ring_state == MARVY_RING_READY);
}

static void test_last_marvy_greeting_keeps_full_duration(void)
{
    marvy_session_t s;
    start(&s, 0); clear_lane(&s);
    s.game.best_score = 0; s.game.rings_left = 1;
    marvy_session_press(&s, 100);
    s.game.sheep[0].x = 32; s.game.sheep[0].speed = 0;
    s.game.sheep[0].marvy = true; s.game.sheep[0].announced = true;
    s.game.sheep[0].hidden_ms = 0;
    assert(marvy_session_release(&s, 100) == MARVY_EVENT_THROW);
    assert(marvy_session_press(&s, 1899) == (MARVY_EVENT_HIT | MARVY_EVENT_MARVY_HIT));
    assert(s.game.phase == MARVY_PHASE_PLAYING && s.game.ring_ms == 1199);
    assert(s.game.score == 5 && s.game.sheep[0].hidden_ms == 501);
    assert(marvy_session_release(&s, 1900) == (MARVY_EVENT_FINISH | MARVY_EVENT_NEW_BEST));
    assert(s.game.phase == MARVY_PHASE_RESULTS && s.game.best_score == 5);
    assert(marvy_session_press(&s, 1901) == MARVY_EVENT_START);
    assert(marvy_session_release(&s, 2000) == 0 && s.game.rings_left == 10);
}

static void test_idle_cancels_charge_and_release_cannot_wake_or_throw(void)
{
    marvy_session_t s;
    start(&s, 0); clear_lane(&s);
    marvy_session_press(&s, 100);
    marvy_session_advance(&s, 45099);
    assert(!s.dimmed && s.button_held && s.game.ring_state == MARVY_RING_CHARGING);
    assert(marvy_session_advance(&s, 45100) == MARVY_SESSION_EVENT_DIM);
    assert(s.dimmed && !s.button_held && s.game.ring_state == MARVY_RING_READY);
    assert(s.game.rings_left == 10);
    assert(marvy_session_release(&s, 45101) == 0 && s.dimmed);
    marvy_game_t paused=s.game;
    assert(marvy_session_advance(&s, 5000000) == 0);
    assert(s.game.phase_ms == paused.phase_ms);
    for (unsigned i=0;i<MARVY_SHEEP_COUNT;++i) assert(s.game.sheep[i].x == paused.sheep[i].x);
    assert(marvy_session_press(&s, 5000001) == MARVY_SESSION_EVENT_WAKE);
    assert(!s.dimmed && s.button_held);
    assert(marvy_session_release(&s, 5000100) == 0);
    assert(s.game.rings_left == 10 && s.game.ring_state == MARVY_RING_READY);
    marvy_session_press(&s, 5000200);
    assert(marvy_session_release(&s, 5000900) == MARVY_EVENT_THROW);
    assert(s.game.ring_target_x == 120 && s.game.rings_left == 9);
}

static void test_cancel_input_is_fail_closed_without_canceling_flight(void)
{
    marvy_session_t s;
    start(&s, 0); clear_lane(&s);
    marvy_session_press(&s, 10); marvy_session_advance(&s, 710);
    marvy_session_cancel_input(&s);
    assert(!s.button_held && s.game.ring_state == MARVY_RING_READY && s.game.rings_left == 10);
    assert(marvy_session_release(&s, 711) == 0);
    marvy_session_press(&s, 800);
    assert(marvy_session_release(&s, 800) == MARVY_EVENT_THROW);
    marvy_session_cancel_input(&s);
    assert(s.game.ring_state == MARVY_RING_FLYING && s.game.rings_left == 9);
    assert(marvy_session_advance(&s, 1400) == MARVY_EVENT_MISS);
}

static void test_wake_in_welcome_or_results_never_starts_on_release(void)
{
    marvy_session_t s;
    marvy_session_init(&s,1,7,0);
    assert(marvy_session_advance(&s, 50000) == MARVY_SESSION_EVENT_DIM);
    assert(s.game.phase == MARVY_PHASE_INTRO);
    assert(marvy_session_press(&s,50001)==MARVY_SESSION_EVENT_WAKE);
    assert(marvy_session_release(&s,50002)==0 && s.game.phase==MARVY_PHASE_INTRO);
    assert(marvy_session_press(&s,50003)==MARVY_EVENT_START);
    marvy_session_release(&s,50004); clear_lane(&s); s.game.rings_left=1;
    marvy_session_press(&s,50010); marvy_session_release(&s,50010);
    marvy_session_advance(&s,51010);
    assert(s.game.phase==MARVY_PHASE_RESULTS);
    marvy_session_advance(&s,100000);
    assert(s.dimmed);
    assert(marvy_session_press(&s,100001)==MARVY_SESSION_EVENT_WAKE);
    assert(marvy_session_release(&s,100002)==0 && s.game.phase==MARVY_PHASE_RESULTS);
}

static void test_stale_events_cannot_release_or_change_a_charge(void)
{
    marvy_session_t s;
    start(&s,100); clear_lane(&s);
    marvy_session_press(&s,200); marvy_session_advance(&s,900);
    assert(marvy_session_release(&s,899)==0 && s.button_held);
    assert(marvy_session_press(&s,899)==0 && s.game.ring_ms==700);
    assert(marvy_session_advance(&s,899)==0 && s.now_ms==900);
    assert(marvy_session_release(&s,900)==MARVY_EVENT_THROW);
    assert(s.game.ring_target_x==120);
}

static void test_uint64_timestamps_and_huge_frames_do_not_wrap(void)
{
    marvy_session_t s;
    start(&s,UINT64_MAX-40000);
    marvy_session_advance(&s,UINT64_MAX);
    assert(!s.dimmed && s.now_ms==UINT64_MAX);
    assert(marvy_session_press(&s,0)==0 && marvy_session_release(&s,0)==0);
    start(&s,UINT64_MAX-MARVY_SESSION_IDLE_MS);
    assert((marvy_session_advance(&s,UINT64_MAX)&MARVY_SESSION_EVENT_DIM)!=0);
    assert(marvy_session_press(&s,UINT64_MAX)==MARVY_SESSION_EVENT_WAKE);
    assert(marvy_session_release(&s,UINT64_MAX)==0);
    marvy_session_init(&s,1,7,0);
    assert(marvy_session_advance(&s,UINT64_MAX)==MARVY_SESSION_EVENT_DIM);
    assert(s.game.phase==MARVY_PHASE_INTRO && s.now_ms==UINT64_MAX);
}

int main(void)
{
    test_welcome_requires_press_and_release_cannot_throw();
    test_press_release_timestamps_control_distance();
    test_repeated_press_does_not_restart_charge();
    test_last_feedback_input_does_not_restart_or_accidentally_throw();
    test_last_marvy_greeting_keeps_full_duration();
    test_idle_cancels_charge_and_release_cannot_wake_or_throw();
    test_cancel_input_is_fail_closed_without_canceling_flight();
    test_wake_in_welcome_or_results_never_starts_on_release();
    test_stale_events_cannot_release_or_change_a_charge();
    test_uint64_timestamps_and_huge_frames_do_not_wrap();
    puts("Marvy charge input/session tests: PASS");
    return 0;
}
