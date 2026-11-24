[简体中文](README.md) · **English**

# Marvy Sheep Ring Toss

An offline Year of the Sheep ring-toss game for FoloToy AI Passport. Time a throw as a sheep crosses the landing marker, then press any function button. A round has 10 rings: ordinary sheep score 1 point, while faster sheep bearing the pink-and-blue Marvy logo score 5.

This `dev` iteration implements only ring toss. Sheep counting, single-button flight, a website QR page, and NFC-tag provisioning are outside its current scope. The welcome screen, moving Marvy sheep, capture animation, and result-page character use the user-supplied Marvy logo. Sheep are original procedural artwork, and short sound effects are original synthesized cues rather than an official Marvy audio logo.

## Play

1. Power on for an approximately 2-second welcome screen showing the Marvy logo and a New Year greeting from Marvy, then play starts automatically. Any function button can skip the introduction.
2. Sheep travel left and right along one catching lane. A throw is judged against their positions when the ring lands; one ring can catch at most one sheep.
3. Up, Down, and OK all throw on the press event. Holding a button does not repeat, and presses during flight or hit feedback do not consume extra rings.
4. Ordinary sheep score 1 point and travel at 30–42 px/s; Marvy sheep score 5 and travel at 48–56 px/s. A Marvy catch displays a 5-point reward and a Happy New Year message for approximately 1.2 seconds. Ordinary-hit and miss feedback lasts 0.4 seconds.
5. Results appear after the tenth ring lands and its full feedback finishes. See the round score and best score; press any function button to restart immediately.
6. A new best score is stored in NVS after a completed round and survives a normal power cycle. Storage failure leaves the game playable with a session-only record.

Lighthearted chiptune-style music loops during play and ducks under sound effects. Background music stops on welcome and result screens and while idle. After 45 seconds without input, the game pauses and the backlight dims. The first press restores brightness without spending a ring. Battery status appears at the top right; an unavailable reading shows `--%`.

## Develop and validate

The target is ESP32-C3 with 8 MB Flash, no PSRAM, and a 240 × 320 display, using ESP-IDF **5.5.3**. The application needs no Wi-Fi, Bluetooth, account, or phone software.

Prepare and activate ESP-IDF using the [environment guide](docs/development/engineering/environment-setup.md), then run from the repository root:

```bash
python -m pip install -r tools/requirements-dev.txt
./tools/validate.sh --static
./tools/validate.sh --firmware
./tools/validate.sh
```

The complete gate retains the merged `build/FoloToy-AI-Passport-full.bin` for offset `0x0`, together with matching debug artifacts under `build/firmware/`. Do not use the application-only image at offset `0x0`. A merged flash can reset existing NVS records. Flashing requires separate approval and does not imply permission to erase the entire chip.

Game logic is independent from the hardware UI for host testing. Delivery must report build, host tests, device tests, and unverified checks separately; a successful build is not physical-device acceptance.

## Fonts and assets

The Chinese UI uses 16 / 20 px fixed-inventory subsets of Noto Sans SC Regular, distributed under OFL 1.1. The full source font is for reproducible generation only; firmware compiles only the generated C font files. After changing displayed strings, run:

```bash
bash tools/generate_marvy_fonts.sh
```

See [assets](assets/README.md) for the source, license, character inventory, and SHA-256. Actual glyphs, layout, and animation still need inspection on the display.

The original logo is retained at `assets/images/marvy-logo.png`. Run `python3 tools/generate_marvy_logo.py` to generate transparent welcome and sheep-badge assets, or `python3 tools/generate_marvy_logo.py --check` to verify reproducibility.

Background music is [beepbox #11](https://freesound.org/s/381316/) by ADnova, used under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). Firmware embeds 16 kHz, 16-bit mono PCM and plays it in small blocks in the audio worker. See the asset documentation for provenance, conversion, and the supplied brand material's usage scope.

## Scope and acceptance

- Original grass-field ring-toss UI, logo welcome screen, 10-ring results, and immediate restart.
- Ordinary / faster Marvy sheep scoring, celebration messages, background music and sound effects, and persistent best score.
- Device checks pending for this iteration: logo legibility, Chinese text and celebration layout, music looping and cue volume, button feel, frame smoothness, record retention after power cycling, and battery readings.
- The original synthesized cues can be replaced with a supplied brand audio logo.

This branch starts from [FoloToy's open development baseline](https://github.com/FoloToy/ai-passport). It reuses the `components/bsp` drivers and keeps application code in `main/`; the product UI does not reuse the hardware-test menu.

See [AGENTS.md](AGENTS.md) for development rules and [docs/README.md](docs/README.md) for engineering references.
