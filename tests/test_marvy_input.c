#include "marvy_input.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

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
    assert(click(input, s, MARVY_KEY_1, 0) == 0 && s->game.phase == MARVY_PHASE_DEMO);
    assert(click(input, s, MARVY_KEY_1, 0) == 0 && s->game.phase == MARVY_PHASE_BRIEF);
    s->game.level_index = (uint8_t)level;
    assert(click(input, s, MARVY_KEY_1, 0) == MARVY_EVENT_START);
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) s->game.sheep[i].hidden_ms = UINT32_MAX;
}

enum { PHYSICAL_KEY_1 = 0, PHYSICAL_KEY_2 = 1, PHYSICAL_KEY_3 = 2 };

/* Physical #1/#2/#3 are raw ADC slots 0/1/2 per the latest device test. Exercise
 * the production adapter for both edges, rather than a second test-only mapping. */
static uint32_t board_send(marvy_input_t *input, marvy_session_t *s,
                           uint8_t board_button, bool pressed, uint64_t at)
{
    marvy_input_event_t event;
    assert(marvy_input_from_board(board_button, pressed, at, &event));
    return marvy_input_handle(input, s, &event);
}

static uint32_t board_click(marvy_input_t *input, marvy_session_t *s,
                            uint8_t board_button, uint64_t at)
{
    uint32_t events = board_send(input, s, board_button, true, at);
    return events | board_send(input, s, board_button, false, at);
}

static void board_start(marvy_input_t *input, marvy_session_t *s, unsigned level)
{
    marvy_input_init(input); marvy_session_init(s, 123, 17, 0);
    marvy_progress_t progress; marvy_progress_init(&progress);
    progress.unlocked_count = (uint8_t)(level + 1); marvy_game_apply_progress(&s->game, &progress);
    assert(board_click(input, s, PHYSICAL_KEY_1, 0) == 0 && s->game.phase == MARVY_PHASE_DEMO);
    assert(board_click(input, s, PHYSICAL_KEY_1, 0) == 0 && s->game.phase == MARVY_PHASE_BRIEF);
    s->game.level_index = (uint8_t)level;
    assert(board_click(input, s, PHYSICAL_KEY_1, 0) == MARVY_EVENT_START);
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) s->game.sheep[i].hidden_ms = UINT32_MAX;
}

static void test_board_mapping_preserves_edges_and_rejects_invalid_slots(void)
{
    static const uint8_t expected[] = {MARVY_KEY_1, MARVY_KEY_2, MARVY_KEY_3};
    for (uint8_t board = 0; board < MARVY_INPUT_BUTTON_COUNT; ++board) {
        for (unsigned pressed = 0; pressed < 2; ++pressed) {
            marvy_input_event_t event = {99, false, 42};
            assert(marvy_input_from_board(board, pressed != 0, UINT64_MAX, &event));
            assert(event.button == expected[board] && event.pressed == (pressed != 0));
            assert(event.at_ms == UINT64_MAX);
        }
    }
    marvy_input_event_t event = {99, true, 42};
    assert(!marvy_input_from_board(3, false, 100, &event));
    assert(event.button == 99 && event.pressed && event.at_ms == 42);
    assert(!marvy_input_from_board(UINT8_MAX, false, 100, &event));
    assert(event.button == 99 && event.pressed && event.at_ms == 42);
    assert(!marvy_input_from_board(1, true, 100, NULL));
}

static void test_numbered_physical_menu_roles_and_start_release_are_safe(void)
{
    marvy_input_t input; marvy_session_t s;
    marvy_input_init(&input); marvy_session_init(&s, 123, 17, 0);
    marvy_progress_t progress; marvy_progress_init(&progress);
    progress.unlocked_count = 2; marvy_game_apply_progress(&s.game, &progress);
    board_send(&input, &s, PHYSICAL_KEY_3, true, 10);
    board_send(&input, &s, PHYSICAL_KEY_1, true, 11); board_send(&input, &s, PHYSICAL_KEY_1, false, 12);
    assert(s.game.phase == MARVY_PHASE_INTRO && s.game.intro_selection == MARVY_INTRO_CONTROLS);
    assert(input.held_button == MARVY_KEY_3 && !s.release_throws);
    board_send(&input, &s, PHYSICAL_KEY_3, false, 13);
    board_send(&input, &s, PHYSICAL_KEY_1, true, 20);
    assert(s.game.phase == MARVY_PHASE_CONTROLS && input.held_button == MARVY_KEY_1);
    board_send(&input, &s, PHYSICAL_KEY_2, false, 21);
    assert(input.held_button == MARVY_KEY_1 && s.button_held);
    board_send(&input, &s, PHYSICAL_KEY_1, false, 22);
    board_click(&input, &s, PHYSICAL_KEY_2, 30); board_click(&input, &s, PHYSICAL_KEY_3, 31);
    assert(s.game.phase == MARVY_PHASE_CONTROLS);
    board_click(&input, &s, PHYSICAL_KEY_1, 40); assert(s.game.phase == MARVY_PHASE_INTRO);
    board_click(&input, &s, PHYSICAL_KEY_1, 50); assert(s.game.phase == MARVY_PHASE_DEMO);
    board_click(&input, &s, PHYSICAL_KEY_1, 50); assert(s.game.phase == MARVY_PHASE_BRIEF);
    board_click(&input, &s, PHYSICAL_KEY_3, 60); assert(s.game.level_index == 1);
    board_click(&input, &s, PHYSICAL_KEY_2, 70); assert(s.game.level_index == 0);
    assert(board_send(&input, &s, PHYSICAL_KEY_1, true, 80) == MARVY_EVENT_START);
    assert(s.game.phase == MARVY_PHASE_PLAYING && !s.release_throws);
    marvy_session_advance(&s, 2000);
    assert(board_send(&input, &s, PHYSICAL_KEY_1, false, 2001) == 0);
    assert(s.game.phase == MARVY_PHASE_PLAYING && s.game.ring_state == MARVY_RING_READY && s.game.throws == 0);
    board_click(&input, &s, PHYSICAL_KEY_1, 2100);
    assert(s.game.phase == MARVY_PHASE_PAUSED && s.game.throws == 0);
}

static void test_both_physical_throw_keys_require_their_own_release(void)
{
    const uint8_t slots[] = {PHYSICAL_KEY_2, PHYSICAL_KEY_3};
    const marvy_button_t owners[] = {MARVY_KEY_2, MARVY_KEY_3};
    for (unsigned i = 0; i < 2; ++i) {
        uint8_t own = slots[i], other = slots[1 - i];
        marvy_input_t input; marvy_session_t s; board_start(&input, &s, 0);
        board_send(&input, &s, own, true, 100);
        assert(s.game.ring_state == MARVY_RING_CHARGING && s.release_throws);
        board_send(&input, &s, own, true, 101);
        board_send(&input, &s, other, true, 102);
        board_send(&input, &s, PHYSICAL_KEY_1, true, 103);
        board_send(&input, &s, other, false, 200);
        assert(input.held_button == (int8_t)owners[i] && s.game.phase == MARVY_PHASE_PLAYING);
        assert(s.game.throws == 0 && s.release_throws);
        assert(board_send(&input, &s, own, false, 800) == MARVY_EVENT_THROW);
        assert(s.game.throws == 1 && s.game.ring_target_x == 120 && !s.release_throws);
        assert(input.held_button == -1 && !s.button_held);
        assert(board_send(&input, &s, own, false, 801) == 0);
    }
}

static void test_timeout_held_throw_key_release_never_throws_again(void)
{
    const uint8_t slots[] = {PHYSICAL_KEY_2, PHYSICAL_KEY_3};
    for (unsigned i = 0; i < 2; ++i) {
        uint8_t own = slots[i], other = slots[1 - i];
        marvy_input_t input; marvy_session_t s; board_start(&input, &s, 3);
        board_send(&input, &s, own, true, 100);
        assert(marvy_session_advance(&s, 6000) == MARVY_EVENT_THROW);
        assert(s.button_held && !s.release_throws);
        board_send(&input, &s, other, true, 6001);
        board_send(&input, &s, other, false, 6002);
        marvy_session_advance(&s, 6800); assert(s.game.ring_state == MARVY_RING_READY);
        assert(board_send(&input, &s, own, false, 6801) == 0);
        assert(!s.button_held && input.held_button == -1 && s.game.throws == 1);
        board_send(&input, &s, own, true, 6802);
        assert(board_send(&input, &s, own, false, 6902) == MARVY_EVENT_THROW);
        assert(s.game.throws == 2);
    }
}

static void test_overflow_and_stale_cancel_clear_permission_without_extending_time(void)
{
    marvy_input_t input; marvy_session_t s; board_start(&input, &s, 3);
    board_send(&input, &s, PHYSICAL_KEY_2, true, 100); marvy_session_advance(&s, 800);
    marvy_input_cancel(&input, &s);
    assert(input.held_button == -1 && !s.button_held && !s.release_throws);
    assert(s.game.ring_state == MARVY_RING_READY && s.game.throws == 0);
    assert(s.game.stage_elapsed_ms == 800 && s.game.turn_ms == 800);
    assert(board_send(&input, &s, PHYSICAL_KEY_2, false, 801) == 0);
    board_send(&input, &s, PHYSICAL_KEY_3, true, 900); marvy_session_advance(&s, 1000);
    assert(board_send(&input, &s, PHYSICAL_KEY_2, false, 999) == 0 && s.button_held);
    assert(board_send(&input, &s, PHYSICAL_KEY_3, false, 999) == 0);
    assert(input.held_button == -1 && !s.button_held && !s.release_throws);
    assert(s.game.ring_state == MARVY_RING_READY && s.game.stage_elapsed_ms == 1000);
    assert(s.game.turn_ms == 1000);
    assert(board_send(&input, &s, PHYSICAL_KEY_2, true, 999) == 0 && input.held_button == -1);
    assert(send(&input, &s, 99, true, 1001) == 0);
    assert(send(&input, &s, 99, false, 1001) == 0);
    assert(send(&input, &s, MARVY_KEY_2, false, 1001) == 0);
    assert(marvy_session_advance(&s, 6000) == MARVY_EVENT_THROW);
}

static void test_return_key_pauses_flight_and_continue_release_cannot_throw(void)
{
    marvy_input_t input; marvy_session_t s; board_start(&input, &s, 0);
    assert(board_click(&input, &s, PHYSICAL_KEY_2, 100) == MARVY_EVENT_THROW);
    board_send(&input, &s, PHYSICAL_KEY_1, true, 200);
    assert(s.game.phase == MARVY_PHASE_PAUSED && s.game.ring_ms == 100 && !s.release_throws);
    board_send(&input, &s, PHYSICAL_KEY_1, false, 201);
    marvy_game_t frozen = s.game;
    marvy_session_advance(&s, 5000); assert(memcmp(&frozen, &s.game, sizeof(frozen)) == 0);
    board_click(&input, &s, PHYSICAL_KEY_3, 5001); assert(s.game.pause_selection == MARVY_PAUSE_EXIT);
    board_click(&input, &s, PHYSICAL_KEY_2, 5002); assert(s.game.pause_selection == MARVY_PAUSE_CONTINUE);
    board_send(&input, &s, PHYSICAL_KEY_1, true, 5003);
    assert(s.game.phase == MARVY_PHASE_PLAYING && !s.release_throws);
    marvy_session_advance(&s, 6000); assert(s.game.ring_state == MARVY_RING_READY);
    assert(board_send(&input, &s, PHYSICAL_KEY_1, false, 6100) == 0 && s.game.throws == 1);
    board_send(&input, &s, PHYSICAL_KEY_3, true, 6200);
    assert(board_send(&input, &s, PHYSICAL_KEY_3, false, 6900) == MARVY_EVENT_THROW);
    assert(s.game.throws == 2 && s.game.ring_target_x == 120);
}

static void test_pause_exit_controls_and_wake_gestures_are_inert(void)
{
    marvy_input_t input; marvy_session_t s; board_start(&input, &s, 0);
    board_click(&input, &s, PHYSICAL_KEY_1, 1000); assert(s.game.phase == MARVY_PHASE_PAUSED);
    board_click(&input, &s, PHYSICAL_KEY_3, 1100); assert(s.game.pause_selection == MARVY_PAUSE_EXIT);
    assert(marvy_session_advance(&s, 46100) == MARVY_SESSION_EVENT_DIM);
    marvy_input_cancel(&input, &s);
    assert(board_send(&input, &s, PHYSICAL_KEY_1, true, 50000) == MARVY_SESSION_EVENT_WAKE);
    assert(board_send(&input, &s, PHYSICAL_KEY_1, false, 50001) == 0);
    assert(s.game.phase == MARVY_PHASE_PAUSED && s.game.stage_elapsed_ms == 1000);
    board_send(&input, &s, PHYSICAL_KEY_1, true, 50002); assert(s.game.phase == MARVY_PHASE_INTRO);
    board_send(&input, &s, PHYSICAL_KEY_1, true, 50003); board_send(&input, &s, PHYSICAL_KEY_1, false, 50004);
    assert(s.game.phase == MARVY_PHASE_INTRO && s.game.throws == 0);
    board_click(&input, &s, PHYSICAL_KEY_2, 50005); assert(s.game.intro_selection == MARVY_INTRO_CONTROLS);
    board_click(&input, &s, PHYSICAL_KEY_1, 50007); assert(s.game.phase == MARVY_PHASE_CONTROLS);
    assert(marvy_session_advance(&s, 95007) == MARVY_SESSION_EVENT_DIM);
    marvy_input_cancel(&input, &s);
    assert(board_click(&input, &s, PHYSICAL_KEY_3, 100000) == MARVY_SESSION_EVENT_WAKE);
    assert(s.game.phase == MARVY_PHASE_CONTROLS);
    board_click(&input, &s, PHYSICAL_KEY_1, 100002); assert(s.game.phase == MARVY_PHASE_INTRO);
}

static void test_any_press_at_finish_cannot_select_or_restart_results(void)
{
    const marvy_button_t keys[] = {MARVY_KEY_1, MARVY_KEY_2, MARVY_KEY_3};
    for (unsigned i = 0; i < 3; ++i) {
        marvy_input_t input; marvy_session_t s; start(&input, &s, 0);
        s.game.stage_elapsed_ms = 44999; s.game.captures = 4; s.game.throws = 4; s.game.score = 4;
        assert(send(&input, &s, keys[i], true, 1) & MARVY_EVENT_FINISH);
        assert(s.game.phase == MARVY_PHASE_RESULTS && s.game.result_selection == MARVY_RESULT_NEXT);
        assert(!s.release_throws);
        send(&input, &s, keys[(i + 1) % 3], true, 2);
        send(&input, &s, keys[(i + 1) % 3], false, 3);
        assert(s.game.result_selection == MARVY_RESULT_NEXT);
        send(&input, &s, keys[i], false, 4); assert(s.game.phase == MARVY_PHASE_RESULTS);
        click(&input, &s, MARVY_KEY_3, 5); assert(s.game.result_selection == MARVY_RESULT_RETRY);
        click(&input, &s, MARVY_KEY_1, 7); assert(s.game.phase == MARVY_PHASE_BRIEF);
        click(&input, &s, MARVY_KEY_1, 9);
        assert(s.game.phase == MARVY_PHASE_PLAYING && s.game.throws == 0);
    }
}

static void test_ignored_throw_gestures_never_create_a_late_cast(void)
{
    const marvy_button_t keys[] = {MARVY_KEY_2, MARVY_KEY_3};
    const uint64_t pressed_at[] = {200, 750};
    for (unsigned i = 0; i < 2; ++i) {
        for (unsigned state = 0; state < 2; ++state) {
            marvy_input_t input; marvy_session_t s; start(&input, &s, 0);
            assert(click(&input, &s, MARVY_KEY_2, 100) == MARVY_EVENT_THROW);
            send(&input, &s, keys[i], true, pressed_at[state]);
            assert(!s.release_throws);
            assert(s.game.ring_state == (state == 0 ? MARVY_RING_FLYING : MARVY_RING_FEEDBACK));
            marvy_session_advance(&s, 1000); assert(s.game.ring_state == MARVY_RING_READY);
            assert(send(&input, &s, keys[i], false, 1100) == 0 && s.game.throws == 1);
        }
    }
}

static void test_first_level_held_throw_keys_end_without_a_late_cast(void)
{
    const uint8_t slots[] = {PHYSICAL_KEY_2, PHYSICAL_KEY_3};
    for (unsigned i = 0; i < 2; ++i) {
        marvy_input_t input; marvy_session_t s; board_start(&input, &s, 0);
        board_send(&input, &s, slots[i], true, 100);
        assert(marvy_session_advance(&s, 44999) == 0 && s.release_throws && s.game.throws == 0);
        assert(marvy_session_advance(&s, 45000) & MARVY_EVENT_FINISH);
        assert(!s.release_throws && s.game.phase == MARVY_PHASE_RESULTS);
        assert(board_send(&input, &s, slots[i], false, 45001) == 0);
        assert(s.game.phase == MARVY_PHASE_RESULTS && input.held_button == -1);
    }
}

int main(void)
{
    test_board_mapping_preserves_edges_and_rejects_invalid_slots();
    test_numbered_physical_menu_roles_and_start_release_are_safe();
    test_both_physical_throw_keys_require_their_own_release();
    test_timeout_held_throw_key_release_never_throws_again();
    test_overflow_and_stale_cancel_clear_permission_without_extending_time();
    test_return_key_pauses_flight_and_continue_release_cannot_throw();
    test_pause_exit_controls_and_wake_gestures_are_inert();
    test_any_press_at_finish_cannot_select_or_restart_results();
    test_ignored_throw_gestures_never_create_a_late_cast();
    test_first_level_held_throw_keys_end_without_a_late_cast();
    puts("Marvy numbered physical controls, ownership and transition input tests: PASS");
    return 0;
}
