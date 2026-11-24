#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "key_guard.h"
#include "marvy_session.h"
#include "marvy_services.h"
#include "marvy_view.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "marvy_ring";
static QueueHandle_t s_input;
static volatile bool s_input_ready;
static key_guard_t s_key_guard;

static void on_key(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)button;
    (void)user;
    /* PRESS is the only action. CLICK/DOUBLE/LONG must not spend extra rings.
     * The shared esp_timer callback only enqueues, never blocks or draws. */
    if (key_guard_consume(&s_key_guard, event == BSP_BTN_RELEASE, esp_timer_get_time())) return;
    if (!s_input_ready || s_input == NULL) return;
    if (event == BSP_BTN_PRESS) {
        uint64_t pressed_at_ms = (uint64_t)(esp_timer_get_time() / 1000);
        (void)xQueueSend(s_input, &pressed_at_ms, 0);
    }
}

static void dispatch_events(uint32_t events, const marvy_game_t *game)
{
    if (events & MARVY_EVENT_NEW_BEST) marvy_services_save_best(game->best_score);
    if (events & MARVY_EVENT_MARVY_HIT) marvy_services_play(MARVY_SOUND_MARVY_HIT);
    else if (events & MARVY_EVENT_HIT) marvy_services_play(MARVY_SOUND_HIT);
    else if (events & MARVY_EVENT_FINISH) marvy_services_play(MARVY_SOUND_FINISH);
    else if (events & MARVY_EVENT_THROW) marvy_services_play(MARVY_SOUND_THROW);
    else if (events & MARVY_EVENT_MISS) marvy_services_play(MARVY_SOUND_MISS);
    else if (events & MARVY_EVENT_MARVY_APPEAR) marvy_services_play(MARVY_SOUND_MARVY_APPEAR);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Marvy sheep ring: branded music iteration 0.2.0-dev");
    if (esp_sleep_get_wakeup_causes() & (1u << ESP_SLEEP_WAKEUP_GPIO)) {
        key_guard_arm(&s_key_guard, esp_timer_get_time(), 5000000);
    }
    esp_err_t nvs_error = nvs_flash_init();
    if (nvs_error != ESP_OK) {
        /* Preserve unrelated settings; never erase NVS automatically. */
        ESP_LOGW(TAG, "NVS unavailable (%s); session-only record", esp_err_to_name(nvs_error));
    }
    if (bsp_display_init() != ESP_OK || bsp_lvgl_init() == NULL) {
        ESP_LOGE(TAG, "Display unavailable; cannot start game");
        return;
    }
    bsp_display_backlight(75);
    /* Finish shared-I2C initialization before starting the audio worker. */
    bool battery_ready = bsp_battery_init() == ESP_OK;
    int battery = battery_ready ? bsp_battery_soc() : -1;
    if (!marvy_services_start()) ESP_LOGW(TAG, "Background services unavailable");
    s_input = xQueueCreate(8, sizeof(uint64_t));
    bool input_ready = s_input != NULL && bsp_button_init(on_key, NULL) == ESP_OK;
    if (!input_ready) ESP_LOGE(TAG, "Button input unavailable");
    if (input_ready && s_key_guard.armed) {
        key_guard_sync_released(&s_key_guard,
                                bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN);
    }

    marvy_session_t session;
    marvy_session_init(&session, esp_random(), marvy_services_best_score(),
                       (uint64_t)(esp_timer_get_time() / 1000));
    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "Cannot acquire display lock");
        return;
    }
    marvy_view_create();
    marvy_view_render(&session.game, battery, input_ready);
    s_input_ready = input_ready;
    bsp_lvgl_unlock();
    marvy_services_play(MARVY_SOUND_INTRO);

    uint64_t last_battery = session.now_ms;
    bool previous_dimmed = false;
    for (;;) {
        /* Sample before draining. An event enqueued just after the empty check
         * then stays newer than this cutoff and remains valid next frame. */
        uint64_t now = (uint64_t)(esp_timer_get_time() / 1000);
        uint32_t events = 0;
        uint64_t pressed_at_ms;
        /* Preserve the order of input and simulation deadlines. A key pressed
         * during the final feedback must not become a results-page restart. */
        while (s_input != NULL && xQueueReceive(s_input, &pressed_at_ms, 0) == pdTRUE) {
            events |= marvy_session_press(&session, pressed_at_ms);
        }
        events |= marvy_session_advance(&session, now);
        marvy_services_set_music(session.game.phase == MARVY_PHASE_PLAYING &&
                                 !session.dimmed);
        dispatch_events(events, &session.game);
        uint16_t stored_best = marvy_services_best_score();
        if (stored_best > session.game.best_score) session.game.best_score = stored_best;
        if (now - last_battery >= 30000) {
            battery = battery_ready ? bsp_battery_soc() : -1;
            last_battery = now;
        }
        if (previous_dimmed != session.dimmed) {
            bsp_display_backlight(session.dimmed ? 12 : 75);
            previous_dimmed = session.dimmed;
        }
        if (!session.dimmed && bsp_lvgl_lock(30)) {
            marvy_view_render(&session.game, battery, input_ready);
            bsp_lvgl_unlock();
        }
        /* Always yield, even when drawing takes longer than a frame budget. */
        vTaskDelay(pdMS_TO_TICKS(session.dimmed ? 100 : 33));
    }
}
