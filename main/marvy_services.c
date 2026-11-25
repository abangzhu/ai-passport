#include "marvy_services.h"

#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"

#include <stddef.h>

#define PCM_SAMPLES 256U
#define SAVE_RETRY_MS 1000U
#define PROGRESS_BLOB_BYTES (2U + MARVY_LEVEL_COUNT * 3U)
#define PROGRESS_SCHEMA 1U

_Static_assert(PROGRESS_BLOB_BYTES == 20, "Campaign schema has six records");

/* The build embeds only the converted PCM; host tests inject a tiny source. */
#ifndef MARVY_BGM_PCM_DATA
extern const uint8_t marvy_bgm_pcm_start[] __asm__("_binary_marvy_bgm_pcm_start");
extern const uint8_t marvy_bgm_pcm_end[] __asm__("_binary_marvy_bgm_pcm_end");
#define MARVY_BGM_PCM_DATA marvy_bgm_pcm_start
#define MARVY_BGM_PCM_BYTES ((size_t)(marvy_bgm_pcm_end - marvy_bgm_pcm_start))
#endif
#ifndef MARVY_HIT_PCM_DATA
extern const uint8_t marvy_hit_pcm_start[] __asm__("_binary_marvy_hit_pcm_start");
extern const uint8_t marvy_hit_pcm_end[] __asm__("_binary_marvy_hit_pcm_end");
#define MARVY_HIT_PCM_DATA marvy_hit_pcm_start
#define MARVY_HIT_PCM_BYTES ((size_t)(marvy_hit_pcm_end - marvy_hit_pcm_start))
#endif
#ifndef MARVY_BONUS_PCM_DATA
extern const uint8_t marvy_bonus_pcm_start[] __asm__("_binary_marvy_bonus_pcm_start");
extern const uint8_t marvy_bonus_pcm_end[] __asm__("_binary_marvy_bonus_pcm_end");
#define MARVY_BONUS_PCM_DATA marvy_bonus_pcm_start
#define MARVY_BONUS_PCM_BYTES ((size_t)(marvy_bonus_pcm_end - marvy_bonus_pcm_start))
#endif
#ifndef MARVY_CLEAR_PCM_DATA
extern const uint8_t marvy_clear_pcm_start[] __asm__("_binary_marvy_clear_pcm_start");
extern const uint8_t marvy_clear_pcm_end[] __asm__("_binary_marvy_clear_pcm_end");
#define MARVY_CLEAR_PCM_DATA marvy_clear_pcm_start
#define MARVY_CLEAR_PCM_BYTES ((size_t)(marvy_clear_pcm_end - marvy_clear_pcm_start))
#endif
#ifndef MARVY_FAIL_PCM_DATA
extern const uint8_t marvy_fail_pcm_start[] __asm__("_binary_marvy_fail_pcm_start");
extern const uint8_t marvy_fail_pcm_end[] __asm__("_binary_marvy_fail_pcm_end");
#define MARVY_FAIL_PCM_DATA marvy_fail_pcm_start
#define MARVY_FAIL_PCM_BYTES ((size_t)(marvy_fail_pcm_end - marvy_fail_pcm_start))
#endif
#ifndef MARVY_COMBO3_PCM_DATA
extern const uint8_t marvy_combo3_pcm_start[] __asm__("_binary_marvy_combo3_pcm_start");
extern const uint8_t marvy_combo3_pcm_end[] __asm__("_binary_marvy_combo3_pcm_end");
#define MARVY_COMBO3_PCM_DATA marvy_combo3_pcm_start
#define MARVY_COMBO3_PCM_BYTES ((size_t)(marvy_combo3_pcm_end - marvy_combo3_pcm_start))
#endif
#ifndef MARVY_COMBO5_PCM_DATA
extern const uint8_t marvy_combo5_pcm_start[] __asm__("_binary_marvy_combo5_pcm_start");
extern const uint8_t marvy_combo5_pcm_end[] __asm__("_binary_marvy_combo5_pcm_end");
#define MARVY_COMBO5_PCM_DATA marvy_combo5_pcm_start
#define MARVY_COMBO5_PCM_BYTES ((size_t)(marvy_combo5_pcm_end - marvy_combo5_pcm_start))
#endif

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
static marvy_progress_t s_progress = {.unlocked_count = 1};
static marvy_progress_t s_saved_progress = {.unlocked_count = 1};
static bool s_progress_loaded;
static bool s_progress_save_attempted;
static TickType_t s_last_progress_save_attempt;
static bool s_music_requested;
static bool s_stream_open;
static marvy_music_t s_mixer;

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

void marvy_services_get_progress(marvy_progress_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&s_state_lock);
    *out = s_progress;
    portEXIT_CRITICAL(&s_state_lock);
}

void marvy_services_save_progress(const marvy_progress_t *progress)
{
    portENTER_CRITICAL(&s_state_lock);
    (void)marvy_progress_merge(&s_progress, progress);
    portEXIT_CRITICAL(&s_state_lock);
}

void marvy_services_play(marvy_sound_t sound)
{
    if (!s_sounds || (unsigned)sound >= MARVY_SOUND_COUNT) return;
    (void)xQueueOverwrite(s_sounds, &sound);
}

void marvy_services_set_music(bool enabled)
{
    portENTER_CRITICAL(&s_state_lock);
    s_music_requested = enabled;
    portEXIT_CRITICAL(&s_state_lock);
}

static bool music_requested(void)
{
    portENTER_CRITICAL(&s_state_lock);
    bool enabled = s_music_requested;
    portEXIT_CRITICAL(&s_state_lock);
    return enabled;
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

typedef enum {
    PROGRESS_READ_OK,
    PROGRESS_READ_MISSING,
    PROGRESS_READ_INVALID,
    PROGRESS_READ_ERROR,
} progress_read_t;

static bool progress_equal(const marvy_progress_t *a, const marvy_progress_t *b)
{
    if (a->unlocked_count != b->unlocked_count) return false;
    for (unsigned i = 0; i < MARVY_LEVEL_COUNT; ++i) {
        if (a->best_score[i] != b->best_score[i] ||
            a->best_stars[i] != b->best_stars[i]) return false;
    }
    return true;
}

static progress_read_t read_progress(nvs_handle_t handle, marvy_progress_t *out)
{
    size_t bytes = 0;
    esp_err_t error = nvs_get_blob(handle, "campaign1", NULL, &bytes);
    if (error == ESP_ERR_NVS_NOT_FOUND) return PROGRESS_READ_MISSING;
    if (error != ESP_OK) return PROGRESS_READ_ERROR;
    if (bytes != PROGRESS_BLOB_BYTES) return PROGRESS_READ_INVALID;
    uint8_t encoded[PROGRESS_BLOB_BYTES];
    error = nvs_get_blob(handle, "campaign1", encoded, &bytes);
    if (error != ESP_OK) return PROGRESS_READ_ERROR;
    if (bytes != PROGRESS_BLOB_BYTES || encoded[0] != PROGRESS_SCHEMA) {
        return PROGRESS_READ_INVALID;
    }
    marvy_progress_t decoded;
    marvy_progress_init(&decoded);
    decoded.unlocked_count = encoded[1];
    for (unsigned i = 0; i < MARVY_LEVEL_COUNT; ++i) {
        unsigned at = 2 + i * 3;
        decoded.best_score[i] = (uint16_t)encoded[at] | (uint16_t)encoded[at + 1] << 8;
        decoded.best_stars[i] = encoded[at + 2];
    }
    if (!marvy_progress_valid(&decoded)) return PROGRESS_READ_INVALID;
    *out = decoded;
    return PROGRESS_READ_OK;
}

static void encode_progress(const marvy_progress_t *progress,
                             uint8_t encoded[PROGRESS_BLOB_BYTES])
{
    encoded[0] = PROGRESS_SCHEMA;
    encoded[1] = progress->unlocked_count;
    for (unsigned i = 0; i < MARVY_LEVEL_COUNT; ++i) {
        unsigned at = 2 + i * 3;
        encoded[at] = (uint8_t)progress->best_score[i];
        encoded[at + 1] = (uint8_t)(progress->best_score[i] >> 8);
        encoded[at + 2] = progress->best_stars[i];
    }
}

static void load_progress(void)
{
    if (s_progress_loaded) return;
    s_progress_loaded = true;
    nvs_handle_t handle;
    esp_err_t error = nvs_open("marvy_ring", NVS_READONLY, &handle);
    if (error != ESP_OK) {
        if (error != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Campaign storage unavailable: %s", esp_err_to_name(error));
        }
        return;
    }
    marvy_progress_t stored;
    progress_read_t result = read_progress(handle, &stored);
    nvs_close(handle);
    if (result == PROGRESS_READ_OK) {
        marvy_services_save_progress(&stored);
        s_saved_progress = stored;
    } else if (result != PROGRESS_READ_MISSING) {
        ESP_LOGW(TAG, "Campaign data unreadable; keeping session records in RAM");
    }
}

static void persist_progress(TickType_t now)
{
    marvy_progress_t snapshot;
    marvy_services_get_progress(&snapshot);
    if (progress_equal(&snapshot, &s_saved_progress)) return;
    if (s_progress_save_attempted &&
        (TickType_t)(now - s_last_progress_save_attempt) < pdMS_TO_TICKS(SAVE_RETRY_MS)) return;
    s_progress_save_attempted = true;
    s_last_progress_save_attempt = now;

    nvs_handle_t handle;
    esp_err_t error = nvs_open("marvy_ring", NVS_READWRITE, &handle);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "Could not open campaign storage: %s", esp_err_to_name(error));
        return;
    }
    marvy_progress_t stored;
    progress_read_t result = read_progress(handle, &stored);
    if (result == PROGRESS_READ_OK) {
        /* The startup read can fail transiently. Merge the disk record into
         * both RAM and this write snapshot before replacing any persisted data. */
        marvy_services_save_progress(&stored);
        (void)marvy_progress_merge(&snapshot, &stored);
        if (progress_equal(&snapshot, &stored)) {
            s_saved_progress = stored;
            nvs_close(handle);
            return;
        }
    } else if (result != PROGRESS_READ_MISSING) {
        /* Never replace an unknown schema or unreadable record with defaults.
         * New session progress remains usable in RAM; no NVS erase is needed. */
        ESP_LOGW(TAG, "Campaign record preserved; save will retry");
        nvs_close(handle);
        return;
    }
    uint8_t encoded[PROGRESS_BLOB_BYTES];
    encode_progress(&snapshot, encoded);
    error = nvs_set_blob(handle, "campaign1", encoded, sizeof(encoded));
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    if (error == ESP_OK) {
        s_saved_progress = snapshot;
    } else {
        ESP_LOGW(TAG, "Campaign stays in RAM; save will retry: %s", esp_err_to_name(error));
    }
}

static void close_stream(void)
{
    esp_err_t error = bsp_audio_sleep();
    s_stream_open = false;
    if (error != ESP_OK) s_audio_ready = false;
}

static bool open_stream(void)
{
    if (s_stream_open) return true;
    esp_err_t error = bsp_audio_wake();
    if (error == ESP_OK) error = bsp_audio_set_format(MARVY_MUSIC_SAMPLE_RATE, 16, 1);
    if (error != ESP_OK) {
        close_stream();
        s_audio_ready = false;
        ESP_LOGW(TAG, "Audio unavailable; continuing silently");
        return false;
    }
    s_stream_open = true;
    return true;
}

static void services_worker(void *arg)
{
    (void)arg;
    if (!s_audio_attempted) {
        s_audio_attempted = true;
        s_audio_ready = bsp_audio_init() == ESP_OK;
        if (s_audio_ready) {
            bsp_audio_set_volume(65);
            s_audio_ready = bsp_audio_sleep() == ESP_OK;
        }
        if (!s_audio_ready) ESP_LOGW(TAG, "Audio init unavailable; score storage remains active");
    }

    for (;;) {
        marvy_music_set_enabled(&s_mixer, music_requested());
        marvy_sound_t sound;
        /* I2S provides pacing while active. If the codec fails, retain the
         * blocking queue wait so a requested BGM cannot busy-spin this task. */
        TickType_t wait = s_audio_ready && marvy_music_active(&s_mixer)
                              ? 0 : pdMS_TO_TICKS(20);
        BaseType_t has_sound = xQueueReceive(s_sounds, &sound, wait);
        if (has_sound == pdTRUE) marvy_music_cue(&s_mixer, sound);
        /* Flush pending scores between 16 ms PCM chunks, not once per song. */
        persist_best(xTaskGetTickCount());
        persist_progress(xTaskGetTickCount());
        if (!s_audio_ready) continue;
        if (marvy_music_active(&s_mixer)) {
            if (!open_stream()) continue;
            marvy_music_render(&s_mixer, s_pcm, PCM_SAMPLES);
            if (bsp_audio_write(s_pcm, sizeof(s_pcm)) != ESP_OK) {
                close_stream();
                s_audio_ready = false;
                ESP_LOGW(TAG, "Audio output failed; continuing silently");
            }
        } else if (s_stream_open) {
            close_stream();
        }
    }
}

bool marvy_services_start(void)
{
    if (s_task) return true;
    load_best();
    load_progress();
    s_sounds = xQueueCreate(1, sizeof(marvy_sound_t));
    if (!s_sounds) return false;
    marvy_music_init(&s_mixer, MARVY_BGM_PCM_DATA, MARVY_BGM_PCM_BYTES);
    marvy_music_set_hit_samples(&s_mixer, MARVY_HIT_PCM_DATA, MARVY_HIT_PCM_BYTES,
                                MARVY_BONUS_PCM_DATA, MARVY_BONUS_PCM_BYTES);
    marvy_music_set_sample(&s_mixer, MARVY_SOUND_LEVEL_CLEAR, MARVY_CLEAR_PCM_DATA,
                           MARVY_CLEAR_PCM_BYTES);
    marvy_music_set_sample(&s_mixer, MARVY_SOUND_LEVEL_FAIL, MARVY_FAIL_PCM_DATA,
                           MARVY_FAIL_PCM_BYTES);
    marvy_music_set_sample(&s_mixer, MARVY_SOUND_COMBO_3, MARVY_COMBO3_PCM_DATA,
                           MARVY_COMBO3_PCM_BYTES);
    marvy_music_set_sample(&s_mixer, MARVY_SOUND_COMBO_5, MARVY_COMBO5_PCM_DATA,
                           MARVY_COMBO5_PCM_BYTES);
    if (xTaskCreate(services_worker, "marvy_services", 3072, NULL, 4, &s_task) != pdPASS) {
        vQueueDelete(s_sounds);
        s_sounds = NULL;
        s_task = NULL;
        return false;
    }
    return true;
}
