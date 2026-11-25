#include "marvy_input.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static uint32_t send(marvy_input_t *input, marvy_session_t *s,
                     unsigned button, bool pressed, uint64_t at)
{
    const marvy_input_event_t event = {(uint8_t)button, pressed, at};
    return marvy_input_handle(input, s, &event);
}

static uint32_t click(marvy_input_t *input, marvy_session_t *s,
                       unsigned button, uint64_t at)
{
    uint32_t events = send(input, s, button, true, at);
    return events | send(input, s, button, false, at);
}

static void start(marvy_input_t *input, marvy_session_t *s, unsigned level)
{
    marvy_input_init(input); marvy_session_init(s, 123, 17, 0);
    marvy_progress_t progress; marvy_progress_init(&progress);
    progress.unlocked_count = (uint8_t)(level + 1); marvy_game_apply_progress(&s->game, &progress);
    assert(click(input, s, MARVY_BUTTON_OK, 0) == 0 && s->game.phase == MARVY_PHASE_BRIEF);
    s->game.level_index = (uint8_t)level;
    assert(click(input, s, MARVY_BUTTON_OK, 0) == MARVY_EVENT_START);
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) s->game.sheep[i].hidden_ms = UINT32_MAX;
}

static void test_welcome_help_and_right_key_charge_ownership(void)
{
    marvy_input_t input; marvy_session_t s;
    marvy_input_init(&input); marvy_session_init(&s, 1, 0, 0);
    assert(send(&input, &s, 99, true, 0) == 0 && input.held_button == -1);
    assert(send(&input, &s, MARVY_BUTTON_UP, false, 0) == 0);
    send(&input, &s, MARVY_BUTTON_DOWN, true, 10);
    assert(s.game.phase == MARVY_PHASE_INTRO && s.game.intro_selection == MARVY_INTRO_CONTROLS);
    send(&input, &s, MARVY_BUTTON_OK, true, 11); send(&input, &s, MARVY_BUTTON_UP, false, 12);
    assert(s.game.phase == MARVY_PHASE_INTRO && input.held_button == MARVY_BUTTON_DOWN);
    send(&input, &s, MARVY_BUTTON_DOWN, false, 13);
    send(&input, &s, MARVY_BUTTON_OK, true, 20);
    assert(s.game.phase == MARVY_PHASE_CONTROLS && !s.release_throws);
    send(&input, &s, MARVY_BUTTON_OK, true, 21); send(&input, &s, MARVY_BUTTON_DOWN, false, 22);
    assert(input.held_button == MARVY_BUTTON_OK && s.game.phase == MARVY_PHASE_CONTROLS);
    send(&input, &s, MARVY_BUTTON_OK, false, 23);
    click(&input, &s, MARVY_BUTTON_UP, 30); assert(s.game.phase == MARVY_PHASE_CONTROLS);
    send(&input, &s, MARVY_BUTTON_OK, true, 40);
    assert(s.game.phase == MARVY_PHASE_INTRO && s.game.intro_selection == MARVY_INTRO_START);
    send(&input, &s, MARVY_BUTTON_OK, true, 44); send(&input, &s, MARVY_BUTTON_OK, false, 50);
    assert(s.game.phase == MARVY_PHASE_INTRO);
    click(&input, &s, MARVY_BUTTON_OK, 60); assert(s.game.phase == MARVY_PHASE_BRIEF);
    click(&input, &s, MARVY_BUTTON_DOWN, 70); assert(s.game.phase == MARVY_PHASE_BRIEF);
    assert(click(&input, &s, MARVY_BUTTON_OK, 80) == MARVY_EVENT_START);
    assert(s.game.throws == 0 && !s.release_throws);
    send(&input, &s, MARVY_BUTTON_DOWN, true, 100);
    send(&input, &s, MARVY_BUTTON_OK, true, 110); send(&input, &s, MARVY_BUTTON_OK, false, 111);
    assert(s.game.phase == MARVY_PHASE_PLAYING && input.held_button == MARVY_BUTTON_DOWN);
    assert(s.game.play_selection == MARVY_PLAY_PAUSE);
    send(&input, &s, MARVY_BUTTON_DOWN, false, 120);
    click(&input, &s, MARVY_BUTTON_UP, 130); assert(s.game.play_selection == MARVY_PLAY_THROW);
    send(&input, &s, MARVY_BUTTON_OK, true, 200);
    assert(s.game.ring_state == MARVY_RING_CHARGING && s.release_throws);
    send(&input, &s, MARVY_BUTTON_DOWN, true, 210); send(&input, &s, MARVY_BUTTON_DOWN, false, 211);
    assert(input.held_button == MARVY_BUTTON_OK && s.game.play_selection == MARVY_PLAY_THROW);
    assert(send(&input, &s, MARVY_BUTTON_OK, false, 900) & MARVY_EVENT_THROW);
    assert(s.game.throws == 1 && s.game.ring_target_x == 120 && !s.release_throws);
    assert(send(&input, &s, MARVY_BUTTON_OK, false, 901) == 0);
}

static void test_timeout_owned_release_never_throws_again(void)
{
    marvy_input_t input; marvy_session_t s; start(&input, &s, 3);
    send(&input, &s, MARVY_BUTTON_OK, true, 100);
    assert(marvy_session_advance(&s, 6000) == MARVY_EVENT_THROW);
    assert(s.button_held && input.held_button == MARVY_BUTTON_OK && !s.release_throws);
    send(&input, &s, MARVY_BUTTON_DOWN, true, 6001);
    send(&input, &s, MARVY_BUTTON_DOWN, false, 6002);
    marvy_session_advance(&s, 6800); assert(s.game.ring_state == MARVY_RING_READY);
    assert(send(&input, &s, MARVY_BUTTON_OK, false, 6801) == 0);
    assert(!s.button_held && input.held_button == -1 && s.game.throws == 1);
    send(&input, &s, MARVY_BUTTON_OK, true, 6802);
    assert(send(&input, &s, MARVY_BUTTON_OK, false, 6902) == MARVY_EVENT_THROW);
    assert(s.game.throws == 2);
}

static void test_overflow_and_stale_cancel_clear_throw_permission_without_extending_time(void)
{
    marvy_input_t input; marvy_session_t s; start(&input, &s, 3);
    send(&input, &s, MARVY_BUTTON_OK, true, 100); marvy_session_advance(&s, 800);
    marvy_input_cancel(&input, &s);
    assert(input.held_button == -1 && !s.button_held && !s.release_throws);
    assert(s.game.ring_state == MARVY_RING_READY && s.game.throws == 0);
    assert(s.game.stage_elapsed_ms == 800 && s.game.turn_ms == 800);
    assert(send(&input, &s, MARVY_BUTTON_OK, false, 801) == 0);
    send(&input, &s, MARVY_BUTTON_OK, true, 900); marvy_session_advance(&s, 1000);
    assert(send(&input, &s, MARVY_BUTTON_DOWN, false, 999) == 0 && s.button_held);
    assert(send(&input, &s, MARVY_BUTTON_OK, false, 999) == 0);
    assert(input.held_button == -1 && !s.button_held && !s.release_throws);
    assert(s.game.ring_state == MARVY_RING_READY && s.game.stage_elapsed_ms == 1000);
    assert(s.game.turn_ms == 1000);
    assert(send(&input, &s, MARVY_BUTTON_UP, true, 999) == 0 && input.held_button == -1);
    assert(send(&input, &s, 99, false, 1001) == 0);
    assert(marvy_session_advance(&s, 6000) == MARVY_EVENT_THROW);
}

static void test_pause_exit_and_controls_wake_releases_are_inert(void)
{
    marvy_input_t input; marvy_session_t s; start(&input, &s, 0);
    click(&input, &s, MARVY_BUTTON_DOWN, 1000);
    send(&input, &s, MARVY_BUTTON_OK, true, 1001); assert(s.game.phase == MARVY_PHASE_PAUSED);
    send(&input, &s, MARVY_BUTTON_DOWN, true, 1002); send(&input, &s, MARVY_BUTTON_UP, false, 1003);
    assert(s.game.pause_selection == MARVY_PAUSE_CONTINUE);
    send(&input, &s, MARVY_BUTTON_OK, false, 1004);
    click(&input, &s, MARVY_BUTTON_UP, 1100); assert(s.game.pause_selection == MARVY_PAUSE_EXIT);
    assert(marvy_session_advance(&s, 46100) == MARVY_SESSION_EVENT_DIM);
    marvy_input_cancel(&input, &s);
    assert(send(&input, &s, MARVY_BUTTON_OK, true, 50000) == MARVY_SESSION_EVENT_WAKE);
    assert(send(&input, &s, MARVY_BUTTON_OK, false, 50001) == 0);
    assert(s.game.phase == MARVY_PHASE_PAUSED && s.game.stage_elapsed_ms == 1001);
    send(&input, &s, MARVY_BUTTON_OK, true, 50002); assert(s.game.phase == MARVY_PHASE_INTRO);
    send(&input, &s, MARVY_BUTTON_OK, true, 50003); send(&input, &s, MARVY_BUTTON_OK, false, 50004);
    assert(s.game.phase == MARVY_PHASE_INTRO && s.game.throws == 0);
    click(&input, &s, MARVY_BUTTON_UP, 50005);
    assert(s.game.phase == MARVY_PHASE_INTRO && s.game.intro_selection == MARVY_INTRO_CONTROLS);
    click(&input, &s, MARVY_BUTTON_OK, 50007); assert(s.game.phase == MARVY_PHASE_CONTROLS);
    assert(marvy_session_advance(&s, 95007) == MARVY_SESSION_EVENT_DIM);
    marvy_input_cancel(&input, &s);
    assert(click(&input, &s, MARVY_BUTTON_UP, 100000) == MARVY_SESSION_EVENT_WAKE);
    assert(s.game.phase == MARVY_PHASE_CONTROLS);
    click(&input, &s, MARVY_BUTTON_OK, 100002); assert(s.game.phase == MARVY_PHASE_INTRO);
}

static void test_press_at_finish_cannot_select_or_restart_results(void)
{
    marvy_input_t input; marvy_session_t s; start(&input, &s, 0);
    s.game.stage_elapsed_ms = 44999; s.game.captures = 4; s.game.throws = 4; s.game.score = 4;
    assert(send(&input, &s, MARVY_BUTTON_OK, true, 1) & MARVY_EVENT_FINISH);
    assert(s.game.phase == MARVY_PHASE_RESULTS && s.game.result_selection == MARVY_RESULT_NEXT);
    assert(!s.release_throws);
    send(&input, &s, MARVY_BUTTON_DOWN, true, 2); send(&input, &s, MARVY_BUTTON_DOWN, false, 3);
    assert(s.game.result_selection == MARVY_RESULT_NEXT);
    send(&input, &s, MARVY_BUTTON_OK, false, 4); assert(s.game.phase == MARVY_PHASE_RESULTS);
    click(&input, &s, MARVY_BUTTON_OK, 5);
    assert(s.game.phase == MARVY_PHASE_BRIEF && s.game.level_index == 1 && s.game.throws == 0);
    click(&input, &s, MARVY_BUTTON_OK, 7);
    assert(s.game.phase == MARVY_PHASE_PLAYING && s.game.throws == 0);
}

static void test_held_focus_key_still_allows_auto_throw_but_needs_ok_to_pause(void)
{
    marvy_input_t input; marvy_session_t s; start(&input, &s, 3);
    send(&input, &s, MARVY_BUTTON_DOWN, true, 100);
    assert(s.game.play_selection == MARVY_PLAY_PAUSE && !s.release_throws);
    assert(marvy_session_advance(&s, 6000) == MARVY_EVENT_THROW);
    send(&input, &s, MARVY_BUTTON_OK, true, 6001);
    assert(s.game.phase == MARVY_PHASE_PLAYING && s.game.throws == 1);
    send(&input, &s, MARVY_BUTTON_DOWN, false, 6002);
    click(&input, &s, MARVY_BUTTON_OK, 6003);
    assert(s.game.phase == MARVY_PHASE_PAUSED && s.game.throws == 1);
}

static void test_ignored_flight_and_continue_gestures_never_create_late_charge(void)
{
    marvy_input_t input; marvy_session_t s; start(&input, &s, 0);
    assert(click(&input, &s, MARVY_BUTTON_OK, 100) == MARVY_EVENT_THROW);
    send(&input, &s, MARVY_BUTTON_OK, true, 200);
    assert(s.game.ring_state == MARVY_RING_FLYING && !s.release_throws);
    send(&input, &s, MARVY_BUTTON_UP, false, 800);
    marvy_session_advance(&s, 900); assert(s.game.ring_state == MARVY_RING_READY);
    assert(send(&input, &s, MARVY_BUTTON_OK, false, 1000) == 0 && s.game.throws == 1);
    click(&input, &s, MARVY_BUTTON_UP, 1100);
    click(&input, &s, MARVY_BUTTON_OK, 1200); assert(s.game.phase == MARVY_PHASE_PAUSED);
    send(&input, &s, MARVY_BUTTON_OK, true, 1300);
    assert(s.game.phase == MARVY_PHASE_PLAYING && s.game.play_selection == MARVY_PLAY_THROW);
    assert(!s.release_throws);
    marvy_session_advance(&s, 2000); assert(s.game.ring_state == MARVY_RING_READY);
    assert(send(&input, &s, MARVY_BUTTON_OK, false, 2100) == 0 && s.game.throws == 1);
}

int main(void)
{
    test_welcome_help_and_right_key_charge_ownership();
    test_timeout_owned_release_never_throws_again();
    test_overflow_and_stale_cancel_clear_throw_permission_without_extending_time();
    test_pause_exit_and_controls_wake_releases_are_inert();
    test_press_at_finish_cannot_select_or_restart_results();
    test_held_focus_key_still_allows_auto_throw_but_needs_ok_to_pause();
    test_ignored_flight_and_continue_gestures_never_create_late_charge();
    puts("Marvy unified controls, ownership, cancellation and transition tests: PASS");
    return 0;
}
