#include "marvy_view.h"
#include "marvy_logo.h"
#include "lvgl.h"

LV_FONT_DECLARE(marvy_font_16);
LV_FONT_DECLARE(marvy_font_20);
LV_FONT_DECLARE(marvy_pixel_16);
LV_FONT_DECLARE(marvy_pixel_20);

/* Welcome retains the smooth brand artwork. All game geometry below is made
 * from hard-edged rectangles; no full-frame canvas or per-frame objects. */
#define PAPER 0xFFF7E8
#define INK 0x302445
#define PURPLE 0x7853CF
#define SKY 0x202F49
#define SKY_LIGHT 0x344B68
#define GRASS 0x659363
#define DARK_GRASS 0x46734C
#define LIGHT_GRASS 0x8DBB77
#define GOLD 0xF0C86A
#define WHITE 0xFFFDF8
#define PINK 0xFC62C7
#define CYAN 0x8DE1DF
#define LILAC 0xDCCCF2
#define CHARGE_SEGMENTS 12
#define FIREWORK_PARTICLES 16
#define RING_PARTS 8

typedef struct {
    lv_obj_t *group;
    lv_obj_t *wool[6];
    lv_obj_t *head;
    lv_obj_t *badge;
    lv_obj_t *sparkle;
    bool marvy;
    bool was_visible;
    uint32_t appeared_at;
} sheep_view_t;

typedef struct {
    lv_obj_t *group;
    lv_obj_t *parts[RING_PARTS];
} ring_view_t;

static lv_obj_t *s_screen, *s_intro, *s_play, *s_results;
static lv_obj_t *s_header, *s_battery, *s_score, *s_best, *s_hint, *s_legend;
static lv_obj_t *s_feedback, *s_feedback_points, *s_feedback_message;
static lv_obj_t *s_feedback_points_shadow, *s_feedback_message_shadow;
static lv_obj_t *s_ring_ticks[MARVY_RING_COUNT], *s_charge[CHARGE_SEGMENTS];
static lv_obj_t *s_firework[FIREWORK_PARTICLES];
static lv_obj_t *s_result_best, *s_result_heading;
static lv_obj_t *s_result_digits[3][15];
static sheep_view_t s_sheep[MARVY_SHEEP_COUNT], s_caught;
static ring_view_t s_ring, s_target;
static int s_previous_phase = -1, s_previous_score = -1;
static int s_previous_best = -1, s_previous_rings = -1;
static int s_previous_battery = -2, s_previous_feedback = -1;
static int s_previous_input = -1, s_previous_ring_state = -1;
static int s_previous_charge = -1;

static void visible(lv_obj_t *object, bool show)
{
    if (show) lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h,
                     uint32_t color)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, w, h);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return object;
}

static lv_obj_t *group(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *object = box(parent, x, y, w, h, PAPER);
    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(object, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    return object;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y,
                       int width, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_width(object, width);
    lv_obj_set_style_text_font(object, font, 0);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(object, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(object, text);
    return object;
}

static void sheep_color(sheep_view_t *sheep, bool marvy)
{
    if (sheep->marvy == marvy) return;
    for (unsigned i = 0; i < 6; ++i) {
        lv_obj_set_style_bg_color(sheep->wool[i],
                                 lv_color_hex(marvy ? LILAC : WHITE), 0);
    }
    visible(sheep->badge, marvy);
    sheep->marvy = marvy;
}

static sheep_view_t sheep_create(lv_obj_t *parent, int x, int y, bool marvy)
{
    sheep_view_t sheep = {0};
    sheep.group = group(parent, x, y, 54, 46);
    box(sheep.group, 7, 39, 39, 3, DARK_GRASS);
    box(sheep.group, 12, 31, 5, 10, INK);
    box(sheep.group, 32, 31, 5, 10, INK);
    /* Layered rectangular steps form the wool silhouette. */
    box(sheep.group, 9, 10, 31, 26, INK);
    box(sheep.group, 4, 16, 40, 17, INK);
    box(sheep.group, 14, 6, 21, 32, INK);
    static const int wool[][4] = {
        {10, 12, 28, 22}, {7, 18, 34, 12}, {16, 9, 16, 26},
        {10, 14, 9, 14}, {27, 13, 10, 17}, {18, 25, 15, 10},
    };
    for (unsigned i = 0; i < 6; ++i) {
        sheep.wool[i] = box(sheep.group, wool[i][0], wool[i][1],
                             wool[i][2], wool[i][3], marvy ? LILAC : WHITE);
    }
    box(sheep.group, 1, 20, 6, 5, INK);
    sheep.head = group(sheep.group, 36, 13, 17, 23);
    box(sheep.head, 2, 0, 9, 5, INK);
    box(sheep.head, 0, 5, 15, 15, INK);
    box(sheep.head, 3, 20, 10, 3, INK);
    box(sheep.head, 10, 8, 3, 3, WHITE);
    sheep.badge = lv_image_create(sheep.group);
    lv_image_set_src(sheep.badge, &marvy_logo_badge);
    lv_obj_set_pos(sheep.badge, 12, 12);
    sheep.sparkle = group(sheep.group, 22, -4, 9, 9);
    box(sheep.sparkle, 3, 0, 3, 9, GOLD);
    box(sheep.sparkle, 0, 3, 9, 3, GOLD);
    visible(sheep.badge, marvy);
    visible(sheep.sparkle, false);
    sheep.marvy = marvy;
    return sheep;
}

static void cloud(lv_obj_t *parent, int x, int y)
{
    box(parent, x + 7, y, 24, 5, SKY_LIGHT);
    box(parent, x, y + 5, 42, 7, SKY_LIGHT);
    box(parent, x + 5, y + 12, 31, 4, SKY_LIGHT);
}

static void landscape(lv_obj_t *parent)
{
    cloud(parent, 14, 114);
    cloud(parent, 180, 104);
    box(parent, 0, 147, 37, 27, DARK_GRASS);
    box(parent, 36, 138, 47, 36, DARK_GRASS);
    box(parent, 83, 149, 55, 25, DARK_GRASS);
    box(parent, 138, 142, 59, 32, DARK_GRASS);
    box(parent, 197, 150, 43, 24, DARK_GRASS);
    box(parent, 0, 170, 240, 54, GRASS);
    box(parent, 0, 167, 240, 5, LIGHT_GRASS);
    for (int x = 9; x < 240; x += 32) {
        box(parent, x, 151, 5, 29, 0xC5B891);
        box(parent, x - 2, 149, 9, 4, GOLD);
    }
    box(parent, 0, 156, 240, 4, 0xC5B891);
    for (int i = 0; i < 14; ++i) {
        int x = 7 + (i * 41) % 226;
        int y = 214 + (i % 3) * 3;
        box(parent, x, y, 3, 4, i % 3 ? DARK_GRASS : GOLD);
    }
    for (int x = 33; x <= 209; x += 44) box(parent, x, 222, 3, 4, SKY_LIGHT);
}

static ring_view_t ring_create(lv_obj_t *parent, uint32_t color)
{
    ring_view_t ring = {0};
    ring.group = group(parent, 0, 0, 44, 18);
    for (unsigned i = 0; i < RING_PARTS; ++i) ring.parts[i] = box(ring.group, 0, 0, 1, 1, color);
    return ring;
}

static void ring_position(ring_view_t *ring, int center_x, int center_y,
                           int width, int height)
{
    static const int parts[RING_PARTS][4] = {
        {10, 0, 24, 3}, {5, 3, 5, 3}, {34, 3, 5, 3}, {2, 6, 3, 6},
        {39, 6, 3, 6}, {5, 12, 5, 3}, {34, 12, 5, 3}, {10, 15, 24, 3},
    };
    lv_obj_set_pos(ring->group, center_x - width / 2, center_y - height / 2);
    lv_obj_set_size(ring->group, width, height);
    for (unsigned i = 0; i < RING_PARTS; ++i) {
        int w = parts[i][2] * width / 44;
        int h = parts[i][3] * height / 18;
        lv_obj_set_pos(ring->parts[i], parts[i][0] * width / 44, parts[i][1] * height / 18);
        lv_obj_set_size(ring->parts[i], w < 1 ? 1 : w, h < 1 ? 1 : h);
    }
}

static void result_digits_create(lv_obj_t *parent)
{
    for (unsigned digit = 0; digit < 3; ++digit) {
        for (unsigned pixel = 0; pixel < 15; ++pixel) {
            s_result_digits[digit][pixel] = box(parent,
                85 + (int)digit * 26 + (int)(pixel % 3) * 6,
                108 + (int)(pixel / 3) * 6, 5, 5, GOLD);
        }
    }
}

static void result_digits_update(unsigned score)
{
    static const char digits[10][16] = {
        "111101101101111", "010110010010111", "111001111100111",
        "111001111001111", "101101111001001", "111100111001111",
        "111100111101111", "111001001001001", "111101111101111",
        "111101111001111",
    };
    for (int digit = 2; digit >= 0; --digit) {
        unsigned value = score % 10;
        score /= 10;
        for (unsigned pixel = 0; pixel < 15; ++pixel) {
            visible(s_result_digits[digit][pixel], digits[value][pixel] == '1');
        }
    }
}

static void fireworks_update(const marvy_game_t *game, bool active)
{
    static const int directions[8][2] = {
        {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1},
    };
    for (unsigned i = 0; i < FIREWORK_PARTICLES; ++i) {
        unsigned burst = i / 8;
        uint32_t delay = burst * 170;
        bool showing = active && game->ring_ms >= delay;
        uint32_t age = showing ? (game->ring_ms - delay) % 650 : 0;
        visible(s_firework[i], showing && age < 580);
        if (!showing) continue;
        int radius = 3 + (int)age / 25;
        int x = burst ? 197 : 42;
        int y = burst ? 137 : 121;
        x += directions[i % 8][0] * radius;
        y += directions[i % 8][1] * radius + (int)(age * age / 50000);
        lv_obj_set_pos(s_firework[i], x / 3 * 3, y / 3 * 3);
        lv_obj_set_style_bg_opa(s_firework[i], (lv_opa_t)(255 - age * 220 / 650), 0);
    }
}

void marvy_view_create(void)
{
    s_previous_phase = s_previous_score = s_previous_best = s_previous_rings = -1;
    s_previous_battery = s_previous_feedback = s_previous_input = -2;
    s_previous_ring_state = s_previous_charge = -1;
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(PAPER), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    s_intro = group(s_screen, 0, 0, 240, 320);
    label(s_intro, "Marvy 套羊", 18, 58, 204, &marvy_font_20, INK);
    lv_obj_t *welcome_logo = lv_image_create(s_intro);
    lv_image_set_src(welcome_logo, &marvy_logo_welcome);
    lv_obj_set_pos(welcome_logo, 64, 90);
    label(s_intro, "Marvy 祝你", 18, 215, 204, &marvy_font_20, INK);
    label(s_intro, "新年快乐", 18, 244, 204, &marvy_font_20, PURPLE);
    label(s_intro, "按任意功能键开始", 12, 278, 216, &marvy_font_16, PURPLE);

    s_play = group(s_screen, 0, 0, 240, 320);
    label(s_play, "Marvy 套羊", 18, 44, 204, &marvy_pixel_20, WHITE);
    s_score = label(s_play, "得分 0", 14, 77, 103, &marvy_pixel_16, WHITE);
    s_best = label(s_play, "最高 0", 124, 77, 103, &marvy_pixel_16, GOLD);
    landscape(s_play);
    s_legend = label(s_play, "普通 +1 / Marvy +5", 12, 110, 216, &marvy_pixel_16, WHITE);
    s_target = ring_create(s_play, CYAN);
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        s_sheep[i] = sheep_create(s_play, 0, MARVY_SHEEP_BODY_Y - 24, false);
    }
    s_caught = sheep_create(s_play, 0, MARVY_SHEEP_BODY_Y - 24, true);
    visible(s_caught.group, false);
    for (unsigned i = 0; i < MARVY_RING_COUNT; ++i) {
        s_ring_ticks[i] = box(s_play, 25 + (int)i * 19, 233, 12, 9, GOLD);
    }
    box(s_play, 29, 251, 182, 13, SKY_LIGHT);
    for (unsigned i = 0; i < CHARGE_SEGMENTS; ++i) {
        s_charge[i] = box(s_play, 32 + (int)i * 15, 254, 11, 7, DARK_GRASS);
    }
    /* Basket and a stationary ring show the throw origin in the near corner. */
    box(s_play, 17, 269, 22, 3, GOLD);
    box(s_play, 20, 272, 16, 7, 0xA36A52);
    s_ring = ring_create(s_play, GOLD);
    s_hint = label(s_play, "按住蓄力 松手投圈", 8, 286, 224, &marvy_pixel_16, WHITE);
    for (unsigned i = 0; i < FIREWORK_PARTICLES; ++i) {
        s_firework[i] = box(s_play, 0, 0, 3, 3, i % 2 ? PINK : GOLD);
        visible(s_firework[i], false);
    }
    /* No panel fill or border: glyphs and small particles are the only overlay. */
    s_feedback = group(s_play, 30, 100, 180, 55);
    s_feedback_points_shadow = label(s_feedback, "", 1, 1, 180, &marvy_pixel_20, INK);
    s_feedback_message_shadow = label(s_feedback, "", 1, 29, 180, &marvy_pixel_20, INK);
    s_feedback_points = label(s_feedback, "", 0, 0, 180, &marvy_pixel_20, GOLD);
    s_feedback_message = label(s_feedback, "", 0, 28, 180, &marvy_pixel_20, WHITE);
    visible(s_feedback, false);

    s_results = group(s_screen, 0, 0, 240, 320);
    s_result_heading = label(s_results, "本局得分", 18, 60, 204, &marvy_pixel_20, WHITE);
    result_digits_create(s_results);
    s_result_best = label(s_results, "最高 0 分", 18, 157, 204, &marvy_pixel_16, GOLD);
    box(s_results, 74, 226, 94, 3, DARK_GRASS);
    (void)sheep_create(s_results, 94, 185, true);
    box(s_results, 35, 250, 174, 36, INK);
    box(s_results, 31, 246, 174, 36, PURPLE);
    box(s_results, 31, 246, 174, 3, LILAC);
    label(s_results, "再套一局", 33, 252, 170, &marvy_pixel_20, WHITE);
    label(s_results, "按任意功能键", 18, 291, 204, &marvy_pixel_16, WHITE);

    /* Chrome is last so both page groups leave the always-visible HUD clear. */
    s_header = label(s_screen, "MARVY / 2027", 20, 17, 145, &marvy_font_16, PURPLE);
    s_battery = label(s_screen, "--%", 178, 17, 43, &marvy_font_16, INK);
    visible(s_play, false);
    visible(s_results, false);
    lv_screen_load(s_screen);
}

void marvy_view_render(const marvy_game_t *game, int battery_percent,
                       bool input_available)
{
    if (s_previous_battery != battery_percent) {
        if (battery_percent < 0) lv_label_set_text(s_battery, "--%");
        else lv_label_set_text_fmt(s_battery, "%d%%", battery_percent);
        s_previous_battery = battery_percent;
    }
    if (s_previous_phase != (int)game->phase) {
        bool intro = game->phase == MARVY_PHASE_INTRO;
        visible(s_intro, intro);
        visible(s_play, game->phase == MARVY_PHASE_PLAYING);
        visible(s_results, game->phase == MARVY_PHASE_RESULTS);
        lv_obj_set_style_bg_color(s_screen, lv_color_hex(intro ? PAPER : SKY), 0);
        lv_obj_set_style_text_font(s_header, intro ? &marvy_font_16 : &marvy_pixel_16, 0);
        lv_obj_set_style_text_font(s_battery, intro ? &marvy_font_16 : &marvy_pixel_16, 0);
        lv_obj_set_style_text_color(s_header, lv_color_hex(intro ? PURPLE : CYAN), 0);
        lv_obj_set_style_text_color(s_battery, lv_color_hex(intro ? INK : WHITE), 0);
        s_previous_phase = game->phase;
        s_previous_feedback = s_previous_ring_state = s_previous_charge = -1;
        for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) s_sheep[i].was_visible = false;
        lv_label_set_text(s_result_heading,
            game->score > 0 && game->score == game->best_score ? "好彩头到手" : "本局得分");
    }
    if (s_previous_score != game->score || s_previous_best != game->best_score) {
        lv_label_set_text_fmt(s_score, "得分 %u", (unsigned)game->score);
        lv_label_set_text_fmt(s_best, "最高 %u", (unsigned)game->best_score);
        result_digits_update(game->score);
        lv_label_set_text_fmt(s_result_best, "最高 %u 分", (unsigned)game->best_score);
        s_previous_score = game->score;
        s_previous_best = game->best_score;
    }
    if (s_previous_rings != game->rings_left) {
        for (unsigned i = 0; i < MARVY_RING_COUNT; ++i) {
            lv_obj_set_style_bg_color(s_ring_ticks[i],
                lv_color_hex(i < game->rings_left ? GOLD : SKY_LIGHT), 0);
        }
        s_previous_rings = game->rings_left;
    }
    if (s_previous_input != (int)input_available || s_previous_ring_state != (int)game->ring_state) {
        const char *hint = "按住蓄力 松手投圈";
        if (!input_available) hint = "按键未就绪";
        else if (game->ring_state == MARVY_RING_CHARGING) hint = "松手投圈";
        else if (game->ring_state == MARVY_RING_FLYING) hint = "圈在空中";
        lv_label_set_text(s_hint, hint);
        s_previous_input = input_available;
        s_previous_ring_state = game->ring_state;
    }
    if (game->phase != MARVY_PHASE_PLAYING) return;

    uint32_t held = game->ring_state == MARVY_RING_CHARGING ? game->ring_ms : 0;
    if (held > MARVY_CHARGE_MS) held = MARVY_CHARGE_MS;
    int lit = (int)((held * CHARGE_SEGMENTS + MARVY_CHARGE_MS - 1) / MARVY_CHARGE_MS);
    if (s_previous_charge != lit) {
        for (int i = 0; i < CHARGE_SEGMENTS; ++i) {
            lv_obj_set_style_bg_color(s_charge[i],
                lv_color_hex(i < lit ? (i < 8 ? CYAN : PINK) : DARK_GRASS), 0);
        }
        s_previous_charge = lit;
    }
    float target_x = game->ring_target_x;
    if (game->ring_state == MARVY_RING_READY) target_x = MARVY_TARGET_MIN_X;
    else if (game->ring_state == MARVY_RING_CHARGING) target_x = marvy_game_charge_target_x(held);
    ring_position(&s_target, (int)target_x, 211, 36, 12);
    visible(s_target.group, true);

    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        const marvy_sheep_t *model = &game->sheep[i];
        sheep_view_t *sheep = &s_sheep[i];
        bool on_screen = model->hidden_ms == 0 && model->x > -25 && model->x < 265;
        if (on_screen && !sheep->was_visible) sheep->appeared_at = game->phase_ms;
        sheep->was_visible = on_screen;
        visible(sheep->group, on_screen);
        sheep_color(sheep, model->marvy);
        bool entry = model->marvy && game->phase_ms - sheep->appeared_at < 700;
        visible(sheep->sparkle, entry);
        int gait = (int)(game->phase_ms / (entry ? 80 : 150) + i) % 2;
        lv_obj_set_pos(sheep->group, ((int)model->x - 27) / 2 * 2,
                        MARVY_SHEEP_BODY_Y - 24 - gait * (entry ? 2 : 1));
        lv_obj_set_x(sheep->head, model->speed < 0 ? 0 : 36);
    }
    bool feedback = game->ring_state == MARVY_RING_FEEDBACK;
    bool celebration = feedback && game->last_hit_marvy && game->last_points > 0;
    visible(s_caught.group, feedback && game->last_points > 0);
    if (feedback && game->last_points > 0) {
        sheep_color(&s_caught, game->last_hit_marvy);
        uint32_t duration = marvy_game_feedback_ms(game);
        lv_obj_set_pos(s_caught.group, (int)game->last_hit_x - 27,
                        MARVY_SHEEP_BODY_Y - 24 - (int)(game->ring_ms * 6 / duration));
    }
    visible(s_feedback, feedback);
    visible(s_legend, !feedback);
    int kind = !feedback ? -1 : celebration ? 5 : game->last_points ? 1 : 0;
    if (s_previous_feedback != kind) {
        lv_label_set_text(s_feedback_points, kind == 5 ? "+5 分" : kind == 1 ? "+1 分" : "差一点");
        lv_label_set_text(s_feedback_message, kind == 5 ? "新年快乐" : kind == 1 ? "套中啦" : "再来");
        lv_label_set_text(s_feedback_points_shadow, kind == 5 ? "+5 分" : kind == 1 ? "+1 分" : "差一点");
        lv_label_set_text(s_feedback_message_shadow, kind == 5 ? "新年快乐" : kind == 1 ? "套中啦" : "再来");
        lv_obj_set_style_text_color(s_feedback_points, lv_color_hex(kind == 5 ? PINK : GOLD), 0);
        s_previous_feedback = kind;
    }
    if (feedback) lv_obj_set_y(s_feedback, 100 - (int)(game->ring_ms * 3 / marvy_game_feedback_ms(game)));
    fireworks_update(game, celebration);

    visible(s_ring.group, true);
    if (game->ring_state == MARVY_RING_FLYING) {
        float t = (float)game->ring_ms / MARVY_FLIGHT_MS;
        float arc = 4.0f * t * (1.0f - t);
        int x = 28 + (int)((game->ring_target_x - 28) * t);
        int y = 266 - (int)(72 * t + 57 * arc);
        ring_position(&s_ring, x, y, 44 - (int)(8 * arc), 18 + (int)(10 * arc));
    } else if (feedback) {
        int x = game->last_points ? (int)game->last_hit_x : (int)game->ring_target_x;
        ring_position(&s_ring, x, MARVY_SHEEP_BODY_Y + 3, 44, 15);
    } else {
        ring_position(&s_ring, 28, 266, 36, 12);
    }
}
