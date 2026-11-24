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
}

static void clear_lane(marvy_game_t *game)
{
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        game->sheep[i].hidden_ms = 100000;
    }
}

static void target(marvy_game_t *game, unsigned index, float x, float speed,
                   bool marvy)
{
    game->sheep[index].x = x;
    game->sheep[index].speed = speed;
    game->sheep[index].marvy = marvy;
    game->sheep[index].hidden_ms = 0;
    game->sheep[index].announced = false;
}

static void test_intro_and_skip(void)
{
    marvy_game_t game;
    marvy_game_init(&game, 1, 17);
    assert(game.phase == MARVY_PHASE_INTRO);
    assert(game.rings_left == 10 && game.score == 0 && game.best_score == 17);
    assert(marvy_game_step(&game, MARVY_INTRO_MS - 1) == 0);
    assert(game.phase == MARVY_PHASE_INTRO);
    assert(marvy_game_step(&game, 1) == MARVY_EVENT_START);
    assert(game.phase == MARVY_PHASE_PLAYING && game.rings_left == 10);
    marvy_game_init(&game, 2, 0);
    assert(marvy_game_press(&game) == MARVY_EVENT_START);
    assert(game.ring_state == MARVY_RING_READY && game.rings_left == 10);
}

static void test_throw_ignores_repeated_input(void)
{
    marvy_game_t game;
    start(&game, 1, 0);
    clear_lane(&game);
    assert(marvy_game_press(&game) == MARVY_EVENT_THROW);
    assert(game.rings_left == 9 && game.ring_state == MARVY_RING_FLYING);
    for (unsigned i = 0; i < 100; ++i) {
        assert(marvy_game_press(&game) == 0);
    }
    assert(game.rings_left == 9);
    assert(marvy_game_step(&game, MARVY_FLIGHT_MS - 1) == 0);
    assert(game.ring_state == MARVY_RING_FLYING && game.score == 0);
    assert(marvy_game_step(&game, 1) == MARVY_EVENT_MISS);
    assert(game.ring_state == MARVY_RING_FEEDBACK);
    assert(marvy_game_feedback_ms(&game) == 400);
    assert(marvy_game_press(&game) == 0);
    assert(marvy_game_step(&game, MARVY_FEEDBACK_MS) == 0);
    assert(game.ring_state == MARVY_RING_READY);
    assert(marvy_game_press(&game) == MARVY_EVENT_THROW);
    assert(game.rings_left == 8);
}

static void test_ordinary_and_marvy_scores(void)
{
    marvy_game_t game;
    start(&game, 1, 0);
    clear_lane(&game);
    target(&game, 0, MARVY_AIM_X, 0, false);
    assert(marvy_game_press(&game) == MARVY_EVENT_THROW);
    uint32_t events = marvy_game_step(&game, MARVY_FLIGHT_MS);
    assert(events == MARVY_EVENT_HIT);
    assert(game.score == 1 && game.last_points == 1 && !game.last_hit_marvy);
    assert(marvy_game_feedback_ms(&game) == 400);
    assert(game.last_hit_sheep == 0);
    assert(game.sheep[0].hidden_ms > 0);
    assert(marvy_game_step(&game, 0) == 0);
    marvy_game_step(&game, MARVY_FEEDBACK_MS);
    clear_lane(&game);
    target(&game, 2, MARVY_AIM_X, 0, true);
    marvy_game_press(&game);
    events = marvy_game_step(&game, MARVY_FLIGHT_MS);
    assert((events & (MARVY_EVENT_HIT | MARVY_EVENT_MARVY_HIT)) ==
           (MARVY_EVENT_HIT | MARVY_EVENT_MARVY_HIT));
    assert(game.score == 6 && game.last_points == 5 && game.last_hit_marvy);
    assert(marvy_game_feedback_ms(&game) == 1200);
    assert(game.last_hit_sheep == 2);
}

static void test_collision_uses_landing_position(void)
{
    marvy_game_t game;
    start(&game, 1, 0);
    clear_lane(&game);
    target(&game, 0, 96, 40, false);
    marvy_game_press(&game);
    assert((marvy_game_step(&game, MARVY_FLIGHT_MS) & MARVY_EVENT_HIT) != 0);
    assert(fabsf(game.last_hit_x - MARVY_AIM_X) < 0.02f);

    start(&game, 1, 0);
    clear_lane(&game);
    target(&game, 0, MARVY_AIM_X, 80, false);
    marvy_game_press(&game);
    assert(marvy_game_step(&game, MARVY_FLIGHT_MS) == MARVY_EVENT_MISS);
    assert(game.score == 0 && game.last_points == 0);
}

static void test_nearest_sheep_only_and_boundary(void)
{
    marvy_game_t game;
    start(&game, 1, 0);
    clear_lane(&game);
    target(&game, 0, MARVY_AIM_X - 18, 0, true);
    target(&game, 1, MARVY_AIM_X + 3, 0, false);
    marvy_game_press(&game);
    marvy_game_step(&game, MARVY_FLIGHT_MS);
    assert(game.score == 1 && game.last_hit_sheep == 1);
    assert(game.sheep[0].hidden_ms == 0);
    start(&game, 1, 0);
    clear_lane(&game);
    target(&game, 0, MARVY_AIM_X + MARVY_HIT_RADIUS, 0, false);
    marvy_game_press(&game);
    assert((marvy_game_step(&game, MARVY_FLIGHT_MS) & MARVY_EVENT_HIT) != 0);
}

static void test_last_ring_settles_and_best_is_retained(void)
{
    marvy_game_t game;
    start(&game, 7, 12);
    for (unsigned i = 0; i < MARVY_RING_COUNT; ++i) {
        clear_lane(&game);
        target(&game, 0, MARVY_AIM_X, 0, true);
        assert(marvy_game_press(&game) == MARVY_EVENT_THROW);
        assert(game.phase == MARVY_PHASE_PLAYING);
        marvy_game_step(&game, MARVY_FLIGHT_MS);
        assert(game.phase == MARVY_PHASE_PLAYING);
        assert(game.score == (i + 1) * 5);
        uint32_t events = marvy_game_step(&game, 1200 - 1);
        assert((events & MARVY_EVENT_FINISH) == 0);
        assert(game.phase == MARVY_PHASE_PLAYING);
        events = marvy_game_step(&game, 1);
        if (i == MARVY_RING_COUNT - 1) {
            assert((events & (MARVY_EVENT_FINISH | MARVY_EVENT_NEW_BEST)) ==
                   (MARVY_EVENT_FINISH | MARVY_EVENT_NEW_BEST));
            assert(game.phase == MARVY_PHASE_RESULTS && game.rings_left == 0);
            assert(game.best_score == 50);
        } else {
            assert((events & MARVY_EVENT_FINISH) == 0);
        }
    }
    assert(marvy_game_step(&game, 5000) == 0);
    assert(marvy_game_press(&game) == MARVY_EVENT_START);
    assert(game.phase == MARVY_PHASE_PLAYING && game.score == 0);
    assert(game.rings_left == MARVY_RING_COUNT && game.best_score == 50);
}

static void test_no_new_best_on_lower_or_equal_score(void)
{
    marvy_game_t game;
    start(&game, 2, 10);
    for (unsigned i = 0; i < MARVY_RING_COUNT; ++i) {
        clear_lane(&game);
        target(&game, 0, MARVY_AIM_X, 0, false);
        marvy_game_press(&game);
        uint32_t events = marvy_game_step(&game, MARVY_FLIGHT_MS + MARVY_FEEDBACK_MS);
        assert((events & MARVY_EVENT_NEW_BEST) == 0);
    }
    assert(game.best_score == 10 && game.score == 10);
    assert(game.phase == MARVY_PHASE_RESULTS);
}

static void test_appearance_event_once_and_after_reentry(void)
{
    marvy_game_t game;
    start(&game, 2, 0);
    clear_lane(&game);
    target(&game, 0, -30, 40, true);
    assert(marvy_game_step(&game, 100) == 0);
    assert((marvy_game_step(&game, 200) & MARVY_EVENT_MARVY_APPEAR) != 0);
    assert(marvy_game_step(&game, 100) == 0);
    target(&game, 0, 100, 0, true);
    assert(marvy_game_step(&game, 1) == MARVY_EVENT_MARVY_APPEAR);
    assert(marvy_game_step(&game, 1) == 0);
}

static void test_captured_sheep_respawns_without_double_scoring(void)
{
    marvy_game_t game;
    start(&game, 31, 0);
    clear_lane(&game);
    target(&game, 0, MARVY_AIM_X, 0, false);
    marvy_game_press(&game);
    assert(marvy_game_step(&game, MARVY_FLIGHT_MS) == MARVY_EVENT_HIT);
    assert(game.score == 1 && game.sheep[0].hidden_ms > 0);
    marvy_game_step(&game, 1000);
    assert(game.sheep[0].hidden_ms == 0 && game.sheep[0].speed != 0);
    assert(game.score == 1);
    assert(game.sheep[0].x < 0 || game.sheep[0].x > MARVY_WIDTH);
}

static void test_large_step_and_deterministic_movement(void)
{
    marvy_game_t a, b;
    start(&a, 0, 9);
    start(&b, 0, 9);
    uint32_t combined = marvy_game_step(&a, 12000);
    uint32_t divided = 0;
    for (unsigned i = 0; i < 1200; ++i) {
        divided |= marvy_game_step(&b, 10);
    }
    assert(combined == divided && a.rng_state == b.rng_state);
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        assert(fabsf(a.sheep[i].x - b.sheep[i].x) < 0.01f);
        assert(a.sheep[i].speed == b.sheep[i].speed);
        assert(a.sheep[i].marvy == b.sheep[i].marvy);
    }
    marvy_game_step(&a, UINT32_MAX);
    assert(a.rings_left == 10 && a.score == 0 && a.best_score == 9);

    start(&a, 1, 0);
    clear_lane(&a);
    marvy_game_press(&a);
    assert(marvy_game_step(&a, 10000) == MARVY_EVENT_MISS);
    assert(a.ring_state == MARVY_RING_READY && a.rings_left == 9);

    marvy_game_init(&a, 1, 0);
    uint32_t events = marvy_game_step(&a, MARVY_INTRO_MS + 1000);
    assert((events & MARVY_EVENT_START) != 0);
    assert(a.phase == MARVY_PHASE_PLAYING && a.phase_ms == 1000);
}

static void assert_sheep_speed_classes(const marvy_game_t *game)
{
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        float speed = fabsf(game->sheep[i].speed);
        if (game->sheep[i].marvy) {
            assert(speed >= 48 && speed <= 56);
            assert(speed > 42); /* Faster than even the fastest ordinary sheep. */
        } else {
            assert(speed >= 30 && speed <= 42);
        }
    }
}

static void test_marvy_speeds_are_strictly_faster_at_start_and_respawn(void)
{
    unsigned ordinary = 0, branded = 0;
    for (uint32_t seed = 1; seed <= 200; ++seed) {
        marvy_game_t game;
        start(&game, seed, 0);
        assert_sheep_speed_classes(&game);
        for (unsigned traversal = 0; traversal < 20; ++traversal) {
            /* Drive real boundary re-entry rather than calling a spawn helper. */
            for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
                game.sheep[i].x = MARVY_WIDTH + 45;
            }
            marvy_game_step(&game, 1);
            assert_sheep_speed_classes(&game);
            for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
                if (game.sheep[i].marvy) ++branded;
                else ++ordinary;
            }
        }
    }
    assert(ordinary > 0 && branded > 0);
}

static void test_marvy_celebration_can_be_read_before_next_throw(void)
{
    marvy_game_t game;
    start(&game, 4, 0);
    clear_lane(&game);
    target(&game, 0, MARVY_AIM_X, 0, true);
    assert(marvy_game_press(&game) == MARVY_EVENT_THROW);
    uint32_t events = marvy_game_step(&game, MARVY_FLIGHT_MS);
    assert((events & MARVY_EVENT_MARVY_HIT) != 0);
    assert(game.score == 5 && game.last_points == 5);
    assert(marvy_game_feedback_ms(&game) == 1200);
    assert(game.sheep[0].hidden_ms == 1700);
    assert(marvy_game_press(&game) == 0);
    marvy_game_step(&game, 1199);
    assert(game.ring_state == MARVY_RING_FEEDBACK && game.ring_ms == 1199);
    assert(game.last_hit_marvy && game.score == 5);
    assert(game.sheep[0].hidden_ms == 501);
    assert(marvy_game_press(&game) == 0 && game.rings_left == 9);
    marvy_game_step(&game, 1);
    assert(game.ring_state == MARVY_RING_READY);
    assert(game.sheep[0].hidden_ms == 500);
    marvy_game_step(&game, 499);
    assert(game.sheep[0].hidden_ms == 1 && game.score == 5);
    marvy_game_step(&game, 1);
    assert(game.sheep[0].hidden_ms == 0 && game.score == 5);
    assert(game.sheep[0].x < 0 || game.sheep[0].x > MARVY_WIDTH);
    assert(marvy_game_press(&game) == MARVY_EVENT_THROW);
    assert(game.rings_left == 8);
}

int main(void)
{
    test_marvy_speeds_are_strictly_faster_at_start_and_respawn();
    test_marvy_celebration_can_be_read_before_next_throw();
    test_intro_and_skip();
    test_throw_ignores_repeated_input();
    test_ordinary_and_marvy_scores();
    test_collision_uses_landing_position();
    test_nearest_sheep_only_and_boundary();
    test_last_ring_settles_and_best_is_retained();
    test_no_new_best_on_lower_or_equal_score();
    test_appearance_event_once_and_after_reentry();
    test_captured_sheep_respawns_without_double_scoring();
    test_large_step_and_deterministic_movement();
    puts("Marvy game tests: PASS");
    return 0;
}
