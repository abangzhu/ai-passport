#include "marvy_completion.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static marvy_game_t result(unsigned level, unsigned stars)
{
    marvy_game_t game = {0};
    game.phase = MARVY_PHASE_RESULTS;
    game.passed = true;
    game.level_index = (uint8_t)level;
    game.stars = (uint8_t)stars;
    return game;
}

static marvy_completion_frame_t frame_at(marvy_game_t *game, uint32_t at_ms)
{
    marvy_completion_frame_t frame;
    game->phase_ms = at_ms;
    marvy_game_t before = *game;
    memset(&frame, 0xa5, sizeof(frame));
    marvy_completion_frame(game, &frame);
    assert(memcmp(game, &before, sizeof(before)) == 0);
    return frame;
}

static void assert_empty(const marvy_completion_frame_t *frame)
{
    const marvy_completion_frame_t empty = {0};
    assert(memcmp(frame, &empty, sizeof(empty)) == 0);
}

static void assert_settled(const marvy_completion_frame_t *frame,
                            unsigned earned, bool finale)
{
    assert(!frame->active && frame->finale == finale);
    assert(frame->lit_stars == earned);
    assert(frame->heading_lift == 0 && frame->sheep_lift == 0 && frame->trophy_lift == 0);
    for (unsigned i = 0; i < 3; ++i) assert(frame->star_lift[i] == 0);
    for (unsigned i = 0; i < MARVY_COMPLETION_PARTICLES; ++i) {
        assert(!frame->particles[i].visible && frame->particles[i].opacity == 0);
    }
}

static void test_invalid_and_failed_results_are_empty(void)
{
    marvy_completion_frame_t frame;
    memset(&frame, 0xa5, sizeof(frame));
    marvy_completion_frame(NULL, &frame);
    assert_empty(&frame);
    marvy_completion_frame(NULL, NULL);
    marvy_game_t game = result(0, 3);
    marvy_completion_frame(&game, NULL);
    for (unsigned phase = 0; phase <= MARVY_PHASE_CONTROLS; ++phase) {
        game.phase = (marvy_phase_t)phase;
        if (game.phase == MARVY_PHASE_RESULTS) continue;
        frame = frame_at(&game, 500);
        assert_empty(&frame);
    }
    game.phase = (marvy_phase_t)UINT8_MAX;
    frame = frame_at(&game, UINT32_MAX);
    assert_empty(&frame);
    game.phase = MARVY_PHASE_RESULTS;
    game.passed = false;
    frame = frame_at(&game, 500);
    assert_empty(&frame);
    game.passed = true;
    game.level_index = MARVY_LEVEL_COUNT;
    frame = frame_at(&game, 500);
    assert_empty(&frame);
}

static void test_stars_light_in_order_and_only_earned_stars_bounce(void)
{
    marvy_game_t game = result(0, 3);
    const uint32_t at[] = {0, 119, 120, 399, 400, 679, 680, 920};
    const unsigned earned[] = {0, 0, 1, 1, 2, 2, 3, 3};
    for (unsigned i = 0; i < sizeof(at) / sizeof(at[0]); ++i) {
        marvy_completion_frame_t frame = frame_at(&game, at[i]);
        assert(frame.active && !frame.finale && frame.lit_stars == earned[i]);
    }
    for (unsigned star = 0; star < 3; ++star) {
        marvy_completion_frame_t frame = frame_at(&game, 120 + star * 280 + 80);
        assert(frame.star_lift[star] > 0 && frame.star_lift[star] <= 9);
        frame = frame_at(&game, 120 + star * 280 + 240);
        assert(frame.star_lift[star] == 0);
    }
    for (unsigned stars = 0; stars <= 3; ++stars) {
        game.stars = (uint8_t)stars;
        marvy_completion_frame_t frame = frame_at(&game, 760);
        assert(frame.lit_stars == stars);
        for (unsigned i = stars; i < 3; ++i) assert(frame.star_lift[i] == 0);
    }
    game.stars = UINT8_MAX;
    marvy_completion_frame_t frame = frame_at(&game, 1000);
    assert(frame.lit_stars == 3);
}

static void test_duration_finale_and_large_time_boundaries(void)
{
    marvy_game_t game = result(0, 2);
    marvy_completion_frame_t frame = frame_at(&game, 1799);
    assert(frame.active && !frame.finale && frame.sheep_lift == 0 && frame.trophy_lift == 0);
    frame = frame_at(&game, 1800);
    assert_settled(&frame, 2, false);
    frame = frame_at(&game, UINT32_MAX);
    assert_settled(&frame, 2, false);
    game.level_index = MARVY_LEVEL_COUNT - 1;
    frame = frame_at(&game, 0);
    assert(frame.active && frame.finale && frame.trophy_lift > 0);
    frame = frame_at(&game, 120);
    assert(frame.sheep_lift > 0);
    frame = frame_at(&game, 1800);
    assert(frame.active && frame.finale);
    frame = frame_at(&game, 2999);
    assert(frame.active && frame.finale);
    frame = frame_at(&game, 3000);
    assert_settled(&frame, 2, true);
    frame = frame_at(&game, UINT32_MAX);
    assert_settled(&frame, 2, true);
}

static void test_particles_stay_in_safe_pixel_regions_and_fade(void)
{
    bool saw_visible[MARVY_COMPLETION_PARTICLES] = {false};
    bool saw_faded[MARVY_COMPLETION_PARTICLES] = {false};
    for (unsigned level = 0; level < MARVY_LEVEL_COUNT; ++level) {
        marvy_game_t game = result(level, 3);
        uint32_t duration = level + 1 == MARVY_LEVEL_COUNT ? 3000 : 1800;
        for (uint32_t at = 0; at <= duration; ++at) {
            marvy_completion_frame_t frame = frame_at(&game, at);
            assert(frame.heading_lift >= 0 && frame.heading_lift <= 3);
            assert(frame.sheep_lift >= 0 && frame.sheep_lift <= 9);
            assert(frame.trophy_lift >= 0 && frame.trophy_lift <= 12);
            for (unsigned star = 0; star < 3; ++star) {
                assert(frame.star_lift[star] >= 0 && frame.star_lift[star] <= 9);
            }
            for (unsigned i = 0; i < MARVY_COMPLETION_PARTICLES; ++i) {
                const marvy_completion_particle_t *particle = &frame.particles[i];
                if (!particle->visible) {
                    assert(particle->opacity == 0);
                    continue;
                }
                assert(frame.active && particle->opacity > 0);
                assert(particle->x % 3 == 0 && particle->y % 3 == 0);
                if (i < 8) {
                    assert(particle->x >= 13 && particle->x <= 61);
                    assert(particle->y >= 64 && particle->y <= 116);
                } else if (i < 16) {
                    assert(particle->x >= 178 && particle->x <= 229);
                    assert(particle->y >= 64 && particle->y <= 116);
                } else {
                    assert(frame.finale);
                    assert(particle->x >= 111 && particle->x <= 129);
                    assert(particle->y >= 36 && particle->y <= 54);
                }
                saw_visible[i] = true;
                if (particle->opacity < 128) saw_faded[i] = true;
            }
        }
    }
    for (unsigned i = 0; i < MARVY_COMPLETION_PARTICLES; ++i) {
        assert(saw_visible[i] && saw_faded[i]);
    }
}

static void test_animation_is_repeatable_without_state_or_input_side_effects(void)
{
    marvy_game_t game = result(MARVY_LEVEL_COUNT - 1, 3);
    marvy_completion_frame_t first = frame_at(&game, 300);
    marvy_completion_frame_t ended = frame_at(&game, 3000);
    assert_settled(&ended, 3, true);
    game.phase = MARVY_PHASE_BRIEF;
    marvy_completion_frame_t hidden = frame_at(&game, 100);
    assert_empty(&hidden);
    game.phase = MARVY_PHASE_RESULTS;
    marvy_completion_frame_t repeated = frame_at(&game, 300);
    assert(memcmp(&first, &repeated, sizeof(first)) == 0);
}

int main(void)
{
    test_invalid_and_failed_results_are_empty();
    test_stars_light_in_order_and_only_earned_stars_bounce();
    test_duration_finale_and_large_time_boundaries();
    test_particles_stay_in_safe_pixel_regions_and_fade();
    test_animation_is_repeatable_without_state_or_input_side_effects();
    puts("Marvy completion animation: PASS");
    return 0;
}
