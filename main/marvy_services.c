#include "marvy_services.h"

#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"

#include <stddef.h>

#define AUDIO_RATE_HZ 16000U
#define PCM_SAMPLES 256U
#define TONE_AMPLITUDE 1200
#define SAVE_RETRY_MS 1000U

typedef struct {
    uint16_t hz;
    uint16_t ms;
} tone_t;

static const char *TAG = "marvy_services";
static QueueHandle_t s_sounds;
static TaskHandle_t s_task;
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
static uint16_t s_best;
static uint16_t s_saved_best;
static bool s_loaded;
static bool s_audio_attempted;
static bool s_audio_ready;
static bool s_save_attempted;
static TickType_t s_last_save_attempt;

/* Only this worker accesses PCM, codec state and the save retry state. */
static int16_t s_pcm[PCM_SAMPLES];

uint16_t marvy_services_best_score(void)
{
    portENTER_CRITICAL(&s_state_lock);
    uint16_t best = s_best;
    portEXIT_CRITICAL(&s_state_lock);
    return best;
}

void marvy_services_save_best(uint16_t score)
{
    portENTER_CRITICAL(&s_state_lock);
    if (score > s_best) s_best = score;
    portEXIT_CRITICAL(&s_state_lock);
}

void marvy_services_play(marvy_sound_t sound)
{
    if (!s_sounds || sound < MARVY_SOUND_INTRO || sound > MARVY_SOUND_FINISH) return;
    (void)xQueueOverwrite(s_sounds, &sound);
}

static void load_best(void)
{
    if (s_loaded) return;
    s_loaded = true;
    nvs_handle_t handle;
    esp_err_t error = nvs_open("marvy_ring", NVS_READONLY, &handle);
    if (error != ESP_OK) {
        if (error != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Best-score storage unavailable: %s", esp_err_to_name(error));
        }
        return;
    }

    uint16_t stored = 0;
    error = nvs_get_u16(handle, "best", &stored);
    nvs_close(handle);
    if (error == ESP_OK) {
        marvy_services_save_best(stored);
        s_saved_best = stored;
    } else if (error != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Could not read best score: %s", esp_err_to_name(error));
    }
}

static void persist_best(TickType_t now)
{
    uint16_t best = marvy_services_best_score();
    if (best <= s_saved_best) return;
    if (s_save_attempted &&
        (TickType_t)(now - s_last_save_attempt) < pdMS_TO_TICKS(SAVE_RETRY_MS)) return;
    s_save_attempted = true;
    s_last_save_attempt = now;

    nvs_handle_t handle;
    esp_err_t error = nvs_open("marvy_ring", NVS_READWRITE, &handle);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "Could not open best-score storage: %s", esp_err_to_name(error));
        return;
    }

    /* Re-read before writing: a transient startup read failure must not replace
     * an existing higher record with the current session's lower score. */
    uint16_t stored = 0;
    error = nvs_get_u16(handle, "best", &stored);
    if (error == ESP_OK && stored >= best) {
        s_saved_best = stored;
        marvy_services_save_best(stored);
        nvs_close(handle);
        return;
    }
    if (error == ESP_OK || error == ESP_ERR_NVS_NOT_FOUND) {
        error = nvs_set_u16(handle, "best", best);
        if (error == ESP_OK) error = nvs_commit(handle);
    }
    nvs_close(handle);

    if (error == ESP_OK) {
        s_saved_best = best;
    } else {
        ESP_LOGW(TAG, "Best score stays in RAM; save will retry: %s", esp_err_to_name(error));
    }
}

static bool play_tone(tone_t tone)
{
    const uint32_t count = AUDIO_RATE_HZ * tone.ms / 1000U;
    const uint32_t phase_step = (uint32_t)(((uint64_t)tone.hz << 32) / AUDIO_RATE_HZ);
    uint32_t phase = 0;
    for (uint32_t offset = 0; offset < count; offset += PCM_SAMPLES) {
        uint32_t chunk = count - offset;
        if (chunk > PCM_SAMPLES) chunk = PCM_SAMPLES;
        for (uint32_t i = 0; i < chunk; i++) {
            uint32_t at = offset + i;
            uint32_t remaining = count - at - 1;
            int32_t gain = TONE_AMPLITUDE;
            if (at < 64U) gain = gain * (int32_t)at / 64;
            if (remaining < 128U) gain = gain * (int32_t)remaining / 128;
            phase += phase_step;
            uint32_t ramp = phase >> 16;
            int32_t triangle = (int32_t)(ramp < 32768U ? ramp : 65535U - ramp) * 2 - 32767;
            s_pcm[i] = (int16_t)(triangle * gain / 32768);
        }
        if (bsp_audio_write(s_pcm, chunk * sizeof(s_pcm[0])) != ESP_OK) return false;
    }
    return true;
}

static void play_sound(marvy_sound_t sound)
{
    /* Small original scale motifs. These are replaceable prototype cues, not
     * recordings or an approved brand sound logo. */
    static const tone_t intro[] = {{523, 65}, {659, 65}, {784, 70}, {1047, 115}};
    static const tone_t thrown[] = {{440, 40}, {660, 45}};
    static const tone_t hit[] = {{880, 65}, {1175, 95}};
    static const tone_t appear[] = {{659, 60}, {880, 75}};
    static const tone_t marvy_hit[] = {{784, 60}, {988, 60}, {1175, 60}, {1568, 120}};
    static const tone_t miss[] = {{330, 65}, {262, 80}};
    static const tone_t finish[] = {{659, 70}, {784, 70}, {1047, 120}};
    const tone_t *notes = NULL;
    size_t count = 0;
#define SELECT_NOTES(array) do { notes = (array); count = sizeof(array) / sizeof((array)[0]); } while (0)
    switch (sound) {
    case MARVY_SOUND_INTRO: SELECT_NOTES(intro); break;
    case MARVY_SOUND_THROW: SELECT_NOTES(thrown); break;
    case MARVY_SOUND_HIT: SELECT_NOTES(hit); break;
    case MARVY_SOUND_MARVY_APPEAR: SELECT_NOTES(appear); break;
    case MARVY_SOUND_MARVY_HIT: SELECT_NOTES(marvy_hit); break;
    case MARVY_SOUND_MISS: SELECT_NOTES(miss); break;
    case MARVY_SOUND_FINISH: SELECT_NOTES(finish); break;
    }
#undef SELECT_NOTES

    esp_err_t error = bsp_audio_wake();
    if (error == ESP_OK) error = bsp_audio_set_format(AUDIO_RATE_HZ, 16, 1);
    bool played = error == ESP_OK;
    for (size_t i = 0; played && i < count; i++) played = play_tone(notes[i]);

    /* The envelope ends at zero; suspend between short cues to avoid keeping
     * the codec active while the player is idle. Flash saves happen only at
     * these playback boundaries, never during a PCM stream. */
    esp_err_t sleep_error = bsp_audio_sleep();
    if (!played || sleep_error != ESP_OK) {
        s_audio_ready = false;
        ESP_LOGW(TAG, "Audio unavailable; continuing silently");
    }
}

static void services_worker(void *arg)
{
    (void)arg;
    if (!s_audio_attempted) {
        s_audio_attempted = true;
        s_audio_ready = bsp_audio_init() == ESP_OK;
        if (s_audio_ready) {
            bsp_audio_set_volume(32);
            s_audio_ready = bsp_audio_sleep() == ESP_OK;
        }
        if (!s_audio_ready) ESP_LOGW(TAG, "Audio init unavailable; score storage remains active");
    }

    for (;;) {
        marvy_sound_t sound;
        BaseType_t has_sound = xQueueReceive(s_sounds, &sound, pdMS_TO_TICKS(20));
        persist_best(xTaskGetTickCount());
        if (has_sound == pdTRUE && s_audio_ready) play_sound(sound);
    }
}

bool marvy_services_start(void)
{
    if (s_task) return true;
    load_best();
    s_sounds = xQueueCreate(1, sizeof(marvy_sound_t));
    if (!s_sounds) return false;
    if (xTaskCreate(services_worker, "marvy_services", 3072, NULL, 4, &s_task) != pdPASS) {
        vQueueDelete(s_sounds);
        s_sounds = NULL;
        s_task = NULL;
        return false;
    }
    return true;
}
