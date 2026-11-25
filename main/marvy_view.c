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
#define WELCOME_SPARKLES 6

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

static lv_obj_t *s_screen, *s_intro, *s_brief, *s_play, *s_results;
static lv_obj_t *s_header, *s_battery, *s_score, *s_timer, *s_level_heading;
static lv_obj_t *s_hint, *s_legend, *s_progress, *s_combo, *s_timebar;
static lv_obj_t *s_feedback, *s_feedback_points, *s_feedback_message;
static lv_obj_t *s_feedback_points_shadow, *s_feedback_message_shadow;
static lv_obj_t *s_charge[CHARGE_SEGMENTS], *s_firework[FIREWORK_PARTICLES];
static lv_obj_t *s_brief_heading, *s_brief_name, *s_brief_goal, *s_brief_best, *s_brief_rule;
static lv_obj_t *s_brief_stars[3][6], *s_result_stars[3][6];
static lv_obj_t *s_result_heading, *s_result_summary, *s_result_metrics;
static lv_obj_t *s_result_rows[3], *s_result_labels[3], *s_result_digits[3][15];
static lv_obj_t *s_countdown, *s_countdown_pixels[15];
static lv_obj_t *s_welcome_logo, *s_welcome_prompt, *s_welcome_sparkles[WELCOME_SPARKLES];
static lv_obj_t *s_pause, *s_pause_rows[2];
static sheep_view_t s_sheep[MARVY_SHEEP_COUNT], s_caught;
static ring_view_t s_ring, s_target;
static marvy_game_t s_previous;
static bool s_first_render;
static int s_previous_battery = -2, s_previous_countdown = -1;
static int s_previous_charge = -1, s_previous_feedback = -2;
static bool s_previous_input;

static const char *const s_level_names[MARVY_LEVEL_COUNT] = {
    "初试身手", "凭手感套", "预判高手", "果断出手", "追赶 Marvy", "新年挑战",
};

static const char s_digits[10][16] = {
    "111101101101111", "010110010010111", "111001111100111",
    "111001111001111", "101101111001001", "111100111001111",
    "111100111101111", "111001001001001", "111101111101111",
    "111101111001111",
};

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
    for (int digit = 2; digit >= 0; --digit) {
        unsigned value = score % 10;
        score /= 10;
        for (unsigned pixel = 0; pixel < 15; ++pixel) {
            visible(s_result_digits[digit][pixel], s_digits[value][pixel] == '1');
        }
    }
}

static void fireworks_update(uint32_t remaining_ms)
{
    static const int directions[8][2] = {
        {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1},
    };
    uint32_t elapsed = remaining_ms > MARVY_MARVY_FEEDBACK_MS ? 0
                       : MARVY_MARVY_FEEDBACK_MS - remaining_ms;
    for (unsigned i = 0; i < FIREWORK_PARTICLES; ++i) {
        unsigned burst = i / 8;
        uint32_t delay = burst * 170;
        bool showing = remaining_ms > 0 && elapsed >= delay;
        uint32_t age = showing ? (elapsed - delay) % 650 : 0;
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

static void stars_create(lv_obj_t *parent, lv_obj_t *stars[3][6], int y)
{
    static const int parts[6][4] = {
        {6, 0, 3, 3}, {0, 3, 15, 3}, {3, 6, 9, 3},
        {0, 9, 15, 3}, {0, 12, 3, 3}, {12, 12, 3, 3},
    };
    for (unsigned star = 0; star < 3; ++star) {
        for (unsigned part = 0; part < 6; ++part) {
            stars[star][part] = box(parent, 85 + (int)star * 28 + parts[part][0],
                                    y + parts[part][1], parts[part][2], parts[part][3], SKY_LIGHT);
        }
    }
}

static void stars_update(lv_obj_t *stars[3][6], unsigned earned)
{
    for (unsigned star = 0; star < 3; ++star) {
        for (unsigned part = 0; part < 6; ++part) {
            lv_obj_set_style_bg_color(stars[star][part],
                                      lv_color_hex(star < earned ? GOLD : SKY_LIGHT), 0);
        }
    }
}

static void row_selected(lv_obj_t *row, bool selected)
{
    lv_obj_set_style_bg_color(row, lv_color_hex(selected ? PURPLE : SKY_LIGHT), 0);
    lv_obj_set_style_border_width(row, selected ? 2 : 0, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(LILAC), 0);
}

/* The existing render loop drives these objects, so hiding/dimming the page
 * needs no extra animation callbacks, timers or temporary drawing buffers. */
static void welcome_update(uint32_t elapsed_ms)
{
    static const int accents[WELCOME_SPARKLES][2] = {
        {40, 111}, {195, 96}, {33, 183}, {207, 177}, {49, 219}, {190, 224},
    };
    uint32_t phase = elapsed_ms % 3200;
    uint32_t breath = phase <= 1600 ? phase : 3200 - phase;
    lv_obj_set_y(s_welcome_logo, 86 + (int)(breath * 8 / 1600));
    lv_obj_set_style_opa(s_welcome_logo, (lv_opa_t)(218 + breath * 37 / 1600), 0);
    lv_obj_set_style_text_opa(s_welcome_prompt,
                             (lv_opa_t)(160 + breath * 95 / 1600), 0);
    for (unsigned i = 0; i < WELCOME_SPARKLES; ++i) {
        uint32_t sparkle_phase = (elapsed_ms % 2400 + i * 370) % 2400;
        uint32_t glow = sparkle_phase <= 1200 ? sparkle_phase : 2400 - sparkle_phase;
        int size = 3 + (int)(glow * 3 / 1200);
        lv_obj_set_size(s_welcome_sparkles[i], size, size);
        lv_obj_set_pos(s_welcome_sparkles[i], accents[i][0] - size / 2,
                        accents[i][1] - size / 2 - (int)(glow * 3 / 1200));
        lv_obj_set_style_bg_opa(s_welcome_sparkles[i],
                                (lv_opa_t)(72 + glow * 132 / 1200), 0);
    }
}

void marvy_view_create(void)
{
    s_first_render = true;
    s_previous_battery = -2;
    s_previous_countdown = s_previous_charge = -1;
    s_previous_feedback = -2;
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(PAPER), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    s_intro = group(s_screen, 0, 0, 240, 320);
    label(s_intro, "Marvy 套羊", 18, 58, 204, &marvy_font_20, INK);
    for (unsigned i = 0; i < WELCOME_SPARKLES; ++i) {
        s_welcome_sparkles[i] = box(s_intro, 0, 0, 4, 4, i % 2 ? PURPLE : PINK);
        lv_obj_set_style_radius(s_welcome_sparkles[i], LV_RADIUS_CIRCLE, 0);
    }
    s_welcome_logo = lv_image_create(s_intro);
    lv_image_set_src(s_welcome_logo, &marvy_logo_welcome);
    lv_obj_set_pos(s_welcome_logo, 64, 90);
    label(s_intro, "Marvy 祝你", 18, 215, 204, &marvy_font_20, INK);
    label(s_intro, "新年快乐", 18, 244, 204, &marvy_font_20, PURPLE);
    s_welcome_prompt = label(s_intro, "按任意功能键选关", 12, 278, 216, &marvy_font_16, PURPLE);

    s_brief = group(s_screen, 0, 0, 240, 320);
    s_brief_heading = label(s_brief, "第 1 关 / 6", 12, 55, 216, &marvy_pixel_16, CYAN);
    s_brief_name = label(s_brief, "初试身手", 12, 86, 216, &marvy_pixel_20, WHITE);
    label(s_brief, "45 秒 不限圈数", 12, 121, 216, &marvy_pixel_16, GOLD);
    s_brief_goal = label(s_brief, "套中 4 只", 12, 151, 216, &marvy_pixel_20, WHITE);
    s_brief_best = label(s_brief, "最高 0 分", 12, 185, 216, &marvy_pixel_16, GOLD);
    stars_create(s_brief, s_brief_stars, 215);
    s_brief_rule = label(s_brief, "有落点提示 无出手限时", 8, 237, 224, &marvy_pixel_16, CYAN);
    label(s_brief, "上下选择已解锁关卡", 10, 262, 220, &marvy_pixel_16, CYAN);
    box(s_brief, 36, 284, 168, 28, PURPLE);
    label(s_brief, "确定开始", 38, 288, 164, &marvy_pixel_16, WHITE);

    s_play = group(s_screen, 0, 0, 240, 320);
    s_level_heading = label(s_play, "第 1 关 初试身手", 8, 44, 224, &marvy_pixel_16, CYAN);
    s_timer = label(s_play, "剩余 45 秒", 8, 73, 139, &marvy_pixel_20, WHITE);
    s_score = label(s_play, "得分 0", 148, 77, 83, &marvy_pixel_16, GOLD);
    landscape(s_play);
    box(s_play, 18, 102, 204, 4, SKY_LIGHT);
    s_timebar = box(s_play, 18, 102, 204, 4, CYAN);
    s_legend = label(s_play, "普通 +1 / Marvy +5", 12, 115, 216, &marvy_pixel_16, WHITE);
    s_target = ring_create(s_play, CYAN);
    lv_obj_set_style_opa(s_target.group, 170, 0);
    for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) {
        s_sheep[i] = sheep_create(s_play, 0, MARVY_SHEEP_BODY_Y - 24, false);
    }
    s_caught = sheep_create(s_play, 0, MARVY_SHEEP_BODY_Y - 24, true);
    visible(s_caught.group, false);
    s_progress = label(s_play, "套中 0 / 4", 8, 230, 137, &marvy_pixel_16, WHITE);
    s_combo = label(s_play, "已投 0", 146, 230, 86, &marvy_pixel_16, CYAN);
    box(s_play, 29, 251, 182, 13, SKY_LIGHT);
    for (unsigned i = 0; i < CHARGE_SEGMENTS; ++i) {
        s_charge[i] = box(s_play, 32 + (int)i * 15, 254, 11, 7, DARK_GRASS);
    }
    box(s_play, 17, 269, 22, 3, GOLD);
    box(s_play, 20, 272, 16, 7, 0xA36A52);
    s_ring = ring_create(s_play, GOLD);
    s_hint = label(s_play, "上下蓄力 松手投圈\n确定暂停", 8, 277, 224, &marvy_pixel_16, WHITE);
    for (unsigned i = 0; i < FIREWORK_PARTICLES; ++i) {
        s_firework[i] = box(s_play, 0, 0, 3, 3, i % 2 ? PINK : GOLD);
        visible(s_firework[i], false);
    }
    s_feedback = group(s_play, 30, 111, 180, 51);
    s_feedback_points_shadow = label(s_feedback, "", 1, 1, 180, &marvy_pixel_20, INK);
    s_feedback_message_shadow = label(s_feedback, "", 1, 28, 180, &marvy_pixel_20, INK);
    s_feedback_points = label(s_feedback, "", 0, 0, 180, &marvy_pixel_20, GOLD);
    s_feedback_message = label(s_feedback, "", 0, 27, 180, &marvy_pixel_20, WHITE);
    visible(s_feedback, false);
    s_countdown = group(s_play, 108, 111, 24, 40);
    for (unsigned pixel = 0; pixel < 15; ++pixel) {
        s_countdown_pixels[pixel] = box(s_countdown,
            (int)(pixel % 3) * 8, (int)(pixel / 3) * 8, 7, 7, GOLD);
    }
    visible(s_countdown, false);

    s_results = group(s_screen, 0, 0, 240, 320);
    s_result_heading = label(s_results, "挑战完成", 12, 58, 216, &marvy_pixel_20, WHITE);
    label(s_results, "本关得分", 12, 84, 216, &marvy_pixel_16, CYAN);
    result_digits_create(s_results);
    stars_create(s_results, s_result_stars, 148);
    s_result_summary = label(s_results, "套中 0 / 4 只", 8, 174, 224, &marvy_pixel_16, WHITE);
    s_result_metrics = label(s_results, "投掷 0 次 命中 0%", 8, 198, 224, &marvy_pixel_16, CYAN);
    static const char *const choices[3] = {"下一关", "重新挑战", "返回欢迎页"};
    for (unsigned i = 0; i < 3; ++i) {
        s_result_rows[i] = box(s_results, 34, 226 + (int)i * 28, 172, 26, SKY_LIGHT);
        s_result_labels[i] = label(s_results, choices[i], 36,
                                   228 + (int)i * 28, 168, &marvy_pixel_16, WHITE);
    }

    s_pause = group(s_screen, 20, 100, 200, 168);
    box(s_pause, 4, 4, 200, 168, INK);
    lv_obj_t *pause_panel = box(s_pause, 0, 0, 200, 168, SKY);
    lv_obj_set_style_border_width(pause_panel, 2, 0);
    lv_obj_set_style_border_color(pause_panel, lv_color_hex(CYAN), 0);
    label(s_pause, "游戏暂停", 8, 12, 184, &marvy_pixel_20, WHITE);
    for (unsigned i = 0; i < 2; ++i) {
        s_pause_rows[i] = box(s_pause, 12, 48 + (int)i * 43, 176, 33, SKY_LIGHT);
    }
    label(s_pause, "继续游戏", 14, 54, 172, &marvy_pixel_16, WHITE);
    label(s_pause, "返回欢迎页", 14, 97, 172, &marvy_pixel_16, WHITE);
    label(s_pause, "上下选择 确定确认", 6, 140, 188, &marvy_pixel_16, CYAN);
    visible(s_pause, false);

    s_header = label(s_screen, "MARVY / 2027", 20, 17, 145, &marvy_font_16, PURPLE);
    s_battery = label(s_screen, "--%", 178, 17, 43, &marvy_font_16, INK);
    visible(s_brief, false);
    visible(s_play, false);
    visible(s_results, false);
    lv_screen_load(s_screen);
}

static void render_brief(const marvy_game_t *game, const marvy_level_config_t *config)
{
    if (s_first_render || s_previous.phase != game->phase ||
        s_previous.level_index != game->level_index ||
        s_previous.progress.best_score[game->level_index] != game->progress.best_score[game->level_index] ||
        s_previous.progress.best_stars[game->level_index] != game->progress.best_stars[game->level_index]) {
        lv_label_set_text_fmt(s_brief_heading, "第 %u 关 / 6", (unsigned)game->level_index + 1);
        lv_label_set_text(s_brief_name, s_level_names[game->level_index]);
        lv_label_set_text_fmt(s_brief_goal, "套中 %u 只", (unsigned)config->target_catches);
        lv_label_set_text_fmt(s_brief_best, "最高 %u 分", (unsigned)game->progress.best_score[game->level_index]);
        stars_update(s_brief_stars, game->progress.best_stars[game->level_index]);
        if (config->show_target) lv_label_set_text(s_brief_rule, "有落点提示 无出手限时");
        else if (config->turn_limit_ms == 0) lv_label_set_text(s_brief_rule, "无落点提示 无出手限时");
        else lv_label_set_text_fmt(s_brief_rule, "无落点提示 %u.%u 秒出手", (unsigned)config->turn_limit_ms / 1000, (unsigned)config->turn_limit_ms % 1000 / 100);
    }
}

static void render_results(const marvy_game_t *game, const marvy_level_config_t *config)
{
    bool changed = s_first_render || s_previous.phase != game->phase ||
                   s_previous.level_index != game->level_index ||
                   s_previous.score != game->score || s_previous.captures != game->captures ||
                   s_previous.throws != game->throws || s_previous.stars != game->stars ||
                   s_previous.passed != game->passed;
    if (changed) {
        const char *heading = !game->passed ? "再试一次"
                            : game->level_index + 1 == MARVY_LEVEL_COUNT ? "全部通关" : "过关啦";
        lv_label_set_text(s_result_heading, heading);
        lv_obj_set_style_text_color(s_result_heading, lv_color_hex(game->passed ? GOLD : WHITE), 0);
        result_digits_update(game->score);
        stars_update(s_result_stars, game->stars);
        lv_label_set_text_fmt(s_result_summary, "套中 %u / %u 只",
                               (unsigned)game->captures, (unsigned)config->target_catches);
        unsigned accuracy = game->throws ? (unsigned)game->captures * 100u / game->throws : 0;
        lv_label_set_text_fmt(s_result_metrics, "投掷 %u 次 命中 %u%%", (unsigned)game->throws, accuracy);
        bool next = marvy_game_can_next(game);
        visible(s_result_rows[0], next);
        visible(s_result_labels[0], next);
    }
    if (changed || s_previous.result_selection != game->result_selection) {
        for (unsigned i = 0; i < 3; ++i) row_selected(s_result_rows[i], i == (unsigned)game->result_selection);
    }
}

static void render_play(const marvy_game_t *game, const marvy_level_config_t *config,
                         bool input_available, bool page_changed)
{
    uint32_t remaining = marvy_game_remaining_ms(game);
    unsigned seconds = (remaining + 999) / 1000;
    if (page_changed || s_previous.level_index != game->level_index) {
        lv_label_set_text_fmt(s_level_heading, "第 %u 关 %s",
                               (unsigned)game->level_index + 1, s_level_names[game->level_index]);
    }
    if (page_changed || seconds != (marvy_game_remaining_ms(&s_previous) + 999) / 1000) {
        lv_label_set_text_fmt(s_timer, "剩余 %u 秒", seconds);
        lv_obj_set_style_text_color(s_timer, lv_color_hex(seconds <= 5 ? PINK : WHITE), 0);
        lv_obj_set_style_bg_color(s_timebar, lv_color_hex(seconds <= 5 ? PINK : CYAN), 0);
    }
    unsigned width = (unsigned)((uint64_t)remaining * 204 / config->duration_ms);
    visible(s_timebar, width > 0);
    lv_obj_set_width(s_timebar, width > 0 ? (int)width : 1);
    if (page_changed || s_previous.score != game->score) {
        lv_label_set_text_fmt(s_score, "得分 %u", (unsigned)game->score);
    }
    if (page_changed || s_previous.captures != game->captures) {
        lv_label_set_text_fmt(s_progress, "套中 %u / %u", (unsigned)game->captures,
                               (unsigned)config->target_catches);
        lv_obj_set_style_text_color(s_progress, lv_color_hex(game->captures >= config->target_catches ? GOLD : WHITE), 0);
    }
    if (page_changed || s_previous.throws != game->throws ||
        (s_previous.combo_ms > 0) != (game->combo_ms > 0) || s_previous.combo_count != game->combo_count) {
        if (game->combo_ms > 0) lv_label_set_text_fmt(s_combo, "%u 连中", (unsigned)game->combo_count);
        else lv_label_set_text_fmt(s_combo, "已投 %u", (unsigned)game->throws);
        lv_obj_set_style_text_color(s_combo, lv_color_hex(game->combo_ms > 0 ? GOLD : CYAN), 0);
    }
    if (page_changed || s_previous_input != input_available ||
        s_previous.ring_state != game->ring_state || s_previous.time_expired != game->time_expired) {
        const char *hint = "上下蓄力 松手投圈\n确定暂停";
        if (!input_available) hint = "按键未就绪";
        else if (game->time_expired) hint = "时间到 等圈落地";
        else if (game->ring_state == MARVY_RING_CHARGING) hint = "松手投圈\n确定暂停";
        else if (game->ring_state == MARVY_RING_FLYING) hint = "圈在空中\n确定暂停";
        lv_label_set_text(s_hint, hint);
    }
    uint32_t held = game->ring_state == MARVY_RING_CHARGING ? game->ring_ms : 0;
    if (held > MARVY_CHARGE_MS) held = MARVY_CHARGE_MS;
    int lit = (int)((held * CHARGE_SEGMENTS + MARVY_CHARGE_MS - 1) / MARVY_CHARGE_MS);
    if (s_previous_charge != lit) {
        for (int i = 0; i < CHARGE_SEGMENTS; ++i) {
            lv_obj_set_style_bg_color(s_charge[i], lv_color_hex(i < lit ? (i < 8 ? CYAN : PINK) : DARK_GRASS), 0);
        }
        s_previous_charge = lit;
    }
    bool aiming = game->ring_state == MARVY_RING_READY || game->ring_state == MARVY_RING_CHARGING;
    visible(s_target.group, config->show_target && aiming && !game->time_expired);
    if (config->show_target && aiming) ring_position(&s_target,
        (int)marvy_game_charge_target_x(held), MARVY_SHEEP_BODY_Y + 22, 38, 10);
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
    bool celebration = game->celebration_ms > 0;
    visible(s_caught.group, celebration || (feedback && game->last_points > 0));
    if (celebration) {
        sheep_color(&s_caught, true);
        uint32_t age = MARVY_MARVY_FEEDBACK_MS - game->celebration_ms;
        lv_obj_set_pos(s_caught.group, (int)game->celebration_x - 27,
                        MARVY_SHEEP_BODY_Y - 24 - (int)(age * 6 / MARVY_MARVY_FEEDBACK_MS));
    } else if (feedback && game->last_points > 0) {
        sheep_color(&s_caught, game->last_hit_marvy);
        lv_obj_set_pos(s_caught.group, (int)game->last_hit_x - 27,
                        MARVY_SHEEP_BODY_Y - 24 - (int)(game->ring_ms * 6 / MARVY_FEEDBACK_MS));
    }
    visible(s_feedback, celebration || feedback);
    int kind = celebration ? 5 : !feedback ? -1 : game->last_points ? 1 : 0;
    if (s_previous_feedback != kind) {
        const char *points = kind == 5 ? "+5 分" : kind == 1 ? "+1 分" : "差一点";
        const char *message = kind == 5 ? "新年快乐" : kind == 1 ? "套中啦" : "再来";
        lv_label_set_text(s_feedback_points, points);
        lv_label_set_text(s_feedback_message, message);
        lv_label_set_text(s_feedback_points_shadow, points);
        lv_label_set_text(s_feedback_message_shadow, message);
        lv_obj_set_style_text_color(s_feedback_points, lv_color_hex(kind == 5 ? PINK : GOLD), 0);
        s_previous_feedback = kind;
    }
    fireworks_update(game->celebration_ms);
    unsigned countdown = marvy_game_countdown(game);
    if (s_previous_countdown != (int)countdown) {
        visible(s_countdown, countdown > 0);
        for (unsigned pixel = 0; pixel < 15; ++pixel) {
            visible(s_countdown_pixels[pixel], countdown > 0 && s_digits[countdown][pixel] == '1');
            lv_obj_set_style_bg_color(s_countdown_pixels[pixel], lv_color_hex(countdown == 1 ? PINK : GOLD), 0);
        }
        s_previous_countdown = (int)countdown;
    }
    visible(s_legend, !feedback && !celebration && countdown == 0);
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
    } else ring_position(&s_ring, 28, 266, 36, 12);
}

void marvy_view_render(const marvy_game_t *game, int battery_percent,
                       bool input_available)
{
    const marvy_level_config_t *config = marvy_game_level_config(game->level_index);
    if (s_previous_battery != battery_percent) {
        if (battery_percent < 0) lv_label_set_text(s_battery, "--%");
        else lv_label_set_text_fmt(s_battery, "%d%%", battery_percent);
        s_previous_battery = battery_percent;
    }
    bool gameplay = game->phase == MARVY_PHASE_PLAYING || game->phase == MARVY_PHASE_PAUSED;
    bool round_restarted = gameplay && (s_previous.level_index != game->level_index ||
                                          game->stage_elapsed_ms < s_previous.stage_elapsed_ms);
    bool page_changed = s_first_render || s_previous.phase != game->phase || round_restarted;
    if (page_changed) {
        bool intro = game->phase == MARVY_PHASE_INTRO;
        bool pause_transition = !round_restarted && gameplay &&
                               (s_previous.phase == MARVY_PHASE_PLAYING ||
                                s_previous.phase == MARVY_PHASE_PAUSED);
        visible(s_intro, intro);
        visible(s_brief, game->phase == MARVY_PHASE_BRIEF);
        visible(s_play, gameplay);
        visible(s_pause, game->phase == MARVY_PHASE_PAUSED);
        visible(s_hint, game->phase != MARVY_PHASE_PAUSED);
        visible(s_results, game->phase == MARVY_PHASE_RESULTS);
        lv_obj_set_style_bg_color(s_screen, lv_color_hex(intro ? PAPER : SKY), 0);
        lv_obj_set_style_text_font(s_header, intro ? &marvy_font_16 : &marvy_pixel_16, 0);
        lv_obj_set_style_text_font(s_battery, intro ? &marvy_font_16 : &marvy_pixel_16, 0);
        lv_obj_set_style_text_color(s_header, lv_color_hex(intro ? PURPLE : CYAN), 0);
        lv_obj_set_style_text_color(s_battery, lv_color_hex(intro ? INK : WHITE), 0);
        s_previous_feedback = -2;
        s_previous_charge = s_previous_countdown = -1;
        if (!pause_transition) {
            for (unsigned i = 0; i < MARVY_SHEEP_COUNT; ++i) s_sheep[i].was_visible = false;
        }
    }
    if (game->phase == MARVY_PHASE_INTRO) welcome_update(game->phase_ms);
    else if (game->phase == MARVY_PHASE_BRIEF) render_brief(game, config);
    else if (game->phase == MARVY_PHASE_RESULTS) render_results(game, config);
    else if (gameplay) render_play(game, config, input_available, page_changed);
    if (game->phase == MARVY_PHASE_PAUSED &&
        (page_changed || s_previous.pause_selection != game->pause_selection)) {
        for (unsigned i = 0; i < 2; ++i) row_selected(s_pause_rows[i], i == (unsigned)game->pause_selection);
    }
    s_previous = *game;
    s_previous_input = input_available;
    s_first_render = false;
}
