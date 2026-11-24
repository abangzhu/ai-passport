#include <assert.h>
#include <stdint.h>
#include <stdio.h>
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
    for (int cue = MARVY_SOUND_INTRO; cue <= MARVY_SOUND_FINISH; cue++) {
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
    puts("Marvy louder BGM, distinct sampled hits, ducking and bounded PCM tests: PASS");
    return 0;
}
