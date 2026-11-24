#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "marvy_services_stubs/marvy_test_stubs.h"
static const uint8_t test_music_pcm[] = {
    0, 0, 0xff, 0x3f, 0xff, 0x7f, 0xff, 0x3f,
    0, 0, 0, 0xc0, 0, 0x80, 0, 0xc0,
};
#define MARVY_BGM_PCM_DATA test_music_pcm
#define MARVY_BGM_PCM_BYTES sizeof(test_music_pcm)
#define MARVY_HIT_PCM_DATA test_music_pcm
#define MARVY_HIT_PCM_BYTES sizeof(test_music_pcm)
#define MARVY_BONUS_PCM_DATA test_music_pcm
#define MARVY_BONUS_PCM_BYTES sizeof(test_music_pcm)
#include "../main/marvy_music.c"
#include "../main/marvy_services.c"

struct test_queue {
    bool has_item;
    marvy_sound_t item;
};

static struct test_queue test_queue;
static bool test_queue_fail, test_task_fail, test_store_exists;
static bool test_open_fail, test_get_fail, test_set_fail, test_commit_fail;
static bool test_audio_init_fail, test_audio_format_fail, test_audio_wake_fail;
static bool test_audio_sleep_fail;
static unsigned test_task_creates, test_queue_deletes, test_overwrites;
static unsigned test_sets, test_commits, test_audio_writes, test_write_fail_at;
static unsigned test_audio_initializes, test_audio_sleeps, test_nonzero_samples;
static unsigned test_audio_formats, test_audio_wakes;
static uint16_t test_disk_score, test_pending_score;
static TickType_t test_ticks;
static void (*test_task)(void *);
static jmp_buf test_worker_exit;
static unsigned test_iterations;

QueueHandle_t xQueueCreate(unsigned length, unsigned item_size)
{
    assert(length == 1 && item_size == sizeof(marvy_sound_t));
    return test_queue_fail ? NULL : &test_queue;
}

void vQueueDelete(QueueHandle_t queue)
{
    assert(queue == &test_queue);
    test_queue_deletes++;
}

BaseType_t xQueueOverwrite(QueueHandle_t queue, const void *item)
{
    assert(queue == &test_queue);
    memcpy(&queue->item, item, sizeof(queue->item));
    queue->has_item = true;
    test_overwrites++;
    return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t ticks)
{
    if (test_iterations-- == 0) longjmp(test_worker_exit, 1);
    assert(queue == &test_queue);
    if (!queue->has_item) {
        test_ticks += ticks;
        return pdFALSE;
    }
    memcpy(item, &queue->item, sizeof(queue->item));
    queue->has_item = false;
    return pdTRUE;
}

BaseType_t xTaskCreate(void (*task)(void *), const char *name, uint32_t stack,
                       void *arg, unsigned priority, TaskHandle_t *out)
{
    (void)name;
    (void)arg;
    assert(stack >= 2048 && priority > 0);
    test_task_creates++;
    if (test_task_fail) return pdFALSE;
    test_task = task;
    *out = (TaskHandle_t)&test_task;
    return pdPASS;
}

TickType_t xTaskGetTickCount(void) { return test_ticks; }

esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *out)
{
    assert(strcmp(name, "marvy_ring") == 0);
    if (test_open_fail) return ESP_FAIL;
    if (mode == NVS_READONLY && !test_store_exists) return ESP_ERR_NVS_NOT_FOUND;
    *out = 1;
    return ESP_OK;
}

esp_err_t nvs_get_u16(nvs_handle_t handle, const char *key, uint16_t *out)
{
    assert(handle == 1 && strcmp(key, "best") == 0);
    if (test_get_fail) return ESP_FAIL;
    if (!test_store_exists) return ESP_ERR_NVS_NOT_FOUND;
    *out = test_disk_score;
    return ESP_OK;
}

esp_err_t nvs_set_u16(nvs_handle_t handle, const char *key, uint16_t value)
{
    assert(handle == 1 && strcmp(key, "best") == 0);
    test_sets++;
    if (test_set_fail) return ESP_FAIL;
    test_pending_score = value;
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    assert(handle == 1);
    test_commits++;
    if (test_commit_fail) return ESP_FAIL;
    test_disk_score = test_pending_score;
    test_store_exists = true;
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle) { assert(handle == 1); }

esp_err_t bsp_audio_init(void)
{
    test_audio_initializes++;
    return test_audio_init_fail ? ESP_FAIL : ESP_OK;
}

esp_err_t bsp_audio_set_format(uint32_t hz, uint8_t bits, uint8_t channels)
{
    assert(hz == 16000 && bits == 16 && channels == 1);
    test_audio_formats++;
    return test_audio_format_fail ? ESP_FAIL : ESP_OK;
}

void bsp_audio_set_volume(uint8_t volume) { assert(volume == 65); }
esp_err_t bsp_audio_wake(void) { test_audio_wakes++; return test_audio_wake_fail ? ESP_FAIL : ESP_OK; }
esp_err_t bsp_audio_sleep(void)
{
    test_audio_sleeps++;
    return test_audio_sleep_fail ? ESP_FAIL : ESP_OK;
}

esp_err_t bsp_audio_write(const void *pcm, size_t bytes)
{
    test_audio_writes++;
    assert(bytes > 0 && bytes <= 512 && bytes % sizeof(int16_t) == 0);
    const int16_t *samples = pcm;
    for (size_t i = 0; i < bytes / sizeof(*samples); i++) {
        assert(samples[i] >= -MARVY_MIX_LIMIT && samples[i] <= MARVY_MIX_LIMIT);
        if (samples[i]) test_nonzero_samples++;
    }
    test_ticks += (TickType_t)(bytes / sizeof(int16_t) / 16);
    return test_audio_writes == test_write_fail_at ? ESP_FAIL : ESP_OK;
}

static void run_worker_frames(unsigned frames)
{
    test_iterations = frames;
    if (setjmp(test_worker_exit) == 0) test_task(NULL);
}

static void run_worker_once(void) { run_worker_frames(1); }

static void reset_test(void)
{
    memset(&test_queue, 0, sizeof(test_queue));
    test_queue_fail = test_task_fail = test_store_exists = false;
    test_open_fail = test_get_fail = test_set_fail = test_commit_fail = false;
    test_audio_init_fail = test_audio_format_fail = test_audio_wake_fail = false;
    test_audio_sleep_fail = false;
    test_task_creates = test_queue_deletes = test_overwrites = 0;
    test_sets = test_commits = test_audio_writes = test_write_fail_at = 0;
    test_audio_initializes = test_audio_sleeps = test_nonzero_samples = 0;
    test_audio_formats = test_audio_wakes = 0;
    test_disk_score = test_pending_score = 0;
    test_ticks = 0;
    s_task = NULL;
    s_sounds = NULL;
    s_best = s_saved_best = 0;
    s_loaded = s_audio_attempted = s_audio_ready = s_save_attempted = false;
    s_last_save_attempt = 0;
    s_music_requested = false;
    s_stream_open = false;
    memset(&s_mixer, 0, sizeof(s_mixer));
}

int main(void)
{
    reset_test();
    marvy_services_play(MARVY_SOUND_HIT);
    assert(test_overwrites == 0);
    test_queue_fail = true;
    assert(!marvy_services_start());
    assert(test_task_creates == 0);
    test_queue_fail = false;
    test_task_fail = true;
    assert(!marvy_services_start());
    assert(test_queue_deletes == 1 && !s_sounds);
    marvy_services_save_best(6);
    assert(marvy_services_best_score() == 6);
    test_task_fail = false;
    assert(marvy_services_start());
    assert(marvy_services_start());
    assert(test_task_creates == 2);

    reset_test();
    test_store_exists = true;
    test_disk_score = 7;
    assert(marvy_services_start());
    assert(marvy_services_best_score() == 7);
    marvy_services_save_best(4);
    run_worker_once();
    assert(test_sets == 0 && marvy_services_best_score() == 7);
    marvy_services_save_best(12);
    marvy_services_save_best(9);
    assert(marvy_services_best_score() == 12 && test_sets == 0);
    run_worker_once();
    assert(test_disk_score == 12 && test_commits == 1);
    run_worker_once();
    assert(test_commits == 1);

    reset_test();
    test_audio_init_fail = true;
    assert(marvy_services_start());
    marvy_services_save_best(5);
    marvy_services_play(MARVY_SOUND_MARVY_HIT);
    run_worker_once();
    assert(test_disk_score == 5 && test_audio_writes == 0);

    for (unsigned failure = 0; failure < 3; failure++) {
        reset_test();
        assert(marvy_services_start());
        test_open_fail = failure == 0;
        test_set_fail = failure == 1;
        test_commit_fail = failure == 2;
        marvy_services_save_best(18);
        run_worker_once();
        assert(test_disk_score == 0 && marvy_services_best_score() == 18);
        unsigned attempts = test_sets;
        run_worker_once();
        assert(test_sets == attempts);
        test_open_fail = test_set_fail = test_commit_fail = false;
        test_ticks += 1000;
        run_worker_once();
        assert(test_disk_score == 18);
    }

    reset_test();
    test_store_exists = true;
    test_disk_score = 23;
    test_get_fail = true;
    assert(marvy_services_start());
    assert(marvy_services_best_score() == 0);
    marvy_services_save_best(8);
    run_worker_once();
    assert(test_sets == 0 && test_disk_score == 23);
    test_get_fail = false;
    test_ticks += 1000;
    run_worker_once();
    assert(marvy_services_best_score() == 23 && test_sets == 0);

    reset_test();
    assert(marvy_services_start());
    marvy_services_save_best(3);
    run_worker_once();
    marvy_services_save_best(7);
    marvy_services_save_best(5);
    run_worker_once();
    assert(test_disk_score == 3 && marvy_services_best_score() == 7);
    test_ticks += 1000;
    run_worker_once();
    assert(test_disk_score == 7);

    reset_test();
    assert(marvy_services_start());
    marvy_services_play((marvy_sound_t)-1);
    marvy_services_play((marvy_sound_t)99);
    assert(test_overwrites == 0);
    marvy_services_play(MARVY_SOUND_THROW);
    marvy_services_play(MARVY_SOUND_MARVY_HIT);
    assert(test_queue.item == MARVY_SOUND_MARVY_HIT);
    for (int sound = MARVY_SOUND_INTRO; sound <= MARVY_SOUND_FINISH; sound++) {
        marvy_services_play((marvy_sound_t)sound);
        run_worker_frames(40);
    }
    assert(test_audio_initializes == 1 && test_audio_writes > 0);
    assert(test_nonzero_samples > 0 && test_audio_sleeps >= 7);

    for (unsigned failure = 0; failure < 4; failure++) {
        reset_test();
        assert(marvy_services_start());
        test_audio_format_fail = failure == 0;
        test_audio_wake_fail = failure == 1;
        test_audio_sleep_fail = failure == 2;
        test_write_fail_at = failure == 3 ? 1 : 0;
        marvy_services_play(MARVY_SOUND_HIT);
        run_worker_once();
        unsigned writes = test_audio_writes;
        marvy_services_play(MARVY_SOUND_HIT);
        marvy_services_save_best(16);
        test_ticks += 1000;
        run_worker_once();
        assert(test_disk_score == 16 && test_audio_writes == writes);
    }

    reset_test();
    assert(marvy_services_start());
    test_commit_fail = true;
    test_ticks = UINT32_MAX - 200;
    marvy_services_save_best(4);
    run_worker_once();
    test_commit_fail = false;
    test_ticks += 1001;
    run_worker_once();
    assert(test_disk_score == 4);
    reset_test();
    assert(marvy_services_start());
    marvy_services_set_music(true);
    assert(test_audio_writes == 0); /* API cannot block on codec or storage. */
    run_worker_frames(40);
    assert(test_audio_writes == 40 && test_nonzero_samples > 0);
    assert(test_audio_wakes == 1 && test_audio_formats == 1);
    unsigned wakes = test_audio_wakes, sleeps = test_audio_sleeps;
    marvy_services_play(MARVY_SOUND_MARVY_HIT);
    marvy_services_save_best(31);
    run_worker_frames(40);
    assert(test_disk_score == 31 && test_audio_writes == 80);
    assert(test_audio_wakes == wakes && test_audio_sleeps == sleeps);
    marvy_services_set_music(false);
    run_worker_frames(2); /* Finish short fade, then suspend once. */
    unsigned writes = test_audio_writes;
    assert(test_audio_sleeps == sleeps + 1);
    run_worker_frames(20);
    assert(test_audio_writes == writes);
    marvy_services_set_music(true);
    run_worker_once();
    assert(test_audio_writes == writes + 1 && test_audio_wakes == wakes + 1);

    reset_test();
    assert(marvy_services_start());
    test_write_fail_at = 2;
    marvy_services_set_music(true);
    run_worker_frames(3);
    assert(test_audio_writes == 2 && !s_audio_ready);
    marvy_services_save_best(42);
    run_worker_frames(3);
    assert(test_disk_score == 42 && test_audio_writes == 2);
    TickType_t before = test_ticks;
    run_worker_frames(3);
    assert(test_ticks - before >= 60); /* Broken music must not busy-spin. */
    puts("Marvy services persistence, streaming BGM, cues and failure tests: PASS");
    return 0;
}
