#include "marvy_game.h"
#include "marvy_completion.h"

#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

enum { SIMULATION_SLICE_MS = 10, CAPTURE_GRACE_MS = 500, SPAWN_RETRY_MS = 200 };

static const marvy_level_config_t s_levels[MARVY_LEVEL_COUNT] = {
    {45000, 4, 0,    70, 25, 1, true},
    {45000, 5, 0,    70, 25, 1, false},
    {45000, 6, 0,    90, 25, 1, false},
    {45000, 7, 6000, 90, 25, 1, false},
    {45000, 9, 6000,115, 30, 2, false},
    {45000,10, 4500,115, 30, 2, false},
};

const marvy_level_config_t *marvy_game_level_config(unsigned level_index)
{
    return level_index < MARVY_LEVEL_COUNT ? &s_levels[level_index] : NULL;
}

uint32_t marvy_game_remaining_ms(const marvy_game_t *game)
{
    uint32_t duration = s_levels[game->level_index].duration_ms;
    return game->stage_elapsed_ms < duration ? duration - game->stage_elapsed_ms : 0;
}

static uint32_t next_random(marvy_game_t *game)
{
    uint32_t x = game->rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    game->rng_state = x;
    return x;
}

static uint32_t saturated_add(uint32_t a, uint32_t b)
{
    return UINT32_MAX - a < b ? UINT32_MAX : a + b;
}

static uint16_t add_counter(uint16_t value, unsigned add)
{
    return add > UINT16_MAX - (unsigned)value ? UINT16_MAX : (uint16_t)(value + add);
}

static float sheep_speed(marvy_game_t *game, bool marvy)
{
    float base = marvy ? 48.0f + (float)(next_random(game) % 9u)
                       : 30.0f + (float)(next_random(game) % 13u);
    return base * (s_levels[game->level_index].speed_percent / 100.0f);
}

static void refill_bag(marvy_game_t *game)
{
    bool advanced = s_levels[game->level_index].marvy_percent == 30;
    game->spawn_bag_size = advanced ? 10 : 4;
    game->spawn_bag_next = 0;
    for (unsigned i = 0; i < game->spawn_bag_size; ++i) {
        game->spawn_bag[i] = i < (advanced ? 3u : 1u);
    }
    for (unsigned i = game->spawn_bag_size - 1; i > 0; --i) {
        unsigned j = next_random(game) % (i + 1);
        uint8_t swap = game->spawn_bag[i];
        game->spawn_bag[i] = game->spawn_bag[j];
        game->spawn_bag[j] = swap;
    }
}

static void spawn_sheep(marvy_game_t *game, marvy_sheep_t *sheep)
{
    if (game->spawn_bag_next >= game->spawn_bag_size) refill_bag(game);
    unsigned branded = 0;
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        const marvy_sheep_t *other = &game->sheep[i];
        if (other != sheep && other->hidden_ms == 0 && other->marvy) ++branded;
    }
    unsigned next = game->spawn_bag_next;
    if (game->spawn_bag[next] && branded >= s_levels[game->level_index].marvy_cap) {
        /* Reorder ordinary entries ahead of a currently blocked branded one.
         * Never discard a bag quota or exceed the simultaneous Marvy cap. */
        unsigned ordinary = next + 1;
        while (ordinary < game->spawn_bag_size && game->spawn_bag[ordinary]) ++ordinary;
        if (ordinary == game->spawn_bag_size) {
            sheep->hidden_ms = SPAWN_RETRY_MS;
            return;
        }
        game->spawn_bag[next] = 0;
        game->spawn_bag[ordinary] = 1;
    }
    bool rightward = (next_random(game) & 1u) != 0;
    sheep->x = rightward ? -32.0f : MARVY_WIDTH + 32.0f;
    sheep->marvy = game->spawn_bag[game->spawn_bag_next++] != 0;
    float speed = sheep_speed(game, sheep->marvy);
    sheep->speed = rightward ? speed : -speed;
    sheep->announced = false;
    sheep->hidden_ms = 0;
}

static void reset_round(marvy_game_t *game, marvy_phase_t phase)
{
    marvy_progress_t progress = game->progress;
    uint32_t rng = game->rng_state;
    uint16_t best = game->best_score;
    uint8_t level = game->level_index;
    memset(game, 0, sizeof(*game));
    game->progress = progress;
    game->rng_state = rng;
    game->best_score = best;
    game->level_index = level;
    game->phase = phase;
    game->pause_selection = MARVY_PAUSE_CONTINUE;
    game->result_selection = MARVY_RESULT_RETRY;
    game->ring_state = MARVY_RING_READY;
    game->ring_target_x = MARVY_TARGET_MIN_X;
    game->last_hit_x = MARVY_AIM_X;
    game->last_hit_sheep = -1;
    static const float positions[MARVY_SHEEP_COUNT] = {-26, 58, 170, 280};
    unsigned marvy_index = next_random(game) % MARVY_SHEEP_COUNT;
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        marvy_sheep_t *sheep = &game->sheep[i];
        sheep->x = positions[i];
        sheep->marvy = i == marvy_index;
        float speed = sheep_speed(game, sheep->marvy);
        sheep->speed = i % 2u == 0 ? speed : -speed;
    }
}

void marvy_game_init(marvy_game_t *game, uint32_t seed, uint16_t legacy_best)
{
    memset(game, 0, sizeof(*game));
    marvy_progress_init(&game->progress);
    game->rng_state = seed == 0 ? UINT32_C(0x9e3779b9) : seed;
    game->best_score = legacy_best;
    reset_round(game, MARVY_PHASE_INTRO);
}

void marvy_game_apply_progress(marvy_game_t *game, const marvy_progress_t *progress)
{
    (void)marvy_progress_merge(&game->progress, progress);
    if (game->level_index >= game->progress.unlocked_count) {
        game->level_index = game->progress.unlocked_count - 1;
    }
}

void marvy_game_brief_move(marvy_game_t *game, int direction)
{
    if (game->phase != MARVY_PHASE_BRIEF || direction == 0) return;
    unsigned count = game->progress.unlocked_count;
    game->level_index = (uint8_t)((game->level_index + (direction > 0 ? 1 : count - 1)) % count);
}

void marvy_game_intro_move(marvy_game_t *game, int direction)
{
    if (game->phase != MARVY_PHASE_INTRO || direction == 0) return;
    game->intro_selection = game->intro_selection == MARVY_INTRO_START
                            ? MARVY_INTRO_CONTROLS : MARVY_INTRO_START;
}

bool marvy_game_can_next(const marvy_game_t *game)
{
    return game->phase == MARVY_PHASE_RESULTS && game->passed &&
           game->level_index + 1u < MARVY_LEVEL_COUNT;
}

bool marvy_game_can_follow(const marvy_game_t *game)
{
    return game->phase == MARVY_PHASE_RESULTS && game->passed &&
           game->level_index + 1u == MARVY_LEVEL_COUNT;
}

void marvy_game_result_move(marvy_game_t *game, int direction)
{
    if (game->phase != MARVY_PHASE_RESULTS || direction == 0) return;
    unsigned selection = game->result_selection;
    do {
        selection = (selection + (direction > 0 ? 1 : 3)) % 4;
    } while ((selection == MARVY_RESULT_NEXT && !marvy_game_can_next(game)) ||
             (selection == MARVY_RESULT_FOLLOW && !marvy_game_can_follow(game)));
    game->result_selection = (marvy_result_selection_t)selection;
}

uint32_t marvy_game_result_confirm(marvy_game_t *game)
{
    if (game->phase != MARVY_PHASE_RESULTS) return 0;
    if (game->result_selection == MARVY_RESULT_WELCOME) {
        reset_round(game, MARVY_PHASE_INTRO);
    } else if (game->result_selection == MARVY_RESULT_RETRY) {
        reset_round(game, MARVY_PHASE_BRIEF);
    } else if (game->result_selection == MARVY_RESULT_NEXT && marvy_game_can_next(game)) {
        ++game->level_index;
        reset_round(game, MARVY_PHASE_BRIEF);
    } else if (game->result_selection == MARVY_RESULT_FOLLOW && marvy_game_can_follow(game)) {
        game->phase = MARVY_PHASE_FOLLOW;
    }
    return 0;
}

uint32_t marvy_game_press(marvy_game_t *game)
{
    if (game->phase == MARVY_PHASE_INTRO) {
        if (game->intro_selection == MARVY_INTRO_CONTROLS) {
            game->phase = MARVY_PHASE_CONTROLS;
            game->phase_ms = 0;
        } else {
            game->phase = MARVY_PHASE_DEMO;
            game->phase_ms = 0;
        }
        return 0;
    }
    if (game->phase == MARVY_PHASE_CONTROLS) {
        reset_round(game, MARVY_PHASE_INTRO);
        return 0;
    }
    if (game->phase == MARVY_PHASE_DEMO) {
        game->phase = MARVY_PHASE_BRIEF;
        game->phase_ms = 0;
        return 0;
    }
    if (game->phase == MARVY_PHASE_FOLLOW) {
        game->phase = MARVY_PHASE_RESULTS;
        game->phase_ms = MARVY_COMPLETION_FINALE_MS;
        return 0;
    }
    if (game->phase == MARVY_PHASE_BRIEF) {
        reset_round(game, MARVY_PHASE_PLAYING);
        return MARVY_EVENT_START;
    }
    if (game->phase == MARVY_PHASE_RESULTS) return marvy_game_result_confirm(game);
    if (game->phase == MARVY_PHASE_PAUSED) {
        marvy_game_pause_confirm(game);
        return 0;
    }
    if (game->phase != MARVY_PHASE_PLAYING || game->time_expired ||
        game->ring_state != MARVY_RING_READY) return 0;
    game->ring_state = MARVY_RING_CHARGING;
    game->ring_ms = 0;
    game->ring_target_x = MARVY_TARGET_MIN_X;
    game->last_points = 0;
    game->last_hit_sheep = -1;
    game->last_hit_marvy = false;
    return 0;
}

void marvy_game_pause(marvy_game_t *game)
{
    if (game->phase != MARVY_PHASE_PLAYING) return;
    marvy_game_cancel_charge(game);
    game->pause_selection = MARVY_PAUSE_CONTINUE;
    game->phase = MARVY_PHASE_PAUSED;
}

void marvy_game_pause_select(marvy_game_t *game, marvy_pause_selection_t selection)
{
    if (game->phase == MARVY_PHASE_PAUSED &&
        (selection == MARVY_PAUSE_CONTINUE || selection == MARVY_PAUSE_EXIT)) {
        game->pause_selection = selection;
    }
}

void marvy_game_pause_move(marvy_game_t *game, int direction)
{
    if (game->phase != MARVY_PHASE_PAUSED || direction == 0) return;
    game->pause_selection = game->pause_selection == MARVY_PAUSE_CONTINUE
                            ? MARVY_PAUSE_EXIT : MARVY_PAUSE_CONTINUE;
}

void marvy_game_pause_confirm(marvy_game_t *game)
{
    if (game->phase != MARVY_PHASE_PAUSED) return;
    if (game->pause_selection == MARVY_PAUSE_EXIT) reset_round(game, MARVY_PHASE_INTRO);
    else game->phase = MARVY_PHASE_PLAYING;
}

float marvy_game_charge_target_x(uint32_t held_ms)
{
    if (held_ms >= MARVY_CHARGE_MS) return MARVY_TARGET_MAX_X;
    return MARVY_TARGET_MIN_X + (MARVY_TARGET_MAX_X - MARVY_TARGET_MIN_X) *
                               ((float)held_ms / MARVY_CHARGE_MS);
}

static uint32_t launch_ring(marvy_game_t *game, uint32_t charge_ms)
{
    game->ring_target_x = marvy_game_charge_target_x(charge_ms);
    game->throws = add_counter(game->throws, 1);
    game->ring_ms = 0;
    game->ring_state = MARVY_RING_FLYING;
    game->last_points = 0;
    game->last_hit_x = MARVY_AIM_X;
    game->last_hit_marvy = false;
    game->last_hit_sheep = -1;
    return MARVY_EVENT_THROW;
}

uint32_t marvy_game_release(marvy_game_t *game)
{
    if (game->phase != MARVY_PHASE_PLAYING || game->time_expired ||
        game->ring_state != MARVY_RING_CHARGING) return 0;
    return launch_ring(game, game->ring_ms);
}

void marvy_game_cancel_charge(marvy_game_t *game)
{
    if (game->ring_state != MARVY_RING_CHARGING) return;
    game->ring_state = MARVY_RING_READY;
    game->ring_ms = 0;
    game->ring_target_x = MARVY_TARGET_MIN_X;
}

unsigned marvy_game_countdown(const marvy_game_t *game)
{
    uint32_t limit = s_levels[game->level_index].turn_limit_ms;
    if (game->phase != MARVY_PHASE_PLAYING || limit == 0 || game->time_expired ||
        marvy_game_remaining_ms(game) <= 5000 ||
        (game->ring_state != MARVY_RING_READY && game->ring_state != MARVY_RING_CHARGING) ||
        game->turn_ms < limit - 3000 || game->turn_ms >= limit) return 0;
    return (limit - game->turn_ms + 999) / 1000;
}

uint32_t marvy_game_feedback_ms(const marvy_game_t *game)
{
    (void)game;
    return MARVY_FEEDBACK_MS;
}

static uint32_t move_sheep(marvy_game_t *game, uint32_t elapsed_ms)
{
    uint32_t events = 0;
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        marvy_sheep_t *sheep = &game->sheep[i];
        if (sheep->hidden_ms > 0) {
            if (sheep->hidden_ms > elapsed_ms) sheep->hidden_ms -= elapsed_ms;
            else spawn_sheep(game, sheep);
            continue;
        }
        sheep->x += sheep->speed * ((float)elapsed_ms / 1000.0f);
        if (sheep->x < -44.0f || sheep->x > MARVY_WIDTH + 44.0f) spawn_sheep(game, sheep);
        if (sheep->hidden_ms == 0 && !sheep->announced &&
            sheep->x >= -20.0f && sheep->x <= MARVY_WIDTH + 20.0f) {
            sheep->announced = true;
            if (sheep->marvy) events |= MARVY_EVENT_MARVY_APPEAR;
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
        if (delta < 0) delta = -delta;
        if (sheep->hidden_ms == 0 && delta <= MARVY_HIT_RADIUS && delta < distance) {
            closest = (int)i; distance = delta;
        }
    }
    game->ring_state = MARVY_RING_FEEDBACK;
    game->ring_ms = 0;
    if (closest < 0) {
        game->hit_streak = 0;
        return MARVY_EVENT_MISS;
    }
    marvy_sheep_t *sheep = &game->sheep[closest];
    game->last_hit_sheep = (int8_t)closest;
    game->last_hit_x = sheep->x;
    game->last_hit_marvy = sheep->marvy;
    game->last_points = sheep->marvy ? 5 : 1;
    game->score = add_counter(game->score, game->last_points);
    game->captures = add_counter(game->captures, 1);
    game->hit_streak = add_counter(game->hit_streak, 1);
    if (game->hit_streak > game->best_streak) game->best_streak = game->hit_streak;
    sheep->hidden_ms = MARVY_FEEDBACK_MS + CAPTURE_GRACE_MS;
    if (sheep->marvy) {
        game->celebration_ms = MARVY_MARVY_FEEDBACK_MS;
        game->celebration_x = sheep->x;
        game->celebration_points = 5;
    }
    uint32_t events = MARVY_EVENT_HIT | (sheep->marvy ? MARVY_EVENT_MARVY_HIT : 0);
    if (game->hit_streak == 3 || game->hit_streak == 5) {
        game->combo_ms = MARVY_COMBO_MS;
        game->combo_count = game->hit_streak;
        events |= game->hit_streak == 3 ? MARVY_EVENT_COMBO_3 : MARVY_EVENT_COMBO_5;
    }
    return events;
}

static uint32_t finish_level(marvy_game_t *game)
{
    const marvy_level_config_t *config = &s_levels[game->level_index];
    game->phase = MARVY_PHASE_RESULTS;
    game->phase_ms = 0;
    game->ring_state = MARVY_RING_READY;
    game->ring_ms = 0;
    game->turn_ms = 0;
    game->passed = game->captures >= config->target_catches;
    game->stars = !game->passed ? 0 : game->captures >= config->target_catches + 2u ? 2 : 1;
    if (game->passed && game->captures >= config->target_catches + 4u && game->throws > 0 &&
        (uint32_t)game->captures * 100u >= (uint32_t)game->throws * 60u) game->stars = 3;
    marvy_progress_t update = game->progress;
    unsigned level = game->level_index;
    if (game->score > update.best_score[level]) update.best_score[level] = game->score;
    if (game->stars > update.best_stars[level]) update.best_stars[level] = game->stars;
    if (game->passed && level + 1u < MARVY_LEVEL_COUNT && update.unlocked_count < level + 2u) {
        update.unlocked_count = (uint8_t)(level + 2u);
    }
    uint32_t events = MARVY_EVENT_FINISH |
                      (game->passed ? MARVY_EVENT_LEVEL_CLEAR : MARVY_EVENT_LEVEL_FAIL);
    if (marvy_progress_merge(&game->progress, &update)) events |= MARVY_EVENT_PROGRESS_CHANGED;
    game->result_selection = marvy_game_can_next(game) ? MARVY_RESULT_NEXT
                             : marvy_game_can_follow(game) ? MARVY_RESULT_FOLLOW
                             : MARVY_RESULT_RETRY;
    return events;
}

static void advance_visuals(marvy_game_t *game, uint32_t elapsed_ms)
{
    game->celebration_ms = game->celebration_ms > elapsed_ms ? game->celebration_ms - elapsed_ms : 0;
    game->combo_ms = game->combo_ms > elapsed_ms ? game->combo_ms - elapsed_ms : 0;
}

uint32_t marvy_game_step(marvy_game_t *game, uint32_t elapsed_ms)
{
    if (game->phase == MARVY_PHASE_PAUSED) return 0;
    if (game->phase == MARVY_PHASE_DEMO) {
        /* Reduce each operand before addition, including malformed large ages.
         * Demo has no simulation events or automatic navigation. */
        game->phase_ms = (game->phase_ms % MARVY_DEMO_MS +
                          elapsed_ms % MARVY_DEMO_MS) % MARVY_DEMO_MS;
        return 0;
    }
    uint32_t events = 0;
    if (elapsed_ms > MARVY_MAX_STEP_MS) elapsed_ms = MARVY_MAX_STEP_MS;
    while (elapsed_ms > 0) {
        if (game->phase != MARVY_PHASE_PLAYING) {
            game->phase_ms = saturated_add(game->phase_ms, elapsed_ms);
            break;
        }
        const marvy_level_config_t *config = &s_levels[game->level_index];
        uint32_t slice = elapsed_ms < SIMULATION_SLICE_MS ? elapsed_ms : SIMULATION_SLICE_MS;
        if (!game->time_expired && slice > marvy_game_remaining_ms(game)) slice = marvy_game_remaining_ms(game);
        if ((game->ring_state == MARVY_RING_READY || game->ring_state == MARVY_RING_CHARGING) &&
            config->turn_limit_ms > 0 && slice > config->turn_limit_ms - game->turn_ms) {
            slice = config->turn_limit_ms - game->turn_ms;
        } else if (game->ring_state == MARVY_RING_FLYING || game->ring_state == MARVY_RING_FEEDBACK) {
            uint32_t duration = game->ring_state == MARVY_RING_FLYING ? MARVY_FLIGHT_MS : MARVY_FEEDBACK_MS;
            if (slice > duration - game->ring_ms) slice = duration - game->ring_ms;
        }
        events |= move_sheep(game, slice);
        advance_visuals(game, slice);
        game->phase_ms = saturated_add(game->phase_ms, slice);
        elapsed_ms -= slice;
        if (!game->time_expired) game->stage_elapsed_ms += slice;
        if (game->ring_state == MARVY_RING_READY || game->ring_state == MARVY_RING_CHARGING) {
            game->turn_ms += slice;
            if (game->ring_state == MARVY_RING_CHARGING) {
                game->ring_ms = game->ring_ms + slice > MARVY_CHARGE_MS
                                ? MARVY_CHARGE_MS : game->ring_ms + slice;
            }
        } else {
            game->ring_ms += slice;
        }
        if (game->stage_elapsed_ms >= config->duration_ms) game->time_expired = true;
        if (game->time_expired && (game->ring_state == MARVY_RING_READY || game->ring_state == MARVY_RING_CHARGING)) {
            marvy_game_cancel_charge(game);
            events |= finish_level(game);
        } else if (game->ring_state == MARVY_RING_FLYING && game->ring_ms == MARVY_FLIGHT_MS) {
            events |= resolve_landing(game);
        } else if (game->ring_state == MARVY_RING_FEEDBACK && game->ring_ms == MARVY_FEEDBACK_MS) {
            game->ring_ms = 0;
            game->turn_ms = 0;
            game->ring_state = MARVY_RING_READY;
            if (game->time_expired) events |= finish_level(game);
        } else if (!game->time_expired && config->turn_limit_ms > 0 &&
                   (game->ring_state == MARVY_RING_READY || game->ring_state == MARVY_RING_CHARGING) &&
                   game->turn_ms == config->turn_limit_ms) {
            events |= launch_ring(game, MARVY_DEFAULT_CHARGE_MS);
        }
    }
    return events;
}
