// Ledger: a light, printed-looking UI for Gaggiuino machines, on the Brewdeck core (LVGL 9).
// Paper background, black ink, one red accent; the one number that matters huge, the rest as a ledger of rows
// with dotted leaders. Screens: Ready (temperature, settings, the profile's plan), Brewing (time, weight, chart),
// Profiles (pick one), an editor for brew temperature / stop weight, and a splash with the not-affiliated line.
// Everything runs on the UI task (see brewdeck_core.h). Build: ./emu.sh (emulator) or ./fw.sh <env> (firmware).
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bd_lvgl9.h"

extern "C" const bd_ui_desc BD_UI = {"ledger", "0.1", 1};

LV_FONT_DECLARE(lg_c300_220);   // digits . - : only
LV_FONT_DECLARE(lg_c300_160);
LV_FONT_DECLARE(lg_c600_160);
LV_FONT_DECLARE(lg_400_36);
LV_FONT_DECLARE(lg_400_17);
LV_FONT_DECLARE(lg_600_17);
LV_FONT_DECLARE(lg_600_24);
LV_FONT_DECLARE(lg_mono_14);
LV_FONT_DECLARE(lg_mono_13);

#define PAPER lv_color_hex(0xEEEDE8)
#define INK lv_color_hex(0x17181A)
#define MUTED lv_color_hex(0x5F6064)
#define ACCENT lv_color_hex(0xC23B1C)
#define LEAD_STRONG lv_color_hex(0xA9AAA4)
#define LEAD_SOFT lv_color_hex(0xC9CAC4)
#define GRID lv_color_hex(0xD2D3CD)
#define PRESSED lv_color_hex(0xDDDBD3)

static const char *DEG = "\xC2\xB0";      // degree sign
static const char *DOT = " \xC2\xB7 ";    // middle dot
static const char *DASH = "\xE2\x80\x94"; // em dash

// ---- small helpers -------------------------------------------------------------------------------------------------
static lv_obj_t *layer(lv_obj_t *parent) {   // a full-screen page
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, 800, 480);
  lv_obj_set_style_bg_color(o, PAPER, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}
static lv_obj_t *text(lv_obj_t *p, const lv_font_t *f, lv_color_t c, const char *t) {
  lv_obj_t *l = lv_label_create(p);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, c, 0);
  if (f == &lg_mono_14 || f == &lg_mono_13) lv_obj_set_style_text_letter_space(l, 2, 0);
  lv_label_set_text(l, t);
  return l;
}
static lv_obj_t *rule(lv_obj_t *p, int x, int y, int w, int h, lv_color_t c) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  return o;
}
static lv_obj_t *row_box(lv_obj_t *p) {   // a flex row with no style (number + unit)
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(o, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}
// an outlined button: 2 px ink border, square corners, 48 px tall
static lv_obj_t *button(lv_obj_t *p, const char *t, lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = lv_obj_create(p);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, LV_SIZE_CONTENT, 48);
  lv_obj_set_style_pad_hor(b, 22, 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_border_color(b, INK, 0);
  lv_obj_set_style_bg_color(b, PRESSED, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *l = text(b, &lg_600_17, INK, t);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}
static void set_text(lv_obj_t *l, const char *t) {   // only when it changed (a label redraw is not free)
  if (strcmp(lv_label_get_text(l), t)) lv_label_set_text(l, t);
}
static void upper(char *dst, size_t cap, const char *src) {
  size_t i = 0;
  for (; src[i] && i + 1 < cap; i++) dst[i] = (src[i] >= 'a' && src[i] <= 'z') ? src[i] - 32 : src[i];
  dst[i] = 0;
}

// one line, "..." when too long: LVGL's dots mode still wraps unless the label is exactly one line tall
static void one_line(lv_obj_t *l, int max_w) {
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_style_max_width(l, max_w, 0);
  lv_obj_set_height(l, lv_font_get_line_height(lv_obj_get_style_text_font(l, LV_PART_MAIN)));
}

// the header of every page: a state word in the accent on the left, mono text on the right, a 2 px rule under them
typedef struct { lv_obj_t *left, *right; } Header;
static Header header(lv_obj_t *p) {
  Header h;
  h.left = text(p, &lg_mono_14, ACCENT, "");
  lv_obj_set_pos(h.left, 24, 24);
  h.right = text(p, &lg_mono_14, INK, "");
  lv_obj_align(h.right, LV_ALIGN_TOP_RIGHT, -24, 24);
  rule(p, 24, 52, 752, 2, INK);
  return h;
}

// a ledger row: name ..... value (the dotted leader fills the space between them)
typedef struct { lv_obj_t *row, *name, *val, *lead; lv_point_precise_t pts[2]; int w; } Row;
static void row_fit(Row *r) {
  lv_obj_update_layout(r->row);
  const int x1 = lv_obj_get_width(r->name) + 8, x2 = r->w - lv_obj_get_width(r->val) - 8;
  r->pts[0].x = x1; r->pts[1].x = x2 > x1 ? x2 : x1;
  r->pts[0].y = r->pts[1].y = 0;
  lv_line_set_points(r->lead, r->pts, 2);
}
static void row_init(Row *r, lv_obj_t *p, const char *name, bool strong, lv_event_cb_t cb) {
  r->w = 320;
  r->row = lv_obj_create(p);
  lv_obj_remove_style_all(r->row);
  lv_obj_set_size(r->row, 320, 40);
  lv_obj_remove_flag(r->row, LV_OBJ_FLAG_SCROLLABLE);
  r->name = text(r->row, &lg_400_17, strong ? INK : MUTED, name);
  lv_obj_align(r->name, LV_ALIGN_LEFT_MID, 0, 0);
  r->val = text(r->row, strong ? &lg_600_17 : &lg_400_17, strong ? INK : MUTED, "");
  lv_obj_align(r->val, LV_ALIGN_RIGHT_MID, 0, 0);
  r->lead = lv_line_create(r->row);
  lv_obj_set_style_line_color(r->lead, strong ? LEAD_STRONG : LEAD_SOFT, 0);
  lv_obj_set_style_line_width(r->lead, 2, 0);
  lv_obj_set_style_line_dash_width(r->lead, 2, 0);
  lv_obj_set_style_line_dash_gap(r->lead, 3, 0);
  lv_obj_set_pos(r->lead, 0, 27);
  if (cb) {
    lv_obj_add_flag(r->row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(r->row, PRESSED, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r->row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(r->row, cb, LV_EVENT_CLICKED, NULL);
  }
  row_fit(r);
}
static void row_set(Row *r, const char *v) {
  if (!strcmp(lv_label_get_text(r->val), v)) return;
  lv_label_set_text(r->val, v);
  row_fit(r);
}

// the pressure sensor reads a little below 0 at rest (-0.02 printed as "-0.0"); not a number = 0 too
static float bar(float p) { return p > 0 ? (p < 20 ? p : 20) : 0; }

// ---- state shared by the pages -------------------------------------------------------------------------------------
static lv_obj_t *g_ready, *g_brew, *g_prof, *g_edit, *g_splash;
static uint32_t g_now, g_splash_until, g_brew_done_at;
static bool g_brewing_shown;

static const char *state_word(const bd_machine *m) {
  if (m->link != BD_LINK_ONLINE) {   // why it is offline, once the core knows
    switch (m->why) {
      case BD_WHY_WIFI_NO_AP: case BD_WHY_WIFI_AUTH: case BD_WHY_WIFI_OTHER: return "NO WI-FI";
      case BD_WHY_NAME_UNKNOWN: case BD_WHY_NO_REPLY: return "MACHINE NOT FOUND";
      default: return m->link == BD_LINK_NO_MACHINE ? "MACHINE NOT FOUND" : "CONNECTING";
    }
  }
  if (m->brewing) return "BREWING";
  if (m->steaming) return "STEAM";
  if (m->hot_water) return "HOT WATER";
  // flushing comes from the mode only: the machine reports readiness FLUSHING for the whole flush mode (water
  // running or not) and keeps it for a moment after the mode is back to HOME
  if (!strcmp(m->mode, "FLUSH_AUTO")) return "FLUSHING";
  if (!strcmp(m->mode, "FLUSH")) return "FLUSH MODE \xC2\xB7 THE BREW SWITCH FLUSHES";
  switch (m->readiness) {
    case BD_RD_STEAMING: return "STEAM";
    case BD_RD_FLUSHING: return m->temperature >= m->target_temp - 1 ? "READY" : "HEATING";   // see above
    case BD_RD_READY: case BD_RD_HEATING:   // the boiler well above the target (after steam): cooling down
      if (m->target_temp > 0 && m->temperature > m->target_temp + 3) return "COOLING";
      return m->readiness == BD_RD_READY ? "READY" : "HEATING";
    case BD_RD_WARMING: return "WARMING UP";
    case BD_RD_STANDBY: return "STANDBY";
    case BD_RD_WAITING: return "WAITING";
    case BD_RD_MAINTENANCE: return "MAINTENANCE";
    default: return m->temperature >= m->target_temp - 1 ? "READY" : "HEATING";
  }
}
static float cur_stop(const bd_machine *m) {
  if (m->running.name[0]) return m->running.stop_weight;
  return bd_active_shape()->id ? bd_active_shape()->stop_weight : 0;
}
static float cur_temp(const bd_machine *m) {
  if (m->running.name[0] && m->running.brew_temp > 0) return m->running.brew_temp;
  return bd_active_shape()->id ? bd_active_shape()->brew_temp : m->target_temp;
}
static const char *result_text(bd_result r) {
  switch (r) {
    case BD_OK: return "SENT";
    case BD_ERR_OFFLINE: return "MACHINE OFFLINE";
    case BD_ERR_BUSY: return "MACHINE BUSY";
    case BD_ERR_RANGE: return "OUT OF RANGE";
    case BD_ERR_PROFILE_CHANGED: return "PROFILE CHANGED";
    case BD_ERR_PENDING: return "STILL WAITING";
    default: return "REFUSED";
  }
}

// ---- Ready ---------------------------------------------------------------------------------------------------------
static Header g_rh, g_sh, g_oh;   // Ready, Steam and Offline headers
static lv_obj_t *g_temp, *g_temp_unit, *g_plan, *g_flush_btn, *g_flush_lbl, *g_note;
static Row g_r_prof, g_r_temp, g_r_stop, g_r_press, g_r_scale, g_r_water;
static bd_preview *g_pv;   // the active profile's plan (PSRAM)
static bool g_pv_ok;
static uint32_t g_note_until;

static void note(const char *t) {   // a short message in the Ready header for 3 s (a refused command)
  set_text(g_note, t);
  lv_obj_remove_flag(g_note, LV_OBJ_FLAG_HIDDEN);
  g_note_until = g_now + 3000;
}

#define PLAN_W 400
static void plan_draw(lv_event_t *e) {
  lv_layer_t *L = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(lv_event_get_target_obj(e), &a);
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = INK;
  d.width = 1;
  d.p1.x = a.x1; d.p1.y = a.y1 + 110; d.p2.x = a.x1 + PLAN_W - 1; d.p2.y = a.y1 + 110;
  lv_draw_line(L, &d);
  for (int x = 0; x < PLAN_W; x += PLAN_W / 4) { d.p1.x = d.p2.x = a.x1 + x; d.p1.y = a.y1 + 104; d.p2.y = a.y1 + 110; lv_draw_line(L, &d); }
  if (!g_pv_ok) return;
  for (int k = 0; k < BD_PV_SERIES; k++) {
    if (k == BD_PV_PRESSURE_LIMIT || k == BD_PV_FLOW_LIMIT) continue;   // what a phase drives, not its limits
    const bd_preview_series *s = &g_pv->s[k];
    const bool flow = k == BD_PV_FLOW;
    d.width = flow ? 2 : 3;
    d.round_start = d.round_end = !flow;
    for (int g = 0; g < s->nseg; g++) {
      float phase = 0;   // dash pattern 6 on / 5 off along the flow line
      for (int i = s->seg[g] + 1; i < s->seg[g + 1]; i++) {
        const float x0 = s->pts[i - 1].x, y0 = s->pts[i - 1].y, x1 = s->pts[i].x, y1 = s->pts[i].y;
        if (!flow) {
          d.p1.x = a.x1 + x0; d.p1.y = a.y1 + y0; d.p2.x = a.x1 + x1; d.p2.y = a.y1 + y1;
          lv_draw_line(L, &d);
          continue;
        }
        const float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
        for (float t = 0; t < len;) {
          const float step = phase < 6 ? 6 - phase : 11 - phase, e2 = t + step < len ? t + step : len;
          if (phase < 6) {
            d.p1.x = a.x1 + x0 + (x1 - x0) * t / len; d.p1.y = a.y1 + y0 + (y1 - y0) * t / len;
            d.p2.x = a.x1 + x0 + (x1 - x0) * e2 / len; d.p2.y = a.y1 + y0 + (y1 - y0) * e2 / len;
            lv_draw_line(L, &d);
          }
          phase += e2 - t;
          if (phase >= 11) phase -= 11;
          t = e2;
        }
      }
    }
  }
}

static void open_profiles(lv_event_t *e);
static void open_settings(lv_event_t *e);
static void open_edit_temp(lv_event_t *e);
static void open_edit_stop(lv_event_t *e);
// tap: flush mode on / off; hold: the machine's timed flush. The tap is SHORT_CLICKED: LVGL also sends CLICKED on
// the release after a hold, which turned the timed flush straight off again.
static void on_flush(lv_event_t *e) {
  const bd_machine *m = bd_machine_state();
  const bool on = !strncmp(m->mode, "FLUSH", 5);
  const bd_result r = bd_flush_mode(lv_event_get_code(e) == LV_EVENT_LONG_PRESSED ? BD_FLUSH_TIMED : (on ? BD_FLUSH_OFF : BD_FLUSH_ON));
  if (r != BD_OK) note(result_text(r));
}

static void build_ready(lv_obj_t *scr) {
  g_ready = layer(scr);
  g_rh = header(g_ready);
  // SETTINGS at the right of the header; the touch area covers the header's height
  lv_obj_t *sb = lv_obj_create(g_ready);
  lv_obj_remove_style_all(sb);
  lv_obj_set_size(sb, LV_SIZE_CONTENT, 50);
  lv_obj_set_style_pad_hor(sb, 12, 0);
  lv_obj_align(sb, LV_ALIGN_TOP_RIGHT, -12, 1);
  lv_obj_set_style_bg_color(sb, PRESSED, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(sb, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_add_flag(sb, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(sb, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(sb, open_settings, LV_EVENT_CLICKED, NULL);
  lv_obj_t *sl = text(sb, &lg_mono_14, INK, "SETTINGS");
  lv_obj_set_style_text_decor(sl, LV_TEXT_DECOR_UNDERLINE, 0);
  lv_obj_set_pos(sl, 0, 23);   // the same line as the clock (header text at y 24)
  lv_obj_update_layout(sb);
  lv_obj_align(g_rh.right, LV_ALIGN_TOP_RIGHT, -(12 + lv_obj_get_width(sb) + 4), 24);   // kept right-aligned as it changes
  g_note = text(g_ready, &lg_mono_14, ACCENT, "");
  lv_obj_set_pos(g_note, 260, 24);
  lv_obj_add_flag(g_note, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *t = row_box(g_ready);
  lv_obj_set_pos(t, 18, 58);
  g_temp = text(t, &lg_c300_220, INK, "--");
  lv_obj_set_style_text_letter_space(g_temp, -4, 0);
  g_temp_unit = text(t, &lg_400_36, INK, "\xC2\xB0" "C");
  lv_obj_set_style_pad_top(g_temp_unit, 36, 0);
  lv_obj_set_style_pad_left(g_temp_unit, 4, 0);

  lv_obj_t *col = lv_obj_create(g_ready);
  lv_obj_remove_style_all(col);
  lv_obj_set_pos(col, 456, 74);
  lv_obj_set_size(col, 320, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  row_init(&g_r_prof, col, "Profile", true, open_profiles);
  one_line(g_r_prof.val, 230);
  row_init(&g_r_temp, col, "Brew temp", true, open_edit_temp);
  row_init(&g_r_stop, col, "Stop weight", true, open_edit_stop);
  row_init(&g_r_press, col, "Pressure", false, NULL);
  row_init(&g_r_scale, col, "Scale", false, NULL);
  row_init(&g_r_water, col, "Water", false, NULL);

  lv_obj_set_pos(text(g_ready, &lg_mono_13, MUTED, "PLAN" " \xC2\xB7 " "PRESSURE \xE2\x80\x94  FLOW - -"), 24, 306);
  g_plan = lv_obj_create(g_ready);
  lv_obj_remove_style_all(g_plan);
  lv_obj_set_pos(g_plan, 24, 336);
  lv_obj_set_size(g_plan, PLAN_W, 120);
  lv_obj_remove_flag(g_plan, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_plan, plan_draw, LV_EVENT_DRAW_MAIN, NULL);
  g_pv = (bd_preview *)bd_alloc(sizeof(bd_preview), BD_MEM_PSRAM);

  lv_obj_t *bp = button(g_ready, "Profiles", open_profiles, NULL);
  lv_obj_align(bp, LV_ALIGN_BOTTOM_RIGHT, -24, -24);
  lv_obj_t *bf = button(g_ready, "Flush", NULL, NULL);
  lv_obj_set_width(bf, 132);   // fixed: "Flushing" must not move it
  lv_obj_add_event_cb(bf, on_flush, LV_EVENT_SHORT_CLICKED, NULL);
  lv_obj_add_event_cb(bf, on_flush, LV_EVENT_LONG_PRESSED, NULL);
  g_flush_btn = bf;
  g_flush_lbl = lv_obj_get_child(bf, 0);
  lv_obj_align_to(bf, bp, LV_ALIGN_OUT_LEFT_MID, -16, 0);
}

static void update_ready(const bd_machine *m) {
  char b[64];
  set_text(g_rh.left, state_word(m));
  const bool on = m->link == BD_LINK_ONLINE;
  if (on) snprintf(b, sizeof b, "%.1f", m->temperature); else snprintf(b, sizeof b, "--");
  set_text(g_temp, b);
  if (on) lv_obj_remove_flag(g_temp_unit, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(g_temp_unit, LV_OBJ_FLAG_HIDDEN);
  const char *name = m->running.name[0] ? m->running.name : m->profile_name;
  row_set(&g_r_prof, name[0] ? name : DASH);
  const float bt = cur_temp(m), sw = cur_stop(m);
  if (bt > 0) snprintf(b, sizeof b, "%.0f %sC", bt, DEG); else snprintf(b, sizeof b, "%s", DASH);
  row_set(&g_r_temp, b);
  if (sw > 0) snprintf(b, sizeof b, "%.1f g", sw);
  else snprintf(b, sizeof b, "%s", on && bd_active_shape()->id ? "none" : DASH);   // "none" only once it is known
  row_set(&g_r_stop, b);
  if (on) snprintf(b, sizeof b, "%.1f bar", bar(m->pressure)); else snprintf(b, sizeof b, "%s", DASH);
  row_set(&g_r_press, b);
  if (on && m->scale_present) snprintf(b, sizeof b, "%.1f g", m->weight);
  else snprintf(b, sizeof b, "%s", on ? (m->scale_type == BD_SCALE_PREDICTIVE ? "predictive" : "not connected") : DASH);
  row_set(&g_r_scale, b);
  if (on) snprintf(b, sizeof b, "%d %%", m->water_level); else snprintf(b, sizeof b, "%s", DASH);
  row_set(&g_r_water, b);
  const bool mode_on = !strcmp(m->mode, "FLUSH"), flushing = !strcmp(m->mode, "FLUSH_AUTO");   // the mode only
  set_text(g_flush_lbl, mode_on ? "Flush on" : (flushing ? "Flushing" : "Flush"));
  static int red = -1;   // red (the accent) while flush mode is on or water runs, outlined otherwise
  if (red != (mode_on || flushing)) {
    red = mode_on || flushing;
    lv_obj_set_style_bg_color(g_flush_btn, ACCENT, 0);
    lv_obj_set_style_bg_opa(g_flush_btn, red ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(g_flush_btn, red ? ACCENT : INK, 0);
    lv_obj_set_style_text_color(g_flush_lbl, red ? PAPER : INK, 0);
    lv_obj_set_style_bg_color(g_flush_btn, red ? lv_color_hex(0x9E2F16) : PRESSED, LV_STATE_PRESSED);
  }
}

// ---- Brewing -------------------------------------------------------------------------------------------------------
// chart 752 x 200 at (24, 250): baseline at 190; pressure 18 px / bar (10 bar = 180 px), flow 36 px per ml/s,
// weight scaled so the stop weight sits at 80 % of the height; time 40 s wide, then +15 s steps
#define CH_W 752
#define CH_H 200
#define CH_BASE 190
static Header g_bh;
static lv_obj_t *g_time, *g_weight, *g_wunit, *g_chart, *g_lg_p, *g_lg_f, *g_lg_t;
static int g_span = 40, g_last_x = -1;
static float g_stop;

static int cx(int t10) { return (int)(t10 * (CH_W - 1) / (g_span * 10.0f) + 0.5f); }
static float wmax(void) { return g_stop > 0 ? g_stop / 0.8f : 50; }

static void chart_draw(lv_event_t *e) {
  lv_layer_t *L = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(lv_event_get_target_obj(e), &a);
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.width = 1;
  d.color = GRID;
  for (int bar = 3; bar <= 9; bar += 3) {
    d.p1.x = a.x1; d.p2.x = a.x1 + CH_W - 1; d.p1.y = d.p2.y = a.y1 + CH_BASE - bar * 18;
    lv_draw_line(L, &d);
  }
  d.color = INK;
  d.p1.y = d.p2.y = a.y1 + CH_BASE;
  lv_draw_line(L, &d);
  if (g_stop > 0) {   // the stop weight: a dotted accent line
    d.color = ACCENT;
    const int y = a.y1 + CH_BASE - (int)(g_stop / wmax() * 180);
    for (int x = 0; x < CH_W; x += 8) { d.p1.x = a.x1 + x; d.p2.x = a.x1 + x + 2; d.p1.y = d.p2.y = y; lv_draw_line(L, &d); }
  }
  const bd_shot *s = bd_live_shot();
  if (!s || s->n < 2) return;
  const lv_area_t *clip = &L->_clip_area;
  for (int k = 0; k < 3; k++) {   // 0 flow (dashed look: thinner, muted), 1 pressure, 2 weight
    d.color = k == 0 ? MUTED : (k == 1 ? INK : ACCENT);
    d.width = k == 0 ? 2 : 3;
    d.round_start = d.round_end = 1;
    int px = -1, py = 0;
    for (int i = 0; i < s->n; i++) {
      const bd_shot_point *p = &s->pt[i];
      const int x = cx(p->t);
      int y;
      if (k == 0) y = CH_BASE - (int)(p->f * 3.6f);
      else if (k == 1) y = CH_BASE - (int)(p->p * 1.8f);
      else y = CH_BASE - (int)(p->w / 10.0f / wmax() * 180);
      if (y < 0) y = 0;
      if (px >= 0 && !(x < clip->x1 - a.x1 - 8 || px > clip->x2 - a.x1 + 8)) {
        d.p1.x = a.x1 + px; d.p1.y = a.y1 + py; d.p2.x = a.x1 + x; d.p2.y = a.y1 + y;
        lv_draw_line(L, &d);
      }
      px = x; py = y;
    }
  }
  d.color = INK;   // "now": a dotted vertical line at the newest point
  d.width = 1;
  const int nx = a.x1 + cx(s->pt[s->n - 1].t);
  for (int y = 0; y < CH_BASE; y += 6) { d.p1.x = d.p2.x = nx; d.p1.y = a.y1 + y; d.p2.y = a.y1 + y + 2; lv_draw_line(L, &d); }
}

static void brew_close(lv_event_t *e) {
  (void)e;
  if (!g_brew_done_at) return;   // a tap during the shot does nothing
  lv_obj_add_flag(g_brew, LV_OBJ_FLAG_HIDDEN);
  g_brewing_shown = false;
}

static void build_brew(lv_obj_t *scr) {
  g_brew = layer(scr);
  lv_obj_add_flag(g_brew, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_brew, brew_close, LV_EVENT_CLICKED, NULL);
  g_bh = header(g_brew);
  lv_obj_t *t = row_box(g_brew);
  lv_obj_set_pos(t, 18, 56);
  g_time = text(t, &lg_c300_160, INK, "0.0");
  lv_obj_set_style_text_letter_space(g_time, -3, 0);
  lv_obj_t *u = text(t, &lg_400_36, INK, " s");
  lv_obj_set_style_pad_top(u, 96, 0);
  lv_obj_t *w = row_box(g_brew);
  lv_obj_align(w, LV_ALIGN_TOP_RIGHT, -24, 56);
  g_weight = text(w, &lg_c600_160, ACCENT, "0.0");
  lv_obj_set_style_text_letter_space(g_weight, -3, 0);
  g_wunit = text(w, &lg_400_36, INK, " g");
  lv_obj_set_style_pad_top(g_wunit, 96, 0);
  g_chart = lv_obj_create(g_brew);
  lv_obj_remove_style_all(g_chart);
  lv_obj_set_pos(g_chart, 24, 250);
  lv_obj_set_size(g_chart, CH_W, CH_H);
  lv_obj_remove_flag(g_chart, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_chart, chart_draw, LV_EVENT_DRAW_MAIN, NULL);
  g_lg_p = text(g_brew, &lg_mono_13, INK, "");
  lv_obj_set_pos(g_lg_p, 24, 450);
  g_lg_f = text(g_brew, &lg_mono_13, MUTED, "");
  lv_obj_set_pos(g_lg_f, 260, 450);
  lv_obj_set_pos(text(g_brew, &lg_mono_13, ACCENT, "\xE2\x80\x94 WEIGHT"), 470, 450);
  g_lg_t = text(g_brew, &lg_mono_13, INK, "");
  lv_obj_align(g_lg_t, LV_ALIGN_TOP_RIGHT, -24, 450);
  lv_obj_add_flag(g_brew, LV_OBJ_FLAG_HIDDEN);
}

static void brew_open(void) {
  const bd_machine *m = bd_machine_state();
  char b[64];
  g_stop = cur_stop(m);
  g_span = 40;
  g_last_x = -1;
  g_brew_done_at = 0;
  set_text(g_bh.left, "BREWING");
  upper(b, sizeof b, m->running.name[0] ? m->running.name : m->profile_name);
  set_text(g_bh.right, b);
  if (g_stop > 0) snprintf(b, sizeof b, " / %.1f g", g_stop); else snprintf(b, sizeof b, " g");
  set_text(g_wunit, b);
  set_text(g_time, "0.0");
  set_text(g_weight, "0.0");
  lv_obj_remove_flag(g_brew, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(g_brew, -1);
  if (g_splash) lv_obj_move_to_index(g_splash, -1);
  lv_obj_invalidate(g_chart);
  g_brewing_shown = true;
}

static void brew_point(const bd_shot_point *p) {
  char b[48];
  snprintf(b, sizeof b, "%.1f", p->t / 10.0f);
  set_text(g_time, b);
  snprintf(b, sizeof b, "%.1f", p->w / 10.0f);
  set_text(g_weight, b);
  snprintf(b, sizeof b, "\xE2\x80\x94 PRESSURE %.1f BAR", p->p / 10.0f);
  set_text(g_lg_p, b);
  snprintf(b, sizeof b, "\xE2\x80\x94 FLOW %.1f ML/S", p->f / 10.0f);
  set_text(g_lg_f, b);
  snprintf(b, sizeof b, "%.1f %sC", p->temp / 10.0f, DEG);
  set_text(g_lg_t, b);
  if (p->t > (g_span - 2) * 10) {   // near the right edge: grow the time axis (a full redraw, so in steps)
    while (p->t > (g_span - 2) * 10) g_span += 15;
    g_last_x = -1;
    lv_obj_invalidate(g_chart);
    return;
  }
  const int x = cx(p->t);   // redraw only the strip from the last point to this one
  lv_area_t a;
  lv_obj_get_coords(g_chart, &a);
  const int x0 = g_last_x < 0 ? 0 : g_last_x;
  lv_area_t r = {a.x1 + x0 - 10, a.y1, a.x1 + x + 10, a.y2};
  lv_obj_invalidate_area(g_chart, &r);
  g_last_x = x;
}

static void brew_end(void) {
  const bd_shot *s = bd_live_shot();
  char b[48];
  if (s && s->n) snprintf(b, sizeof b, "DONE%s%.1f G IN %.1f S", DOT, s->pt[s->n - 1].w / 10.0f, s->dur10 / 10.0f);
  else snprintf(b, sizeof b, "DONE");
  set_text(g_bh.left, b);
  g_brew_done_at = g_now ? g_now : 1;
}

// ---- Steam ---------------------------------------------------------------------------------------------------------
// Opens while the machine steams, over Ready (under Brewing and the pages). Boiler temperature huge, rows for set
// point, pressure, time and water, and a chart of the session: boiler (ink), pressure (grey), set point (dotted red).
// One sample per second; the time axis is 1 min, then grows in 30 s steps (a full redraw only then).
#define ST_W 752
#define ST_H 150
#define ST_BASE 140
#define ST_MAX 900   // 15 min of samples
static lv_obj_t *g_steam, *g_st_temp, *g_st_chart;
static Row g_r_set, g_r_spress, g_r_time, g_r_swater;
static int16_t g_st_t[ST_MAX], g_st_p[ST_MAX];   // degC x10, bar x10
static int g_st_n, g_st_span = 60, g_st_set;
static uint32_t g_st_start, g_st_next;
static bool g_steam_shown;

static float st_lo(void) { return 80; }
static float st_hi(void) { return (g_st_set > 0 ? g_st_set : 155) + 15; }
static int st_x(int i) { return (int)(i * (ST_W - 1) / (float)g_st_span + 0.5f); }
static int st_ty(int t10) {
  const float y = ST_BASE - (t10 / 10.0f - st_lo()) * (ST_BASE - 4) / (st_hi() - st_lo());
  return y < 0 ? 0 : (y > ST_BASE ? ST_BASE : (int)(y + 0.5f));
}
static int st_py(int p10) {   // 0-15 bar over the same height
  const float y = ST_BASE - p10 / 150.0f * (ST_BASE - 4);
  return y < 0 ? 0 : (int)(y + 0.5f);
}

static void steam_draw(lv_event_t *e) {
  lv_layer_t *L = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(lv_event_get_target_obj(e), &a);
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.width = 1;
  d.color = INK;
  d.p1.x = a.x1; d.p2.x = a.x1 + ST_W - 1; d.p1.y = d.p2.y = a.y1 + ST_BASE;
  lv_draw_line(L, &d);
  for (int sx = 0; sx <= g_st_span; sx += 30) {   // a tick every 30 s
    d.p1.x = d.p2.x = a.x1 + st_x(sx); d.p1.y = a.y1 + ST_BASE - 6; d.p2.y = a.y1 + ST_BASE;
    lv_draw_line(L, &d);
  }
  if (g_st_set > 0) {
    d.color = ACCENT;
    const int y = a.y1 + st_ty(g_st_set * 10);
    for (int x = 0; x < ST_W; x += 8) { d.p1.x = a.x1 + x; d.p2.x = a.x1 + x + 2; d.p1.y = d.p2.y = y; lv_draw_line(L, &d); }
  }
  if (g_st_n < 2) return;
  const lv_area_t *clip = &L->_clip_area;
  for (int k = 0; k < 2; k++) {   // 0 pressure (grey, under), 1 boiler (ink)
    d.color = k ? INK : MUTED;
    d.width = k ? 3 : 2;
    d.round_start = d.round_end = 1;
    for (int i = 1; i < g_st_n; i++) {
      const int x0 = st_x(i - 1), x1 = st_x(i);
      if (x1 < clip->x1 - a.x1 - 8 || x0 > clip->x2 - a.x1 + 8) continue;
      d.p1.x = a.x1 + x0; d.p2.x = a.x1 + x1;
      d.p1.y = a.y1 + (k ? st_ty(g_st_t[i - 1]) : st_py(g_st_p[i - 1]));
      d.p2.y = a.y1 + (k ? st_ty(g_st_t[i]) : st_py(g_st_p[i]));
      lv_draw_line(L, &d);
    }
  }
}

static void build_steam(lv_obj_t *scr) {
  g_steam = layer(scr);
  g_sh = header(g_steam);
  set_text(g_sh.left, "STEAM");
  lv_obj_t *t = row_box(g_steam);
  lv_obj_set_pos(t, 18, 58);
  g_st_temp = text(t, &lg_c300_220, INK, "--");
  lv_obj_set_style_text_letter_space(g_st_temp, -4, 0);
  lv_obj_t *u = text(t, &lg_400_36, INK, "\xC2\xB0" "C");
  lv_obj_set_style_pad_top(u, 36, 0);
  lv_obj_set_style_pad_left(u, 4, 0);
  lv_obj_t *col = lv_obj_create(g_steam);
  lv_obj_remove_style_all(col);
  lv_obj_set_pos(col, 456, 74);
  lv_obj_set_size(col, 320, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  row_init(&g_r_set, col, "Set point", true, NULL);
  row_init(&g_r_time, col, "Time", true, NULL);
  row_init(&g_r_spress, col, "Pressure", false, NULL);
  row_init(&g_r_swater, col, "Water", false, NULL);
  lv_obj_set_pos(text(g_steam, &lg_mono_13, MUTED, "BOILER \xE2\x80\x94  PRESSURE \xE2\x80\x94  SET POINT \xC2\xB7 \xC2\xB7 \xC2\xB7"), 24, 278);
  g_st_chart = lv_obj_create(g_steam);
  lv_obj_remove_style_all(g_st_chart);
  lv_obj_set_pos(g_st_chart, 24, 306);
  lv_obj_set_size(g_st_chart, ST_W, ST_H);
  lv_obj_remove_flag(g_st_chart, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_st_chart, steam_draw, LV_EVENT_DRAW_MAIN, NULL);
  lv_obj_add_flag(g_steam, LV_OBJ_FLAG_HIDDEN);
}

static void steam_rows(const bd_machine *m) {
  char b[40];
  if (g_st_set > 0) {
    const bool holding = m->temperature >= g_st_set - 3;
    snprintf(b, sizeof b, "%s %d %sC", holding ? "holding" : "heating to", g_st_set, DEG);
  } else snprintf(b, sizeof b, "%s", DASH);
  row_set(&g_r_set, b);
  const uint32_t s = (g_now - g_st_start) / 1000;
  snprintf(b, sizeof b, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
  row_set(&g_r_time, b);
  snprintf(b, sizeof b, "%.1f bar", bar(m->pressure));
  row_set(&g_r_spress, b);
  snprintf(b, sizeof b, "%d %%", m->water_level);
  row_set(&g_r_swater, b);
  snprintf(b, sizeof b, "%.1f", m->temperature);
  set_text(g_st_temp, b);
}

static void steam_open(const bd_machine *m) {
  g_st_n = 0;
  g_st_span = 60;
  g_st_set = m->steam_set;
  g_st_start = g_now;
  g_st_next = g_now;
  steam_rows(m);
  lv_obj_remove_flag(g_steam, LV_OBJ_FLAG_HIDDEN);
  lv_obj_invalidate(g_st_chart);
  g_steam_shown = true;
}

// once per tick while steaming: a sample per second (missed seconds after a stall get the last value)
static void steam_step(const bd_machine *m) {
  if (m->steam_set != g_st_set) { g_st_set = m->steam_set; lv_obj_invalidate(g_st_chart); }
  if ((int32_t)(g_now - g_st_next) < 0) return;
  bool full = false;
  const int from = g_st_n;
  while ((int32_t)(g_now - g_st_next) >= 0 && g_st_n < ST_MAX) {
    g_st_t[g_st_n] = (int16_t)(m->temperature * 10);
    g_st_p[g_st_n] = (int16_t)(bar(m->pressure) * 10);
    g_st_n++;
    g_st_next += 1000;
  }
  if (g_st_n >= ST_MAX) g_st_next = g_now + 1000;
  while (g_st_n - 1 > g_st_span) { g_st_span += 30; full = true; }
  steam_rows(m);
  if (full) { lv_obj_invalidate(g_st_chart); return; }
  lv_area_t a;
  lv_obj_get_coords(g_st_chart, &a);
  lv_area_t r = {a.x1 + st_x(from > 0 ? from - 1 : 0) - 8, a.y1, a.x1 + st_x(g_st_n - 1) + 8, a.y2};
  lv_obj_invalidate_area(g_st_chart, &r);
}

static void steam_close(void) {
  lv_obj_add_flag(g_steam, LV_OBJ_FLAG_HIDDEN);
  g_steam_shown = false;
}

// ---- Profiles ------------------------------------------------------------------------------------------------------
static Header g_ph;
static lv_obj_t *g_plist;
static Row g_prows[BD_MAX_PROFILES];

static void close_layer(lv_event_t *e) { lv_obj_add_flag((lv_obj_t *)lv_event_get_user_data(e), LV_OBJ_FLAG_HIDDEN); }
static void on_pick(lv_event_t *e) {
  const int id = (int)(intptr_t)lv_event_get_user_data(e);
  const bd_result r = bd_select_profile(id, true);
  if (r == BD_OK) lv_obj_add_flag(g_prof, LV_OBJ_FLAG_HIDDEN);
  else set_text(g_ph.right, result_text(r));
}
static void fill_profiles(void) {
  lv_obj_clean(g_plist);
  const bd_profiles *l = bd_profile_list();
  char b[48];
  for (int i = 0; i < l->count; i++) {
    const bd_profile *p = &l->p[i];
    Row *r = &g_prows[i];
    row_init(r, g_plist, "", true, NULL);
    lv_obj_set_size(r->row, 752, 52);
    lv_label_set_text(r->name, p->name);
    if (p->active) {
      lv_obj_set_style_text_font(r->name, &lg_600_17, 0);
      lv_obj_set_style_text_color(r->name, ACCENT, 0);
    }
    one_line(r->name, 520);
    lv_obj_set_style_text_font(r->val, &lg_400_17, 0);
    if (p->detail && p->brew_temp > 0) {
      if (p->stop_weight > 0) snprintf(b, sizeof b, "%.0f %sC%s%.1f g", p->brew_temp, DEG, DOT, p->stop_weight);
      else snprintf(b, sizeof b, "%.0f %sC", p->brew_temp, DEG);
    } else b[0] = 0;
    lv_label_set_text(r->val, b);
    lv_obj_add_flag(r->row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(r->row, PRESSED, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r->row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(r->row, on_pick, LV_EVENT_CLICKED, (void *)(intptr_t)p->id);
    // leader across the wider row
    lv_obj_update_layout(r->row);
    const int x1 = lv_obj_get_width(r->name) + 8, x2 = 752 - lv_obj_get_width(r->val) - 8;
    r->pts[0].x = x1; r->pts[1].x = x2 > x1 ? x2 : x1; r->pts[0].y = r->pts[1].y = 0;
    lv_line_set_points(r->lead, r->pts, 2);
    lv_obj_set_pos(r->lead, 0, 33);
  }
  if (!l->count) lv_label_set_text(text(g_plist, &lg_400_17, MUTED, ""), "No profiles yet: they come once the machine answers.");
}
static void open_profiles(lv_event_t *e) {
  (void)e;
  set_text(g_ph.right, "");
  fill_profiles();
  lv_obj_remove_flag(g_prof, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(g_prof, -1);
}
static void build_profiles(lv_obj_t *scr) {
  g_prof = layer(scr);
  g_ph = header(g_prof);
  set_text(g_ph.left, "PROFILES");
  lv_obj_t *c = button(g_prof, "Close", close_layer, g_prof);
  lv_obj_align(c, LV_ALIGN_BOTTOM_RIGHT, -24, -24);
  g_plist = lv_obj_create(g_prof);
  lv_obj_remove_style_all(g_plist);
  lv_obj_set_pos(g_plist, 24, 64);
  lv_obj_set_size(g_plist, 752, 330);
  lv_obj_set_flex_flow(g_plist, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(g_prof, LV_OBJ_FLAG_HIDDEN);
}

// ---- Editor (brew temperature / stop weight) -----------------------------------------------------------------------
static Header g_eh;
static lv_obj_t *g_ev, *g_ehint;
static bool g_e_temp;
static float g_e_val;

static void edit_show(void) {
  char b[16];
  if (g_e_temp) snprintf(b, sizeof b, "%.0f", g_e_val); else snprintf(b, sizeof b, "%.1f", g_e_val);
  set_text(g_ev, b);
}
static void edit_open(bool temp) {
  const bd_machine *m = bd_machine_state();
  g_e_temp = temp;
  g_e_val = temp ? cur_temp(m) : cur_stop(m);
  if (temp && g_e_val < BD_BREW_TEMP_MIN) g_e_val = 93;
  set_text(g_eh.left, temp ? "BREW TEMP" : "STOP WEIGHT");
  set_text(g_eh.right, "");
  set_text(g_ehint, temp ? "1 \xE2\x80\x93 105 \xC2\xB0" "C" : "0 \xE2\x80\x93 200 g" " \xC2\xB7 " "0 = NO STOP WEIGHT");
  edit_show();
  lv_obj_remove_flag(g_edit, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(g_edit, -1);
}
static void open_edit_temp(lv_event_t *e) { (void)e; edit_open(true); }
static void open_edit_stop(lv_event_t *e) { (void)e; edit_open(false); }
static void on_step(lv_event_t *e) {
  const int dir = (int)(intptr_t)lv_event_get_user_data(e);
  const float step = g_e_temp ? 1 : 0.5f, lo = g_e_temp ? BD_BREW_TEMP_MIN : 0, hi = g_e_temp ? BD_BREW_TEMP_MAX : BD_STOP_WEIGHT_MAX;
  g_e_val += dir * step;
  if (g_e_val < lo) g_e_val = lo;
  if (g_e_val > hi) g_e_val = hi;
  edit_show();
}
static void on_set(lv_event_t *e) {
  (void)e;
  const bd_machine *m = bd_machine_state();
  const bd_result r = g_e_temp ? bd_set_brew_temp(m->profile_id, (int)g_e_val, true)
                               : bd_set_stop_weight(m->profile_id, g_e_val, true);
  if (r == BD_OK) lv_obj_add_flag(g_edit, LV_OBJ_FLAG_HIDDEN);
  else set_text(g_eh.right, result_text(r));
}
static lv_obj_t *step_button(lv_obj_t *p, const char *t, int dir) {
  lv_obj_t *b = button(p, t, on_step, (void *)(intptr_t)dir);
  lv_obj_set_size(b, 112, 112);
  lv_obj_set_style_pad_hor(b, 0, 0);
  lv_obj_set_style_text_font(lv_obj_get_child(b, 0), &lg_400_36, 0);
  lv_obj_add_event_cb(b, on_step, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)dir);
  return b;
}
static void build_edit(lv_obj_t *scr) {
  g_edit = layer(scr);
  g_eh = header(g_edit);
  g_ev = text(g_edit, &lg_c300_220, INK, "");
  lv_obj_set_style_text_letter_space(g_ev, -4, 0);
  lv_obj_align(g_ev, LV_ALIGN_TOP_MID, 0, 74);
  lv_obj_align(step_button(g_edit, "\xE2\x88\x92", -1), LV_ALIGN_TOP_LEFT, 24, 150);
  lv_obj_align(step_button(g_edit, "+", 1), LV_ALIGN_TOP_RIGHT, -24, 150);
  g_ehint = text(g_edit, &lg_mono_13, MUTED, "");
  lv_obj_set_pos(g_ehint, 24, 442);
  lv_obj_t *s = button(g_edit, "Set", on_set, NULL);
  lv_obj_align(s, LV_ALIGN_BOTTOM_RIGHT, -24, -24);
  lv_obj_t *c = button(g_edit, "Cancel", close_layer, g_edit);
  lv_obj_align_to(c, s, LV_ALIGN_OUT_LEFT_MID, -16, 0);
  lv_obj_add_flag(g_edit, LV_OBJ_FLAG_HIDDEN);
}

// ---- Splash --------------------------------------------------------------------------------------------------------
static void splash_close(lv_event_t *e) {
  (void)e;
  if (!g_splash) return;
  lv_obj_delete(g_splash);
  g_splash = NULL;
}
static void build_splash(lv_obj_t *scr) {
  g_splash = layer(scr);
  lv_obj_add_flag(g_splash, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_splash, splash_close, LV_EVENT_CLICKED, NULL);
  rule(g_splash, 24, 52, 752, 2, INK);
  lv_obj_set_pos(text(g_splash, &lg_mono_14, ACCENT, "LEDGER"), 24, 24);
  lv_obj_t *t = text(g_splash, &lg_600_24, INK, "A screen for Gaggiuino machines, on the Brewdeck core.");
  lv_obj_set_pos(t, 24, 300);
  lv_obj_t *n = text(g_splash, &lg_400_17, MUTED, "Unofficial: not made by or affiliated with the Gaggiuino project.");
  lv_obj_set_pos(n, 24, 344);
  rule(g_splash, 24, 426, 752, 2, INK);
  lv_obj_set_pos(text(g_splash, &lg_mono_13, INK, "VERSION 0.1"), 24, 440);
  g_splash_until = 0;   // set on the first tick: the UI starts ~2.5 s after boot
}

// ---- Offline help ----------------------------------------------------------------------------------------------
// After 5 s without the machine (never over a shot or steam): what the screen is doing (joining Wi-Fi, looking for
// the machine); after 20 s of one try, what is wrong and the likely fix as buttons. Gone when the machine answers.
static lv_obj_t *g_off_pg, *g_off_title, *g_off_body, *g_ob_retry, *g_ob_addr, *g_ob_wifi;
static Row g_or_screen, g_or_net, g_or_machine;
static uint32_t g_offline_at;   // when this try started (0 = online)
static bool g_off_shown;
static void open_wifi(lv_event_t *e);
static void open_addr(lv_event_t *e);
static void on_retry(lv_event_t *e) { (void)e; bd_wifi_retry(); g_offline_at = g_now | 1; }

static lv_obj_t *column(lv_obj_t *p, int x, int y, int w) {
  lv_obj_t *c = lv_obj_create(p);
  lv_obj_remove_style_all(c);
  lv_obj_set_pos(c, x, y);
  lv_obj_set_size(c, w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}
static lv_obj_t *button_bar(lv_obj_t *p) {   // a row of buttons, right-aligned at the bottom
  lv_obj_t *b = lv_obj_create(p);
  lv_obj_remove_style_all(b);
  lv_obj_set_pos(b, 24, 408);
  lv_obj_set_size(b, 752, 48);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(b, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(b, 16, 0);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  return b;
}
static void wide_row(Row *r, lv_obj_t *col, const char *name, int w) {
  row_init(r, col, name, false, NULL);
  r->w = w;
  lv_obj_set_width(r->row, w);
  row_fit(r);
}
static void show(lv_obj_t *o, bool on) { if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); }

static void build_offline(lv_obj_t *scr) {
  g_off_pg = layer(scr);
  g_oh = header(g_off_pg);
  set_text(g_oh.left, "OFFLINE");
  g_off_title = text(g_off_pg, &lg_400_36, INK, "");
  lv_obj_set_pos(g_off_title, 22, 84);
  g_off_body = text(g_off_pg, &lg_400_17, INK, "");
  lv_obj_set_pos(g_off_body, 24, 146);
  lv_obj_set_width(g_off_body, 620);
  lv_label_set_long_mode(g_off_body, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_line_space(g_off_body, 6, 0);
  lv_obj_t *col = column(g_off_pg, 24, 236, 520);
  wide_row(&g_or_screen, col, "Screen", 520);
  wide_row(&g_or_net, col, "Wi-Fi", 520);
  wide_row(&g_or_machine, col, "Machine", 520);
  lv_obj_t *bar = button_bar(g_off_pg);
  g_ob_retry = button(bar, "Try again", on_retry, NULL);
  g_ob_addr = button(bar, "Machine address", open_addr, NULL);
  g_ob_wifi = button(bar, "Wi-Fi settings", open_wifi, NULL);
  lv_obj_add_flag(g_off_pg, LV_OBJ_FLAG_HIDDEN);
}

static void offline_text(const bd_machine *m) {
  const bd_network *n = bd_network_state();
  const bool trying = g_now - g_offline_at < 20000 || m->why == BD_WHY_NONE;
  const char *net = n->ssid[0] ? n->ssid : n->target;
  const char *addr = n->machine_addr[0] ? n->machine_addr : "the machine";
  char t[48], b[240];
  bool retry = false, maddr = false, wifi = true;
  if (!n->ssid[0] && !n->target[0]) {
    snprintf(t, sizeof t, "No Wi-Fi set up");
    snprintf(b, sizeof b, "Choose the Wi-Fi network your machine is on, and type its password.");
  } else if (!n->ssid[0]) {
    retry = true;
    if (trying) {
      snprintf(t, sizeof t, "Joining Wi-Fi");
      snprintf(b, sizeof b, "Connecting to %s.", net);
    } else {
      snprintf(t, sizeof t, "Not connected to Wi-Fi");
      if (m->why == BD_WHY_WIFI_NO_AP) snprintf(b, sizeof b, "%s is not in range. The screen keeps trying.", net);
      else if (m->why == BD_WHY_WIFI_AUTH) snprintf(b, sizeof b, "%s refused the password. Choose it again in Wi-Fi settings and retype it.", net);
      else snprintf(b, sizeof b, "%s did not let the screen join (a router may block it). The screen keeps trying.", net);
    }
  } else {
    maddr = true;
    if (trying) {
      snprintf(t, sizeof t, "Looking for your machine");
      snprintf(b, sizeof b, "Asking for %s on %s.", addr, net);
    } else {
      snprintf(t, sizeof t, "Machine not found");
      if (m->why == BD_WHY_NO_REPLY)
        snprintf(b, sizeof b, "%s was found at %s but does not answer. Is the machine starting up?", addr, n->why_ip[0] ? n->why_ip : "its address");
      else snprintf(b, sizeof b, "Nothing on %s answers to %s. Is the machine on? If it has another name or a fixed address, set it here.", net, addr);
    }
  }
  set_text(g_off_title, t);
  set_text(g_off_body, b);
  set_text(lv_obj_get_child(g_ob_wifi, 0), n->target[0] || n->ssid[0] ? "Wi-Fi settings" : "Choose network");
  show(g_ob_retry, retry);
  show(g_ob_addr, maddr);
  show(g_ob_wifi, wifi);
  row_set(&g_or_screen, n->screen_ip[0] ? n->screen_ip : DASH);
  if (n->ssid[0]) snprintf(b, sizeof b, "%s (%d dBm)", n->ssid, n->rssi);
  else snprintf(b, sizeof b, "%s", n->target[0] ? n->target : "none");
  row_set(&g_or_net, b);
  row_set(&g_or_machine, n->machine_where[0] ? n->machine_where : addr);
}

static void offline_step(const bd_machine *m) {
  if (m->link == BD_LINK_ONLINE) {
    g_offline_at = 0;
    if (g_off_shown) { lv_obj_add_flag(g_off_pg, LV_OBJ_FLAG_HIDDEN); g_off_shown = false; }
    return;
  }
  if (!g_offline_at) g_offline_at = g_now | 1;
  if (!g_off_shown && g_now - g_offline_at > 5000 && !g_brewing_shown && !g_steam_shown) {
    lv_obj_remove_flag(g_off_pg, LV_OBJ_FLAG_HIDDEN);
    g_off_shown = true;
  }
  static uint32_t upd;
  if (g_off_shown && g_now - upd > 500) { upd = g_now; offline_text(m); }
}

// ---- Wi-Fi list ----------------------------------------------------------------------------------------------------
static Header g_wh;
static lv_obj_t *g_wifi, *g_wlist, *g_wforget;
static Row g_wrows[BD_MAX_NETS + 1];
static bd_wifi_net g_nets[BD_MAX_NETS];
static int g_nn;
static bool g_scanning;
static uint32_t g_forget_at;
static void kbd_open(int mode, const char *ssid);
enum { KB_PASS, KB_ADDR };

static void on_net(lv_event_t *e) {
  const int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i < 0 || i >= g_nn) return;
  if (!g_nets[i].locked) {   // an open network joins at once
    bd_wifi_join(g_nets[i].ssid, "");
    set_text(g_wh.right, "JOINING");
  } else kbd_open(KB_PASS, g_nets[i].ssid);
}
static void fill_wifi(void) {
  lv_obj_clean(g_wlist);
  const bd_network *n = bd_network_state();
  char b[48];
  int k = 0;
  if (n->ssid[0]) {
    Row *r = &g_wrows[k++];
    wide_row(r, g_wlist, "", 752);
    lv_obj_set_height(r->row, 52);
    lv_obj_set_style_text_font(r->name, &lg_600_17, 0);
    lv_obj_set_style_text_color(r->name, ACCENT, 0);
    lv_label_set_text(r->name, n->ssid);
    one_line(r->name, 520);
    snprintf(b, sizeof b, "connected%s%d dBm", DOT, n->rssi);
    lv_label_set_text(r->val, b);
    row_fit(r);
  }
  for (int i = 0; i < g_nn && k <= BD_MAX_NETS; i++) {
    if (!strcmp(g_nets[i].ssid, n->ssid)) continue;
    Row *r = &g_wrows[k++];
    wide_row(r, g_wlist, "", 752);
    lv_obj_set_height(r->row, 52);
    lv_obj_set_style_text_color(r->name, INK, 0);
    lv_label_set_text(r->name, g_nets[i].ssid);
    one_line(r->name, 520);
    snprintf(b, sizeof b, "%s%d dBm", g_nets[i].locked ? "" : "open \xC2\xB7 ", g_nets[i].rssi);
    lv_label_set_text(r->val, b);
    row_fit(r);
    lv_obj_add_flag(r->row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(r->row, PRESSED, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r->row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(r->row, on_net, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }
  if (!k) lv_label_set_text(text(g_wlist, &lg_400_17, MUTED, ""), g_scanning ? "Searching for networks..." : "No networks found. Only 2.4 GHz networks work.");
  show(g_wforget, n->ssid[0] || n->target[0]);
}
static void open_wifi(lv_event_t *e) {
  (void)e;
  g_scanning = true;
  g_nn = 0;
  set_text(g_wh.right, "SEARCHING");
  bd_wifi_scan();
  fill_wifi();
  lv_obj_remove_flag(g_wifi, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(g_wifi, -1);
}
static void close_wifi(lv_event_t *e) { (void)e; lv_obj_add_flag(g_wifi, LV_OBJ_FLAG_HIDDEN); }
static void on_forget(lv_event_t *e) {
  (void)e;
  if (!g_forget_at) {   // two taps: the first one asks
    g_forget_at = g_now | 1;
    set_text(lv_obj_get_child(g_wforget, 0), "Tap again to forget");
    lv_obj_set_style_text_color(lv_obj_get_child(g_wforget, 0), ACCENT, 0);
    return;
  }
  g_forget_at = 0;
  bd_wifi_forget();
  set_text(lv_obj_get_child(g_wforget, 0), "Forget network");
  lv_obj_set_style_text_color(lv_obj_get_child(g_wforget, 0), INK, 0);
  set_text(g_wh.right, "FORGOTTEN");
  fill_wifi();
}
static void build_wifi(lv_obj_t *scr) {
  g_wifi = layer(scr);
  g_wh = header(g_wifi);
  set_text(g_wh.left, "WI-FI");
  g_wlist = lv_obj_create(g_wifi);
  lv_obj_remove_style_all(g_wlist);
  lv_obj_set_pos(g_wlist, 24, 64);
  lv_obj_set_size(g_wlist, 752, 330);
  lv_obj_set_flex_flow(g_wlist, LV_FLEX_FLOW_COLUMN);
  lv_obj_t *bar = button_bar(g_wifi);
  g_wforget = button(bar, "Forget network", on_forget, NULL);
  button(bar, "Machine address", open_addr, NULL);
  button(bar, "Close", close_wifi, NULL);
  lv_obj_add_flag(g_wifi, LV_OBJ_FLAG_HIDDEN);
}

// ---- Keyboard (Wi-Fi password, machine address) --------------------------------------------------------------------
// Letters, shift (one character), two symbol layers: every printable ASCII character. The text is shown as typed
// (this is the owner's own screen); it is never logged.
static Header g_kh;
static lv_obj_t *g_kbd, *g_kfield, *g_kok, *g_kkeys[3][10], *g_kspecial, *g_klayer;
static char g_ktext[64], g_kssid[33];
static int g_klen, g_kmode, g_klay;   // layer: 0 lower, 1 upper, 2 symbols, 3 more symbols
static const char *KLAY[4][3] = {{"qwertyuiop", "asdfghjkl", "zxcvbnm"},
                                 {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"},
                                 {"1234567890", "@#$_&-+()", "*\"':;!?"},     // the middle row has 9 keys
                                 {"~`|\\<>{}[]", "=^%/,.", "!?.,-_@"}};

static void kbd_show(void) {
  char b[80];
  const int tail = 32;   // the end of a long text, after "..."
  snprintf(b, sizeof b, "%s%s|", g_klen > tail ? "..." : "", g_klen > tail ? g_ktext + g_klen - tail : g_ktext);
  set_text(g_kfield, b);
  for (int r = 0; r < 3; r++) {
    const char *row = KLAY[g_klay][r];
    const int n = (int)strlen(row);
    for (int c = 0; c < 10; c++) {
      if (!g_kkeys[r][c]) continue;
      show(g_kkeys[r][c], c < n);
      if (c < n) { char k[2] = {row[c], 0}; set_text(lv_obj_get_child(g_kkeys[r][c], 0), k); }
    }
  }
  set_text(lv_obj_get_child(g_kspecial, 0), g_klay == 0 ? "shift" : g_klay == 1 ? "SHIFT" : g_klay == 2 ? "#+=" : "123");
  set_text(lv_obj_get_child(g_klayer, 0), g_klay >= 2 ? "ABC" : "?123");
  const bool ok = g_kmode == KB_PASS ? g_klen >= 8 : g_klen > 0;
  lv_obj_set_style_border_color(g_kok, ok ? INK : LEAD_SOFT, 0);
  lv_obj_set_style_text_color(lv_obj_get_child(g_kok, 0), ok ? INK : LEAD_STRONG, 0);
  if (ok) lv_obj_add_flag(g_kok, LV_OBJ_FLAG_CLICKABLE); else lv_obj_remove_flag(g_kok, LV_OBJ_FLAG_CLICKABLE);
}
static void kbd_type(char ch) {
  if (g_klen + 1 >= (int)sizeof g_ktext) return;
  g_ktext[g_klen++] = ch;
  g_ktext[g_klen] = 0;
  if (g_klay == 1) g_klay = 0;   // shift is for one character
  kbd_show();
}
static void on_key(lv_event_t *e) {
  const int rc = (int)(intptr_t)lv_event_get_user_data(e), r = rc >> 4, c = rc & 15;
  if (c < (int)strlen(KLAY[g_klay][r])) kbd_type(KLAY[g_klay][r][c]);
}
static void on_space(lv_event_t *e) { (void)e; kbd_type(' '); }
static void on_back(lv_event_t *e) { (void)e; if (g_klen) g_ktext[--g_klen] = 0; kbd_show(); }
static void on_special(lv_event_t *e) { (void)e; g_klay = g_klay == 0 ? 1 : g_klay == 1 ? 0 : g_klay == 2 ? 3 : 2; kbd_show(); }
static void on_layer(lv_event_t *e) { (void)e; g_klay = g_klay >= 2 ? 0 : 2; kbd_show(); }
static void kbd_close(void) {
  memset(g_ktext, 0, sizeof g_ktext);   // no password left in memory
  g_klen = 0;
  lv_obj_add_flag(g_kbd, LV_OBJ_FLAG_HIDDEN);
}
static void on_kcancel(lv_event_t *e) { (void)e; kbd_close(); }
static void on_kok(lv_event_t *e) {
  (void)e;
  if (g_kmode == KB_PASS) {
    bd_wifi_join(g_kssid, g_ktext);
    set_text(g_kh.right, "JOINING");
  } else {
    bd_set_machine_address(g_ktext);
    kbd_close();
    lv_obj_add_flag(g_wifi, LV_OBJ_FLAG_HIDDEN);
    g_offline_at = g_offline_at ? (g_now | 1) : 0;   // a new try with the new address
    note("MACHINE ADDRESS SET");
  }
}
static void kbd_open(int mode, const char *ssid) {
  char b[64];
  g_kmode = mode;
  g_klay = 0;
  if (mode == KB_PASS) {
    snprintf(g_kssid, sizeof g_kssid, "%s", ssid);
    g_klen = 0;
    g_ktext[0] = 0;
    upper(b, sizeof b, ssid);
    char h[80];
    snprintf(h, sizeof h, "PASSWORD FOR %s", b);
    set_text(g_kh.left, h);
    set_text(lv_obj_get_child(g_kok, 0), "Join");
  } else {
    const bd_network *n = bd_network_state();
    snprintf(g_ktext, sizeof g_ktext, "%s", n->machine_addr);
    g_klen = (int)strlen(g_ktext);
    set_text(g_kh.left, "MACHINE ADDRESS \xC2\xB7 A NAME OR AN IP");
    set_text(lv_obj_get_child(g_kok, 0), "Use");
  }
  set_text(g_kh.right, "");
  kbd_show();
  lv_obj_remove_flag(g_kbd, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(g_kbd, -1);
}
static void open_addr(lv_event_t *e) { (void)e; kbd_open(KB_ADDR, NULL); }

static lv_obj_t *key(lv_obj_t *p, int x, int y, int w, const char *t, lv_event_cb_t cb, void *ud) {
  lv_obj_t *k = button(p, t, NULL, NULL);
  lv_obj_set_pos(k, x, y);
  lv_obj_set_size(k, w, 52);
  lv_obj_set_style_pad_hor(k, 0, 0);
  lv_obj_set_style_text_font(lv_obj_get_child(k, 0), &lg_600_24, 0);
  lv_obj_add_event_cb(k, cb, LV_EVENT_CLICKED, ud);
  return k;
}
static void build_kbd(lv_obj_t *scr) {
  g_kbd = layer(scr);
  g_kh = header(g_kbd);
  g_kfield = text(g_kbd, &lg_400_36, INK, "|");
  lv_obj_set_pos(g_kfield, 24, 70);
  rule(g_kbd, 24, 124, 752, 2, INK);
  const int KW = 69, G = 6, Y0 = 146, RH = 58;
  for (int c = 0; c < 10; c++) g_kkeys[0][c] = key(g_kbd, 24 + 4 + c * (KW + G), Y0, KW, "", on_key, (void *)(intptr_t)(0 << 4 | c));
  for (int c = 0; c < 9; c++) g_kkeys[1][c] = key(g_kbd, 24 + 4 + (KW + G) / 2 + c * (KW + G), Y0 + RH, KW, "", on_key, (void *)(intptr_t)(1 << 4 | c));
  g_kspecial = key(g_kbd, 28, Y0 + 2 * RH, 107, "shift", on_special, NULL);
  lv_obj_set_style_text_font(lv_obj_get_child(g_kspecial, 0), &lg_600_17, 0);
  for (int c = 0; c < 7; c++) g_kkeys[2][c] = key(g_kbd, 28 + 107 + G + c * (KW + G), Y0 + 2 * RH, KW, "", on_key, (void *)(intptr_t)(2 << 4 | c));
  lv_obj_t *bk = key(g_kbd, 28 + 107 + G + 7 * (KW + G), Y0 + 2 * RH, 107, "delete", on_back, NULL);
  lv_obj_set_style_text_font(lv_obj_get_child(bk, 0), &lg_600_17, 0);
  lv_obj_add_event_cb(bk, on_back, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
  g_klayer = key(g_kbd, 28, Y0 + 3 * RH, 107, "?123", on_layer, NULL);
  lv_obj_set_style_text_font(lv_obj_get_child(g_klayer, 0), &lg_600_17, 0);
  lv_obj_t *sp = key(g_kbd, 28 + 107 + G, Y0 + 3 * RH, 7 * (KW + G) - G, "space", on_space, NULL);
  lv_obj_set_style_text_font(lv_obj_get_child(sp, 0), &lg_600_17, 0);
  lv_obj_t *bar = button_bar(g_kbd);
  button(bar, "Cancel", on_kcancel, NULL);
  g_kok = button(bar, "Join", on_kok, NULL);
  lv_obj_add_flag(g_kbd, LV_OBJ_FLAG_HIDDEN);
}

// ---- Settings ------------------------------------------------------------------------------------------------------
// Wi-Fi and the machine address (tap), then what the screen and the machine run (read-only), and the unofficial line.
static Header g_gh;
static lv_obj_t *g_set;
static Row g_gr_wifi, g_gr_addr, g_gr_ip, g_gr_ver, g_gr_up, g_gr_fw, g_gr_mscr, g_gr_upd;

static const char *reset_text(bd_reset r) {
  switch (r) {
    case BD_RESET_POWER_ON: case BD_RESET_PIN: case BD_RESET_USB: return "since power-on";
    case BD_RESET_SOFTWARE: return "after a restart";
    case BD_RESET_CRASH: return "after a crash";
    case BD_RESET_WATCHDOG: return "after a freeze";
    case BD_RESET_BROWNOUT: return "after a power dip";
    default: return "";
  }
}
static void settings_update(void) {
  const bd_network *n = bd_network_state();
  const bd_machine *m = bd_machine_state();
  const bd_core_info *c = bd_core();
  char b[96];
  if (n->ssid[0]) snprintf(b, sizeof b, "%s%s%d dBm", n->ssid, DOT, n->rssi);
  else snprintf(b, sizeof b, "%s", n->target[0] ? "not connected" : "none set up");
  row_set(&g_gr_wifi, b);
  row_set(&g_gr_addr, n->machine_addr[0] ? n->machine_addr : DASH);
  row_set(&g_gr_ip, n->screen_ip[0] ? n->screen_ip : DASH);
  snprintf(b, sizeof b, "%s%score %s", BD_UI.version, DOT, c->version[0] ? c->version : c->build);
  row_set(&g_gr_ver, b);
  const uint32_t up = c->uptime_s;
  if (up < 3600) snprintf(b, sizeof b, "%lu min%s%s", (unsigned long)(up / 60), DOT, reset_text(c->reset));
  else snprintf(b, sizeof b, "%lu h %lu min%s%s", (unsigned long)(up / 3600), (unsigned long)(up / 60 % 60), DOT, reset_text(c->reset));
  row_set(&g_gr_up, b);
  if (m->core_version[0]) snprintf(b, sizeof b, "%s%s%s", m->core_version, m->core_board[0] ? DOT : "", m->core_board);
  else snprintf(b, sizeof b, "%s", DASH);
  row_set(&g_gr_fw, b);
  if (m->screen_version[0] || m->web_version[0])
    snprintf(b, sizeof b, "%s%s%s", m->screen_version[0] ? m->screen_version : DASH, DOT, m->web_version[0] ? m->web_version : DASH);
  else snprintf(b, sizeof b, "%s", DASH);
  row_set(&g_gr_mscr, b);
  if (m->update_available) snprintf(b, sizeof b, "%s%sinstall it in the web UI", m->update[0] ? m->update : "available", DOT);
  else snprintf(b, sizeof b, "none");
  row_set(&g_gr_upd, b);
  lv_obj_set_style_text_color(g_gr_upd.val, m->update_available ? ACCENT : MUTED, 0);
}
static void open_settings(lv_event_t *e) {
  (void)e;
  settings_update();
  lv_obj_remove_flag(g_set, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(g_set, -1);
}
static void build_settings(lv_obj_t *scr) {
  g_set = layer(scr);
  g_gh = header(g_set);
  set_text(g_gh.left, "SETTINGS");
  lv_obj_t *col = column(g_set, 24, 66, 752);
  row_init(&g_gr_wifi, col, "Wi-Fi", true, open_wifi);
  row_init(&g_gr_addr, col, "Machine address", true, open_addr);
  row_init(&g_gr_ip, col, "Screen address", false, NULL);
  row_init(&g_gr_ver, col, "Ledger", false, NULL);
  row_init(&g_gr_up, col, "Running for", false, NULL);
  row_init(&g_gr_fw, col, "Machine firmware", false, NULL);
  row_init(&g_gr_mscr, col, "Machine screen, web UI", false, NULL);
  row_init(&g_gr_upd, col, "Machine update", false, NULL);
  Row *rows[] = {&g_gr_wifi, &g_gr_addr, &g_gr_ip, &g_gr_ver, &g_gr_up, &g_gr_fw, &g_gr_mscr, &g_gr_upd};
  for (Row *r : rows) { r->w = 752; lv_obj_set_width(r->row, 752); one_line(r->val, 480); row_fit(r); }
  lv_obj_t *n = text(g_set, &lg_400_17, MUTED, "Unofficial: not made by or affiliated with the Gaggiuino project.");
  lv_obj_set_pos(n, 24, 422);
  lv_obj_t *bar = button_bar(g_set);
  lv_obj_set_width(bar, LV_SIZE_CONTENT);
  lv_obj_align(bar, LV_ALIGN_BOTTOM_RIGHT, -24, -24);
  button(bar, "Close", close_layer, g_set);
  lv_obj_add_flag(g_set, LV_OBJ_FLAG_HIDDEN);
}

// ---- Pressure release ----------------------------------------------------------------------------------------------
// The machine counts down ("Dropping beats in: 3 / 2 / 1 / 0") before it releases pressure through the group: water
// comes out. A red bar over the top and a red frame round the screen, pulsing on each beat, on LVGL's top layer (over
// every page and the idle clock; taps pass through). It stays 2.5 s after the last beat, 5 s after "Releasing".
static lv_obj_t *g_rel_frame, *g_rel_bar, *g_rel_text;
static uint32_t g_rel_until;

static void rel_border(void *o, int32_t v) { lv_obj_set_style_border_width((lv_obj_t *)o, v, 0); }
static void build_release(void) {
  lv_obj_t *top = lv_layer_top();
  g_rel_frame = lv_obj_create(top);
  lv_obj_remove_style_all(g_rel_frame);
  lv_obj_set_size(g_rel_frame, 800, 480);
  lv_obj_set_style_border_color(g_rel_frame, ACCENT, 0);
  lv_obj_set_style_border_width(g_rel_frame, 8, 0);
  lv_obj_remove_flag(g_rel_frame, LV_OBJ_FLAG_CLICKABLE);
  g_rel_bar = rule(top, 0, 0, 800, 56, ACCENT);
  lv_obj_remove_flag(g_rel_bar, LV_OBJ_FLAG_CLICKABLE);
  g_rel_text = text(g_rel_bar, &lg_mono_14, PAPER, "");
  lv_obj_set_style_text_font(g_rel_text, &lg_600_24, 0);
  lv_obj_set_style_text_letter_space(g_rel_text, 0, 0);
  lv_obj_align(g_rel_text, LV_ALIGN_LEFT_MID, 24, 0);
  lv_obj_add_flag(g_rel_frame, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(g_rel_bar, LV_OBJ_FLAG_HIDDEN);
}
static void release_event(int count) {
  char b[64];
  if (count >= 0) {
    snprintf(b, sizeof b, "Water coming: pressure release in %d", count);
    g_rel_until = (g_now + 2500) | 1;
  } else {
    snprintf(b, sizeof b, "Releasing pressure: water coming");
    g_rel_until = (g_now + 5000) | 1;
  }
  set_text(g_rel_text, b);
  lv_obj_remove_flag(g_rel_frame, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(g_rel_bar, LV_OBJ_FLAG_HIDDEN);
  lv_anim_t a;   // the beat: the frame thick, then back to 8 px
  lv_anim_init(&a);
  lv_anim_set_var(&a, g_rel_frame);
  lv_anim_set_exec_cb(&a, rel_border);
  lv_anim_set_values(&a, 22, 8);
  lv_anim_set_duration(&a, 700);
  lv_anim_start(&a);
}
static void release_step(void) {
  if (g_rel_until && (int32_t)(g_now - g_rel_until) > 0) {
    g_rel_until = 0;
    lv_obj_add_flag(g_rel_frame, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_rel_bar, LV_OBJ_FLAG_HIDDEN);
  }
}

// ---- Standby -------------------------------------------------------------------------------------------------------
// Dim: after DIM_MS without a touch and nothing running, an idle page (clock, boiler, state) at half brightness; it
// moves a little every 2 min against image retention. Off: after OFF_MS without a touch, or OFF_AWAY_MS untouched while
// the machine is unreachable (switched off at night): black and backlight 0. A running shot / steam / flush counts as
// activity. Wake: a touch (the first one only wakes: the page on top takes it), bd_ui_wake (the core: shot, steam,
// flush, a notice), or the machine answering again when it was away.
static uint32_t DIM_MS = 10 * 60000, OFF_MS = 30 * 60000, OFF_AWAY_MS = 2 * 60000;
#define BL_ON 100
#define BL_DIM 50
static lv_obj_t *g_idle, *g_idle_box, *g_idle_clock, *g_idle_line, *g_off;
static int g_sb;   // 0 awake, 1 dim (idle page), 2 off
static bool g_off_away;
static uint32_t g_away_since, g_idle_moved;

static void standby_wake(void) {
  if (!g_sb) return;
  lv_obj_add_flag(g_idle, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(g_off, LV_OBJ_FLAG_HIDDEN);
  g_sb = 0;
  bd_set_backlight(BL_ON);
  lv_display_trigger_activity(NULL);
  bd_log("ledger: screen on");
}
static void on_standby_tap(lv_event_t *e) { (void)e; standby_wake(); }

static void idle_update(const bd_machine *m) {
  char b[64];
  int h, mi, wd;
  if (bd_local_time(&h, &mi, &wd)) snprintf(b, sizeof b, "%02d:%02d", h, mi); else snprintf(b, sizeof b, "--:--");
  set_text(g_idle_clock, b);
  if (m->link == BD_LINK_ONLINE) snprintf(b, sizeof b, "%.1f %sC%s%s", m->temperature, DEG, DOT, state_word(m));
  else snprintf(b, sizeof b, "%s", state_word(m));
  set_text(g_idle_line, b);
}
static void idle_move(void) {   // a new spot within +-24 px of the centre
  lv_obj_align(g_idle_box, LV_ALIGN_CENTER, (rand() % 49) - 24, (rand() % 49) - 24);
  g_idle_moved = g_now;
}

static void build_standby(lv_obj_t *scr) {
  g_idle = layer(scr);
  lv_obj_add_flag(g_idle, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_idle, on_standby_tap, LV_EVENT_CLICKED, NULL);
  g_idle_box = lv_obj_create(g_idle);
  lv_obj_remove_style_all(g_idle_box);
  lv_obj_set_size(g_idle_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(g_idle_box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(g_idle_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(g_idle_box, LV_OBJ_FLAG_CLICKABLE);
  g_idle_clock = text(g_idle_box, &lg_c300_220, INK, "--:--");
  lv_obj_set_style_text_letter_space(g_idle_clock, -4, 0);
  g_idle_line = text(g_idle_box, &lg_mono_14, ACCENT, "");
  lv_obj_align(g_idle_box, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(g_idle, LV_OBJ_FLAG_HIDDEN);
  g_off = layer(scr);
  lv_obj_set_style_bg_color(g_off, lv_color_black(), 0);
  lv_obj_add_flag(g_off, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_off, on_standby_tap, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(g_off, LV_OBJ_FLAG_HIDDEN);
#ifdef LV_SIM_HOST   // emulator: LEDGER_DIM_S / LEDGER_OFF_S / LEDGER_AWAY_S shorten the times for testing
  if (getenv("LEDGER_DIM_S")) DIM_MS = atoi(getenv("LEDGER_DIM_S")) * 1000u;
  if (getenv("LEDGER_OFF_S")) OFF_MS = atoi(getenv("LEDGER_OFF_S")) * 1000u;
  if (getenv("LEDGER_AWAY_S")) OFF_AWAY_MS = atoi(getenv("LEDGER_AWAY_S")) * 1000u;
#endif
}

static bool running(const bd_machine *m) {
  return m->brewing || m->steaming || m->hot_water || !strncmp(m->mode, "FLUSH", 5) || !strcmp(m->mode, "STEAM") ||
         (g_brewing_shown && !g_brew_done_at);
}

static void standby_step(const bd_machine *m) {
  if (running(m)) { lv_display_trigger_activity(NULL); standby_wake(); return; }
  const bool away = m->link != BD_LINK_ONLINE;
  if (!away) g_away_since = 0; else if (!g_away_since) g_away_since = g_now | 1;
  if (g_sb == 2 && g_off_away && !away) { standby_wake(); return; }   // the machine is back
  const uint32_t still = lv_display_get_inactive_time(NULL);
  if (g_sb < 2 && (still > OFF_MS || (away && g_now - g_away_since > OFF_AWAY_MS && still > OFF_AWAY_MS))) {
    g_sb = 2;
    g_off_away = still <= OFF_MS;
    lv_obj_add_flag(g_idle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(g_off, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(g_off, -1);
    lv_refr_now(NULL);   // black on the panel before the backlight goes (no picture held)
    bd_set_backlight(0);
    bd_log("ledger: screen off%s", g_off_away ? " (machine away)" : "");
    return;
  }
  if (g_sb == 0 && still > DIM_MS) {
    g_sb = 1;
    idle_update(m);
    idle_move();
    lv_obj_remove_flag(g_idle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(g_idle, -1);
    bd_set_backlight(BL_DIM);
    bd_log("ledger: idle clock");
  }
  if (g_sb == 1 && g_now - g_idle_moved > 120000) idle_move();
  static uint32_t upd;
  if (g_sb == 1 && g_now - upd > 1000) { upd = g_now; idle_update(m); }
}

// ---- what the core calls -------------------------------------------------------------------------------------------
void bd_ui_init(void) {
  bd_lvgl9_setup();
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, PAPER, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  build_ready(scr);
  build_offline(scr);   // over Ready, under Steam / Brewing and the pages
  build_steam(scr);   // under Brewing and the pages
  build_brew(scr);
  build_profiles(scr);
  build_edit(scr);
  build_settings(scr);   // under the Wi-Fi page and the keyboard it opens
  build_wifi(scr);
  build_kbd(scr);
  build_standby(scr);
  build_splash(scr);
  build_release();
}

void bd_ui_tick(uint32_t now) {
  static uint32_t seen_m = ~0u, seen_shape = ~0u, seen_p = ~0u;
  g_now = now;
  if (g_splash && !g_splash_until) g_splash_until = (now + 5000) | 1;   // 5 s from when it is first shown
  if (g_splash && (int32_t)(now - g_splash_until) > 0) splash_close(NULL);
  const bd_machine *m = bd_machine_state();
  if (bd_seq_machine() != seen_m) { seen_m = bd_seq_machine(); update_ready(m); }
  if (bd_seq_shape() != seen_shape) {
    seen_shape = bd_seq_shape();
    g_pv_ok = g_pv && bd_active_shape()->id && bd_profile_preview(bd_active_shape(), PLAN_W, 110, g_pv);
    lv_obj_invalidate(g_plan);
    update_ready(m);
  }
  if (bd_seq_profiles() != seen_p) {
    seen_p = bd_seq_profiles();
    if (!lv_obj_has_flag(g_prof, LV_OBJ_FLAG_HIDDEN)) fill_profiles();
  }
  if (g_note_until && now > g_note_until) { g_note_until = 0; lv_obj_add_flag(g_note, LV_OBJ_FLAG_HIDDEN); }
  if (g_brewing_shown && g_brew_done_at && now - g_brew_done_at > 60000) brew_close(NULL);
  const bool steaming = m->link == BD_LINK_ONLINE && (m->steaming || !strcmp(m->mode, "STEAM"));
  if (steaming && !g_steam_shown) steam_open(m);
  else if (steaming) steam_step(m);
  else if (g_steam_shown) steam_close();
  offline_step(m);
  static uint32_t set_upd;
  if (!lv_obj_has_flag(g_set, LV_OBJ_FLAG_HIDDEN) && now - set_upd > 1000) { set_upd = now; settings_update(); }
  if (g_forget_at && g_now - g_forget_at > 3000) {   // "Tap again to forget" lasts 3 s
    g_forget_at = 0;
    set_text(lv_obj_get_child(g_wforget, 0), "Forget network");
    lv_obj_set_style_text_color(lv_obj_get_child(g_wforget, 0), INK, 0);
  }
  standby_step(m);
  release_step();
  bd_lvgl9_tick(now);
}

void bd_ui_event(const bd_event *e) {
  switch (e->type) {
    case BD_EV_SHOT_START: brew_open(); break;
    case BD_EV_SHOT_POINT:
      if (!g_brewing_shown) brew_open();
      g_brew_done_at = 0;
      set_text(g_bh.left, "BREWING");
      brew_point(e->u.point);
      break;
    case BD_EV_SHOT_END: if (g_brewing_shown) brew_end(); break;
    case BD_EV_WIFI_SCAN:
      g_nn = 0;
      for (int i = 0; i < e->u.scan.n && i < BD_MAX_NETS; i++) g_nets[g_nn++] = e->u.scan.nets[i];
      g_scanning = e->u.scan.scanning;
      if (!lv_obj_has_flag(g_wifi, LV_OBJ_FLAG_HIDDEN)) { fill_wifi(); set_text(g_wh.right, g_scanning ? "SEARCHING" : ""); }
      break;
    case BD_EV_WIFI_JOIN: {
      const bool kb = !lv_obj_has_flag(g_kbd, LV_OBJ_FLAG_HIDDEN);
      Header *h = kb ? &g_kh : &g_wh;
      if (e->u.join.state == BD_JOIN_BUSY) set_text(h->right, "JOINING");
      else if (e->u.join.state == BD_JOIN_OK) {
        if (kb) kbd_close();
        lv_obj_add_flag(g_wifi, LV_OBJ_FLAG_HIDDEN);
        g_offline_at = g_offline_at ? (g_now | 1) : 0;
        note("WI-FI JOINED");
      } else if (e->u.join.state == BD_JOIN_FAILED) set_text(h->right, kb ? "NOT JOINED Â· CHECK THE PASSWORD" : "NOT JOINED");
      break;
    }
    case BD_EV_PRESSURE_RELEASE: standby_wake(); release_event(e->u.release.count); break;
    case BD_EV_COMMAND:
      if (e->u.command.answer != BD_ANSWER_OK) note("THE MACHINE DID NOT TAKE IT");
      break;
    default: break;   // notices, stored shots, Wi-Fi: not used yet
  }
}

void bd_ui_wake(void) { standby_wake(); }
