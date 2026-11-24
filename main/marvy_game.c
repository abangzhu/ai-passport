#include "marvy_game.h"

#include <float.h>
#include <limits.h>
#include <string.h>

enum {
    SIMULATION_SLICE_MS = 10,
    CAPTURE_GRACE_MS = 500,
};

static uint32_t next_random(marvy_game_t *game)
{
    uint32_t x = game->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    game->rng_state = x;
    return x;
}

static uint32_t saturated_add(uint32_t a, uint32_t b)
{
    return UINT32_MAX - a < b ? UINT32_MAX : a + b;
}

static float sheep_speed(marvy_game_t *game, bool marvy)
{
    /* Separate ranges guarantee every branded sheep is faster than every
     * ordinary sheep, including after either type respawns. */
    return marvy ? 48.0f + (float)(next_random(game) % 9u)
                 : 30.0f + (float)(next_random(game) % 13u);
}

static void spawn_sheep(marvy_game_t *game, marvy_sheep_t *sheep)
{
    bool rightward = (next_random(game) & 1u) != 0;
    sheep->x = rightward ? -32.0f : MARVY_WIDTH + 32.0f;
    sheep->marvy = next_random(game) % 5u == 0;
    float speed = sheep_speed(game, sheep->marvy);
    sheep->speed = rightward ? speed : -speed;
    sheep->announced = false;
    sheep->hidden_ms = 0;
}

static void start_round(marvy_game_t *game)
{
    game->phase = MARVY_PHASE_PLAYING;
    game->ring_state = MARVY_RING_READY;
    game->score = 0;
    game->rings_left = MARVY_RING_COUNT;
    game->phase_ms = 0;
    game->ring_ms = 0;
    game->ring_target_x = MARVY_TARGET_MIN_X;
    game->last_points = 0;
    game->last_hit_x = MARVY_AIM_X;
    game->last_hit_marvy = false;
    game->last_hit_sheep = -1;
    static const float positions[MARVY_SHEEP_COUNT] = {-26, 58, 170, 280};
    unsigned marvy_index = next_random(game) % MARVY_SHEEP_COUNT;
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        marvy_sheep_t *sheep = &game->sheep[i];
        sheep->x = positions[i];
        sheep->marvy = i == marvy_index;
        float speed = sheep_speed(game, sheep->marvy);
        sheep->speed = i % 2u == 0 ? speed : -speed;
        sheep->announced = false;
        sheep->hidden_ms = 0;
    }
}

void marvy_game_init(marvy_game_t *game, uint32_t seed, uint16_t best)
{
    memset(game, 0, sizeof(*game));
    game->rng_state = seed == 0 ? UINT32_C(0x9e3779b9) : seed;
    game->best_score = best;
    start_round(game);
    game->phase = MARVY_PHASE_INTRO;
}

uint32_t marvy_game_press(marvy_game_t *game)
{
    if (game->phase == MARVY_PHASE_INTRO) {
        game->phase = MARVY_PHASE_PLAYING;
        game->phase_ms = 0;
        return MARVY_EVENT_START;
    }
    if (game->phase == MARVY_PHASE_RESULTS) {
        start_round(game);
        return MARVY_EVENT_START;
    }
    if (game->ring_state != MARVY_RING_READY || game->rings_left == 0) {
        return 0;
    }
    game->ring_state = MARVY_RING_CHARGING;
    game->ring_ms = 0;
    game->ring_target_x = MARVY_TARGET_MIN_X;
    game->last_points = 0;
    game->last_hit_sheep = -1;
    game->last_hit_marvy = false;
    return 0;
}

float marvy_game_charge_target_x(uint32_t held_ms)
{
    if (held_ms >= MARVY_CHARGE_MS) return MARVY_TARGET_MAX_X;
    return MARVY_TARGET_MIN_X +
           (MARVY_TARGET_MAX_X - MARVY_TARGET_MIN_X) * ((float)held_ms / MARVY_CHARGE_MS);
}

uint32_t marvy_game_release(marvy_game_t *game)
{
    if (game->phase != MARVY_PHASE_PLAYING ||
        game->ring_state != MARVY_RING_CHARGING || game->rings_left == 0) return 0;
    game->ring_target_x = marvy_game_charge_target_x(game->ring_ms);
    --game->rings_left;
    game->ring_ms = 0;
    game->ring_state = MARVY_RING_FLYING;
    return MARVY_EVENT_THROW;
}

void marvy_game_cancel_charge(marvy_game_t *game)
{
    if (game->ring_state != MARVY_RING_CHARGING) return;
    game->ring_state = MARVY_RING_READY;
    game->ring_ms = 0;
    game->ring_target_x = MARVY_TARGET_MIN_X;
}

uint32_t marvy_game_feedback_ms(const marvy_game_t *game)
{
    return game->last_hit_marvy && game->last_points == 5
           ? MARVY_MARVY_FEEDBACK_MS : MARVY_FEEDBACK_MS;
}

static uint32_t move_sheep(marvy_game_t *game, uint32_t elapsed_ms)
{
    uint32_t events = 0;
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        marvy_sheep_t *sheep = &game->sheep[i];
        if (sheep->hidden_ms > 0) {
            if (sheep->hidden_ms > elapsed_ms) {
                sheep->hidden_ms -= elapsed_ms;
            } else {
                spawn_sheep(game, sheep);
            }
            continue;
        }
        sheep->x += sheep->speed * ((float)elapsed_ms / 1000.0f);
        if (sheep->x < -44.0f || sheep->x > MARVY_WIDTH + 44.0f) {
            spawn_sheep(game, sheep);
        }
        if (!sheep->announced && sheep->x >= -20.0f &&
            sheep->x <= MARVY_WIDTH + 20.0f) {
            sheep->announced = true;
            if (sheep->marvy) {
                events |= MARVY_EVENT_MARVY_APPEAR;
            }
        }
    }
    return events;
}

static uint32_t resolve_landing(marvy_game_t *game)
{
    int closest = -1;
    float distance = FLT_MAX;
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        const marvy_sheep_t *sheep = &game->sheep[i];
        float delta = sheep->x - game->ring_target_x;
        if (delta < 0) {
            delta = -delta;
        }
        if (sheep->hidden_ms == 0 && delta <= MARVY_HIT_RADIUS && delta < distance) {
            closest = (int)i;
            distance = delta;
        }
    }
    game->ring_state = MARVY_RING_FEEDBACK;
    game->ring_ms = 0;
    if (closest < 0) {
        return MARVY_EVENT_MISS;
    }
    marvy_sheep_t *sheep = &game->sheep[closest];
    game->last_hit_sheep = (int8_t)closest;
    game->last_hit_x = sheep->x;
    game->last_hit_marvy = sheep->marvy;
    game->last_points = sheep->marvy ? 5 : 1;
    game->score += game->last_points;
    sheep->hidden_ms = marvy_game_feedback_ms(game) + CAPTURE_GRACE_MS;
    return MARVY_EVENT_HIT | (sheep->marvy ? MARVY_EVENT_MARVY_HIT : 0);
}

static uint32_t finish_feedback(marvy_game_t *game)
{
    game->ring_ms = 0;
    game->ring_state = MARVY_RING_READY;
    if (game->rings_left > 0) {
        return 0;
    }
    game->phase = MARVY_PHASE_RESULTS;
    game->phase_ms = 0;
    uint32_t events = MARVY_EVENT_FINISH;
    if (game->score > game->best_score) {
        game->best_score = game->score;
        events |= MARVY_EVENT_NEW_BEST;
    }
    return events;
}

uint32_t marvy_game_step(marvy_game_t *game, uint32_t elapsed_ms)
{
    uint32_t events = 0;
    if (elapsed_ms > MARVY_MAX_STEP_MS) {
        elapsed_ms = MARVY_MAX_STEP_MS;
    }
    while (elapsed_ms > 0) {
        uint32_t slice = elapsed_ms < SIMULATION_SLICE_MS ? elapsed_ms : SIMULATION_SLICE_MS;
        if (game->phase == MARVY_PHASE_RESULTS || game->phase == MARVY_PHASE_INTRO) {
            game->phase_ms = saturated_add(game->phase_ms, elapsed_ms);
            break;
        }
        if (game->ring_state == MARVY_RING_FLYING || game->ring_state == MARVY_RING_FEEDBACK) {
            uint32_t duration = game->ring_state == MARVY_RING_FLYING ?
                                MARVY_FLIGHT_MS : marvy_game_feedback_ms(game);
            uint32_t remaining = duration - game->ring_ms;
            if (slice > remaining) {
                slice = remaining;
            }
        }
        events |= move_sheep(game, slice);
        game->phase_ms = saturated_add(game->phase_ms, slice);
        elapsed_ms -= slice;
        if (game->ring_state == MARVY_RING_CHARGING) {
            game->ring_ms = game->ring_ms + slice > MARVY_CHARGE_MS
                            ? MARVY_CHARGE_MS : game->ring_ms + slice;
        } else if (game->ring_state == MARVY_RING_FLYING) {
            game->ring_ms += slice;
            if (game->ring_ms == MARVY_FLIGHT_MS) {
                events |= resolve_landing(game);
            }
        } else if (game->ring_state == MARVY_RING_FEEDBACK) {
            game->ring_ms += slice;
            if (game->ring_ms == marvy_game_feedback_ms(game)) {
                events |= finish_feedback(game);
            }
        }
    }
    return events;
}
