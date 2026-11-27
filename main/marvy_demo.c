#include "marvy_demo.h"

#include <limits.h>
#include <string.h>

static void charge(marvy_demo_frame_t *frame, uint32_t age_ms)
{
    frame->step = MARVY_DEMO_CHARGE;
    frame->key_held = true;
    frame->scene.ring_state = MARVY_RING_CHARGING;
    frame->scene.ring_ms = age_ms;
    frame->scene.ring_target_x = marvy_game_charge_target_x(age_ms);
}

static void flight(marvy_demo_frame_t *frame, uint32_t age_ms, unsigned held_ms)
{
    frame->step = MARVY_DEMO_THROW;
    frame->scene.ring_state = MARVY_RING_FLYING;
    frame->scene.ring_ms = age_ms;
    frame->scene.ring_target_x = marvy_game_charge_target_x(held_ms);
}

static void caught(marvy_demo_frame_t *frame, uint32_t age_ms, bool marvy)
{
    marvy_game_t *scene = &frame->scene;
    frame->step = marvy ? MARVY_DEMO_BONUS : MARVY_DEMO_HIT;
    scene->ring_state = MARVY_RING_FEEDBACK;
    scene->ring_ms = age_ms < MARVY_FEEDBACK_MS ? age_ms : MARVY_FEEDBACK_MS;
    scene->last_points = marvy ? 5 : 1;
    scene->last_hit_marvy = marvy;
    scene->last_hit_sheep = marvy ? 1 : 0;
    scene->last_hit_x = marvy_game_charge_target_x(marvy ? 700 : 900);
    scene->ring_target_x = scene->last_hit_x;
    if (marvy) {
        scene->celebration_ms = MARVY_MARVY_FEEDBACK_MS - age_ms;
        scene->celebration_x = scene->last_hit_x;
        scene->celebration_points = 5;
    }
}

void marvy_demo_frame(uint32_t elapsed_ms, marvy_demo_frame_t *frame)
{
    if (!frame) return;
    memset(frame, 0, sizeof(*frame));
    uint32_t at_ms = elapsed_ms % MARVY_DEMO_MS;
    marvy_game_t *scene = &frame->scene;
    scene->phase = MARVY_PHASE_PLAYING;
    scene->phase_ms = at_ms;
    scene->ring_state = MARVY_RING_READY;
    scene->ring_target_x = MARVY_TARGET_MIN_X;
    scene->last_hit_sheep = -1;
    scene->score = at_ms < 2000 ? 0 : at_ms < 3900 ? 1 : 6;
    scene->captures = at_ms < 2000 ? 0 : at_ms < 3900 ? 1 : 2;
    scene->throws = at_ms < 1400 ? 0 : at_ms < 3300 ? 1 : 2;
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) scene->sheep[i].hidden_ms = UINT32_MAX;
    /* These two trajectories meet the exact charge-derived landing points.
     * Their copies are discarded after rendering; no gameplay RNG or events. */
    scene->sheep[0].x = marvy_game_charge_target_x(900) + (2000.0f - (float)at_ms) * 0.024f;
    scene->sheep[0].speed = -24;
    scene->sheep[0].announced = true;
    if (at_ms < 2000) scene->sheep[0].hidden_ms = 0;
    scene->sheep[1].x = marvy_game_charge_target_x(700) - (3900.0f - (float)at_ms) * 0.048f;
    scene->sheep[1].speed = 48;
    scene->sheep[1].marvy = true;
    scene->sheep[1].announced = true;
    if (at_ms < 3900) scene->sheep[1].hidden_ms = 0;

    if (at_ms < 500) frame->step = MARVY_DEMO_READY;
    else if (at_ms < 1400) charge(frame, at_ms - 500);
    else if (at_ms < 2000) flight(frame, at_ms - 1400, 900);
    else if (at_ms < 2600) caught(frame, at_ms - 2000, false);
    else if (at_ms < 3300) charge(frame, at_ms - 2600);
    else if (at_ms < 3900) flight(frame, at_ms - 3300, 700);
    else if (at_ms < 5100) caught(frame, at_ms - 3900, true);
    else frame->step = MARVY_DEMO_DONE;
}
