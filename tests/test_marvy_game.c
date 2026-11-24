#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "marvy_game.h"

static void start(marvy_game_t *game, uint32_t seed, uint16_t best)
{
    marvy_game_init(game, seed, best);
    assert(marvy_game_press(game) == MARVY_EVENT_START);
    assert(game->phase == MARVY_PHASE_PLAYING);
    assert(marvy_game_release(game) == 0);
}

static void clear_lane(marvy_game_t *game)
{
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        game->sheep[i].hidden_ms = 100000;
    }
}

static void target(marvy_game_t *game, unsigned index, float x, float speed, bool marvy)
{
    game->sheep[index].x = x;
    game->sheep[index].speed = speed;
    game->sheep[index].marvy = marvy;
    game->sheep[index].hidden_ms = 0;
    game->sheep[index].announced = true;
}

static void charge(marvy_game_t *game, uint32_t held_ms)
{
    assert(marvy_game_press(game) == 0);
    assert(game->ring_state == MARVY_RING_CHARGING);
    marvy_game_step(game, held_ms);
}

static void test_welcome_never_starts_without_press(void)
{
    marvy_game_t game;
    marvy_game_init(&game, 1, 17);
    assert(game.phase == MARVY_PHASE_INTRO);
    float x = game.sheep[1].x;
    for (unsigned i = 0; i < 10; ++i) {
        assert(marvy_game_step(&game, MARVY_MAX_STEP_MS) == 0);
        assert(game.phase == MARVY_PHASE_INTRO);
        assert(game.sheep[1].x == x);
    }
    assert(game.rings_left == 10 && game.score == 0 && game.best_score == 17);
    assert(marvy_game_release(&game) == 0);
    assert(game.phase == MARVY_PHASE_INTRO);
    assert(marvy_game_press(&game) == MARVY_EVENT_START);
    assert(game.phase == MARVY_PHASE_PLAYING && game.phase_ms == 0);
    assert(marvy_game_release(&game) == 0);
    assert(game.ring_state == MARVY_RING_READY && game.rings_left == 10);
}

static void test_charge_distance_mapping(void)
{
    assert(marvy_game_charge_target_x(0) == 32);
    assert(marvy_game_charge_target_x(700) == 120);
    assert(marvy_game_charge_target_x(1400) == 208);
    assert(marvy_game_charge_target_x(UINT32_MAX) == 208);
    float previous = 0;
    for (unsigned ms = 0; ms <= 2000; ++ms) {
        float x = marvy_game_charge_target_x(ms);
        assert(x >= previous && x >= 32 && x <= 208);
        previous = x;
    }
}

static void test_only_release_spends_a_ring_and_locks_distance(void)
{
    marvy_game_t game;
    start(&game, 1, 0);
    clear_lane(&game);
    assert(marvy_game_release(&game) == 0);
    charge(&game, 700);
    assert(game.rings_left == 10 && game.ring_ms == 700);
    for (unsigned i = 0; i < 100; ++i) assert(marvy_game_press(&game) == 0);
    assert(game.ring_ms == 700 && game.rings_left == 10);
    assert(marvy_game_release(&game) == MARVY_EVENT_THROW);
    assert(game.rings_left == 9 && game.ring_state == MARVY_RING_FLYING);
    assert(game.ring_target_x == 120 && game.ring_ms == 0);
    assert(marvy_game_release(&game) == 0 && marvy_game_press(&game) == 0);
    assert(marvy_game_step(&game, MARVY_FLIGHT_MS - 1) == 0);
    assert(game.ring_target_x == 120 && game.score == 0);
    assert(marvy_game_step(&game, 1) == MARVY_EVENT_MISS);
    assert(game.ring_state == MARVY_RING_FEEDBACK);
    assert(marvy_game_press(&game) == 0 && marvy_game_release(&game) == 0);
    marvy_game_step(&game, MARVY_FEEDBACK_MS);
    charge(&game, 1400);
    assert(game.rings_left == 9);
    assert(marvy_game_release(&game) == MARVY_EVENT_THROW);
    assert(game.ring_target_x == 208 && game.rings_left == 8);
}

static void test_long_hold_clamps_without_automatic_throw(void)
{
    marvy_game_t game;
    start(&game, 8, 0);
    clear_lane(&game);
    charge(&game, UINT32_MAX);
    assert(game.ring_state == MARVY_RING_CHARGING);
    assert(game.ring_ms == MARVY_CHARGE_MS && game.rings_left == 10);
    assert(marvy_game_release(&game) == MARVY_EVENT_THROW);
    assert(game.ring_target_x == MARVY_TARGET_MAX_X && game.rings_left == 9);
}

static void test_ordinary_and_marvy_scores(void)
{
    marvy_game_t game;
    start(&game, 1, 0);
    clear_lane(&game);
    charge(&game, 0);
    target(&game, 0, 32, 0, false);
    assert(marvy_game_release(&game) == MARVY_EVENT_THROW);
    assert(marvy_game_step(&game, MARVY_FLIGHT_MS) == MARVY_EVENT_HIT);
    assert(game.score == 1 && game.last_points == 1 && !game.last_hit_marvy);
    assert(marvy_game_feedback_ms(&game) == 400 && game.last_hit_sheep == 0);
    marvy_game_step(&game, MARVY_FEEDBACK_MS);
    clear_lane(&game);
    charge(&game, 1400);
    target(&game, 2, 208, 0, true);
    assert(marvy_game_release(&game) == MARVY_EVENT_THROW);
    assert(marvy_game_step(&game, MARVY_FLIGHT_MS) ==
           (MARVY_EVENT_HIT | MARVY_EVENT_MARVY_HIT));
    assert(game.score == 6 && game.last_points == 5 && game.last_hit_marvy);
    assert(marvy_game_feedback_ms(&game) == 1200 && game.last_hit_sheep == 2);
}

static void test_collision_uses_landing_position_and_selected_distance(void)
{
    marvy_game_t game;
    start(&game, 1, 0);
    clear_lane(&game);
    charge(&game, 350); /* x=76, not the old fixed centre. */
    target(&game, 0, 52, 40, false);
    marvy_game_release(&game);
    assert(marvy_game_step(&game, MARVY_FLIGHT_MS) == MARVY_EVENT_HIT);
    assert(fabsf(game.last_hit_x - 76) < 0.02f && game.ring_target_x == 76);

    start(&game, 1, 0);
    clear_lane(&game);
    charge(&game, 0);
    target(&game, 0, 120, 0, false);
    marvy_game_release(&game);
    assert(marvy_game_step(&game, MARVY_FLIGHT_MS) == MARVY_EVENT_MISS);
    assert(game.score == 0);
}

static void test_nearest_only_and_tighter_hit_boundary(void)
{
    marvy_game_t game;
    start(&game, 1, 0);
    clear_lane(&game);
    charge(&game, 700);
    target(&game, 0, 105, 0, true);
    target(&game, 1, 123, 0, false);
    marvy_game_release(&game);
    marvy_game_step(&game, MARVY_FLIGHT_MS);
    assert(game.score == 1 && game.last_hit_sheep == 1);
    assert(game.sheep[0].hidden_ms == 0);
    start(&game, 1, 0);
    clear_lane(&game);
    charge(&game, 700);
    target(&game, 0, 120 + MARVY_HIT_RADIUS, 0, false);
    marvy_game_release(&game);
    assert(marvy_game_step(&game, MARVY_FLIGHT_MS) == MARVY_EVENT_HIT);
    start(&game, 1, 0);
    clear_lane(&game);
    charge(&game, 700);
    target(&game, 0, 137, 0, false);
    marvy_game_release(&game);
    assert(marvy_game_step(&game, MARVY_FLIGHT_MS) == MARVY_EVENT_MISS);
}

static void test_last_ring_settles_and_best_is_retained(void)
{
    marvy_game_t game;
    start(&game, 7, 12);
    for (unsigned i = 0; i < MARVY_RING_COUNT; ++i) {
        clear_lane(&game);
        charge(&game, 700);
        target(&game, 0, 120, 0, true);
        assert(marvy_game_release(&game) == MARVY_EVENT_THROW);
        assert(game.phase == MARVY_PHASE_PLAYING);
        marvy_game_step(&game, MARVY_FLIGHT_MS);
        assert(game.score == (i + 1) * 5 && game.phase == MARVY_PHASE_PLAYING);
        assert((marvy_game_step(&game, 1199) & MARVY_EVENT_FINISH) == 0);
        assert(marvy_game_press(&game) == 0 && marvy_game_release(&game) == 0);
        uint32_t events = marvy_game_step(&game, 1);
        if (i == MARVY_RING_COUNT - 1) {
            assert(events == (MARVY_EVENT_FINISH | MARVY_EVENT_NEW_BEST));
            assert(game.phase == MARVY_PHASE_RESULTS && game.best_score == 50);
        } else assert((events & MARVY_EVENT_FINISH) == 0);
    }
    assert(game.rings_left == 0 && marvy_game_step(&game, 5000) == 0);
    assert(marvy_game_press(&game) == MARVY_EVENT_START);
    assert(game.score == 0 && game.rings_left == 10 && game.best_score == 50);
    assert(marvy_game_release(&game) == 0 && game.rings_left == 10);
}

static void test_no_new_best_on_equal_score(void)
{
    marvy_game_t game;
    start(&game, 2, 10);
    for (unsigned i = 0; i < MARVY_RING_COUNT; ++i) {
        clear_lane(&game);
        charge(&game, 0);
        target(&game, 0, 32, 0, false);
        marvy_game_release(&game);
        uint32_t events = marvy_game_step(&game, MARVY_FLIGHT_MS + MARVY_FEEDBACK_MS);
        assert((events & MARVY_EVENT_NEW_BEST) == 0);
    }
    assert(game.best_score == 10 && game.score == 10 && game.phase == MARVY_PHASE_RESULTS);
}

static void test_appearance_once_and_capture_hide_duration(void)
{
    marvy_game_t game;
    start(&game, 2, 0);
    clear_lane(&game);
    target(&game, 0, -30, 40, true);
    game.sheep[0].announced = false;
    assert(marvy_game_step(&game, 100) == 0);
    assert(marvy_game_step(&game, 200) == MARVY_EVENT_MARVY_APPEAR);
    assert(marvy_game_step(&game, 100) == 0);
    clear_lane(&game);
    charge(&game, 700);
    target(&game, 0, 120, 0, true);
    marvy_game_release(&game);
    marvy_game_step(&game, MARVY_FLIGHT_MS);
    assert(game.sheep[0].hidden_ms == 1700);
    marvy_game_step(&game, 1199);
    assert(game.sheep[0].hidden_ms == 501 && game.ring_state == MARVY_RING_FEEDBACK);
    marvy_game_step(&game, 1);
    assert(game.sheep[0].hidden_ms == 500 && game.ring_state == MARVY_RING_READY);
    marvy_game_step(&game, 500);
    assert(game.sheep[0].hidden_ms == 0 && game.score == 5);
}

static void test_large_step_and_deterministic_movement(void)
{
    marvy_game_t a, b;
    start(&a, 0, 9);
    start(&b, 0, 9);
    uint32_t combined = marvy_game_step(&a, 12000), divided = 0;
    for (unsigned i = 0; i < 1200; ++i) divided |= marvy_game_step(&b, 10);
    assert(combined == divided && a.rng_state == b.rng_state);
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        assert(fabsf(a.sheep[i].x - b.sheep[i].x) < 0.01f);
        assert(a.sheep[i].speed == b.sheep[i].speed);
    }
    marvy_game_step(&a, UINT32_MAX);
    assert(a.rings_left == 10 && a.score == 0 && a.best_score == 9);
}

static void assert_sheep_speed_classes(const marvy_game_t *game)
{
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        float speed = fabsf(game->sheep[i].speed);
        if (game->sheep[i].marvy) assert(speed >= 48 && speed <= 56);
        else assert(speed >= 30 && speed <= 42);
    }
}

static void test_marvy_speeds_at_start_and_respawn(void)
{
    for (uint32_t seed = 1; seed <= 200; ++seed) {
        marvy_game_t game;
        start(&game, seed, 0);
        assert_sheep_speed_classes(&game);
        for (unsigned traversal = 0; traversal < 20; ++traversal) {
            for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) game.sheep[i].x = MARVY_WIDTH + 45;
            marvy_game_step(&game, 1);
            assert_sheep_speed_classes(&game);
        }
    }
}

int main(void)
{
    test_welcome_never_starts_without_press();
    test_charge_distance_mapping();
    test_only_release_spends_a_ring_and_locks_distance();
    test_long_hold_clamps_without_automatic_throw();
    test_ordinary_and_marvy_scores();
    test_collision_uses_landing_position_and_selected_distance();
    test_nearest_only_and_tighter_hit_boundary();
    test_last_ring_settles_and_best_is_retained();
    test_no_new_best_on_equal_score();
    test_appearance_once_and_capture_hide_duration();
    test_large_step_and_deterministic_movement();
    test_marvy_speeds_at_start_and_respawn();
    puts("Marvy charge-and-release game tests: PASS");
    return 0;
}
