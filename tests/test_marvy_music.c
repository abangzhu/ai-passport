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
    puts("Marvy BGM loop, cue mixing, ducking and bounded PCM tests: PASS");
    return 0;
}
