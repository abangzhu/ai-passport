#include "marvy_view.h"
#include "marvy_logo.h"
#include "lvgl.h"

LV_FONT_DECLARE(marvy_font_16);
LV_FONT_DECLARE(marvy_font_20);

/* Original sheep artwork uses the supplied Marvy logo as its shared badge. */
#define PAPER 0xFFF7E8
#define INK 0x302445
#define PURPLE 0x7853CF
#define LILAC 0xDCCCF2
#define GRASS 0xBDD1A2
#define DARK_GRASS 0x91AD77
#define GOLD 0xF0B953
#define WHITE 0xFFFDF8
#define PINK 0xF52BBE

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

static lv_obj_t *s_screen, *s_intro, *s_play, *s_results;
static lv_obj_t *s_battery, *s_score, *s_best, *s_hint;
static lv_obj_t *s_feedback, *s_feedback_points, *s_feedback_message, *s_ring;
static lv_obj_t *s_ring_ticks[MARVY_RING_COUNT];
static lv_obj_t *s_result_score, *s_result_best, *s_result_heading;
static sheep_view_t s_sheep[MARVY_SHEEP_COUNT], s_caught;
static int s_previous_phase = -1, s_previous_score = -1;
static int s_previous_best = -1, s_previous_rings = -1;
static int s_previous_battery = -2, s_previous_feedback = -1;
static int s_previous_input = -1;

static void visible(lv_obj_t *object, bool show)
{
    if (show) lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h,
                     uint32_t color, int radius)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, w, h);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(object, radius, 0);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return object;
}

static lv_obj_t *group(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *object = box(parent, x, y, w, h, PAPER, 0);
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

static sheep_view_t sheep_create(lv_obj_t *parent, int x, int y, int scale,
                                 bool marvy)
{
    sheep_view_t sheep = {0};
    sheep.group = group(parent, x, y, 54 * scale, 46 * scale);
    box(sheep.group, 5 * scale, 38 * scale, 40 * scale, 4 * scale,
        DARK_GRASS, LV_RADIUS_CIRCLE);
    for (unsigned i = 0; i < 2; ++i) {
        box(sheep.group, (15 + (int)i * 20) * scale, 33 * scale,
            5 * scale, 9 * scale, INK, 2 * scale);
    }
    static const int wool_x[] = {8, 20, 30, 5, 18, 29};
    static const int wool_y[] = {9, 7, 10, 19, 19, 20};
    for (unsigned i = 0; i < 6; ++i) {
        sheep.wool[i] = box(sheep.group, wool_x[i] * scale, wool_y[i] * scale,
                             22 * scale, 21 * scale,
                             marvy ? LILAC : WHITE, LV_RADIUS_CIRCLE);
    }
    sheep.head = group(sheep.group, 35 * scale, 13 * scale, 18 * scale, 23 * scale);
    box(sheep.head, 2 * scale, 0, 12 * scale, 8 * scale, INK, LV_RADIUS_CIRCLE);
    box(sheep.head, 0, 4 * scale, 16 * scale, 19 * scale, INK, 7 * scale);
    box(sheep.head, 10 * scale, 8 * scale, 3 * scale, 3 * scale,
        WHITE, LV_RADIUS_CIRCLE);
    sheep.badge = lv_image_create(sheep.group);
    lv_image_set_src(sheep.badge, &marvy_logo_badge);
    lv_obj_set_pos(sheep.badge, 13 * scale, 16 * scale);
    sheep.sparkle = label(sheep.group, "+", 16 * scale, -9 * scale,
                           24 * scale, &lv_font_montserrat_20, PURPLE);
    visible(sheep.badge, marvy);
    visible(sheep.sparkle, false);
    sheep.marvy = marvy;
    return sheep;
}

static void landscape(lv_obj_t *parent)
{
    box(parent, -34, 108, 180, 90, 0xDCE4BB, LV_RADIUS_CIRCLE);
    box(parent, 126, 117, 150, 80, 0xD1DDB3, LV_RADIUS_CIRCLE);
    box(parent, 0, 152, 240, 89, GRASS, 0);
    box(parent, 0, 170, 240, 3, 0xA0B788, 0);
    for (int x = 11; x < 240; x += 34) {
        box(parent, x, 143, 5, 30, 0xF7EBD4, 2);
    }
    box(parent, 0, 150, 240, 4, 0xF7EBD4, 1);
    for (int i = 0; i < 9; ++i) {
        box(parent, 16 + (i * 53) % 210, 219 + (i % 3) * 6, 3, 4,
            i % 2 ? DARK_GRASS : WHITE, 1);
    }
    lv_obj_t *target = box(parent, MARVY_AIM_X - 24, 205, 48, 10,
                           GRASS, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(target, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(target, lv_color_hex(PURPLE), 0);
    lv_obj_set_style_border_width(target, 1, 0);
    lv_obj_set_style_border_opa(target, LV_OPA_60, 0);
    box(parent, MARVY_AIM_X - 1, 217, 2, 6, PURPLE, 1);
}

void marvy_view_create(void)
{
    s_previous_phase = s_previous_score = s_previous_best = s_previous_rings = -1;
    s_previous_battery = s_previous_feedback = s_previous_input = -2;
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(PAPER), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    label(s_screen, "MARVY / 2027", 22, 19, 140, &lv_font_montserrat_12, PURPLE);
    s_battery = label(s_screen, "--%", 178, 19, 43, &lv_font_montserrat_12, INK);

    s_intro = group(s_screen, 0, 0, 240, 320);
    label(s_intro, "Marvy 套羊", 18, 58, 204, &marvy_font_20, INK);
    lv_obj_t *welcome_logo = lv_image_create(s_intro);
    lv_image_set_src(welcome_logo, &marvy_logo_welcome);
    lv_obj_set_pos(welcome_logo, 64, 90);
    label(s_intro, "Marvy 祝你", 18, 215, 204, &marvy_font_20, INK);
    label(s_intro, "新年快乐", 18, 244, 204, &marvy_font_20, PURPLE);
    label(s_intro, "按任意功能键开始", 12, 278, 216, &marvy_font_16, PURPLE);

    s_play = group(s_screen, 0, 0, 240, 320);
    label(s_play, "Marvy 套羊", 18, 44, 204, &marvy_font_20, INK);
    s_score = label(s_play, "得分 0", 16, 77, 100, &marvy_font_16, INK);
    s_best = label(s_play, "最高 0", 122, 77, 100, &marvy_font_16, PURPLE);
    landscape(s_play);
    label(s_play, "普通羊 +1 / Marvy +5", 12, 111, 216, &marvy_font_16, INK);
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        s_sheep[i] = sheep_create(s_play, 0, MARVY_SHEEP_BODY_Y - 24, 1, false);
    }
    s_caught = sheep_create(s_play, 0, MARVY_SHEEP_BODY_Y - 24, 1, true);
    visible(s_caught.group, false);
    s_ring = box(s_play, 95, 262, 50, 17, PAPER, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(s_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ring, 3, 0);
    lv_obj_set_style_border_color(s_ring, lv_color_hex(GOLD), 0);
    s_feedback = box(s_play, 26, 107, 188, 65, WHITE, 12);
    lv_obj_set_style_border_width(s_feedback, 2, 0);
    lv_obj_set_style_border_color(s_feedback, lv_color_hex(LILAC), 0);
    s_feedback_points = label(s_feedback, "", 0, 3, 184, &marvy_font_20, PURPLE);
    s_feedback_message = label(s_feedback, "", 0, 32, 184, &marvy_font_20, INK);
    visible(s_feedback, false);
    for (unsigned i = 0; i < MARVY_RING_COUNT; ++i) {
        s_ring_ticks[i] = box(s_play, 23 + (int)i * 20, 249, 13, 13,
                              GOLD, LV_RADIUS_CIRCLE);
    }
    s_hint = label(s_play, "任意键扔圈", 14, 285, 212, &marvy_font_16, INK);

    s_results = group(s_screen, 0, 0, 240, 320);
    s_result_heading = label(s_results, "本局得分", 18, 60, 204, &marvy_font_20, INK);
    s_result_score = label(s_results, "0", 18, 96, 204, &lv_font_montserrat_48, PURPLE);
    s_result_best = label(s_results, "最高 0 分", 18, 153, 204, &marvy_font_16, INK);
    (void)sheep_create(s_results, 94, 181, 1, true);
    box(s_results, 36, 245, 168, 39, PURPLE, 19);
    label(s_results, "再套一局", 38, 252, 164, &marvy_font_20, WHITE);
    label(s_results, "按任意功能键", 18, 291, 204, &marvy_font_16, INK);
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
        visible(s_intro, game->phase == MARVY_PHASE_INTRO);
        visible(s_play, game->phase == MARVY_PHASE_PLAYING);
        visible(s_results, game->phase == MARVY_PHASE_RESULTS);
        s_previous_phase = game->phase;
        s_previous_feedback = -1;
        for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) s_sheep[i].was_visible = false;
        lv_label_set_text(s_result_heading,
            game->score > 0 && game->score == game->best_score ? "好彩头到手" : "本局得分");
    }
    if (s_previous_score != game->score || s_previous_best != game->best_score) {
        lv_label_set_text_fmt(s_score, "得分 %u", (unsigned)game->score);
        lv_label_set_text_fmt(s_best, "最高 %u", (unsigned)game->best_score);
        lv_label_set_text_fmt(s_result_score, "%u", (unsigned)game->score);
        lv_label_set_text_fmt(s_result_best, "最高 %u 分", (unsigned)game->best_score);
        s_previous_score = game->score;
        s_previous_best = game->best_score;
    }
    if (s_previous_rings != game->rings_left) {
        for (unsigned i = 0; i < MARVY_RING_COUNT; ++i) {
            lv_obj_set_style_bg_color(s_ring_ticks[i],
                lv_color_hex(i < game->rings_left ? GOLD : 0xE4DDCF), 0);
        }
        s_previous_rings = game->rings_left;
    }
    if (s_previous_input != (int)input_available) {
        lv_label_set_text(s_hint, input_available ? "任意键扔圈" : "按键未就绪");
        s_previous_input = input_available;
    }
    if (game->phase != MARVY_PHASE_PLAYING) return;
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
        lv_obj_set_pos(sheep->group, (int)model->x - 27,
                        MARVY_SHEEP_BODY_Y - 24 - gait * (entry ? 3 : 1));
        lv_obj_set_x(sheep->head, model->speed < 0 ? 0 : 35);
    }
    bool feedback = game->ring_state == MARVY_RING_FEEDBACK;
    visible(s_caught.group, feedback && game->last_points > 0);
    if (feedback && game->last_points > 0) {
        sheep_color(&s_caught, game->last_hit_marvy);
        uint32_t duration = marvy_game_feedback_ms(game);
        lv_obj_set_pos(s_caught.group, (int)game->last_hit_x - 27,
                        MARVY_SHEEP_BODY_Y - 24 - (int)(game->ring_ms * 6 / duration));
    }
    visible(s_feedback, feedback);
    int feedback_kind = !feedback ? -1 : game->last_hit_marvy ? 5 : game->last_points ? 1 : 0;
    if (s_previous_feedback != feedback_kind) {
        lv_label_set_text(s_feedback_points, feedback_kind == 5 ? "+5 分"
                              : feedback_kind == 1 ? "+1 分" : "差一点");
        lv_label_set_text(s_feedback_message, feedback_kind == 5 ? "新年快乐"
                              : feedback_kind == 1 ? "套中啦" : "再来");
        lv_obj_set_style_border_color(s_feedback,
            lv_color_hex(feedback_kind == 5 ? PINK : LILAC), 0);
        s_previous_feedback = feedback_kind;
    }
    if (feedback) {
        uint32_t duration = marvy_game_feedback_ms(game);
        lv_obj_set_y(s_feedback, 107 - (int)(game->ring_ms * 4 / duration));
    }
    visible(s_ring, game->ring_state != MARVY_RING_READY);
    if (game->ring_state == MARVY_RING_FLYING) {
        float t = (float)game->ring_ms / MARVY_FLIGHT_MS;
        float arc = 4.0f * t * (1.0f - t);
        int width = 52 - (int)(10 * arc), height = 18 + (int)(14 * arc);
        lv_obj_set_size(s_ring, width, height);
        lv_obj_set_pos(s_ring, MARVY_AIM_X - width / 2,
                        266 - (int)(72 * t + 57 * arc) - height / 2);
    } else if (feedback) {
        int width = 50 + (int)(game->ring_ms * 11 / marvy_game_feedback_ms(game));
        lv_obj_set_size(s_ring, width, 15);
        lv_obj_set_pos(s_ring,
            (game->last_points ? (int)game->last_hit_x : MARVY_AIM_X) - width / 2,
            MARVY_SHEEP_BODY_Y - 5);
    }
}
