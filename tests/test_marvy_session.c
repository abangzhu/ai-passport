#include "marvy_session.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void clear_lane(marvy_session_t *s)
{
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) s->game.sheep[i].hidden_ms = UINT32_MAX;
}

static uint32_t click(marvy_session_t *s, marvy_button_t button, uint64_t at)
{
    uint32_t events = marvy_session_press_button(s, button, at);
    return events | marvy_session_release(s, at);
}

static void start(marvy_session_t *s, unsigned level, uint64_t at)
{
    marvy_session_init(s, 123, 17, at);
    marvy_progress_t p; marvy_progress_init(&p); p.unlocked_count = (uint8_t)(level + 1);
    marvy_game_apply_progress(&s->game, &p);
    assert(click(s, MARVY_BUTTON_OK, at) == 0 && s->game.phase == MARVY_PHASE_BRIEF);
    s->game.level_index = (uint8_t)level;
    assert(click(s, MARVY_BUTTON_OK, at) == MARVY_EVENT_START);
    assert(!s->release_throws && !s->button_held);
    clear_lane(s);
}

static void test_welcome_controls_and_brief_use_focus_then_confirm(void)
{
    marvy_session_t s; marvy_session_init(&s, 1, 9, 0);
    assert(marvy_session_advance(&s, 10000) == 0 && s.game.phase == MARVY_PHASE_INTRO);
    click(&s, MARVY_BUTTON_DOWN, 10001);
    assert(s.game.phase == MARVY_PHASE_INTRO && s.game.intro_selection == MARVY_INTRO_CONTROLS);
    click(&s, MARVY_BUTTON_UP, 10002);
    assert(s.game.intro_selection == MARVY_INTRO_START);
    click(&s, MARVY_BUTTON_UP, 10003);
    marvy_session_press_button(&s, MARVY_BUTTON_OK, 10004);
    assert(s.game.phase == MARVY_PHASE_CONTROLS && !s.release_throws);
    marvy_session_advance(&s, 20000);
    assert(s.game.stage_elapsed_ms == 0 && s.game.throws == 0);
    assert(marvy_session_release(&s, 20001) == 0 && s.game.phase == MARVY_PHASE_CONTROLS);
    click(&s, MARVY_BUTTON_DOWN, 20002); click(&s, MARVY_BUTTON_UP, 20003);
    assert(s.game.phase == MARVY_PHASE_CONTROLS);
    marvy_session_press_button(&s, MARVY_BUTTON_OK, 20004);
    assert(s.game.phase == MARVY_PHASE_INTRO && s.game.intro_selection == MARVY_INTRO_START);
    marvy_session_release(&s, 30000);
    assert(s.game.phase == MARVY_PHASE_INTRO && s.game.throws == 0);
    click(&s, MARVY_BUTTON_OK, 30001);
    assert(s.game.phase == MARVY_PHASE_BRIEF && s.game.stage_elapsed_ms == 0);
    marvy_progress_t p; marvy_progress_init(&p); p.unlocked_count = 6;
    marvy_game_apply_progress(&s.game, &p);
    click(&s, MARVY_BUTTON_UP, 30002); assert(s.game.level_index == 5);
    click(&s, MARVY_BUTTON_DOWN, 30003); assert(s.game.level_index == 0);
    assert(click(&s, MARVY_BUTTON_OK, 30004) == MARVY_EVENT_START);
    assert(s.game.phase == MARVY_PHASE_PLAYING && s.game.ring_state == MARVY_RING_READY);
    assert(s.game.throws == 0 && !s.release_throws);
}

static void test_timestamped_ok_charge_duplicates_and_stale_edges(void)
{
    marvy_session_t s; start(&s, 0, 100);
    marvy_session_press(&s, 200);
    assert(s.game.ring_state == MARVY_RING_CHARGING && s.release_throws);
    assert(marvy_session_press(&s, 500) == 0 && s.game.ring_ms == 300);
    marvy_session_advance(&s, 900);
    assert(marvy_session_release(&s, 899) == 0 && s.button_held && s.release_throws);
    assert(marvy_session_press(&s, 899) == 0);
    assert(marvy_session_advance(&s, 899) == 0 && s.now_ms == 900);
    assert(marvy_session_release(&s, 900) == MARVY_EVENT_THROW);
    assert(s.game.throws == 1 && s.game.ring_target_x == 120 && !s.release_throws);
    assert(marvy_session_release(&s, 901) == 0);
    marvy_session_cancel_input(&s); assert(s.game.ring_state == MARVY_RING_FLYING);
    assert(marvy_session_advance(&s, 1500) == MARVY_EVENT_MISS);
}

static void test_arrows_only_move_focus_and_do_not_freeze_or_extend_deadline(void)
{
    marvy_session_t s; start(&s, 3, 0);
    click(&s, MARVY_BUTTON_DOWN, 100);
    assert(s.game.play_selection == MARVY_PLAY_PAUSE && s.game.phase == MARVY_PHASE_PLAYING);
    assert(s.game.ring_state == MARVY_RING_READY && s.game.turn_ms == 100 && s.game.throws == 0);
    assert(marvy_session_advance(&s, 5000) == 0);
    click(&s, MARVY_BUTTON_UP, 5001);
    assert(s.game.play_selection == MARVY_PLAY_THROW && s.game.turn_ms == 5001);
    click(&s, MARVY_BUTTON_UP, 5999);
    assert(s.game.play_selection == MARVY_PLAY_PAUSE && !s.release_throws);
    assert(marvy_session_advance(&s, 6000) == MARVY_EVENT_THROW);
    assert(s.game.phase == MARVY_PHASE_PLAYING && s.game.throws == 1);
    click(&s, MARVY_BUTTON_OK, 6001);
    assert(s.game.phase == MARVY_PHASE_PAUSED && s.game.ring_ms == 1);
    click(&s, MARVY_BUTTON_OK, 6500);
    assert(s.game.phase == MARVY_PHASE_PLAYING && s.game.play_selection == MARVY_PLAY_THROW);
    assert(s.game.stage_elapsed_ms == 6001 && !s.release_throws);
}

static void test_pause_at_deadline_and_in_flight_freezes_all_clocks(void)
{
    marvy_session_t s; start(&s, 3, 0);
    click(&s, MARVY_BUTTON_DOWN, 0);
    marvy_session_press_button(&s, MARVY_BUTTON_OK, 5999);
    assert(s.game.phase == MARVY_PHASE_PAUSED && s.game.turn_ms == 5999);
    assert(s.game.stage_elapsed_ms == 5999 && !s.release_throws);
    marvy_session_release(&s, 6000);
    marvy_game_t frozen = s.game;
    assert(marvy_session_advance(&s, 10000) == 0 && memcmp(&frozen, &s.game, sizeof(frozen)) == 0);
    click(&s, MARVY_BUTTON_OK, 10001);
    assert(s.game.phase == MARVY_PHASE_PLAYING && s.game.throws == 0);
    assert(marvy_session_advance(&s, 10002) == MARVY_EVENT_THROW);
    assert(s.game.stage_elapsed_ms == 6000 && s.game.throws == 1);
    click(&s, MARVY_BUTTON_DOWN, 10102); click(&s, MARVY_BUTTON_OK, 10103);
    assert(s.game.phase == MARVY_PHASE_PAUSED && s.game.ring_ms == 101);
    marvy_session_advance(&s, 20000); assert(s.game.ring_ms == 101);
    click(&s, MARVY_BUTTON_OK, 20001);
    assert(marvy_session_advance(&s, 20500) == MARVY_EVENT_MISS);
    start(&s, 3, 0); click(&s, MARVY_BUTTON_DOWN, 0);
    assert(marvy_session_press_button(&s, MARVY_BUTTON_OK, 6000) == MARVY_EVENT_THROW);
    assert(s.game.phase == MARVY_PHASE_PAUSED && s.game.ring_state == MARVY_RING_FLYING);
}

static void test_ignored_ok_during_flight_or_feedback_cannot_release_a_later_throw(void)
{
    marvy_session_t s; start(&s, 0, 0);
    marvy_session_press(&s, 100); marvy_session_release(&s, 800);
    assert(s.game.throws == 1);
    marvy_session_press_button(&s, MARVY_BUTTON_OK, 900);
    assert(!s.release_throws && s.game.ring_state == MARVY_RING_FLYING);
    marvy_session_advance(&s, 1600);
    assert(s.game.ring_state == MARVY_RING_READY);
    assert(marvy_session_release(&s, 1700) == 0 && s.game.throws == 1);
    marvy_session_press(&s, 1800); marvy_session_release(&s, 1900);
    assert(s.game.throws == 2);
    marvy_session_press(&s, 2550);
    assert(s.game.ring_state == MARVY_RING_FEEDBACK && !s.release_throws);
    assert(marvy_session_release(&s, 3000) == 0 && s.game.throws == 2);
}

static void test_first_level_hold_has_no_timeout_until_stage_end(void)
{
    marvy_session_t s; start(&s, 0, 0); marvy_session_press(&s, 100);
    assert(marvy_session_advance(&s, 44999) == 0 && !s.dimmed && s.release_throws);
    assert(s.game.throws == 0 && s.game.ring_state == MARVY_RING_CHARGING);
    uint32_t events = marvy_session_advance(&s, 45000);
    assert(events & MARVY_EVENT_FINISH); assert(events & MARVY_EVENT_LEVEL_FAIL);
    assert(!(events & MARVY_SESSION_EVENT_DIM));
    assert(s.game.phase == MARVY_PHASE_RESULTS && s.last_input_ms == 45000);
    assert(marvy_session_release(&s, 45001) == 0 && !s.release_throws);
    assert(marvy_session_advance(&s, 90000) == 0);
    assert(marvy_session_advance(&s, 90001) == MARVY_SESSION_EVENT_DIM);
}

static void test_idle_wake_does_not_move_focus_or_confirm(void)
{
    marvy_session_t s; marvy_session_init(&s, 1, 9, 0);
    click(&s, MARVY_BUTTON_DOWN, 0); click(&s, MARVY_BUTTON_OK, 1);
    assert(s.game.phase == MARVY_PHASE_CONTROLS);
    assert(marvy_session_advance(&s, 45001) == MARVY_SESSION_EVENT_DIM);
    assert(marvy_session_press_button(&s, MARVY_BUTTON_OK, 50000) == MARVY_SESSION_EVENT_WAKE);
    marvy_session_release(&s, 50001); assert(s.game.phase == MARVY_PHASE_CONTROLS);
    click(&s, MARVY_BUTTON_OK, 50002);
    assert(s.game.phase == MARVY_PHASE_INTRO && s.game.intro_selection == MARVY_INTRO_START);
    assert(marvy_session_advance(&s, 95002) == MARVY_SESSION_EVENT_DIM);
    assert(marvy_session_press_button(&s, MARVY_BUTTON_UP, 95003) == MARVY_SESSION_EVENT_WAKE);
    marvy_session_release(&s, 95004); assert(s.game.intro_selection == MARVY_INTRO_START);
    click(&s, MARVY_BUTTON_OK, 95005);
    assert(marvy_session_advance(&s, 140005) == MARVY_SESSION_EVENT_DIM);
    assert(marvy_session_press_button(&s, MARVY_BUTTON_OK, 140006) == MARVY_SESSION_EVENT_WAKE);
    marvy_session_release(&s, 140007); assert(s.game.phase == MARVY_PHASE_BRIEF);
    start(&s, 0, 0);
    click(&s, MARVY_BUTTON_DOWN, 1000); click(&s, MARVY_BUTTON_OK, 1001);
    click(&s, MARVY_BUTTON_DOWN, 1100); assert(s.game.pause_selection == MARVY_PAUSE_EXIT);
    assert(marvy_session_advance(&s, 46100) == MARVY_SESSION_EVENT_DIM);
    assert(marvy_session_press_button(&s, MARVY_BUTTON_OK, 50000) == MARVY_SESSION_EVENT_WAKE);
    marvy_session_release(&s, 50001); assert(s.game.phase == MARVY_PHASE_PAUSED);
    click(&s, MARVY_BUTTON_OK, 50002); assert(s.game.phase == MARVY_PHASE_INTRO);
    assert(s.game.best_score == 17 && s.game.throws == 0);
}

static void test_finish_press_is_consumed_before_result_navigation(void)
{
    marvy_session_t s; start(&s, 0, 0);
    s.game.stage_elapsed_ms = 44999; s.game.captures = 4; s.game.throws = 4; s.game.score = 4;
    uint32_t events = marvy_session_press_button(&s, MARVY_BUTTON_DOWN, 1);
    assert(events & MARVY_EVENT_FINISH);
    assert(s.game.phase == MARVY_PHASE_RESULTS && s.game.result_selection == MARVY_RESULT_NEXT);
    assert(marvy_session_release(&s, 2) == 0 && s.game.phase == MARVY_PHASE_RESULTS);
    click(&s, MARVY_BUTTON_DOWN, 3); assert(s.game.result_selection == MARVY_RESULT_RETRY);
    click(&s, MARVY_BUTTON_OK, 5);
    assert(s.game.phase == MARVY_PHASE_BRIEF && s.game.level_index == 0 && s.game.throws == 0);
    click(&s, MARVY_BUTTON_DOWN, 7); assert(s.game.level_index == 1);
    click(&s, MARVY_BUTTON_UP, 9); assert(s.game.level_index == 0);
}

static void test_cancel_invalid_and_uint64_boundaries(void)
{
    marvy_session_t s; start(&s, 3, 0);
    assert(marvy_session_press_button(&s, (marvy_button_t)99, 100) == 0 && s.now_ms == 0);
    assert(marvy_session_press_button(&s, (marvy_button_t)-1, 100) == 0);
    marvy_session_press(&s, 100); marvy_session_advance(&s, 800);
    marvy_session_cancel_input(&s);
    assert(!s.button_held && !s.release_throws && s.game.ring_state == MARVY_RING_READY);
    assert(s.game.turn_ms == 800 && marvy_session_release(&s, 801) == 0);
    assert(marvy_session_advance(&s, UINT64_MAX) & MARVY_EVENT_FINISH);
    assert(s.now_ms == UINT64_MAX && s.last_input_ms == UINT64_MAX && !s.dimmed);
    assert(marvy_session_press(&s, 0) == 0 && marvy_session_release(&s, 0) == 0);
    start(&s, 0, UINT64_MAX - 40000); marvy_session_advance(&s, UINT64_MAX);
    assert(s.game.phase == MARVY_PHASE_PLAYING && !s.dimmed && s.game.stage_elapsed_ms == 40000);
    marvy_session_init(&s, 1, 0, 0);
    click(&s, MARVY_BUTTON_DOWN, 0); click(&s, MARVY_BUTTON_OK, 0);
    assert(marvy_session_advance(&s, UINT64_MAX) == MARVY_SESSION_EVENT_DIM);
    assert(s.now_ms == UINT64_MAX && s.game.phase == MARVY_PHASE_CONTROLS && s.game.stage_elapsed_ms == 0);
}

int main(void)
{
    test_welcome_controls_and_brief_use_focus_then_confirm();
    test_timestamped_ok_charge_duplicates_and_stale_edges();
    test_arrows_only_move_focus_and_do_not_freeze_or_extend_deadline();
    test_pause_at_deadline_and_in_flight_freezes_all_clocks();
    test_ignored_ok_during_flight_or_feedback_cannot_release_a_later_throw();
    test_first_level_hold_has_no_timeout_until_stage_end();
    test_idle_wake_does_not_move_focus_or_confirm();
    test_finish_press_is_consumed_before_result_navigation();
    test_cancel_invalid_and_uint64_boundaries();
    puts("Marvy unified controls, timestamp, pause and release session tests: PASS");
    return 0;
}
