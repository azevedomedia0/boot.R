/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Steven Azevedo */
#include "bm.h"

static INTN sc = 1;
static int show_diag;
#define U(n) ((INTN)(n) * sc)

static void scat(char *d, const char *s) { while (*d) d++; while ((*d++ = *s++)) ; }
static void itoa_(INTN v, char *o) {
  char t[12]; int n = 0, i;
  if (v <= 0) { o[0] = '0'; o[1] = 0; return; }
  while (v) { t[n++] = '0' + v % 10; v /= 10; }
  for (i = 0; i < n; i++) o[i] = t[n - 1 - i];
  o[n] = 0;
}
static int seq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static void fit(const Font *f, const char *s, INTN maxw, char *out) {
  INTN n = 0;
  while (s[n] && n < 60) { out[n] = s[n]; n++; }
  out[n] = 0;
  if (text_w(f, out, sc) <= maxw) return;
  while (n > 1) {
    n--; out[n] = '.'; out[n + 1] = '.'; out[n + 2] = '.'; out[n + 3] = 0;
    if (text_w(f, out, sc) <= maxw) return;
  }
}

static void chip(INTN ent, INTN x, INTN y, INTN d, UINT32 chipcol, UINT32 maskcol, INTN isz) {
  INTN ic = ents[ent].icon;
  if (ic >= 0) { fill_rrect(x, y, d, d, d / 2, C_TXT); draw_logo((int)ic, x + (d - isz) / 2, y + (d - isz) / 2, isz); }
  else { fill_rrect(x, y, d, d, d / 2, chipcol); draw_mask(ents[ent].kind == 1 ? I_CPU : I_DISK, x + (d - isz) / 2, y + (d - isz) / 2, isz, maskcol); }
}

static void cbtn(INTN x, INTN y, INTN d, UINT32 bg, int ic, UINT32 fg, INTN isz, int ring, UINT32 ringbg) {
  if (ring) {
    fill_rrect(x - U(4), y - U(4), d + U(8), d + U(8), (d + U(8)) / 2, C_ACC);
    fill_rrect(x - U(2), y - U(2), d + U(4), d + U(4), (d + U(4)) / 2, ringbg);
  }
  fill_rrect(x, y, d, d, d / 2, bg);
  draw_mask(ic, x + (d - isz) / 2, y + (d - isz) / 2, isz, fg);
}

/* ---------- menu ---------- */
static void draw_menu(INTN focus, INTN sel, INTN cd, VRow *all, INTN *vis, INTN nv) {
  INTN btn = U(52), gap = U(16), pw = U(880), maxpw = SW - 2 * (btn + gap + U(24));
  INTN FH = U(34), ph, px, py, hy, i;
  INTN vx, vw, vy, tw = U(200), tg = U(12), total, x0, fy, bx;
  if (pw > maxpw) pw = maxpw;
  ph = U(28) + U(42) + U(20) + U(200) + U(20) + FH + U(28);
  px = (SW - pw) / 2; py = (SH - ph) / 2; hy = py + U(28);
  gfx_noclip();
  hit_reset();
  fill_rect(0, 0, SW, SH, C_BG);
  fill_rrect(px, py, pw, ph, U(28), C_PANEL);
  draw_text(&font_big, px + U(34), hy + (U(42) - font_big.ch * sc) / 2, "Select Operating System", C_TXT, sc);
  bx = px + pw - U(28) - U(42);
  hit_add(bx, hy, U(42), U(42), H_SETTINGS, 0, 0);
  hit_add(bx - U(52), hy, U(42), U(42), H_REFRESH, 0, 0);
  hit_add(px - gap - btn, py + (ph - btn) / 2, btn, btn, H_PREV, 0, 0);
  hit_add(px + pw + gap, py + (ph - btn) / 2, btn, btn, H_NEXT, 0, 0);
  cbtn(bx, hy, U(42), C_BTN, I_SETTINGS, C_TXT, U(18), focus == 2, C_PANEL);
  cbtn(bx - U(52), hy, U(42), C_BTN, I_REFRESH, C_TXT, U(18), focus == 1, C_PANEL);
  cbtn(px - gap - btn, py + (ph - btn) / 2, btn, C_BTN, I_LEFT, C_TXT, U(22), 0, C_BG);
  cbtn(px + pw + gap, py + (ph - btn) / 2, btn, C_BTN, I_RIGHT, C_TXT, U(22), 0, C_BG);
  vx = px + U(28); vw = pw - U(56); vy = hy + U(42) + U(20);
  if (nv == 0) {
    const char *m = "No systems found. Press R to rescan.";
    draw_text(&font_med, vx + (vw - text_w(&font_med, m, sc)) / 2, vy + U(86), m, C_DIM, sc);
  } else {
    total = nv * tw + (nv - 1) * tg;
    if (total <= vw) x0 = vx + (vw - total) / 2;
    else { INTN s = sel * (tw + tg) + tw / 2 - vw / 2; if (s < 0) s = 0; if (s > total - vw) s = total - vw; x0 = vx - s; }
    gfx_clip(vx, vy, vw, U(200));
    hit_clip(vx, vy, vw, U(200));
    for (i = 0; i < nv; i++) {
      VRow v = all[vis[i]]; INTN x = x0 + i * (tw + tg), on = (i == sel), cy = vy + U(34), d = U(88);
      char nm[70];
      fill_rrect(x, vy, tw, U(200), U(20), on ? C_ACC : C_TILE);
      hit_add(x, vy, tw, U(200), H_TILE, i, 0);
      chip(v.ent, x + (tw - d) / 2, cy, d, on ? 0x5e8bb6 : 0x51575d, on ? C_DARK : C_TXT, ents[v.ent].icon >= 0 ? U(52) : U(46));
      fit(&font_med, disp_name(v), tw - U(24), nm);
      draw_text(&font_med, x + (tw - text_w(&font_med, nm, sc)) / 2, cy + d + U(14), nm, on ? C_DARK : C_TXT, sc);
    }
    gfx_noclip();
  }
  fy = py + ph - U(28) - FH;
  if (!cfg.hide_hints)
    draw_text(&font_small, px + U(34), fy + (FH - font_small.ch * sc) / 2,
              "Arrows select    Enter boots    Up: rescan and settings    R rescans", C_DIM, sc);
  if (cd > 0 && nv > 0) {
    char t[120] = "Booting ", n[12], nm[40];
    fit(&font_small, disp_name(all[vis[sel]]), U(200), nm);
    scat(t, nm); scat(t, " in "); itoa_(cd, n); scat(t, n); scat(t, "s");
    { INTN w = text_w(&font_small, t, sc) + U(32), x = px + pw - U(28) - w;
      fill_rrect(x, fy, w, FH, FH / 2, C_ACC);
      draw_text(&font_small, x + U(16), fy + (FH - font_small.ch * sc) / 2, t, C_DARK, sc); }
  }
  if (show_diag) {
    INTN k, n = diag_count(), lh = font_small.ch * sc + U(2); char st[100];
    diag_stats(st);
    fill_rect(0, 0, SW / 2 > U(760) ? SW / 2 : U(760), U(20) + (n + 2) * lh, 0x000000);
    draw_text(&font_small, U(12), U(10), "Input info (press I to hide)", C_ACC, sc);
    for (k = 0; k < n; k++) draw_text(&font_small, U(12), U(10) + (k + 1) * lh, diag_line(k), C_TXT, sc);
    if (st[0]) draw_text(&font_small, U(12), U(10) + (n + 1) * lh, st, C_TXT, sc);
  }
  gfx_flush();
}

/* ---------- settings ---------- */
static void group(INTN xr, INTN yc, const char **lb, INTN n, INTN sel, INTN row) {
  INTN hh = U(40), pd = U(4), w = pd * 2, i, x, y, cx;
  for (i = 0; i < n; i++) w += text_w(&font_small, lb[i], sc) + U(36);
  x = xr - w; y = yc - hh / 2;
  fill_rrect(x, y, w, hh, hh / 2, C_PANEL);
  cx = x + pd;
  for (i = 0; i < n; i++) {
    INTN tw2 = text_w(&font_small, lb[i], sc) + U(36);
    hit_add(cx, y + pd, tw2, hh - 2 * pd, H_SEG, row, i);
    if (i == sel) fill_rrect(cx, y + pd, tw2, hh - 2 * pd, (hh - 2 * pd) / 2, C_ACC);
    draw_text(&font_small, cx + U(18), y + (hh - font_small.ch * sc) / 2, lb[i], i == sel ? C_DARK : C_TXT, sc);
    cx += tw2;
  }
}

static void ring_rr(INTN x, INTN y, INTN w, INTN h, INTN r, UINT32 bg) {
  fill_rrect(x - U(4), y - U(4), w + U(8), h + U(8), r + U(4), C_ACC);
  fill_rrect(x - U(2), y - U(2), w + U(4), h + U(4), r + U(2), bg);
}

static const UINT32 timers[4] = { 0, 5, 10, 30 };

static void draw_settings(INTN frow, INTN fcol, int renaming, const char *rbuf, VRow *all, INTN nall, INTN *scroll) {
  INTN pw = U(880), maxw = SW - U(48), pad = U(28), hdr = U(42) + U(16);
  INTN RH = U(76), G = U(12), ERH = U(52), EG = U(8), bodyH, ph, maxh, px, py, cw, bx, vtop, vh, ft = 0, fb = 0, maxs, y0, k, i;
  int fits;
  if (pw > maxw) pw = maxw;
  bodyH = 3 * (RH + G) + U(72) + nall * (ERH + EG);
  ph = pad + hdr + bodyH + pad; maxh = SH - U(48); fits = ph <= maxh; if (!fits) ph = maxh;
  px = (SW - pw) / 2; py = (SH - ph) / 2; cw = pw - 2 * pad; bx = px + pad; vtop = py + pad + hdr; vh = ph - pad - hdr - pad;
  if (frow >= 0 && frow < 3) { ft = frow * (RH + G); fb = ft + RH; }
  else if (frow >= 3) { ft = 3 * (RH + G) + U(72) + (frow - 3) * (ERH + EG); fb = ft + ERH; }
  if (fits || frow < 0) *scroll = 0;
  else { if (ft < *scroll) *scroll = ft; if (fb > *scroll + vh) *scroll = fb - vh; }
  maxs = bodyH - vh; if (maxs < 0) maxs = 0; if (*scroll > maxs) *scroll = maxs; if (*scroll < 0) *scroll = 0;

  gfx_noclip();
  hit_reset();
  fill_rect(0, 0, SW, SH, C_BG);
  fill_rrect(px, py, pw, ph, U(28), C_PANEL);
  hit_add(bx, py + pad, U(42), U(42), H_BACK, 0, 0);
  cbtn(bx, py + pad, U(42), C_BTN, I_BACK, C_TXT, U(20), frow == -1, C_PANEL);
  draw_text(&font_big, bx + U(42) + U(14), py + pad + (U(42) - font_big.ch * sc) / 2, "Settings", C_TXT, sc);

  if (maxs > 0) {
    /* Scroll bar in the panel's right margin: the thumb shows where the visible part sits in the whole list. */
    INTN bw = U(6), sx = px + pw - U(20), th = vh * vh / bodyH, ty;
    if (th < U(32)) th = U(32);
    ty = vtop + (vh - th) * *scroll / maxs;
    fill_rrect(sx, vtop, bw, vh, bw / 2, C_TILE);
    fill_rrect(sx, ty, bw, th, bw / 2, C_DIM);
    hit_add(sx - U(8), vtop, bw + U(16), ty - vtop, H_SCROLL, -1, 0);
    hit_add(sx - U(8), ty + th, bw + U(16), vtop + vh - ty - th, H_SCROLL, 1, 0);
  }
  gfx_clip(bx - U(8), vtop - U(2), cw + U(16), vh + U(4));
  hit_clip(bx - U(8), vtop - U(2), cw + U(16), vh + U(4));
  y0 = vtop - *scroll;
  for (i = 0; i < 3; i++) {
    INTN yi = y0 + i * (RH + G), yc = yi + RH / 2;
    const char *lab, *desc;
    if (i == 0) { lab = "Default system"; desc = "Pre-selected when the menu opens."; }
    else if (i == 1) { lab = "Auto-boot timer"; desc = "Boots after a countdown. Off waits for you."; }
    else { lab = "Hide boot instructions"; desc = "Removes the key hints from the menu."; }
    hit_add(bx, yi, cw, RH, H_ROW, i, 0);
    if (frow == i) ring_rr(bx, yi, cw, RH, U(20), C_PANEL);
    fill_rrect(bx, yi, cw, RH, U(20), C_TILE);
    draw_text(&font_med, bx + U(20), yi + U(10), lab, C_TXT, sc);
    draw_text(&font_small, bx + U(20), yi + U(10) + font_med.ch * sc + U(2), desc, C_DIM, sc);
    if (i == 0) {
      char t[60] = "<  "; INTN d;
      for (d = 0; d < nall; d++) if (seq(cfg.e[all[d].sidx].key, cfg.def)) { char nm[40]; fit(&font_small, disp_name(all[d]), U(220), nm); scat(t, nm); break; }
      scat(t, "  >");
      { const char *lb[1]; lb[0] = t; group(bx + cw - U(20), yc, lb, 1, 0, 0); }
    } else if (i == 1) {
      const char *lb[4] = { "Off", "5s", "10s", "30s" }; INTN s = 0, q;
      for (q = 0; q < 4; q++) if (timers[q] == cfg.timer) s = q;
      group(bx + cw - U(20), yc, lb, 4, s, 1);
    } else {
      const char *lb[2] = { "Off", "On" };
      group(bx + cw - U(20), yc, lb, 2, cfg.hide_hints ? 1 : 0, 2);
    }
  }
  { INTN sy = y0 + 3 * (RH + G);
    draw_text(&font_med, bx + U(6), sy + U(4), "Edit operating systems", C_TXT, sc);
    draw_text(&font_small, bx + U(6), sy + U(4) + font_med.ch * sc + U(2), "Pencil renames. Eye hides. Arrows reorder.", C_DIM, sc); }
  for (k = 0; k < nall; k++) {
    VRow v = all[k]; INTN ey = y0 + 3 * (RH + G) + U(72) + k * (ERH + EG), si = v.sidx, hid = cfg.e[si].hidden, rx = bx + cw - U(8) - U(36);
    int isf = (frow == 3 + k), ren = isf && renaming;
    char nm[70];
    hit_add(bx, ey, cw, ERH, H_ROW, 3 + k, 0);
    hit_add(rx - U(132), ey + U(8), U(36), U(36), H_BTN, k, 0);
    hit_add(rx - U(88), ey + U(8), U(36), U(36), H_BTN, k, 1);
    hit_add(rx - U(44), ey + U(8), U(36), U(36), H_BTN, k, 2);
    hit_add(rx, ey + U(8), U(36), U(36), H_BTN, k, 3);
    fill_rrect(bx, ey, cw, ERH, ERH / 2, C_TILE);
    chip(v.ent, bx + U(8), ey + U(8), U(36), 0x51575d, C_TXT, U(20));
    if (ren) {
      INTN ix = bx + U(56), iw = cw - U(56) - U(8) - (4 * U(36) + 3 * U(8)) - U(8);
      char t[40] = ""; scat(t, rbuf); scat(t, "_");
      fill_rrect(ix, ey + U(7), iw, ERH - U(14), (ERH - U(14)) / 2, C_ACC);
      fill_rrect(ix + U(2), ey + U(9), iw - U(4), ERH - U(18), (ERH - U(18)) / 2, 0x2a2f34);
      draw_text(&font_small, ix + U(16), ey + (ERH - font_small.ch * sc) / 2, t, C_TXT, sc);
    } else {
      fit(&font_med, disp_name(v), cw - U(56) - U(180), nm);
      draw_text(&font_med, bx + U(56), ey + (ERH - font_med.ch * sc) / 2, nm, hid ? 0x8b8f92 : C_TXT, sc);
    }
    cbtn(rx - U(132), ey + U(8), U(36), C_BTN, ren ? I_CHECK : I_PENCIL, C_TXT, U(18), isf && fcol == 0, C_TILE);
    cbtn(rx - U(88), ey + U(8), U(36), hid ? C_BTN : C_ACC, hid ? I_EYEOFF : I_EYE, hid ? C_TXT : C_DARK, U(18), isf && fcol == 1, C_TILE);
    cbtn(rx - U(44), ey + U(8), U(36), C_BTN, I_UP, k == 0 ? 0x7c8084 : C_TXT, U(18), isf && fcol == 2, C_TILE);
    cbtn(rx, ey + U(8), U(36), C_BTN, I_DOWN, k == nall - 1 ? 0x7c8084 : C_TXT, U(18), isf && fcol == 3, C_TILE);
  }
  gfx_noclip();
  gfx_flush();
}

static void settings_screen(void) {
  VRow all[MAXE]; INTN nall, frow = 0, fcol = 0, scroll = 0, rlen = 0; int renaming = 0; char rbuf[32] = "";
  for (;;) {
    EFI_INPUT_KEY k; INTN last, cx, cy; int ev;
    nall = build_view(all);
    last = 2 + nall;
    draw_settings(frow, fcol, renaming, rbuf, all, nall, &scroll);
    ev = wait_input(NULL, &k, &cx, &cy);
    if (ev == EV_WHEEL) {
      if (!renaming) { if (cx > 0) { if (frow < last) frow++; } else if (frow > -1) frow--; }
      continue;
    }
    if (ev == EV_CLICK) {
      /* A click is turned into the key press that does the same thing. */
      const Hit *h = hit_find(cx, cy);
      if (renaming && !(h && h->kind == H_BTN && h->a == frow - 3 && h->b == 0)) renaming = 0;
      if (!h) continue;
      k.ScanCode = 0; k.UnicodeChar = 0;
      if (h->kind == H_BACK) { frow = -1; k.UnicodeChar = CHAR_CARRIAGE_RETURN; }
      else if (h->kind == H_ROW) { frow = h->a; continue; }
      else if (h->kind == H_SCROLL) {
        /* Clicking the bar above or below the thumb moves a page by moving the focus, which the view follows. */
        INTN to = (frow < 0 ? 0 : frow) + h->a * 6;
        frow = to < 0 ? 0 : to > last ? last : to;
        continue;
      }
      else if (h->kind == H_SEG) {
        frow = h->a;
        if (h->a == 0) k.ScanCode = cx < h->x + h->w / 2 ? SCAN_LEFT : SCAN_RIGHT;
        else if (h->a == 1) { cfg.timer = timers[h->b]; continue; }
        else { cfg.hide_hints = (int)h->b; continue; }
      } else if (h->kind == H_BTN) { frow = 3 + h->a; fcol = h->b; k.UnicodeChar = CHAR_CARRIAGE_RETURN; }
      else continue;
    }
    if (renaming) {
      VRow v = all[frow - 3];
      if (k.UnicodeChar == CHAR_CARRIAGE_RETURN) {
        INTN q; for (q = 0; q < rlen; q++) cfg.e[v.sidx].name[q] = rbuf[q];
        cfg.e[v.sidx].name[rlen] = 0; renaming = 0;
      } else if (k.ScanCode == SCAN_ESC) renaming = 0;
      else if (k.UnicodeChar == CHAR_BACKSPACE) { if (rlen > 0) rbuf[--rlen] = 0; }
      else if (k.UnicodeChar >= 32 && k.UnicodeChar < 127 && rlen < 24) { rbuf[rlen++] = (char)k.UnicodeChar; rbuf[rlen] = 0; }
      continue;
    }
    if (k.ScanCode == SCAN_ESC) break;
    if (k.ScanCode == SCAN_UP) { if (frow > -1) frow--; }
    else if (k.ScanCode == SCAN_DOWN) { if (frow < last) frow++; }
    else if (k.ScanCode == SCAN_LEFT || k.ScanCode == SCAN_RIGHT || k.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      INTN dir = k.ScanCode == SCAN_LEFT ? -1 : 1; int enter = k.UnicodeChar == CHAR_CARRIAGE_RETURN;
      if (frow == -1) { if (enter) break; }
      else if (frow == 0 && nall) {
        INTN d = 0, q; for (q = 0; q < nall; q++) if (seq(cfg.e[all[q].sidx].key, cfg.def)) d = q;
        d = (d + dir + nall) % nall;
        { INTN q2; for (q2 = 0; q2 < 24; q2++) cfg.def[q2] = cfg.e[all[d].sidx].key[q2]; }
      } else if (frow == 1) {
        INTN s = 0, q; for (q = 0; q < 4; q++) if (timers[q] == cfg.timer) s = q;
        cfg.timer = timers[(s + dir + 4) % 4];
      } else if (frow == 2) cfg.hide_hints = !cfg.hide_hints;
      else if (frow >= 3) {
        INTN kk = frow - 3;
        if (!enter) { fcol += dir; if (fcol < 0) fcol = 0; if (fcol > 3) fcol = 3; }
        else if (fcol == 0) {
          const char *nm = disp_name(all[kk]); INTN q = 0;
          while (nm[q] && q < 24) { rbuf[q] = nm[q]; q++; } rbuf[q] = 0; rlen = q; renaming = 1;
        } else if (fcol == 1) cfg.e[all[kk].sidx].hidden = !cfg.e[all[kk].sidx].hidden;
        else {
          INTN to = fcol == 2 ? kk - 1 : kk + 1;
          if (to >= 0 && to < nall) {
            SEnt t = cfg.e[all[kk].sidx]; cfg.e[all[kk].sidx] = cfg.e[all[to].sidx]; cfg.e[all[to].sidx] = t;
            frow = 3 + to;
          }
        }
      }
    }
  }
  settings_save();
}

/* ---------- boot ---------- */
static void draw_boot(const char *name, INTN pct) {
  INTN m = U(48), bh = U(8), by = SH - m - bh, w = SW - 2 * m;
  gfx_noclip();
  fill_rect(0, 0, SW, SH, C_BG);
  draw_icon(m, m, U(64));
  draw_text(&font_big, m + U(64) + U(18), m + (U(64) - font_big.ch * sc) / 2, APP_NAME, C_TXT, sc);
  draw_text(&font_small, m, m + U(64) + U(28), "BOOTING", C_DIM, sc);
  draw_text(&font_big, m, SH / 2 - font_big.ch * sc, name, C_TXT, sc * 2);
  fill_rrect(m, by, w, bh, bh / 2, 0x514f4f);
  if (pct > 0) fill_rrect(m, by, w * pct / 100, bh, bh / 2, C_TXT);
  gfx_flush();
}

static void boot(EFI_HANDLE image, VRow v) {
  const char *name = disp_name(v); Entry *e = &ents[v.ent];
  settings_save();
  gfx_cursor_hide();
  if (e->kind == 1) {
    EFI_GUID g = EFI_GLOBAL_VARIABLE; UINT64 val = 0; UINTN sz = 8;
    uefi_call_wrapper(RT->GetVariable, 5, L"OsIndications", &g, NULL, &sz, &val);
    val |= 1;
    uefi_call_wrapper(RT->SetVariable, 5, L"OsIndications", &g,
                      EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS, (UINTN)8, &val);
    uefi_call_wrapper(RT->ResetSystem, 4, EfiResetCold, EFI_SUCCESS, (UINTN)0, NULL);
    return;
  }
  {
    EFI_DEVICE_PATH *dp = FileDevicePath(e->dev, e->path); EFI_HANDLE h = NULL; UINTN xs = 0;
    EFI_STATUS s = uefi_call_wrapper(BS->LoadImage, 6, FALSE, image, dp, NULL, (UINTN)0, &h);
    if (EFI_ERROR(s)) {
      draw_boot(name, 0);
      draw_text(&font_med, U(48), SH / 2 + U(20), "Could not load this system.", C_ACC, sc);
      gfx_flush(); uefi_call_wrapper(BS->Stall, 1, (UINTN)2000000);
      return;
    }
    if (e->opts[0]) {
      EFI_GUID lig = LOADED_IMAGE_PROTOCOL; EFI_LOADED_IMAGE *li = NULL;
      if (!EFI_ERROR(uefi_call_wrapper(BS->HandleProtocol, 3, h, &lig, (void **)&li)) && li) {
        li->LoadOptions = e->opts; li->LoadOptionsSize = (UINT32)((StrLen(e->opts) + 1) * 2);
      }
    }
    uefi_call_wrapper(BS->StartImage, 3, h, &xs, NULL);
  }
}

/* ---------- main loop ---------- */
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
  VRow all[MAXE]; INTN vis[MAXE], nall, nv, sel = 0, focus = 0, cd, i;
  EFI_EVENT tmr = NULL;
  InitializeLib(image, st);
  uefi_call_wrapper(BS->SetWatchdogTimer, 4, (UINTN)0, (UINT64)0, (UINTN)0, NULL);
  uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, FALSE);
  if (EFI_ERROR(gfx_init())) { Print(L"boot.R: no graphics output available.\n"); return EFI_UNSUPPORTED; }
  sc = SH >= 1400 ? 2 : 1;
  settings_load(); scan_all(image); settings_sync();
  mouse_init();
  uefi_call_wrapper(BS->CreateEvent, 5, EVT_TIMER, TPL_APPLICATION, NULL, NULL, &tmr);
  uefi_call_wrapper(BS->SetTimer, 3, tmr, TimerPeriodic, (UINT64)10000000);

#define REFRESH() do { nall = build_view(all); nv = 0; \
  for (i = 0; i < nall; i++) if (!cfg.e[all[i].sidx].hidden) vis[nv++] = i; \
  if (sel >= nv) sel = nv ? nv - 1 : 0; } while (0)
#define SELECT_DEFAULT() do { for (i = 0; i < nv; i++) if (seq(cfg.e[all[vis[i]].sidx].key, cfg.def)) sel = i; } while (0)
  REFRESH();
  SELECT_DEFAULT();
  cd = (cfg.timer > 0 && nv > 0) ? (INTN)cfg.timer : -1;

  for (;;) {
    EFI_INPUT_KEY k; INTN cx, cy; int ev;
    draw_menu(focus, sel, cd, all, vis, nv);
    ev = wait_input(tmr, &k, &cx, &cy);
    if (ev == EV_TICK) {
      if (cd > 0) { cd--; if (cd == 0) { cd = -1; if (nv) { boot(image, all[vis[sel]]); focus = 0; } } }
      continue;
    }
    cd = -1;
    if (ev == EV_WHEEL) {
      focus = 0;
      if (cx > 0) { if (sel < nv - 1) sel++; } else if (sel > 0) sel--;
      continue;
    }
    if (ev == EV_CLICK) {
      /* A click is turned into the key press that does the same thing. */
      const Hit *h = hit_find(cx, cy);
      if (!h) continue;
      k.ScanCode = 0; k.UnicodeChar = 0;
      if (h->kind == H_TILE) { focus = 0; if (h->a == sel) k.UnicodeChar = CHAR_CARRIAGE_RETURN; else { sel = h->a; continue; } }
      else if (h->kind == H_PREV) { focus = 0; if (sel > 0) sel--; continue; }
      else if (h->kind == H_NEXT) { focus = 0; if (sel < nv - 1) sel++; continue; }
      else if (h->kind == H_REFRESH) { focus = 1; k.UnicodeChar = CHAR_CARRIAGE_RETURN; }
      else if (h->kind == H_SETTINGS) { focus = 2; k.UnicodeChar = CHAR_CARRIAGE_RETURN; }
      else continue;
    }
    if (focus == 0) {
      if (k.ScanCode == SCAN_LEFT) { if (sel > 0) sel--; }
      else if (k.ScanCode == SCAN_RIGHT) { if (sel < nv - 1) sel++; }
      else if (k.ScanCode == SCAN_UP) focus = 1;
      else if (k.UnicodeChar == CHAR_CARRIAGE_RETURN) { if (nv) { boot(image, all[vis[sel]]); } }
      else if (k.UnicodeChar == 'r' || k.UnicodeChar == 'R') { scan_all(image); settings_sync(); REFRESH(); }
      else if (k.UnicodeChar == 'i' || k.UnicodeChar == 'I') show_diag = !show_diag;
      else if (k.UnicodeChar == 't' || k.UnicodeChar == 'T') mouse_i2c_retry();
    } else {
      if (k.ScanCode == SCAN_DOWN) focus = 0;
      else if (k.ScanCode == SCAN_LEFT) focus = 1;
      else if (k.ScanCode == SCAN_RIGHT) focus = 2;
      else if (k.UnicodeChar == CHAR_CARRIAGE_RETURN) {
        if (focus == 1) { scan_all(image); settings_sync(); REFRESH(); }
        else { settings_screen(); REFRESH(); focus = 0; SELECT_DEFAULT(); cd = (cfg.timer > 0 && nv > 0) ? (INTN)cfg.timer : -1; }
      } else if (k.UnicodeChar == 'r' || k.UnicodeChar == 'R') { scan_all(image); settings_sync(); REFRESH(); }
    }
  }
  return EFI_SUCCESS;
}
