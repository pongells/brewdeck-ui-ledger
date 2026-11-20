# Ledger

A light, printed-looking UI for Gaggiuino machines, running on the [Brewdeck](https://github.com/pongells/brewdeck)
core (LVGL 9, 800 x 480). Paper background, black ink, one red accent: the number that matters set huge, everything
else as a ledger of rows with dotted leaders.

Unofficial: not made by or affiliated with the Gaggiuino project.

## Screens

- **Ready:** boiler temperature; rows for profile, brew temperature, stop weight (tap to change), pressure, scale,
  water; the active profile's plan (pressure solid, flow dashed); Flush (tap: flush mode, hold: timed flush) and
  Profiles.
- **Brewing:** time and weight (to the stop weight), a chart of pressure (ink), flow (grey) and weight (red) with the
  stop weight dotted. "Done" with the result when the shot ends; a tap or 60 s goes back to Ready.
- **Profiles:** every profile with its brew temperature and stop weight; a tap selects it (and saves it as the
  machine's boot profile).
- **Editor:** brew temperature (1-105 °C) or stop weight (0-200 g, 0 = none) with − / +, saved on the machine.
- **Splash:** 5 s at start (or a tap), with the not-affiliated line.

## Build

Needs a checkout of Brewdeck next to this folder (`../gaggi-screen`, or set `BREWDECK=`).

```sh
./emu.sh                    # the emulator: a fake machine in a window (needs SDL2: brew install sdl2)
./fw.sh ui-p4-debug         # firmware for the Guition ESP32-P4 board, with the debug web server
./fw.sh ui --flash          # firmware for the Sunton ESP32-S3, then flash (see Brewdeck's README first)
```

## Fonts

Archivo (condensed for the big numbers) and IBM Plex Mono, both under the SIL Open Font License (`fonts/src`).
`tools/make_fonts.sh` converts them to LVGL fonts in `fonts/` (needs uv and Node). Each font holds only the
characters listed in that script: add a character there before using it in the UI.

## Compared with Brewdeck's UI

[FEATURES.md](FEATURES.md): every feature of Brewdeck's own UI, with what Ledger has, lacks, and what was seen on
the panel.
