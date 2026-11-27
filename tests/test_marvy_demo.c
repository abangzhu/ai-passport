#include "marvy_demo.h"
#include "marvy_completion.h"
#include "marvy_input.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t key(marvy_input_t *input, marvy_session_t *session,
                    marvy_button_t button, bool pressed, uint64_t at)
{
    const marvy_input_event_t event = {(uint8_t)button, pressed, at};
    return marvy_input_handle(input, session, &event);
}

static uint32_t tap(marvy_input_t *input, marvy_session_t *session,
                    marvy_button_t button, uint64_t at)
{
    uint32_t events = key(input, session, button, true, at);
    return events | key(input, session, button, false, at);
}

static void begin_demo(marvy_input_t *input, marvy_session_t *session)
{
    marvy_input_init(input);
    marvy_session_init(session, 123, 9, 0);
    assert(tap(input, session, MARVY_KEY_1, 0) == 0);
    assert(session->game.phase == MARVY_PHASE_DEMO);
}

static void assert_business_unchanged(const marvy_game_t *before,
                                     const marvy_game_t *after)
{
    marvy_game_t expected = *before;
    expected.phase = after->phase;
    expected.phase_ms = after->phase_ms;
    assert(memcmp(&expected, after, sizeof(expected)) == 0);
}

static void test_demo_loops_until_confirmation_without_business_changes(void)
{
    marvy_input_t input; marvy_session_t session;
    begin_demo(&input, &session);
    marvy_game_t before = session.game;
    assert(marvy_session_advance(&session, 499) == 0);
    assert(session.game.phase == MARVY_PHASE_DEMO && session.game.phase_ms == 499);
    assert(tap(&input, &session, MARVY_KEY_2, 500) == 0);
    assert(tap(&input, &session, MARVY_KEY_3, 1400) == 0);
    assert(marvy_session_advance(&session, 5999) == 0);
    assert_business_unchanged(&before, &session.game);
    assert(marvy_session_advance(&session, 6000) == 0);
    assert(session.game.phase == MARVY_PHASE_DEMO && session.game.phase_ms == 0);
    assert(marvy_session_advance(&session, 12000) == 0);
    assert(session.game.phase == MARVY_PHASE_DEMO && session.game.phase_ms == 0);
    assert(marvy_session_advance(&session, 20000) == 0);
    assert(session.game.phase == MARVY_PHASE_DEMO && session.game.phase_ms == 2000);
    assert_business_unchanged(&before, &session.game);
    assert(tap(&input, &session, MARVY_KEY_1, 20001) == 0);
    assert(session.game.phase == MARVY_PHASE_BRIEF && session.game.stage_elapsed_ms == 0);
    assert(session.last_input_ms == 20001 && !session.dimmed);
    assert(tap(&input, &session, MARVY_KEY_1, 20002) == MARVY_EVENT_START);
    assert(session.game.phase == MARVY_PHASE_PLAYING && session.game.throws == 0);
}

static void test_loop_boundary_keys_and_confirm_release_do_not_start_play(void)
{
    for (unsigned i = 0; i < MARVY_BUTTON_COUNT; ++i) {
        marvy_input_t input; marvy_session_t session;
        begin_demo(&input, &session);
        marvy_phase_t expected = i == MARVY_KEY_1 ? MARVY_PHASE_BRIEF : MARVY_PHASE_DEMO;
        assert(key(&input, &session, (marvy_button_t)i, true, 6000) == 0);
        assert(session.game.phase == expected && !session.release_throws);
        assert(key(&input, &session, (marvy_button_t)((i + 1) % 3), false, 6001) == 0);
        assert(session.button_held && session.game.phase == expected);
        assert(key(&input, &session, (marvy_button_t)i, false, 6500) == 0);
        assert(session.game.phase == expected && session.game.level_index == 0);
        if (expected == MARVY_PHASE_DEMO) {
            assert(tap(&input, &session, MARVY_KEY_1, 6501) == 0);
            assert(session.game.phase == MARVY_PHASE_BRIEF);
        }
        assert(tap(&input, &session, MARVY_KEY_1, 6502) == MARVY_EVENT_START);
    }
    marvy_input_t input; marvy_session_t session;
    begin_demo(&input, &session);
    assert(key(&input, &session, MARVY_KEY_1, true, 100) == 0);
    assert(session.game.phase == MARVY_PHASE_BRIEF && !session.release_throws);
    marvy_session_advance(&session, 1000);
    assert(key(&input, &session, MARVY_KEY_1, false, 1001) == 0);
    assert(session.game.phase == MARVY_PHASE_BRIEF && session.game.throws == 0);
    assert(tap(&input, &session, MARVY_KEY_1, 1002) == MARVY_EVENT_START);

    marvy_input_init(&input); marvy_session_init(&session, 1, 0, 0);
    assert(key(&input, &session, MARVY_KEY_1, true, 0) == 0);
    assert(session.game.phase == MARVY_PHASE_DEMO && session.button_held);
    assert(marvy_session_advance(&session, 18000) == 0);
    assert(key(&input, &session, MARVY_KEY_1, true, 18000) == 0);
    assert(key(&input, &session, MARVY_KEY_1, false, 18001) == 0);
    assert(session.game.phase == MARVY_PHASE_DEMO && session.game.throws == 0);
    assert(tap(&input, &session, MARVY_KEY_1, 18002) == 0);
    assert(session.game.phase == MARVY_PHASE_BRIEF);
    assert(tap(&input, &session, MARVY_KEY_1, 18003) == MARVY_EVENT_START);

    begin_demo(&input, &session);
    assert(tap(&input, &session, MARVY_KEY_2, 6000) == 0);
    assert(tap(&input, &session, MARVY_KEY_3, 12000) == 0);
    assert(session.game.phase == MARVY_PHASE_DEMO && session.game.phase_ms == 0);
    assert(tap(&input, &session, MARVY_KEY_1, 18000) == 0);
    assert(session.game.phase == MARVY_PHASE_BRIEF && session.game.stage_elapsed_ms == 0);

    for (uint64_t boundary = 6000; boundary <= 12000; boundary += 6000) {
        begin_demo(&input, &session);
        assert(key(&input, &session, MARVY_KEY_1, true, boundary) == 0);
        assert(session.game.phase == MARVY_PHASE_BRIEF && session.game.stage_elapsed_ms == 0);
        assert(key(&input, &session, MARVY_KEY_1, false, boundary + 1) == 0);
        assert(session.game.phase == MARVY_PHASE_BRIEF && !session.release_throws);
    }
    const marvy_button_t inert[] = {MARVY_KEY_2, MARVY_KEY_3};
    for (unsigned i = 0; i < 2; ++i) {
        begin_demo(&input, &session);
        assert(key(&input, &session, inert[i], true, 100) == 0);
        assert(marvy_session_advance(&session, 18100) == 0);
        assert(key(&input, &session, inert[i], false, 18101) == 0);
        assert(session.game.phase == MARVY_PHASE_DEMO && session.game.phase_ms == 101);
        assert(session.game.score == 0 && session.game.throws == 0 && !session.release_throws);
    }
}

static void test_long_demo_frames_preserve_remainders_and_never_dim(void)
{
    marvy_input_t input; marvy_session_t session;
    begin_demo(&input, &session);
    marvy_game_t before = session.game;
    assert(marvy_session_advance(&session, UINT64_MAX) == 0);
    assert(session.game.phase == MARVY_PHASE_DEMO);
    assert(session.game.phase_ms == UINT64_MAX % MARVY_DEMO_MS);
    assert(!session.dimmed && session.last_input_ms == 0);
    assert_business_unchanged(&before, &session.game);
    assert(tap(&input, &session, MARVY_KEY_1, UINT64_MAX) == 0);
    assert(session.game.phase == MARVY_PHASE_BRIEF && session.last_input_ms == UINT64_MAX);

    marvy_input_init(&input); marvy_session_init(&session, 1, 0, UINT64_MAX - 10000);
    assert(tap(&input, &session, MARVY_KEY_1, UINT64_MAX - 10000) == 0);
    assert(marvy_session_advance(&session, UINT64_MAX) == 0);
    assert(session.game.phase == MARVY_PHASE_DEMO && session.game.phase_ms == 4000);

    marvy_game_t game; marvy_game_init(&game, 1, 0);
    marvy_game_press(&game); game.phase_ms = UINT32_MAX;
    before = game;
    assert(marvy_game_step(&game, UINT32_MAX) == 0 && game.phase == MARVY_PHASE_DEMO);
    assert(game.phase_ms == ((UINT32_MAX % MARVY_DEMO_MS) * 2u) % MARVY_DEMO_MS);
    assert_business_unchanged(&before, &game);
    game.phase_ms = 5999;
    assert(marvy_game_step(&game, 12002) == 0 && game.phase_ms == 1);
    assert(marvy_game_step(&game, 0) == 0 && game.phase_ms == 1);

    begin_demo(&input, &session);
    assert(marvy_session_advance(&session, 1000000) == 0 && !session.dimmed);
    assert(tap(&input, &session, MARVY_KEY_1, 1000001) == 0);
    assert(marvy_session_advance(&session, 1045000) == 0 && !session.dimmed);
    assert(marvy_session_advance(&session, 1045001) == MARVY_SESSION_EVENT_DIM);
}

static void test_only_successful_final_results_offer_follow_and_return_intact(void)
{
    marvy_game_t game; marvy_game_init(&game, 1, 9);
    for (unsigned level = 0; level < MARVY_LEVEL_COUNT; ++level) {
        for (unsigned passed = 0; passed < 2; ++passed) {
            game.phase = MARVY_PHASE_RESULTS; game.level_index = (uint8_t)level;
            game.passed = passed != 0;
            assert(marvy_game_can_follow(&game) == (passed && level + 1 == MARVY_LEVEL_COUNT));
            game.result_selection = MARVY_RESULT_RETRY;
            marvy_game_result_move(&game, -1);
            assert(game.result_selection == (passed ? level + 1 == MARVY_LEVEL_COUNT
                   ? MARVY_RESULT_FOLLOW : MARVY_RESULT_NEXT : MARVY_RESULT_WELCOME));
            marvy_game_result_move(&game, 1); assert(game.result_selection == MARVY_RESULT_RETRY);
            game.result_selection = MARVY_RESULT_FOLLOW;
            marvy_game_result_confirm(&game);
            assert(game.phase == (passed && level + 1 == MARVY_LEVEL_COUNT
                                  ? MARVY_PHASE_FOLLOW : MARVY_PHASE_RESULTS));
        }
    }
    game.phase = MARVY_PHASE_RESULTS; game.level_index = MARVY_LEVEL_COUNT - 1;
    game.passed = true; game.stars = 3; game.score = 42; game.captures = 14; game.throws = 20;
    game.phase_ms = 100; game.result_selection = MARVY_RESULT_FOLLOW;
    marvy_game_t before = game;
    assert(marvy_game_result_confirm(&game) == 0 && game.phase == MARVY_PHASE_FOLLOW);
    assert_business_unchanged(&before, &game);
    assert(!marvy_game_can_follow(&game));
    marvy_game_result_move(&game, 1); assert(game.result_selection == MARVY_RESULT_FOLLOW);
    assert(marvy_game_press(&game) == 0 && game.phase == MARVY_PHASE_RESULTS);
    assert(game.phase_ms == MARVY_COMPLETION_FINALE_MS);
    assert_business_unchanged(&before, &game);
    game.result_selection = MARVY_RESULT_RETRY;
    assert(marvy_game_result_confirm(&game) == 0 && game.phase == MARVY_PHASE_BRIEF);
    game.phase = MARVY_PHASE_RESULTS; game.level_index = 0; game.passed = true;
    game.result_selection = MARVY_RESULT_NEXT;
    assert(marvy_game_result_confirm(&game) == 0 && game.phase == MARVY_PHASE_BRIEF);
}

static void test_follow_menu_edges_wake_and_release_do_not_start_a_game(void)
{
    marvy_input_t input; marvy_session_t session;
    marvy_input_init(&input); marvy_session_init(&session, 1, 0, 0);
    session.game.phase = MARVY_PHASE_RESULTS; session.game.passed = true;
    session.game.level_index = MARVY_LEVEL_COUNT - 1;
    session.game.result_selection = MARVY_RESULT_FOLLOW;
    assert(key(&input, &session, MARVY_KEY_1, true, 1) == 0);
    assert(session.game.phase == MARVY_PHASE_FOLLOW);
    assert(key(&input, &session, MARVY_KEY_1, false, 2) == 0);
    tap(&input, &session, MARVY_KEY_2, 3); tap(&input, &session, MARVY_KEY_3, 4);
    assert(session.game.phase == MARVY_PHASE_FOLLOW);
    assert(marvy_session_advance(&session, 45004) == MARVY_SESSION_EVENT_DIM);
    marvy_input_cancel(&input, &session);
    assert(tap(&input, &session, MARVY_KEY_1, 45005) == MARVY_SESSION_EVENT_WAKE);
    assert(session.game.phase == MARVY_PHASE_FOLLOW);
    assert(tap(&input, &session, MARVY_KEY_1, 45006) == 0);
    assert(session.game.phase == MARVY_PHASE_RESULTS && session.game.throws == 0);
}

static void test_demo_frames_show_two_matched_catches_without_external_state(void)
{
    marvy_demo_frame(0, NULL);
    const uint32_t times[] = {0, 499, 500, 1399, 1400, 1999, 2000, 2599,
                              2600, 3299, 3300, 3899, 3900, 5099, 5100, 6000, UINT32_MAX};
    const marvy_demo_step_t steps[] = {MARVY_DEMO_READY, MARVY_DEMO_READY,
        MARVY_DEMO_CHARGE, MARVY_DEMO_CHARGE, MARVY_DEMO_THROW, MARVY_DEMO_THROW,
        MARVY_DEMO_HIT, MARVY_DEMO_HIT, MARVY_DEMO_CHARGE, MARVY_DEMO_CHARGE,
        MARVY_DEMO_THROW, MARVY_DEMO_THROW, MARVY_DEMO_BONUS, MARVY_DEMO_BONUS,
        MARVY_DEMO_DONE, MARVY_DEMO_READY, MARVY_DEMO_DONE};
    marvy_demo_frame_t first, repeated;
    marvy_demo_frame(750, &first);
    for (unsigned i = 0; i < sizeof(times) / sizeof(times[0]); ++i) {
        marvy_demo_frame_t frame;
        memset(&frame, 0xa5, sizeof(frame));
        marvy_demo_frame(times[i], &frame);
        assert(frame.step == steps[i] && frame.scene.phase == MARVY_PHASE_PLAYING);
        assert(frame.scene.phase_ms == times[i] % MARVY_DEMO_MS && frame.scene.stage_elapsed_ms == 0);
        uint32_t clip_ms = times[i] % MARVY_DEMO_MS;
        assert(frame.key_held == (steps[i] == MARVY_DEMO_CHARGE));
        assert(frame.scene.score == (clip_ms < 2000 ? 0 : clip_ms < 3900 ? 1 : 6));
        assert(frame.scene.captures == (clip_ms < 2000 ? 0 : clip_ms < 3900 ? 1 : 2));
        assert(isfinite(frame.scene.ring_target_x));
    }
    marvy_demo_frame(12750, &repeated); assert(memcmp(&first, &repeated, sizeof(first)) == 0);
    marvy_demo_frame(0, &first); marvy_demo_frame(6000, &repeated);
    assert(memcmp(&first, &repeated, sizeof(first)) == 0);
    marvy_demo_frame(UINT32_MAX, &first); marvy_demo_frame(UINT32_MAX % MARVY_DEMO_MS, &repeated);
    assert(memcmp(&first, &repeated, sizeof(first)) == 0);
    marvy_demo_frame(2000, &first);
    assert(first.scene.last_points == 1 && !first.scene.last_hit_marvy);
    assert(first.scene.last_hit_x == marvy_game_charge_target_x(900));
    assert(first.scene.sheep[0].x == first.scene.last_hit_x);
    assert(first.scene.ring_state == MARVY_RING_FEEDBACK);
    marvy_demo_frame(3900, &first);
    assert(first.scene.last_points == 5 && first.scene.last_hit_marvy);
    assert(first.scene.last_hit_x == marvy_game_charge_target_x(700));
    assert(first.scene.sheep[1].x == first.scene.last_hit_x);
    assert(first.scene.celebration_ms == MARVY_MARVY_FEEDBACK_MS);
    for (uint32_t at = 0; at <= MARVY_DEMO_MS; ++at) {
        marvy_demo_frame(at, &first);
        assert(first.scene.ring_ms <= (first.scene.ring_state == MARVY_RING_CHARGING
               ? MARVY_CHARGE_MS : first.scene.ring_state == MARVY_RING_FLYING
               ? MARVY_FLIGHT_MS : MARVY_DEMO_MS));
        for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) assert(isfinite(first.scene.sheep[i].x));
    }
}

int main(void)
{
    test_demo_loops_until_confirmation_without_business_changes();
    test_loop_boundary_keys_and_confirm_release_do_not_start_play();
    test_long_demo_frames_preserve_remainders_and_never_dim();
    test_only_successful_final_results_offer_follow_and_return_intact();
    test_follow_menu_edges_wake_and_release_do_not_start_a_game();
    test_demo_frames_show_two_matched_catches_without_external_state();
    puts("Marvy demo isolation, boundary input and follow-menu tests: PASS");
    return 0;
}
