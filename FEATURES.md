# Ledger vs. Brewdeck's UI

What Ledger does compared with Brewdeck's own UI (the reference: Brewdeck's `docs/manual.md`). Status: **done**,
**partial** (works, but less than Brewdeck), **missing**. "Panel" = seen working on the real screen (P4), not only in
the emulator. Last updated 2026-11-21.

Each line has an ID (`R1`, `B2`, ...) so a later standard test list can refer to it.

## Start-up

| ID | Feature (Brewdeck) | Ledger | Panel | Notes |
|---|---|---|---|---|
| S1 | Splash 5 s, tap skips, with the not-affiliated line | done | yes | no setting to turn it off (Brewdeck: Display > Splash at start) |
| S2 | Shows what it is doing while the machine is not reachable yet | partial | yes | header word only (CONNECTING / MACHINE NOT FOUND / NO WI-FI); see Offline |

## Ready

| ID | Feature (Brewdeck) | Ledger | Panel | Notes |
|---|---|---|---|---|
| R1 | State from the machine's readiness: Warming up, Heating, Ready, Standby, Waiting, Maintenance | done | yes | header word, upper case |
| R2 | Busy states: Brewing, Steam, Hot water, Flush, Descale | partial | yes | no DESCALE word (falls back to the readiness) |
| R3 | Cooling after steam (boiler > target + 3) | done | yes | |
| R4 | Ready hold: Ready only after 30 s steady READY after a heating phase | missing | | Ledger shows READY / HEATING as reported (flips while warming) |
| R5 | Boiler temperature, large | done | yes | |
| R6 | Active profile name | done | yes | one line with "..." |
| R7 | Profile preview (what a phase drives, solid / its limits, dim) | partial | yes | pressure solid, flow dashed; limits not drawn; no colours |
| R8 | Pressure, scale (type, connected), water | done | yes | as ledger rows; pressure clamped to 0-20 bar |
| R9 | Pressure warning (orange while pressure is left in the system) | missing | | |
| R10 | No scale (orange), Refill below 20 % (orange) | missing | | values are shown, never highlighted |
| R11 | Clock | done | yes | in the header (Brewdeck shows it on the idle clock) |

## Brew temperature and stop weight

| ID | Feature (Brewdeck) | Ledger | Panel | Notes |
|---|---|---|---|---|
| T1 | Change brew temp (1 °C steps, 1-105) | done | | editor page with − / +, hold repeats; not yet tried on the machine |
| T2 | Change stop weight (0.5 g steps) | done | | 0-200 g (Brewdeck's -/+: 5-80 g) |
| T3 | Keypad to type a value | missing | | − / + only |
| T4 | No stop weight (∞), set / remove it | partial | | shows "none"; 0 on the editor removes it; no ∞ glyph used |
| T5 | Sent 1 s after the last change, saved on the machine a few s later | partial | | sent on Set (saved: yes) |
| T6 | Value goes back if the machine does not take it | partial | | a "THE MACHINE DID NOT TAKE IT" note; the row shows the machine's value anyway |
| T7 | "edited" label and Reset to the original | missing | | Ledger keeps no originals |
| T8 | "Not saved: other unsaved changes (web UI)" banner | missing | | |
| T9 | Only while idle in HOME (refusal shown) | done | | the core refuses; Ledger shows the reason in the header |

## Profiles

| ID | Feature (Brewdeck) | Ledger | Panel | Notes |
|---|---|---|---|---|
| P1 | List of profiles, tap selects (and saves as boot profile) | done | yes | |
| P2 | Order: active first, recently used, then by name | missing | | machine order |
| P3 | Per row: stop weight, brew temp, last used, Community tag | partial | yes | temp and weight; no last used, no Community tag (it is in the name) |
| P4 | Originals per profile, Restore / Keep current | missing | | |
| P5 | Tapping the active profile just closes | partial | | sends the select again (harmless) |

## Shot

| ID | Feature (Brewdeck) | Ledger | Panel | Notes |
|---|---|---|---|---|
| B1 | Brewing screen opens at shot start | done | yes | |
| B2 | Live chart: pressure, flow, weight, temperature | partial | yes | pressure, flow, weight; no temperature line (value in the legend) |
| B3 | Dashed target pressure / flow | missing | | |
| B4 | Weight with the stop weight (10.3 / 21.0 g), shot time | done | yes | stop weight also as a dotted line |
| B5 | Weight drawn as a ramp between slow scale readings | missing | | staircase |
| B6 | Time axis grows in steps (redraw only the new strip otherwise) | done | | 40 s, +15 s; growth not yet seen on the panel |
| B7 | End: review opens 3 s later | partial | yes | "DONE · g IN s" on the same screen; tap or 60 s back to Ready (60 s not seen on the panel) |

## Shot review and history

| ID | Feature (Brewdeck) | Ledger | Panel | Notes |
|---|---|---|---|---|
| V1 | Review: whole shot, Target / Previous comparison | missing | | |
| V2 | Previous shot of the same profile loaded after the shot | missing | | core: bd_load_previous_shot |
| V3 | Scrubber with values at a moment | missing | | |
| V4 | Close / 60 s untouched | partial | | the brewing screen closes on tap / 60 s |
| V5 | Shot history (newest 50), tap to review | missing | | core: bd_shot_history, bd_load_stored_shot |

## Steam, flush, notices

| ID | Feature (Brewdeck) | Ledger | Panel | Notes |
|---|---|---|---|---|
| M1 | Steam screen: boiler vs set point, pressure, chart from steam start | done | | rows + chart; not yet seen on the panel |
| M2 | Steam: Refill warning when water is low | missing | | water value shown, not highlighted |
| F1 | Tap Flush: flush mode on / off | done | yes | red "Flush on" |
| F2 | Hold Flush: timed flush | done | yes | red "Flushing"; no fill while holding, no seconds count |
| N1 | Pressure-release warning (glowing edges, pulses on each beat) | missing | | core: BD_EV_PRESSURE_RELEASE |
| N2 | Machine notices as a 5 s banner, bell with the last 10 | missing | | core: BD_EV_NOTICE |
| N3 | Update available notice, kept in About | missing | | |

## Standby

| ID | Feature (Brewdeck) | Ledger | Panel | Notes |
|---|---|---|---|---|
| I1 | Idle clock after "Dim after" (default 10 min), dimmed | done | | 10 min fixed (no setting); clock, boiler and state at 50 % backlight; the first touch only wakes |
| I2 | Still / Bouncing idle clock | partial | | still only: moves up to 24 px every 2 min against retention |
| I3 | Screen off after 30 min, or 2 min on the offline screen | done | | 2 min untouched while the machine is unreachable |
| I4 | Wake on touch, shot, steam, flush, notice, machine back | done | | bd_ui_wake + a running shot / steam / flush; machine back only when it went off because the machine was away |

## Menu and settings

| ID | Feature (Brewdeck) | Ledger | Panel | Notes |
|---|---|---|---|---|
| U1 | Menu | missing | | |
| U2 | Scale page | missing | | |
| U3 | Display: brightness, Dim after, idle clock, splash | missing | | |
| U4 | About: versions, uptime, restart reason, memory, machine versions | missing | | splash shows "VERSION 0.1" only |

## Wi-Fi and offline

| ID | Feature (Brewdeck) | Ledger | Panel | Notes |
|---|---|---|---|---|
| W1 | Wi-Fi list, join with an on-screen keyboard | missing | | Ledger can only use a network already saved (by Brewdeck or wifi_config.h) |
| W2 | Network details, Forget | missing | | |
| W3 | Machine address (name or IP) | missing | | |
| O1 | Offline screen after 5 s: what it is doing, then after 20 s what is wrong, with buttons | partial | yes | header word only; values become dashes; no buttons, no help |
| O2 | Never covers a running shot or the review | done | | there is no offline screen to cover them |

## Summary

Done or mostly done: start-up, Ready values, profile pick, brew temp / stop weight editing (basic), the live shot,
steam, flush, standby (dim / screen off). Missing, roughly by how much a daily user would miss them: offline help and Wi-Fi setup (needed by anyone who is not already set up), notices and the
pressure-release warning, the shot review with the previous shot, shot history, the menu (display settings, about),
profile originals.

## Towards standard UI tests (to design later)

Every UI on the core should pass the same list. Ideas:
- Use the IDs above (plus Brewdeck's own) as the list of behaviours; a UI declares which it implements.
- Scripted emulator scenarios (the emulator already takes `tap`, `hold`, `key`, `button`, `wait`, `save` on stdin
  and has a fake machine with recorded data): one script per behaviour, checking the core side (what command was
  sent, the `machine <-` lines in the emulator log) rather than pixels, so the same script works for any UI layout.
- What the UI must expose for that: a way to find its controls without coordinates (e.g. a small `bd_debug_route`
  that lists named buttons and their positions, or a naming convention for LVGL objects).
- Panel checks that the emulator cannot do (touch feel, banding, frame times): a short manual checklist per release,
  plus `tools/record.sh` recordings of a shot and a steam session.
