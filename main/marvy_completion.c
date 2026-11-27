#include "marvy_completion.h"

#include <string.h>

static int8_t bounce(uint32_t age_ms, unsigned height)
{
    if (age_ms >= 240) return 0;
    return (int8_t)(age_ms <= 80 ? height * age_ms / 80
                    : height * (240 - age_ms) / 160);
}

static void particle_frame(unsigned index, uint32_t elapsed_ms,
                            uint32_t duration_ms,
                            marvy_completion_particle_t *particle)
{
    static const int8_t directions[8][2] = {
        {1, 0}, {1, 1}, {0, 1}, {-1, 1},
        {-1, 0}, {-1, -1}, {0, -1}, {1, -1},
    };
    static const int16_t centers[3][2] = {{36, 90}, {204, 90}, {120, 45}};
    unsigned burst = index / 8;
    uint32_t delay_ms = burst * 180;
    if (elapsed_ms < delay_ms) return;
    uint32_t start_ms = delay_ms + (elapsed_ms - delay_ms) / 720 * 720;
    uint32_t age_ms = elapsed_ms - start_ms;
    /* Let every launched burst fade completely before the animation ends. */
    if (start_ms > duration_ms - 600 || age_ms >= 600) return;
    unsigned spread = burst == 2 ? 9 : 21;
    int radius = 3 + (int)(age_ms * spread / 600 / 3 * 3);
    particle->x = (int16_t)(centers[burst][0] + directions[index % 8][0] * radius);
    particle->y = (int16_t)(centers[burst][1] + directions[index % 8][1] * radius);
    particle->opacity = (uint8_t)(255 - age_ms * 235 / 600);
    particle->visible = true;
}

void marvy_completion_frame(const marvy_game_t *game,
                             marvy_completion_frame_t *frame)
{
    if (!frame) return;
    memset(frame, 0, sizeof(*frame));
    if (!game || game->phase != MARVY_PHASE_RESULTS || !game->passed ||
        game->level_index >= MARVY_LEVEL_COUNT) return;

    frame->finale = game->level_index == MARVY_LEVEL_COUNT - 1;
    unsigned earned = game->stars > 3 ? 3 : game->stars;
    uint32_t elapsed_ms = game->phase_ms;
    uint32_t duration_ms = frame->finale ? MARVY_COMPLETION_FINALE_MS : MARVY_COMPLETION_MS;
    if (elapsed_ms >= duration_ms) {
        frame->lit_stars = (uint8_t)earned;
        return;
    }

    frame->active = true;
    frame->heading_lift = bounce(elapsed_ms % 600, 3);
    if (frame->finale) {
        frame->sheep_lift = bounce(elapsed_ms % 650, 9);
        frame->trophy_lift = elapsed_ms < 600 ? (int8_t)(12 - elapsed_ms * 12 / 600) : 0;
    }
    for (unsigned star = 0; star < earned; ++star) {
        uint32_t reveal_ms = 120 + star * 280;
        if (elapsed_ms < reveal_ms) continue;
        frame->lit_stars++;
        frame->star_lift[star] = bounce(elapsed_ms - reveal_ms, 9);
    }
    unsigned count = frame->finale ? MARVY_COMPLETION_PARTICLES : 16;
    for (unsigned i = 0; i < count; ++i) {
        particle_frame(i, elapsed_ms, duration_ms, &frame->particles[i]);
    }
}
