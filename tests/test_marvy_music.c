#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "marvy_music.h"

static const uint8_t waveform[] = {
    0, 0, 0xff, 0x3f, 0xff, 0x7f, 0xff, 0x3f,
    0, 0, 0, 0xc0, 0, 0x80, 0, 0xc0,
};

static unsigned nonzero(const int16_t *samples, size_t count)
{
    unsigned result = 0;
    for (size_t i = 0; i < count; i++) {
        assert(samples[i] >= -MARVY_MIX_LIMIT && samples[i] <= MARVY_MIX_LIMIT);
        result += samples[i] != 0;
    }
    return result;
}

static uint32_t campaign_asset(marvy_sound_t cue, const char *path,
                                size_t minimum_bytes, size_t maximum_bytes)
{
    FILE *source = fopen(path, "rb");
    assert(source != NULL);
    assert(fseek(source, 0, SEEK_END) == 0);
    long length = ftell(source);
    assert(length >= 0 && (size_t)length >= minimum_bytes &&
           (size_t)length <= maximum_bytes && length % 2 == 0);
    rewind(source);
    uint8_t *pcm = malloc((size_t)length);
    assert(pcm != NULL && fread(pcm, 1, (size_t)length, source) == (size_t)length);
    assert(fclose(source) == 0);
    unsigned peak = 0;
    uint32_t fingerprint = UINT32_C(2166136261);
    for (size_t at = 0; at < (size_t)length; at += 2) {
        uint16_t bits = (uint16_t)pcm[at] | (uint16_t)pcm[at + 1] << 8;
        int32_t value = bits < 32768 ? bits : (int32_t)bits - 65536;
        unsigned magnitude = (unsigned)(value < 0 ? -value : value);
        if (magnitude > peak) peak = magnitude;
        fingerprint = (fingerprint ^ pcm[at]) * UINT32_C(16777619);
        fingerprint = (fingerprint ^ pcm[at + 1]) * UINT32_C(16777619);
    }
    assert(peak == 27852); /* Reproducible normalization, with output headroom. */
    marvy_music_t mixer;
    marvy_music_init(&mixer, NULL, 0);
    marvy_music_set_sample(&mixer, cue, pcm, (size_t)length);
    marvy_music_cue(&mixer, cue);
    int16_t samples[512];
    unsigned audible = 0;
    size_t blocks = ((size_t)length / 2 + 511) / 512;
    for (size_t i = 0; i < blocks; ++i) {
        marvy_music_render(&mixer, samples, 512);
        audible += nonzero(samples, 512);
    }
    assert(audible > (size_t)length / 40 && !marvy_music_active(&mixer));
    marvy_music_render(&mixer, samples, 512);
    assert(nonzero(samples, 512) == 0);
    free(pcm);
    return fingerprint;
}

int main(void)
{
    marvy_music_t mixer;
    int16_t samples[512];
    marvy_music_init(&mixer, waveform, sizeof(waveform));
    assert(!marvy_music_active(&mixer));
    memset(samples, 1, sizeof(samples));
    marvy_music_render(&mixer, samples, 512);
    assert(nonzero(samples, 512) == 0);

    marvy_music_set_enabled(&mixer, true);
    assert(marvy_music_active(&mixer));
    marvy_music_render(&mixer, samples, 512);
    assert(nonzero(samples, 512) > 0);
    /* Actual licensed samples are finite, audible, normalized, distinct and
     * remain inside the same limiter while playing to completion. */
    uint32_t fingerprints[] = {
        campaign_asset(MARVY_SOUND_LEVEL_CLEAR, "assets/music/marvy_clear.pcm", 38000, 41000),
        campaign_asset(MARVY_SOUND_LEVEL_FAIL, "assets/music/marvy_fail.pcm", 6400, 12800),
        campaign_asset(MARVY_SOUND_COMBO_3, "assets/music/marvy_combo3.pcm", 9600, 16000),
        campaign_asset(MARVY_SOUND_COMBO_5, "assets/music/marvy_combo5.pcm", 12800, 16000),
    };
    for (unsigned i = 0; i < 4; ++i) {
        for (unsigned j = 0; j < i; ++j) assert(fingerprints[i] != fingerprints[j]);
    }
    for (unsigned i = 0; i < 300; i++) marvy_music_render(&mixer, samples, 512);
    assert(marvy_music_active(&mixer));
    assert(mixer.bgm_offset < sizeof(waveform));

    marvy_music_set_enabled(&mixer, false);
    marvy_music_render(&mixer, samples, 512);
    assert(!marvy_music_active(&mixer));
    marvy_music_render(&mixer, samples, 512);
    assert(nonzero(samples, 512) == 0);
    marvy_music_set_enabled(&mixer, true);
    assert(mixer.bgm_offset == 0);
    marvy_music_render(&mixer, samples, 512);
    size_t offset = mixer.bgm_offset;
    marvy_music_set_enabled(&mixer, true);
    assert(mixer.bgm_offset == offset); /* Repeated enable must not restart. */

    uint16_t duck_before = mixer.bgm_gain;
    marvy_music_cue(&mixer, MARVY_SOUND_MARVY_HIT);
    assert(marvy_music_active(&mixer));
    marvy_music_render(&mixer, samples, 512);
    assert(nonzero(samples, 512) > 0 && mixer.bgm_gain < duck_before);
    for (unsigned i = 0; i < 50; i++) marvy_music_render(&mixer, samples, 512);
    assert(mixer.bgm_gain == MARVY_MUSIC_FULL_GAIN);
    assert(marvy_music_active(&mixer)); /* BGM survives the cue. */

    marvy_music_set_enabled(&mixer, false);
    marvy_music_render(&mixer, samples, 512);
    for (int cue = MARVY_SOUND_INTRO; cue < MARVY_SOUND_COUNT; cue++) {
        marvy_music_cue(&mixer, (marvy_sound_t)cue);
        marvy_music_render(&mixer, samples, 512);
        assert(nonzero(samples, 512) > 0);
        for (unsigned i = 0; i < 50; i++) marvy_music_render(&mixer, samples, 512);
        assert(!marvy_music_active(&mixer));
        assert(nonzero(samples, 512) == 0);
    }
    marvy_music_cue(&mixer, (marvy_sound_t)-1);
    marvy_music_cue(&mixer, (marvy_sound_t)99);
    assert(!marvy_music_active(&mixer));

    marvy_music_init(&mixer, NULL, 0);
    marvy_music_set_enabled(&mixer, true);
    assert(!marvy_music_active(&mixer));
    marvy_music_render(&mixer, samples, 512);
    assert(nonzero(samples, 512) == 0);
    marvy_music_cue(&mixer, MARVY_SOUND_HIT);
    assert(marvy_music_active(&mixer));
    marvy_music_render(&mixer, samples, 512);
    assert(nonzero(samples, 512) > 0);

    /* Ignore a trailing odd PCM byte and never read beyond an empty pair. */
    marvy_music_init(&mixer, waveform, 1);
    marvy_music_set_enabled(&mixer, true);
    assert(!marvy_music_active(&mixer));
    marvy_music_init(&mixer, waveform, sizeof(waveform)-1);
    marvy_music_set_enabled(&mixer, true);
    marvy_music_render(&mixer, samples, 512);
    assert(mixer.bgm_offset < sizeof(waveform)-1 && mixer.bgm_offset % 2 == 0);
    /* Licensed one-shots must be distinct, and replace the synthesized hit
     * fallback without restarting or allocating the background loop. */
    uint8_t normal[1024], bonus[1024];
    for (size_t i = 0; i < sizeof(normal); i += 2) {
        normal[i] = bonus[i] = 0;
        normal[i + 1] = 0x40;
        bonus[i + 1] = 0xc0;
    }
    marvy_music_init(&mixer, NULL, 0);
    marvy_music_set_hit_samples(&mixer, normal, sizeof(normal), bonus, sizeof(bonus));
    marvy_music_cue(&mixer, MARVY_SOUND_HIT);
    marvy_music_render(&mixer, samples, 256);
    assert(samples[128] > 4000);
    marvy_music_render(&mixer, samples, 256);
    assert(!marvy_music_active(&mixer));
    marvy_music_cue(&mixer, MARVY_SOUND_MARVY_HIT);
    marvy_music_render(&mixer, samples, 256);
    assert(samples[128] < -4000);
    marvy_music_cue(&mixer, MARVY_SOUND_THROW);
    assert(mixer.cue_pcm_bytes == 0); /* New synth cue replaces the one-shot. */
    marvy_music_render(&mixer, samples, 512);
    assert(nonzero(samples, 512) > 0);

    /* Each campaign cue can bind its own bounded one-shot. Registration ignores
     * odd trailing bytes; replacing a cue uses the newly requested source. */
    for (int cue = MARVY_SOUND_LEVEL_CLEAR; cue < MARVY_SOUND_COUNT; ++cue) {
        marvy_music_init(&mixer, NULL, 0);
        const uint8_t *source = cue % 2 ? normal : bonus;
        marvy_music_set_sample(&mixer, (marvy_sound_t)cue, source, sizeof(normal) - 1);
        marvy_music_cue(&mixer, (marvy_sound_t)cue);
        assert(mixer.cue_pcm == source && mixer.cue_pcm_bytes == sizeof(normal) - 2);
        marvy_music_render(&mixer, samples, 256);
        assert(nonzero(samples, 256) > 0);
        assert((samples[128] > 0) == (source == normal));
        marvy_music_render(&mixer, samples, 256);
        assert(!marvy_music_active(&mixer));
        marvy_music_set_sample(&mixer, (marvy_sound_t)cue, NULL, 999);
        marvy_music_cue(&mixer, (marvy_sound_t)cue);
        assert(mixer.cue_pcm_bytes == 0);
        marvy_music_render(&mixer, samples, 512);
        assert(nonzero(samples, 512) > 0); /* Synthesized fallback remains usable. */
    }
    marvy_music_init(&mixer, NULL, 0);
    marvy_music_set_sample(&mixer, MARVY_SOUND_LEVEL_CLEAR, normal, sizeof(normal));
    marvy_music_set_sample(&mixer, MARVY_SOUND_LEVEL_FAIL, bonus, sizeof(bonus));
    marvy_music_cue(&mixer, MARVY_SOUND_LEVEL_CLEAR);
    marvy_music_render(&mixer, samples, 128);
    marvy_music_cue(&mixer, MARVY_SOUND_LEVEL_FAIL);
    assert(mixer.cue_pcm == bonus && mixer.cue_pcm_offset == 0);
    marvy_music_render(&mixer, samples, 256);
    assert(samples[128] < 0); /* Latest requested cue wins. */
    marvy_music_t before = mixer;
    marvy_music_set_sample(&mixer, (marvy_sound_t)-1, normal, sizeof(normal));
    marvy_music_set_sample(&mixer, MARVY_SOUND_COUNT, normal, sizeof(normal));
    marvy_music_cue(&mixer, MARVY_SOUND_COUNT);
    assert(memcmp(&mixer, &before, sizeof(mixer)) == 0);

    /* BGM's digital gain is visibly higher than the old /16 implementation. */
    uint8_t sustained[2048];
    for (size_t i = 0; i < sizeof(sustained); i += 2) {
        sustained[i] = 0;
        sustained[i + 1] = 0x40;
    }
    marvy_music_init(&mixer, sustained, sizeof(sustained));
    marvy_music_set_enabled(&mixer, true);
    marvy_music_render(&mixer, samples, 512);
    assert(samples[400] == 4096);

    /* Missing/odd hit sources keep the known-good synthesized fallback. */
    marvy_music_set_hit_samples(&mixer, NULL, 999, normal, 1);
    marvy_music_cue(&mixer, MARVY_SOUND_MARVY_HIT);
    marvy_music_render(&mixer, samples, 512);
    assert(nonzero(samples, 512) > 0);
    puts("Marvy sampled hit/campaign cues, fallback, ducking and bounded PCM tests: PASS");
    return 0;
}
