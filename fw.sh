#!/usr/bin/env bash
# Builds (and flashes) the firmware with Ledger as its UI, through Brewdeck's build.sh.
#   ./fw.sh ui-p4-debug [--flash]     any env of firmware/platformio.ini (ui, ui-debug, ui-p4, ui-p4-debug)
# BREWDECK=path to the Brewdeck checkout (default ../gaggi-screen); PORT, GAGGI_BACKUP, DRY_RUN as for build.sh.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
BD="${BREWDECK:-$HERE/../gaggi-screen}"
UI="$HERE" exec "$BD/firmware/build.sh" "$@"
