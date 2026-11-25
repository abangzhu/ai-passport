#include "marvy_music.h"

#include <string.h>

typedef struct marvy_note {
    uint16_t hz;
    uint16_t ms;
} note_t;

static void begin_note(marvy_music_t *mixer)
{
    const note_t *note = &mixer->notes[mixer->note_index];
    mixer->note_samples = MARVY_MUSIC_SAMPLE_RATE * (uint32_t)note->ms / 1000U;
    mixer->phase_step = (uint32_t)(((uint64_t)note->hz << 32) / MARVY_MUSIC_SAMPLE_RATE);
    mixer->phase = 0;
    mixer->note_position = 0;
}

void marvy_music_init(marvy_music_t *mixer, const uint8_t *pcm, size_t bytes)
{
    memset(mixer, 0, sizeof(*mixer));
    mixer->bgm = pcm;
    mixer->bgm_bytes = pcm ? bytes - bytes % 2 : 0;
}

void marvy_music_set_hit_samples(marvy_music_t *mixer,
                                 const uint8_t *ordinary, size_t ordinary_bytes,
                                 const uint8_t *marvy, size_t marvy_bytes)
{
    marvy_music_set_sample(mixer, MARVY_SOUND_HIT, ordinary, ordinary_bytes);
    marvy_music_set_sample(mixer, MARVY_SOUND_MARVY_HIT, marvy, marvy_bytes);
    mixer->cue_pcm = NULL;
    mixer->cue_pcm_bytes = mixer->cue_pcm_offset = 0;
}

void marvy_music_set_sample(marvy_music_t *mixer, marvy_sound_t sound,
                            const uint8_t *pcm, size_t bytes)
{
    if (sound < MARVY_SOUND_INTRO || sound >= MARVY_SOUND_COUNT) return;
    mixer->cue_sources[sound] = pcm;
    mixer->cue_source_bytes[sound] = pcm ? bytes - bytes % 2 : 0;
}

static bool cue_active(const marvy_music_t *mixer)
{
    return mixer->cue_pcm_offset < mixer->cue_pcm_bytes
           || mixer->note_index < mixer->note_count;
}

void marvy_music_set_enabled(marvy_music_t *mixer, bool enabled)
{
    if (enabled && !mixer->enabled) mixer->bgm_offset = 0;
    mixer->enabled = enabled;
}

void marvy_music_cue(marvy_music_t *mixer, marvy_sound_t sound)
{
    if (sound < MARVY_SOUND_INTRO || sound >= MARVY_SOUND_COUNT) return;
    const uint8_t *pcm = mixer->cue_sources[sound];
    size_t bytes = mixer->cue_source_bytes[sound];
    if (pcm && bytes > 0) {
        mixer->cue_pcm = pcm;
        mixer->cue_pcm_bytes = bytes;
        mixer->cue_pcm_offset = 0;
        mixer->note_count = mixer->note_index = 0;
        return;
    }
    /* Original short feedback motifs remain for other cues and as fallback. */
    static const note_t intro[] = {{523, 65}, {659, 65}, {784, 70}, {1047, 115}};
    static const note_t thrown[] = {{440, 40}, {660, 45}};
    static const note_t hit[] = {{880, 65}, {1175, 95}};
    static const note_t appear[] = {{659, 60}, {880, 75}};
    static const note_t marvy_hit[] = {{784, 60}, {988, 60}, {1175, 60}, {1568, 120}};
    static const note_t miss[] = {{330, 65}, {262, 80}};
    static const note_t finish[] = {{659, 70}, {784, 70}, {1047, 120}};
    static const note_t level_clear[] = {{523, 80}, {659, 80}, {784, 80}, {1047, 220}};
    static const note_t level_fail[] = {{392, 80}, {330, 80}, {262, 180}};
    static const note_t combo_3[] = {{988, 55}, {1319, 95}};
    static const note_t combo_5[] = {{988, 45}, {1319, 45}, {1568, 120}};
    const note_t *notes;
    size_t count;
#define SELECT_NOTES(array) do { notes = (array); count = sizeof(array) / sizeof((array)[0]); } while (0)
    switch (sound) {
    case MARVY_SOUND_INTRO: SELECT_NOTES(intro); break;
    case MARVY_SOUND_THROW: SELECT_NOTES(thrown); break;
    case MARVY_SOUND_HIT: SELECT_NOTES(hit); break;
    case MARVY_SOUND_MARVY_APPEAR: SELECT_NOTES(appear); break;
    case MARVY_SOUND_MARVY_HIT: SELECT_NOTES(marvy_hit); break;
    case MARVY_SOUND_MISS: SELECT_NOTES(miss); break;
    case MARVY_SOUND_FINISH: SELECT_NOTES(finish); break;
    case MARVY_SOUND_LEVEL_CLEAR: SELECT_NOTES(level_clear); break;
    case MARVY_SOUND_LEVEL_FAIL: SELECT_NOTES(level_fail); break;
    case MARVY_SOUND_COMBO_3: SELECT_NOTES(combo_3); break;
    case MARVY_SOUND_COMBO_5: SELECT_NOTES(combo_5); break;
    default: return;
    }
#undef SELECT_NOTES
    mixer->cue_pcm = NULL;
    mixer->cue_pcm_bytes = mixer->cue_pcm_offset = 0;
    mixer->notes = notes;
    mixer->note_count = count;
    mixer->note_index = 0;
    begin_note(mixer);
}

bool marvy_music_active(const marvy_music_t *mixer)
{
    return (mixer->bgm_bytes > 0 && (mixer->enabled || mixer->bgm_gain > 0))
           || cue_active(mixer);
}

static int32_t read_pcm_sample(const uint8_t *pcm, size_t at)
{
    uint16_t bits = (uint16_t)pcm[at] | (uint16_t)pcm[at + 1] << 8;
    return bits < 32768U ? (int32_t)bits : (int32_t)bits - 65536;
}

static int32_t cue_sample(marvy_music_t *mixer)
{
    if (mixer->cue_pcm_offset < mixer->cue_pcm_bytes) {
        size_t at = mixer->cue_pcm_offset;
        int32_t value = read_pcm_sample(mixer->cue_pcm, at);
        size_t position = at / 2;
        size_t remaining = (mixer->cue_pcm_bytes - at) / 2 - 1;
        if (position < 32) value = value * (int32_t)position / 32;
        if (remaining < 64) value = value * (int32_t)remaining / 64;
        mixer->cue_pcm_offset += 2;
        return value / 3;
    }
    if (mixer->note_index >= mixer->note_count) return 0;
    uint32_t remaining = mixer->note_samples - mixer->note_position - 1;
    int32_t gain = 4800;
    if (mixer->note_position < 64U) gain = gain * (int32_t)mixer->note_position / 64;
    if (remaining < 128U) gain = gain * (int32_t)remaining / 128;
    mixer->phase += mixer->phase_step;
    uint32_t ramp = mixer->phase >> 16;
    int32_t triangle = (int32_t)(ramp < 32768U ? ramp : 65535U - ramp) * 2 - 32767;
    int32_t value = triangle * gain / 32768;
    mixer->note_position++;
    if (mixer->note_position == mixer->note_samples) {
        mixer->note_index++;
        if (mixer->note_index < mixer->note_count) begin_note(mixer);
    }
    return value;
}

static int32_t music_sample(marvy_music_t *mixer)
{
    if (mixer->bgm_bytes == 0) return 0;
    bool cue = cue_active(mixer);
    uint16_t target = !mixer->enabled ? 0 : cue ? 64 : MARVY_MUSIC_FULL_GAIN;
    /* A 16 ms gain ramp avoids an abrupt BGM cut on pause or cue ducking. */
    if (mixer->bgm_gain < target) mixer->bgm_gain++;
    else if (mixer->bgm_gain > target) mixer->bgm_gain--;
    if (mixer->bgm_gain == 0) return 0;

    size_t at = mixer->bgm_offset;
    int32_t sample = read_pcm_sample(mixer->bgm, at);
    size_t position = at / 2;
    size_t remaining = (mixer->bgm_bytes - at) / 2 - 1;
    /* Short seam envelopes tame decoded MP3 edge padding without adding a
     * second PCM buffer or moving loop points on every effect. */
    if (position < 64) sample = sample * (int32_t)position / 64;
    if (remaining < 64) sample = sample * (int32_t)remaining / 64;
    mixer->bgm_offset = at + 2 == mixer->bgm_bytes ? 0 : at + 2;
    return sample / 4 * (int32_t)mixer->bgm_gain / MARVY_MUSIC_FULL_GAIN;
}

void marvy_music_render(marvy_music_t *mixer, int16_t *out, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        int32_t sample = music_sample(mixer) + cue_sample(mixer);
        if (sample > MARVY_MIX_LIMIT) sample = MARVY_MIX_LIMIT;
        if (sample < -MARVY_MIX_LIMIT) sample = -MARVY_MIX_LIMIT;
        out[i] = (int16_t)sample;
    }
}
