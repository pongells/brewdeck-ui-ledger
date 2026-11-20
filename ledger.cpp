// Ledger: a light, printed-looking UI for Gaggiuino machines, on the Brewdeck core (LVGL 9).
// Paper background, black ink, one red accent; the one number that matters huge, the rest as a ledger of rows
// with dotted leaders. Screens: Ready (temperature, settings, the profile's plan), Brewing (time, weight, chart),
// Profiles (pick one), an editor for brew temperature / stop weight, and a splash with the not-affiliated line.
// Everything runs on the UI task (see brewdeck_core.h). Build: ./emu.sh (emulator) or ./fw.sh <env> (firmware).
#include <math.h>
#include <stdio.h>
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
typedef struct { lv_obj_t *row, *name, *val, *lead; lv_point_precise_t pts[2]; } Row;
static void row_fit(Row *r) {
  lv_obj_update_layout(r->row);
  const int x1 = lv_obj_get_width(r->name) + 8, x2 = 320 - lv_obj_get_width(r->val) - 8;
  r->pts[0].x = x1; r->pts[1].x = x2 > x1 ? x2 : x1;
  r->pts[0].y = r->pts[1].y = 0;
  lv_line_set_points(r->lead, r->pts, 2);
}
static void row_init(Row *r, lv_obj_t *p, const char *name, bool strong, lv_event_cb_t cb) {
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
static Header g_rh, g_sh;   // Ready and Steam headers (the clock is on both)
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

static void plan_draw(lv_event_t *e) {
  lv_layer_t *L = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(lv_event_get_target_obj(e), &a);
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = INK;
  d.width = 1;
  d.p1.x = a.x1; d.p1.y = a.y1 + 110; d.p2.x = a.x1 + 399; d.p2.y = a.y1 + 110;
  lv_draw_line(L, &d);
  for (int x = 0; x < 400; x += 100) { d.p1.x = d.p2.x = a.x1 + x; d.p1.y = a.y1 + 104; d.p2.y = a.y1 + 110; lv_draw_line(L, &d); }
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
  lv_obj_set_size(g_plan, 400, 120);
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

static void update_clock(void) {
  static const char *DAY[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  int h, mi, wd;
  char b[24] = "";
  if (bd_local_time(&h, &mi, &wd)) snprintf(b, sizeof b, "%s%s%02d:%02d", DAY[wd % 7], DOT, h, mi);
  set_text(g_rh.right, b);
  set_text(g_sh.right, b);
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
  g_splash_until = 3000;
}

// ---- what the core calls -------------------------------------------------------------------------------------------
void bd_ui_init(void) {
  bd_lvgl9_setup();
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, PAPER, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  build_ready(scr);
  build_steam(scr);   // under Brewing and the pages
  build_brew(scr);
  build_profiles(scr);
  build_edit(scr);
  build_splash(scr);
}

void bd_ui_tick(uint32_t now) {
  static uint32_t seen_m = ~0u, seen_shape = ~0u, seen_p = ~0u, last_clock;
  g_now = now;
  if (g_splash && now > g_splash_until) splash_close(NULL);
  const bd_machine *m = bd_machine_state();
  if (bd_seq_machine() != seen_m) { seen_m = bd_seq_machine(); update_ready(m); }
  if (bd_seq_shape() != seen_shape) {
    seen_shape = bd_seq_shape();
    g_pv_ok = g_pv && bd_active_shape()->id && bd_profile_preview(bd_active_shape(), 400, 110, g_pv);
    lv_obj_invalidate(g_plan);
    update_ready(m);
  }
  if (bd_seq_profiles() != seen_p) {
    seen_p = bd_seq_profiles();
    if (!lv_obj_has_flag(g_prof, LV_OBJ_FLAG_HIDDEN)) fill_profiles();
  }
  if (now - last_clock > 1000) { last_clock = now; update_clock(); }
  if (g_note_until && now > g_note_until) { g_note_until = 0; lv_obj_add_flag(g_note, LV_OBJ_FLAG_HIDDEN); }
  if (g_brewing_shown && g_brew_done_at && now - g_brew_done_at > 60000) brew_close(NULL);
  const bool steaming = m->link == BD_LINK_ONLINE && (m->steaming || !strcmp(m->mode, "STEAM"));
  if (steaming && !g_steam_shown) steam_open(m);
  else if (steaming) steam_step(m);
  else if (g_steam_shown) steam_close();
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
    case BD_EV_COMMAND:
      if (e->u.command.answer != BD_ANSWER_OK) note("THE MACHINE DID NOT TAKE IT");
      break;
    default: break;   // notices, stored shots, Wi-Fi: not used yet
  }
}

void bd_ui_wake(void) {}
