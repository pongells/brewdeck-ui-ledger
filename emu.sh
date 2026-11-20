#!/usr/bin/env bash
# Builds Ledger into the Brewdeck emulator and runs it (a fake machine, no hardware).
#   ./emu.sh            build and open the window
#   ./emu.sh build      build only (BREWDECK=path to the Brewdeck checkout, default ../gaggi-screen)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
BD="${BREWDECK:-$HERE/../gaggi-screen}"
UI="$HERE" "$BD/sim/build-emu.sh"
[[ "${1:-}" == build ]] || exec "$BD/sim/emu-brewdeck-ledger"
