# Gravity Defied — Arduboy

Trials bike game for Arduboy (ATmega32u4, 128×64, 1-bit). 10 levels, fixed-point
physics, lap-time records in EEPROM, splash screen, menu with level list and
options, engine sound and music via `ArduboyTones`.

## Install

### Option A — flash the prebuilt binary (no toolchain)

| File | Use with |
|---|---|
| `Gravity_Defied.arduboy` | Arduboy Toolset / Arduboy Manager — packaged `info.json` + hex |
| `TrialsFX.hex` | any flasher that takes a raw hex |

`info.json` declares the target device as `ArduboyMini`.

### Option B — build from source

Requirements:

- Arduino IDE 2.x **or** `arduino-cli`
- Board package: [Arduboy homemade package](https://github.com/MrBlinky/Arduboy-homemade-package)
  (needed for the Arduboy Mini / FX board targets; a stock Arduboy also builds
  on the plain `Arduino Leonardo` board)
- Libraries: `Arduboy2`, `ArduboyTones` (Library Manager)

Arduino requires the sketch folder to be named after the `.ino`, so copy the
sources into a folder called `TrialsFX/` (or rename the checkout) before building.

```bash
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/MrBlinky/Arduboy-homemade-package/main/package_arduboy_homemade_index.json
arduino-cli core update-index
arduino-cli core install arduboy-homemade:avr
arduino-cli lib install Arduboy2 ArduboyTones

# FQBN: arduboy-homemade:avr:arduboy-mini | :arduboy | :arduboy-fx
arduino-cli compile -b arduboy-homemade:avr:arduboy-mini TrialsFX
arduino-cli board list                      # find the port
arduino-cli upload  -b arduboy-homemade:avr:arduboy-mini -p /dev/cu.usbmodemXXXX TrialsFX
```

The Arduboy resets into the bootloader for ~8 s when it appears as a new serial
port; if upload misses the window, double-tap reset and retry.

### Desktop tools (optional)

`sim/` builds with any C++17 compiler; the generator scripts need `python3`, and
the image ones need `Pillow` (`pip install pillow`).

## Controls

| Screen | Button | Action |
|---|---|---|
| splash | A / B / ↓ | skip (auto-advances after ~2.5 s) |
| menu | ↑/↓, A | move, select — `PLAY` (level 1), `LEVELS`, `OPTIONS` |
| menu | ↑+↓ held 3 s | exit to the FX/bootloader menu |
| levels | ↑/↓, A, B | move, start, back — last row is `BACK`; rows show difficulty + best time |
| options | ↑/↓ | move between `SOUND` and `BACK` |
| options | ←/→ or A | toggle sound (saved in EEPROM) |
| game | A | throttle |
| game | B | brake / reverse |
| game | ←/→ | lean (raise / drop the nose) |
| game | ↓ | back to menu |
| crash / finish | A | retry level |
| finish | → | next level |
| crash / finish | ↓ | back to menu |

## Layout

| File | What it is |
|---|---|
| `TrialsFX.ino` | hardware adapter: input, camera, HUD, states, sound |
| `gd_core.h` | bike and terrain physics, no Arduboy dependency |
| `fp.h` | fixed-point math |
| `game_types.h` | state enum, menu items — in a header so Arduino's generated prototypes see them |
| `levels.h` | generated tracks (`sim/genlevel.py`) |
| `bike_sprite.h`, `tinyfont.h` | graphics |
| `music.h` | `ArduboyTones` music and sound effects |
| `records.h` | EEPROM records |
| `Arduboy2.h`, `EEPROM.h` | desktop mocks — **not** the real libraries |
| `sim/` | desktop checks and asset generators; excluded from the AVR build |
| `cover.png`, `title.png`, `sprite.png` | source art |

Physics lives in `namespace gd` on purpose: `Arduboy2.h` declares its own
`struct Point`, and without the namespace the compiler silently picks the wrong
type.

Sound is one voice only — engine tone and menu music share the channel, so the
engine is silent in menus and the music stops in game.

## Check before flashing

```bash
cd sim && bash check.sh          # BEAM=2500 by default; lower it for a quick pass
```

Builds the desktop tools, compiles the `.ino` the way `arduino-cli` does
(generated prototypes included), runs the physics under a bot, verifies camera
framing, brute-forces all 10 levels for solvability, checks font, records and
sprite. Non-zero exit = do not flash.

`sim/` is named that way deliberately: the Arduino build compiles the sketch root
plus `src/` recursively and ignores other subfolders, so desktop-only files there
never reach the AVR build.

## Known gaps

- `sim/headless.cpp` still includes the removed `level1.h` / `level2.h` instead of
  `levels.h`, so step 1 of `sim/check.sh` fails to build.
- There is no desktop mock for `ArduboyTones.h`, so the `.ino` compile step and
  `sim/inocheck.py` fail.
- The desktop mocks `Arduboy2.h` / `EEPROM.h` sit in the sketch root, where the
  AVR build's include path can pick them up ahead of the real libraries.
- Root `check.sh` and `genfont.py` are stale duplicates of the `sim/` copies and
  use `../` paths, so they only work from inside `sim/`.
- `sim/gentitle.py` imports a `gencover` module that is not in the repo.
- `Sketch.hex` is not a build of this sketch — `TrialsFX.hex` is.
