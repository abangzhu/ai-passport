[简体中文](README.md) · **English**

# Marvy Sheep Ring

An offline Year of the Sheep ring-toss game for FoloToy AI Passport. The three right-side buttons are numbered #1, #2, and #3 from top to bottom. In menus, #1 confirms, #2 moves up, and #3 moves down. During play, #1 pauses; hold either #2 or #3 to charge and release it to throw, judging distance and moving sheep. Six levels each last 45 seconds, with unlimited throws. Ordinary sheep score 1 point; faster sheep bearing the pink-and-blue Marvy logo score 5. Every sheep counts as one capture toward passing, while points track per-level records.

This `0.8.1-dev` iteration implements the six-level Marvy ring-toss campaign. Sheep counting, single-button flight, a website QR page, and NFC-tag provisioning are outside its scope. The welcome screen retains the smooth Marvy logo and New Year greeting, with gentle floating, opacity pulses, and twinkling accents. Controls help, level briefs, gameplay, pause menus, and results use hard-edge fonts and a pixel style with block-shaped sheep and grass. Moving and caught Marvy sheep carry the user-supplied logo.

## Levels and controls

The right-side buttons keep fixed numbers from top to bottom. Prompts and Controls help use #1, #2, and #3 throughout:

This patch corrects the physical binding of #1 and #2, using the same number for each key's press and release.

| Number | Position | In menus, including pause | During play |
| --- | --- | --- | --- |
| #1 | Upper-right | Confirm the selected item. | Immediately pause and open Continue / Return welcome. |
| #2 | Middle-right | Select the previous item. | Hold to charge; release to throw. |
| #3 | Lower-right | Select the next item. | Hold to charge; release to throw, with the same effect as #2. |

The welcome menu offers Start game and Controls. The single-page Controls screen shows a vertical right-side key diagram with numbers, positions, and both contexts. #1 returns to welcome; #2 and #3 have no action on that page. During play, either throw key works directly.

| Level | Captures in 45 seconds | Ground landing hint | Per-throw deadline | Sheep speed multiplier | Marvy spawn quota |
| --- | ---: | --- | --- | ---: | ---: |
| 1 · First throws | 4 | Shown | None | 0.70 | 25% |
| 2 · Trust your touch | 5 | Hidden | None | 0.70 | 25% |
| 3 · Read the movement | 6 | Hidden | None | 0.90 | 25% |
| 4 · Commit to the throw | 7 | Hidden | 6 seconds | 0.90 | 25% |
| 5 · Chase Marvy | 9 | Hidden | 6 seconds | 1.15 | 30% |
| 6 · New Year challenge | 10 | Hidden | 4.5 seconds | 1.15 | 30% |

Multipliers apply to base speeds of 30–42 px/s for ordinary sheep and 48–56 px/s for Marvy sheep. Each level starts with one Marvy sheep. Later spawns use shuffled quota bags: one Marvy per four entries in levels 1–4, and three per ten in levels 5–6. At most one Marvy is active at once in levels 1–4, or two in levels 5–6. Entries can be reordered or deferred at that cap, so the quota is not a promise about every short time window.

1. Power on to the animated welcome screen. #2 and #3 switch between Start game and Controls; #1 confirms. Start game opens the untimed level brief, where #2 and #3 select an unlocked level and #1 confirmation starts its 45-second clock.
2. Either #2 or #3 directly throws during play: hold one key to charge, then release that same key to throw. A 0–1400 ms charge moves the horizontal landing point from 32 px on the left to 208 px on the right; longer holds remain at maximum distance. Level 1 shows a charge-following landing hint, while later levels hide it. Every level retains the charge bar and actual flying ring.
3. Flight lasts 600 ms; catches use the sheep's position at landing. One ring catches at most one sheep, with a current tolerance of 16 px. Every landing uses the same 200 ms input lock before the next ring is ready, including Marvy catches.
4. Levels 1–3 have no per-throw timeout, but their whole-level clock still lasts 45 seconds. In levels 4–6, waiting and charging share the throw deadline from each ready state. Repeated presses and cancelled charges do not reset it. Both clocks freeze after #1 opens the pause menu. Pixel digits `3 / 2 / 1` mark the final three seconds; timeout automatically throws at half power (700 ms charge, 120 px landing), and a later release never throws twice. During the stage's last five seconds, the overall clock takes priority and the per-throw countdown is hidden.
5. Reaching the target early still leaves the rest of the 45 seconds for points and stars. At zero, no new throw is accepted and an unreleased charge is cancelled. A ring already airborne finishes landing before settlement. The stage cutoff wins over a simultaneous throw timeout or release, without adding a throw.
6. Press #1 during play to pause immediately and open the menu, defaulting to Continue. Menu rules apply again: #2 moves up, #3 moves down between Continue and Return welcome, and #1 confirms. Pause freezes sheep, flight, feedback, celebration, and both clocks. Continue preserves the remaining time; releasing its confirmation key does not throw. Return welcome discards the unsettled stage score while keeping saved progress.
7. Results show points, captures, throws, accuracy, and stars. #2 and #3 select Next level, Retry, or Return welcome; #1 confirms. Next level appears only after a pass with another level available. Retry never requires replaying earlier stages.
8. Releases belonging to start, help, menu, or wake gestures never throw. Pressing #2 or #3 during flight or landing feedback does not queue a later throw. Only the key that actually started charging may throw on its matching release; releasing the other key cannot substitute. A release after an automatic timeout throw never throws again. Lost or cancelled input safely cancels an unreleased charge without extending its deadline.

## Stars, feedback, and saved progress

- One star requires the level's capture target; two stars require two extra captures; three require four extra captures and at least 60% accuracy. Meeting the base target unlocks the next level; stars are not an unlock requirement.
- Ordinary sheep score 1 point and Marvy sheep score 5, but both count as one capture toward passing. Three and five consecutive catches show a combo prompt and play different effects. A miss clears the streak; base scoring is unchanged.
- A Marvy catch independently displays transparent 5-point and Happy New Year text with pixel fireworks for approximately 1.2 seconds. It can overlap preparation for the next ring and has no solid scene-covering card.
- After a level settles, a background worker merges and saves unlocked levels, per-level best points, and best stars to NVS. These survive a normal power cycle. Storage failure retains progress in RAM and retries without automatically erasing NVS. The old ten-ring best remains in a separate key and is not mapped to a timed-level record.

Cheerful chiptune music loops during play. Ordinary catches, Marvy catches, passes, failures, and three-/five-catch combos use distinct sampled cues, with music ducking beneath them. Welcome, Controls, level briefs, pause menus, and results have no BGM and dim after 45 seconds without input. The first press only restores brightness; its release neither starts play nor confirms a menu. Active gameplay never pauses or dims because of inactivity. Battery status appears at the top right; unavailable readings show `--%`.

## Develop and validate

Target hardware is ESP32-C3 with 8 MB Flash, no PSRAM, and a 240 × 320 display, using ESP-IDF **5.5.3**. The application needs no Wi-Fi, Bluetooth, account, or phone software.

Prepare and activate ESP-IDF using the [environment guide](docs/development/engineering/environment-setup.md), then run from the repository root:

```bash
python -m pip install -r tools/requirements-dev.txt
./tools/validate.sh --static
./tools/validate.sh --firmware
./tools/validate.sh
```

The complete gate retains the merged `build/FoloToy-AI-Passport-full.bin` for flashing at `0x0`, with a matching debug archive under `build/firmware/`. Never treat the application-only image as a merged `0x0` image. A merged flash can reset NVS records; flashing requires separate approval and does not imply a full-chip erase.

Game logic, timestamped input pairing, and hardware UI are separated for host testing. Report Build, Host tests, Device tests, and Unverified separately at delivery; a successful build is not hardware acceptance.

## Fonts and assets

Chinese UI text uses a fixed Noto Sans SC Regular subset under OFL 1.1. The welcome screen uses smooth 16 / 20 px, 4 bpp fonts; other pages use the same sizes at 1 bpp for hard edges. All four fonts contain 95 printable ASCII characters and the 129 Chinese UI characters in the [fixed inventory](assets/fonts/marvy_characters.txt), for 224 glyphs each, shared by numbered Controls help and page-specific button prompts. The full source font is for rebuilding only; firmware compiles the generated C subsets. After changing visible text, run:

```bash
bash tools/generate_marvy_fonts.sh
```

See [assets](assets/README.md) for the source, license, character inventory, and SHA-256. Actual glyphs, layout, and animation still need inspection on the display.

The original logo is retained at `assets/images/marvy-logo.png`. Run `python3 tools/generate_marvy_logo.py` to generate transparent welcome and sheep-badge assets, or `python3 tools/generate_marvy_logo.py --check` to verify reproducibility.

Existing background music and ordinary/Marvy catch samples remain, with new pass, failure, and two combo cues. All seven clips are used under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). New PCM occupies 73,966 bytes; all embedded PCM totals 457,506 bytes, at 16 kHz, 16-bit mono, played in small blocks by the audio worker. Authors, source pages, retrieval-time download/rating snapshots, trimming and normalization, and source/PCM SHA-256 values are in [assets](assets/README.md) and the [audio manifest](assets/music/marvy_audio.source.json).

## Scope and acceptance

- Six timed, unlimited-throw levels with unlocks and retries, a landing hint only in level 1, and progressive throw pressure and sheep speed.
- Whole-level and per-throw clocks, last airborne-ring settlement, separate capture and 1/5-point scoring, stars, combos, controlled Marvy spawns, and background progress persistence.
- Animated welcome, Controls help, level briefs, pixel-style gameplay and results, pause/continue/return navigation, and transparent greetings and fireworks.
- Numbered #1 / #2 / #3 right-side key diagram: confirm / up / down in menus, and immediate #1 pause with direct #2 / #3 hold/release throws during play.
- Device checks pending: recheck the physical binding of #1 and #2, and confirm that #1 / #2 / #3 match the upper-, middle-, and lower-right positions. Verify #1 confirmation, #2 previous, and #3 next in menus; immediate #1 pause and both #2 / #3 charge/release throws during play, using only the matching key release. Check that pause freezes all clocks, Continue confirmation releases remain inert, flight-time presses do not queue throws, releases after timeout do not throw twice, and the numbered Controls diagram and idle/wake work correctly. Regression checks remain for level difficulty, both clocks and late-ring settlement, the level-1 hint, Chinese/menu layout, sound and frame rate, power-cycle progress, storage-failure fallback, and battery readings.
- Other short feedback remains original synthesis; these assets are not an official Marvy sound logo.

This branch starts from [FoloToy's open development baseline](https://github.com/FoloToy/ai-passport). It reuses the `components/bsp` drivers and keeps application code in `main/`; the product UI does not reuse the hardware-test menu.

See [AGENTS.md](AGENTS.md) for development rules and [docs/README.md](docs/README.md) for engineering references.
