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
| [`fonts/marvy_characters.txt`](fonts/marvy_characters.txt) | Deduplicated, sorted inventory of 48 fixed Chinese UI characters. Both generated sizes also include 95 printable ASCII characters U+0020–U+007E. |
| [`fonts/marvy_font_16.c`](fonts/marvy_font_16.c) | 16 px, 4 bpp, uncompressed LVGL bitmap font for labels and hints; exported symbol `marvy_font_16`. |
| [`fonts/marvy_font_20.c`](fonts/marvy_font_20.c) | 20 px, 4 bpp, uncompressed LVGL bitmap font for headings; exported symbol `marvy_font_20`. |

Source: [Noto CJK official repository](https://github.com/notofonts/noto-cjk),
[`Sans/SubsetOTF/SC/NotoSansSC-Regular.otf`](https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf),
with its [license](https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/LICENSE).
The vendored source bytes are pinned by SHA-256
`faa6c9df652116dde789d351359f3d7e5d2285a2b2a1f04a2d7244df706d5ea9`.

Regenerate from the repository root with
`bash tools/generate_marvy_fonts.sh`. The script validates the source checksum,
uses `lv_font_conv` **1.5.3**, and requests `--bpp 4 --format lvgl
--no-compress --lv-include lvgl.h` for sizes 16 and 20. Generated C files go
into the application's CMake source list; UI labels must explicitly select
the appropriate font. Rebuild both sizes after changing the inventory and run
the glyph coverage gate. This fixed subset does not support arbitrary input;
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
| [`images/marvy-logo-badge.png`](images/marvy-logo-badge.png) | 20 × 20, PNG RGBA | Badge preview used to identify moving, caught, and result-page Marvy sheep. |

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

### Marvy background music

The game uses [beepbox #11](https://freesound.org/s/381316/) by **ADnova**, under
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). It is a relaxed,
happy chiptune loop, selected through the sound skill's Freesound search for
the offline ring-toss game. Retrieval details, license, file hashes, and
conversion settings are retained in [`music/marvy_bgm.source.json`](music/marvy_bgm.source.json).

| File | Format and use |
| --- | --- |
| [`music/fs_381316_beepbox-11.mp3`](music/fs_381316_beepbox-11.mp3) | Downloaded Freesound high-quality MP3 preview, 44.1 kHz mono; conversion source. |
| [`music/marvy_bgm.pcm`](music/marvy_bgm.pcm) | Signed 16-bit little-endian PCM, 16 kHz mono; 203,407 samples / 406,814 bytes / approximately 12.713 seconds. Embedded in firmware. |
| [`music/marvy_bgm.source.json`](music/marvy_bgm.source.json) | Source, CC0 license, retrieval metadata, SHA-256 values, and processing recipe. |

Convert from the repository root with FFmpeg:

```bash
ffmpeg -hide_banner -loglevel error -y \
  -i assets/music/fs_381316_beepbox-11.mp3 \
  -ac 1 -ar 16000 -c:a pcm_s16le -f s16le assets/music/marvy_bgm.pcm
```

The application embeds the PCM as `marvy_bgm_pcm` and streams it from Flash
through one 256-sample worker buffer. Runtime processing scales the background
to 1/16 of source amplitude, fades the loop seam over 4 ms, and ramps music
on/off over 16 ms. Original synthesized feedback cues mix over the music;
while a cue plays, the background level is reduced to another 1/4. The audio
worker alone owns the codec, with output volume set to 32%. Background music
loops only during active play and stops on welcome/results screens and idle
pause. These software levels are not a measured loudness guarantee on the
physical speaker; final listening remains an on-device check.

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
