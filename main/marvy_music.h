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
    MARVY_MIX_LIMIT = 16384,
};

struct marvy_note;
typedef struct {
    const uint8_t *bgm;
    size_t bgm_bytes;
    size_t bgm_offset;
    bool enabled;
    uint16_t bgm_gain;
    const uint8_t *ordinary_hit_pcm;
    size_t ordinary_hit_bytes;
    const uint8_t *marvy_hit_pcm;
    size_t marvy_hit_bytes;
    const uint8_t *cue_pcm;
    size_t cue_pcm_bytes;
    size_t cue_pcm_offset;
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
/* Optional, lifetime-stable one-shots replace HIT/MARVY_HIT synthesis. Missing
 * sources retain the synthesized fallback; odd trailing bytes are ignored. */
void marvy_music_set_hit_samples(marvy_music_t *mixer,
                                 const uint8_t *ordinary, size_t ordinary_bytes,
                                 const uint8_t *marvy, size_t marvy_bytes);
void marvy_music_set_enabled(marvy_music_t *mixer, bool enabled);
void marvy_music_cue(marvy_music_t *mixer, marvy_sound_t sound);
bool marvy_music_active(const marvy_music_t *mixer);
void marvy_music_render(marvy_music_t *mixer, int16_t *out, size_t count);
