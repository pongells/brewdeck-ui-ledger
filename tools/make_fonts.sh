#!/usr/bin/env bash
# Rebuilds the LVGL fonts in ui/fonts/ from the OFL sources in ui/fonts/src (Archivo variable, IBM Plex Mono).
# Needs uv (fonttools, to cut static instances out of the variable font) and Node (lv_font_conv).
# Each font holds only the characters listed here: a character not in its range shows as nothing on the screen.
set -euo pipefail
cd "$(dirname "$0")/.."
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
inst() {   # inst <out.ttf> <wght> <wdth>: a static instance of Archivo
  uvx --from fonttools fonttools varLib.instancer ui/fonts/src/Archivo-VF.ttf wght="$2" wdth="$3" -q -o "$TMP/$1"
}
inst a_c300.ttf 300 62     # condensed light: the big numbers
inst a_c600.ttf 600 62     # condensed semibold: the big weight
inst a_400.ttf 400 100     # text
inst a_600.ttf 600 100     # text, bold

CONV="npx --yes lv_font_conv@1.5.3"
DIGITS="0x30-0x39,0x2E,0x2D,0x3A"          # 0-9 . - :
TEXT="0x20-0x7E,0xB0,0xB7,0x2013,0x2014,0x2212,0x221E"   # ASCII, degree, middle dot, en and em dash, minus, infinity
f() {   # f <name> <ttf> <size> <range>
  $CONV --bpp 4 --size "$3" --font "$2" -r "$4" --format lvgl --no-compress --lv-font-name "$1" \
        --lv-include lvgl.h -o "ui/fonts/$1.c"
  echo "ui/fonts/$1.c"
}
f lg_c300_220 "$TMP/a_c300.ttf" 220 "$DIGITS"
f lg_c300_160 "$TMP/a_c300.ttf" 160 "$DIGITS"
f lg_c600_160 "$TMP/a_c600.ttf" 160 "$DIGITS"
f lg_400_36   "$TMP/a_400.ttf" 36 "$TEXT"
f lg_400_17   "$TMP/a_400.ttf" 17 "$TEXT"
f lg_600_17   "$TMP/a_600.ttf" 17 "$TEXT"
f lg_600_24   "$TMP/a_600.ttf" 24 "$TEXT"
f lg_mono_14  ui/fonts/src/IBMPlexMono-Medium.ttf 14 "$TEXT"
f lg_mono_13  ui/fonts/src/IBMPlexMono-Regular.ttf 13 "$TEXT"
