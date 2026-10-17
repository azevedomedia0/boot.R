/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Steven Azevedo */
#include "bm.h"

INTN SW, SH;
static UINT32 *bb;
static EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
static INTN cx0, cy0, cx1, cy1;

static UINT32 isq(UINT32 n) {
  UINT32 r = 0, b = 1u << 30;
  while (b > n) b >>= 2;
  while (b) { if (n >= r + b) { n -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
  return r;
}

EFI_STATUS gfx_init(void) {
  EFI_GUID g = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
  EFI_STATUS s;
  UINT32 i, best, bw = 0, bh = 0;
  if (bb) return EFI_SUCCESS;
  s = uefi_call_wrapper(BS->LocateProtocol, 3, &g, NULL, (void **)&gop);
  if (EFI_ERROR(s)) return s;
  best = gop->Mode->Mode;
  for (i = 0; i < gop->Mode->MaxMode; i++) {
    UINTN sz; EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *inf;
    UINT32 w, h;
    if (EFI_ERROR(uefi_call_wrapper(gop->QueryMode, 4, gop, i, &sz, &inf))) continue;
    w = inf->HorizontalResolution; h = inf->VerticalResolution;
    FreePool(inf);
    if ((UINT64)w * h > (UINT64)bw * bh) { bw = w; bh = h; best = i; }
  }
  if (bw && best != gop->Mode->Mode) uefi_call_wrapper(gop->SetMode, 2, gop, best);
  SW = gop->Mode->Info->HorizontalResolution;
  SH = gop->Mode->Info->VerticalResolution;
  bb = AllocatePool((UINTN)SW * (UINTN)SH * 4);
  if (!bb) return EFI_OUT_OF_RESOURCES;
  gfx_noclip();
  return EFI_SUCCESS;
}

/* Mouse pointer: an arrow sprite composited over the back buffer and blitted on its own, so moving it
 * does not redraw the screen. gfx_flush puts it back after each full redraw. */
#define CUR_W 12
#define CUR_H 19
static const char *cur_art[CUR_H] = {
  "X           ", "XX          ", "X.X         ", "X..X        ", "X...X       ", "X....X      ",
  "X.....X     ", "X......X    ", "X.......X   ", "X........X  ", "X.....XXXXX ", "X..X..X     ",
  "X.X X..X    ", "XX  X..X    ", "X    X..X   ", "     X..X   ", "      X..X  ", "      X..X  ",
  "       XX   " };
static UINT32 cur_save[CUR_W * 2 * CUR_H * 2];
static INTN cur_x, cur_y; static int cur_vis;

/* The sprite sits at the pointer, nudged inward at the right and bottom edges so it never gets clipped away. */
static void cur_rect(INTN *x, INTN *y, INTN *w, INTN *h) {
  INTN s = SH >= 1400 ? 2 : 1;
  *w = CUR_W * s; *h = CUR_H * s; *x = cur_x; *y = cur_y;
  if (*x > SW - *w) *x = SW - *w;
  if (*y > SH - *h) *y = SH - *h;
  if (*x < 0) *x = 0;
  if (*y < 0) *y = 0;
}

/* Keeps the pixels under the sprite in cur_save, so moving it is just: put them back, save, draw. */
static void cur_under(INTN x, INTN y, INTN w, INTN h, int restore) {
  INTN i, j;
  for (j = 0; j < h; j++) for (i = 0; i < w; i++) {
    if (restore) bb[(y + j) * SW + x + i] = cur_save[j * w + i];
    else cur_save[j * w + i] = bb[(y + j) * SW + x + i];
  }
}

static void cur_draw(INTN x, INTN y, INTN w, INTN h) {
  INTN s = w / CUR_W, i, j;
  for (j = 0; j < h; j++) for (i = 0; i < w; i++) {
    char a = cur_art[j / s][i / s];
    if (a == 'X') bb[(y + j) * SW + x + i] = 0x000000; else if (a == '.') bb[(y + j) * SW + x + i] = 0xffffff;
  }
}

/* Sends just this rectangle of the back buffer to the screen, the same call gfx_flush makes for the whole of it. */
static void cur_present(INTN x, INTN y, INTN w, INTN h) {
  uefi_call_wrapper(gop->Blt, 10, gop, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)bb, EfiBltBufferToVideo,
                    (UINTN)x, (UINTN)y, (UINTN)x, (UINTN)y, (UINTN)w, (UINTN)h, (UINTN)SW * 4);
}

static void cur_erase(void) {
  INTN x, y, w, h;
  cur_rect(&x, &y, &w, &h);
  cur_under(x, y, w, h, 1);
  cur_present(x, y, w, h);
}

void gfx_cursor_move(INTN x, INTN y) {
  INTN rx, ry, w, h;
  if (cur_vis) cur_erase();
  cur_x = x; cur_y = y; cur_vis = 1;
  cur_rect(&rx, &ry, &w, &h);
  cur_under(rx, ry, w, h, 0);
  cur_draw(rx, ry, w, h);
  cur_present(rx, ry, w, h);
}

void gfx_cursor_hide(void) {
  if (cur_vis) cur_erase();
  cur_vis = 0;
}

void gfx_flush(void) {
  if (cur_vis) {
    INTN x, y, w, h;
    cur_rect(&x, &y, &w, &h);
    cur_under(x, y, w, h, 0);
    cur_draw(x, y, w, h);
  }
  uefi_call_wrapper(gop->Blt, 10, gop, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)bb, EfiBltBufferToVideo,
                    (UINTN)0, (UINTN)0, (UINTN)0, (UINTN)0, (UINTN)SW, (UINTN)SH, (UINTN)0);
}

void gfx_noclip(void) { cx0 = 0; cy0 = 0; cx1 = SW; cy1 = SH; }
void gfx_clip(INTN x, INTN y, INTN w, INTN h) {
  cx0 = x < 0 ? 0 : x; cy0 = y < 0 ? 0 : y;
  cx1 = x + w > SW ? SW : x + w; cy1 = y + h > SH ? SH : y + h;
}

static void px(INTN x, INTN y, UINT32 c, UINT32 a) {
  UINT32 *p, d, r, g, b;
  if (a == 0 || x < cx0 || y < cy0 || x >= cx1 || y >= cy1) return;
  p = &bb[y * SW + x];
  if (a >= 255) { *p = c; return; }
  d = *p;
  r = (((c >> 16) & 255) * a + ((d >> 16) & 255) * (255 - a)) / 255;
  g = (((c >> 8) & 255) * a + ((d >> 8) & 255) * (255 - a)) / 255;
  b = ((c & 255) * a + (d & 255) * (255 - a)) / 255;
  *p = (r << 16) | (g << 8) | b;
}

void fill_rect(INTN x, INTN y, INTN w, INTN h, UINT32 c) {
  INTN i, j, x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
  INTN x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
  for (j = y0; j < y1; j++) for (i = x0; i < x1; i++) bb[j * SW + i] = c;
}

void fill_rrect(INTN x, INTN y, INTN w, INTN h, INTN r, UINT32 c) {
  INTN i, j, x0, y0, x1, y1;
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;
  x0 = x < cx0 ? cx0 : x; y0 = y < cy0 ? cy0 : y;
  x1 = x + w > cx1 ? cx1 : x + w; y1 = y + h > cy1 ? cy1 : y + h;
  for (j = y0; j < y1; j++) {
    INTN ly = j - y, dyc = 0;
    if (ly < r) dyc = r * 16 - (ly * 16 + 8); else if (ly >= h - r) dyc = (ly * 16 + 8) - (h - r) * 16;
    for (i = x0; i < x1; i++) {
      INTN lx = i - x, dxc = 0; UINT32 a = 255;
      if (lx < r) dxc = r * 16 - (lx * 16 + 8); else if (lx >= w - r) dxc = (lx * 16 + 8) - (w - r) * 16;
      if (dxc > 0 && dyc > 0) {
        INTN d = (INTN)isq((UINT32)(dxc * dxc + dyc * dyc));
        INTN cv = ((r * 16 - d) + 8) * 255 / 16;
        a = cv < 0 ? 0 : cv > 255 ? 255 : (UINT32)cv;
      }
      px(i, j, c, a);
    }
  }
}

/* At scale 2 use the font's own 2x glyphs when it has them, rather than doubling the 1x pixels. */
static const Font *pick(const Font *f, INTN *sc) {
  if (*sc == 2 && f->hi) { *sc = 1; return f->hi; }
  return f;
}
INTN text_w(const Font *f, const char *s, INTN sc) {
  INTN w = 0;
  f = pick(f, &sc);
  for (; *s; s++) { int ch = (unsigned char)*s; if (ch < 32 || ch > 126) ch = '?'; w += f->adv[ch - 32] * sc; }
  return w;
}

void draw_text(const Font *f, INTN x, INTN y, const char *s, UINT32 c, INTN sc) {
  f = pick(f, &sc);
  for (; *s; s++) {
    int ch = (unsigned char)*s; INTN gi, gx, gy, sx, sy; const UINT8 *g;
    if (ch < 32 || ch > 126) ch = '?';
    gi = ch - 32; g = f->data + gi * f->cw * f->ch;
    for (gy = 0; gy < f->ch; gy++) for (gx = 0; gx < f->cw; gx++) {
      UINT8 a = g[gy * f->cw + gx];
      if (!a) continue;
      for (sy = 0; sy < sc; sy++) for (sx = 0; sx < sc; sx++) px(x + gx * sc + sx, y + gy * sc + sy, c, a);
    }
    x += f->adv[gi] * sc;
  }
}

static INTN cl(INTN v, INTN n) { return v < 0 ? 0 : v >= n ? n - 1 : v; }

/* Draws an n x n alpha mask in colour c at size x size, bilinearly resampled. */
void draw_alpha(const UINT8 *m, INTN n, INTN x, INTN y, INTN size, UINT32 c) {
  INTN i, j;
  for (j = 0; j < size; j++) {
    INTN fy = ((2 * j + 1) * n * 128) / size - 128, y0 = fy >> 8, ty = fy & 255;
    INTN ya = cl(y0, n), yb = cl(y0 + 1, n);
    for (i = 0; i < size; i++) {
      INTN fx = ((2 * i + 1) * n * 128) / size - 128, x0 = fx >> 8, tx = fx & 255;
      INTN xa = cl(x0, n), xb = cl(x0 + 1, n);
      INTN a = (m[ya * n + xa] * (256 - tx) * (256 - ty) + m[ya * n + xb] * tx * (256 - ty) +
                m[yb * n + xa] * (256 - tx) * ty + m[yb * n + xb] * tx * ty) >> 16;
      px(x + i, y + j, c, (UINT32)a);
    }
  }
}
void draw_mask(int icon, INTN x, INTN y, INTN size, UINT32 c) { draw_alpha(masks[icon], 48, x, y, size, c); }
static void tex(UINT32 t, INTN *o) {
  INTN a = t >> 24;
  o[0] = a; o[1] = (((t >> 16) & 255) * a) / 255; o[2] = (((t >> 8) & 255) * a) / 255; o[3] = ((t & 255) * a) / 255;
}

void draw_image(const UINT32 *m, INTN n, INTN x, INTN y, INTN size) {
  INTN i, j, k;
  for (j = 0; j < size; j++) {
    INTN fy = ((2 * j + 1) * n * 128) / size - 128, y0 = fy >> 8, ty = fy & 255;
    INTN ya = cl(y0, n), yb = cl(y0 + 1, n);
    for (i = 0; i < size; i++) {
      INTN fx = ((2 * i + 1) * n * 128) / size - 128, x0 = fx >> 8, tx = fx & 255;
      INTN xa = cl(x0, n), xb = cl(x0 + 1, n);
      INTN t00[4], t10[4], t01[4], t11[4], o[4];
      tex(m[ya * n + xa], t00); tex(m[ya * n + xb], t10); tex(m[yb * n + xa], t01); tex(m[yb * n + xb], t11);
      for (k = 0; k < 4; k++)
        o[k] = (t00[k] * (256 - tx) * (256 - ty) + t10[k] * tx * (256 - ty) + t01[k] * (256 - tx) * ty + t11[k] * tx * ty) >> 16;
      if (o[0] > 0 && x + i >= cx0 && y + j >= cy0 && x + i < cx1 && y + j < cy1) {
        UINT32 *p = &bb[(y + j) * SW + x + i], d = *p;
        INTN ia = 255 - o[0];
        INTN r = o[1] + (((d >> 16) & 255) * ia) / 255, g = o[2] + (((d >> 8) & 255) * ia) / 255, b = o[3] + ((d & 255) * ia) / 255;
        if (r > 255) { r = 255; }
        if (g > 255) { g = 255; }
        if (b > 255) { b = 255; }
        *p = ((UINT32)r << 16) | ((UINT32)g << 8) | (UINT32)b;
      }
    }
  }
}

/* Past 64 px the 64x64 logos would be stretched and soft, so use the 128x128 masks (see tools/mklogos.py). */
void draw_logo(int idx, INTN x, INTN y, INTN size) {
  if (size > 64) draw_alpha(logos_hi[idx], 128, x, y, size, logo_col[idx]);
  else draw_image(logos[idx], 64, x, y, size);
}
void draw_icon(INTN x, INTN y, INTN size) { draw_image(icon_bootr, 128, x, y, size); }
