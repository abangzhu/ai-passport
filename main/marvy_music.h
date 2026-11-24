#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MARVY_SOUND_INTRO = 0,
    MARVY_SOUND_THROW,
    MARVY_SOUND_HIT,
    MARVY_SOUND_MARVY_APPEAR,
    MARVY_SOUND_MARVY_HIT,
    MARVY_SOUND_MISS,
    MARVY_SOUND_FINISH,
} marvy_sound_t;

enum {
    MARVY_MUSIC_SAMPLE_RATE = 16000,
    MARVY_MUSIC_FULL_GAIN = 256,
    MARVY_MIX_LIMIT = 4096,
};

struct marvy_note;
typedef struct {
    const uint8_t *bgm;
    size_t bgm_bytes;
    size_t bgm_offset;
    bool enabled;
    uint16_t bgm_gain;
    const struct marvy_note *notes;
    size_t note_count;
    size_t note_index;
    uint32_t note_position;
    uint32_t note_samples;
    uint32_t phase;
    uint32_t phase_step;
} marvy_music_t;

/* Pure, allocation-free mixer. Source is signed little-endian 16-bit mono PCM
 * at 16 kHz and must outlive the mixer. Odd trailing bytes are ignored. */
void marvy_music_init(marvy_music_t *mixer, const uint8_t *pcm, size_t bytes);
void marvy_music_set_enabled(marvy_music_t *mixer, bool enabled);
void marvy_music_cue(marvy_music_t *mixer, marvy_sound_t sound);
bool marvy_music_active(const marvy_music_t *mixer);
void marvy_music_render(marvy_music_t *mixer, int16_t *out, size_t count);
