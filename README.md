# Gravity Defied — Arduboy

Trials bike game for Arduboy (ATmega32u4, 128×64, 1-bit). 10 levels, fixed-point
physics, lap-time records in EEPROM.

## Controls

| Button | Action |
|---|---|
| A | throttle |
| B | brake |
| ←/→ | lean (raise / drop the nose) |
| ←/→ on title | select level |
| A on title | start |
| ↓ | back to title |
| → after finish | next level |
| ↑+↓ on title | reset records |

## Layout

| File | What it is |
|---|---|
| `TrialsFX.ino` | hardware adapter: input, camera, HUD, states |
| `gd_core.h` | bike and terrain physics, no Arduboy dependency |
| `fp.h` | fixed-point math |
| `levels.h` | generated tracks (`sim/genlevel.py`) |
| `bike_sprite.h`, `tinyfont.h` | graphics |
| `records.h` | EEPROM records |
| `sim/` | desktop checks; excluded from the AVR build |

Physics lives in `namespace gd` on purpose: `Arduboy2.h` declares its own
`struct Point`, and without the namespace the compiler silently picks the wrong
type.

## Check before flashing

```bash
cd sim && ./check.sh
```

Builds the simulator, runs the physics under a bot, verifies camera framing,
brute-forces all 10 levels for solvability, checks font, records and sprite.
Non-zero exit = do not flash.

## Build

Arduino IDE / arduino-cli, Arduboy board, `Arduboy2` library. Prebuilt
`TrialsFX.hex` sits in the repo root.
