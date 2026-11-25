<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, sound effects, and battery profiles, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

### Marvy UI fonts

| File | Purpose |
| --- | --- |
| [`fonts/NotoSansSC-Regular.otf`](fonts/NotoSansSC-Regular.otf) | Unmodified Noto Sans SC Regular source for reproducible subset generation; not compiled into the firmware. |
| [`fonts/OFL.txt`](fonts/OFL.txt) | Source font's SIL Open Font License 1.1 and copyright notice. Retain it with redistributed font assets. |
| [`fonts/marvy_characters.txt`](fonts/marvy_characters.txt) | Deduplicated, sorted inventory of 117 fixed Chinese UI characters, including level, pause-menu, and result text. All four fonts also include 95 printable ASCII characters U+0020–U+007E, for 212 glyphs each. |
| [`fonts/marvy_font_16.c`](fonts/marvy_font_16.c) | 16 px, 4 bpp, uncompressed smooth LVGL bitmap font for welcome-screen text; exported symbol `marvy_font_16`. |
| [`fonts/marvy_font_20.c`](fonts/marvy_font_20.c) | 20 px, 4 bpp, uncompressed smooth LVGL bitmap font for welcome-screen headings; exported symbol `marvy_font_20`. |
| [`fonts/marvy_pixel_16.c`](fonts/marvy_pixel_16.c) | 16 px, 1 bpp, uncompressed hard-edge LVGL bitmap font for level briefs, gameplay HUD, menu hints, and controls; exported symbol `marvy_pixel_16`. |
| [`fonts/marvy_pixel_20.c`](fonts/marvy_pixel_20.c) | 20 px, 1 bpp, uncompressed hard-edge LVGL bitmap font for level, pause-menu, and result headings and celebration text; exported symbol `marvy_pixel_20`. |

Source: [Noto CJK official repository](https://github.com/notofonts/noto-cjk),
[`Sans/SubsetOTF/SC/NotoSansSC-Regular.otf`](https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf),
with its [license](https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/LICENSE).
The vendored source bytes are pinned by SHA-256
`faa6c9df652116dde789d351359f3d7e5d2285a2b2a1f04a2d7244df706d5ea9`.

Regenerate from the repository root with
`bash tools/generate_marvy_fonts.sh`. The script validates the source checksum,
uses `lv_font_conv` **1.5.3**, and generates sizes 16 and 20 in two styles:
`--bpp 4` for smooth welcome fonts and `--bpp 1` for hard-edge game fonts. Both
use `--format lvgl --no-compress --lv-include lvgl.h`. Generated C files go
into the application's CMake source list; UI labels explicitly select their
font. Rebuild all four files after changing the inventory and run the glyph
coverage gate. This fixed subset does not support arbitrary input;
on-device glyph and layout inspection remains a separate acceptance check.

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image retained for the upstream hardware documentation. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; retained for light backgrounds in upstream hardware documentation. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark for dark backgrounds in upstream hardware documentation. |
| [`images/marvy-logo.png`](images/marvy-logo.png) | 600 × 600, PNG RGBA | Unmodified Marvy logo supplied by the user on 2026-11-24 for this product. Transparent background and pink/blue gradient retained. |
| [`images/marvy-logo-welcome.png`](images/marvy-logo-welcome.png) | 112 × 112, PNG RGBA | Welcome-screen preview generated from the supplied logo. |
| [`images/marvy-logo-badge.png`](images/marvy-logo-badge.png) | 20 × 20, PNG RGBA | Badge preview used to identify Marvy sheep in the application. |

The supplied Marvy logo is brand material for this product, not a public-domain
or open-source logo. Its source SHA-256 is
`4ab5057babe4ee8a9f07c99fd5cd7b105e2751d5d7dd36c035ce935ba56efa99`.
Run `python3 tools/generate_marvy_logo.py` from the repository root to fit the
full image without cropping or distortion and generate the two PNG previews
and `main/marvy_logo.h` / `main/marvy_logo.c`. The converter requires Pillow;
`python3 tools/generate_marvy_logo.py --check` verifies the generated files
without modifying them. Firmware uses LVGL RGB565A8 descriptors: a
little-endian RGB565 color plane followed by a separate A8 plane. The two
Flash-resident pixel arrays total 38,832 bytes and need no runtime PNG decoder
or full-image heap allocation.

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.

### Marvy music and sampled effects

Seven clips selected through the sound skill's Freesound search are used under
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/), which permits use
in this commercial gift application. The metadata retains source pages,
authors, retrieval-time download/rating snapshots, source and PCM SHA-256
values, and the conversion recipe:
[`music/marvy_audio.source.json`](music/marvy_audio.source.json).

| Role and source | Conversion source | Embedded PCM |
| --- | --- | --- |
| BGM: [adventure](https://freesound.org/s/749008/) by **solarpsychedelic** | [`fs_749008_adventure.mp3`](music/fs_749008_adventure.mp3), downloaded high-quality preview | [`marvy_bgm.pcm`](music/marvy_bgm.pcm): 177,824 samples / 355,648 bytes / 11.114 seconds; cheerful chiptune loop. |
| Ordinary catch: [8-bit coin1.1.wav](https://freesound.org/s/162805/) by **timgormly** | [`fs_162805_8-bit-coin1-1-wav.mp3`](music/fs_162805_8-bit-coin1-1-wav.mp3), downloaded high-quality preview | [`marvy_hit.pcm`](music/marvy_hit.pcm): 5,917 samples / 11,834 bytes / 0.3698125 seconds; coin effect. |
| Marvy catch: [8-Bit Powerup](https://freesound.org/s/431329/) by **SomeGuy22** | [`fs_431329_8-Bit-Powerup.mp3`](music/fs_431329_8-Bit-Powerup.mp3), downloaded high-quality preview | [`marvy_bonus.pcm`](music/marvy_bonus.pcm): 8,029 samples / 16,058 bytes / 0.5018125 seconds; distinct power-up effect. |
| Level clear: [Completed.wav](https://freesound.org/s/609336/) by **Kenneth_Cooney** | [`fs_609336_Completed-wav.mp3`](music/fs_609336_Completed-wav.mp3), downloaded high-quality preview | [`marvy_clear.pcm`](music/marvy_clear.pcm): 20,000 samples / 40,000 bytes / 1.25 seconds; completion cue. |
| Level failure: [8-Bit - Error](https://freesound.org/s/450616/) by **Breviceps** | [`fs_450616_8-Bit-Error.mp3`](music/fs_450616_8-Bit-Error.mp3), downloaded high-quality preview | [`marvy_fail.pcm`](music/marvy_fail.pcm): 4,315 samples / 8,630 bytes / 0.2696875 seconds; short retry cue. |
| Three consecutive catches: [8-bit Happy Ding](https://freesound.org/s/361264/) by **JapanYoshiTheGamer** | [`fs_361264_8-bit-Happy-Ding.mp3`](music/fs_361264_8-bit-Happy-Ding.mp3), downloaded high-quality preview | [`marvy_combo3.pcm`](music/marvy_combo3.pcm): 5,948 samples / 11,896 bytes / 0.37175 seconds; C-major bonus cue. |
| Five consecutive catches: [LevelUp.wav](https://freesound.org/s/609335/) by **Kenneth_Cooney** | [`fs_609335_LevelUp-wav.mp3`](music/fs_609335_LevelUp-wav.mp3), downloaded high-quality preview | [`marvy_combo5.pcm`](music/marvy_combo5.pcm): 6,720 samples / 13,440 bytes / 0.42 seconds; successful-event cue. |

The original three runtime files remain unchanged at **383,540 bytes**. The four
campaign effects add **73,966 bytes**, for **457,506 bytes** total. All runtime
files are signed 16-bit little-endian PCM at 16 kHz mono. Each source is
independently peak-normalized to **27,852** (85% of 16-bit full scale). MP3
previews remain preparation inputs and are not compiled into firmware.

The 2026-11-25 selection snapshot records 7,612 downloads and 4.8/5 from 127
ratings for Completed; 7,463 and 4.7/5 from 66 for Error; 1,021 and 4.3/5 from
16 for Happy Ding; and 3,689 and 4.9/5 from 68 for LevelUp. Selection followed
uploader descriptions, popularity signals, and measured duration/energy,
without a claim of listening. Completed is trimmed from 3.2 to 1.25 seconds;
LevelUp from 0.621625 to 0.42 seconds. Both receive a 20 ms tail fade. Full
source previews and the processing reasons remain in the metadata.

Rebuild with [`music/prepare_audio.py`](music/prepare_audio.py):

```bash
python3 assets/music/prepare_audio.py
# If FFmpeg is not on PATH:
python3 assets/music/prepare_audio.py --ffmpeg /path/to/ffmpeg
```

The script resolves inputs relative to itself, verifies each pinned source
checksum, decodes with FFmpeg, applies only metadata-declared trimming/fades,
normalizes PCM, and updates processing metadata and hashes. FFmpeg must be
installed or supplied through `--ffmpeg`; its actual version is recorded.
No normalization is performed on the device.

[`main/CMakeLists.txt`](../main/CMakeLists.txt) embeds all seven PCM files. The
single codec-owning worker streams from Flash through one **256-sample** buffer.
Runtime BGM amplitude is 1/4 of normalized PCM; sampled effects are 1/3. Music
loops with a 4 ms seam fade and uses 16 ms on/off and ducking ramps. While any
cue plays, BGM is reduced to another 1/4. Final samples are bounded to
**±16,384**, with master volume **65%**. One cue plays at a time and a new request
replaces it; level-result dispatch takes priority over combo/hit feedback,
and the five-catch cue takes priority over the three-catch cue. Missing samples
retain synthesized fallbacks. Other short cues remain original synthesis,
not an official Marvy sound logo. BGM stops on welcome, level-brief, pause,
and result screens, including idle dimming.

These software settings do not establish physical speaker loudness. Volume,
loop seams, cue distinction and priority, pause/resume, and simultaneous
progress-saving playback still require on-device listening.

## Battery profiles

[`battery/wanli-500mah-profile2-xil40x-20261020.txt`](battery/wanli-500mah-profile2-xil40x-20261020.txt)
contains the supplied CW2017 profile for the Wanli New Energy 500 mAh cell
(profile2, XIL40X, 2026-10-20). It is an unchanged copy of the project-supplied
file, retained at the maintainer's request for this firmware update. Its
original filename is recorded in the paired Chinese document. No separate
license was included in the supplied file.

The format is 80 lines of two hexadecimal digits, one byte per line in register
order. The BSP embeds those bytes in `components/bsp/src/bsp_battery.c`; the
host gate checks the source file and initializer for an exact match. Text-file
SHA-256: `7ea96eba4634f4495a37c25ffacd54c1deaa0693b162c892d671a2ba68460919`.
Decoded 80-byte SHA-256:
`fa7365671093ef449c5bdcfb4adb279e3d275f888109f50b6b2f21e9b96220d5`.
This is a fuel-gauge profile, not a charging configuration or a generic profile
for every 500 mAh cell; see the [hardware guide](../docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md#9-cw2017-fuel-gauge).
