[简体中文](README.md) · **English**

# Marvy Sheep Ring Toss

An offline Year of the Sheep ring-toss game for FoloToy AI Passport. Hold any function button to charge, then release to throw; judge both distance and the moving sheep. A round has 10 rings: ordinary sheep score 1 point, while faster sheep bearing the pink-and-blue Marvy logo score 5.

This `0.3.0-dev` iteration implements only ring toss. Sheep counting, single-button flight, a website QR page, and NFC-tag provisioning are outside its current scope. The welcome screen retains the smooth Marvy logo and a New Year greeting from Marvy. Gameplay and results use hard-edge fonts, block-shaped sheep, and a pixel-style field. Moving, caught, and result-page Marvy sheep all carry the user-supplied logo.

## Play

1. Power on to a welcome screen that waits until a function button is pressed. Play no longer starts automatically.
2. Up, Down, and OK all use the same gesture: press to charge, then release to throw and spend one ring. Short holds land near; longer holds land farther. The charge bar and landing marker follow the hold duration.
3. A 0–1400 ms charge moves the landing point from 32 px on the left to 208 px on the right. Longer holds stay at maximum distance without firing automatically. Sheep travel left and right along one lane; catches use their positions when the ring lands. One ring can catch at most one sheep, with a current hit tolerance of 16 px.
4. The release belonging to a welcome, restart, or wake press never throws. Input during flight and feedback does not spend another ring. Lost or cancelled input safely cancels an unreleased charge.
5. Ordinary sheep score 1 point and travel at 30–42 px/s; Marvy sheep score 5 and travel at 48–56 px/s. A Marvy catch displays a transparent 5-point reward and Happy New Year message for approximately 1.2 seconds, with simple pixel fireworks and no solid card covering the scene. Ordinary-hit and miss feedback lasts 0.4 seconds.
6. Results appear after the tenth ring lands and its full feedback finishes. See the round score and best score; press any function button to restart, then use a new hold-and-release gesture for the next round's first ring.
7. A new best score is stored in NVS after a completed round and survives a normal power cycle. Storage failure leaves the game playable with a session-only record.

Cheerful chiptune music loops during play. Ordinary catches use a coin sound; Marvy catches use a distinct power-up sound, and the music ducks under effects. Background music stops on welcome and result screens and while idle. After 45 seconds without input, the game pauses, the backlight dims, and any unreleased charge is cancelled. The first press only restores brightness; its release spends no ring. Battery status appears at the top right; an unavailable reading shows `--%`.

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

Game logic, timestamped input pairing, and the hardware UI are separated for host testing. Delivery must report build, host tests, device tests, and unverified checks separately; a successful build is not physical-device acceptance.

## Fonts and assets

The Chinese UI uses fixed-inventory subsets of Noto Sans SC Regular, distributed under OFL 1.1. The welcome screen uses smooth 16 / 20 px, 4 bpp fonts; gameplay and results use hard-edge 1 bpp fonts at the same sizes. Each of the four fonts covers 95 printable ASCII and 53 fixed Chinese characters. The full source font is for reproducible generation only; firmware compiles only the generated C fonts. After changing displayed strings, run:

```bash
bash tools/generate_marvy_fonts.sh
```

See [assets](assets/README.md) for the source, license, character inventory, and SHA-256. Actual glyphs, layout, and animation still need inspection on the display.

The original logo is retained at `assets/images/marvy-logo.png`. Run `python3 tools/generate_marvy_logo.py` to generate transparent welcome and sheep-badge assets, or `python3 tools/generate_marvy_logo.py --check` to verify reproducibility.

Background music is [adventure](https://freesound.org/s/749008/) by solarpsychedelic. Ordinary catches use [8-bit coin1.1.wav](https://freesound.org/s/162805/) by timgormly; Marvy catches use [8-Bit Powerup](https://freesound.org/s/431329/) by SomeGuy22. All three are used under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). Firmware embeds 16 kHz, 16-bit mono PCM and plays it in small blocks in the audio worker; output volume and digital mix gains have been increased. See the asset documentation for provenance, normalization and rebuilding, and the supplied brand material's usage scope.

## Scope and acceptance

- A logo welcome screen that waits for a press, plus pixel-style gameplay and results.
- Charge-controlled distance, 10-ring results, ordinary / faster Marvy sheep scoring, transparent celebration messages, and pixel fireworks.
- Distinct sampled ordinary / Marvy hit sounds, background music, and persistent best score.
- Device checks pending for this iteration: welcome waiting and release behavior, charge bar and actual landing point, logo and Chinese legibility, celebration/firework obstruction, music looping and loudness, frame smoothness, record retention after power cycling, and battery readings.
- Other short feedback cues remain original synthesis; an official brand sound logo can be supplied separately.

This branch starts from [FoloToy's open development baseline](https://github.com/FoloToy/ai-passport). It reuses the `components/bsp` drivers and keeps application code in `main/`; the product UI does not reuse the hardware-test menu.

See [AGENTS.md](AGENTS.md) for development rules and [docs/README.md](docs/README.md) for engineering references.
