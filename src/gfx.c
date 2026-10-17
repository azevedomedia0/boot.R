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

void gfx_flush(void) {
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

INTN text_w(const Font *f, const char *s, INTN sc) {
  INTN w = 0;
  for (; *s; s++) { int ch = (unsigned char)*s; if (ch < 32 || ch > 126) ch = '?'; w += f->adv[ch - 32] * sc; }
  return w;
}

void draw_text(const Font *f, INTN x, INTN y, const char *s, UINT32 c, INTN sc) {
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

void draw_mask(int icon, INTN x, INTN y, INTN size, UINT32 c) {
  const UINT8 *m = masks[icon]; INTN i, j;
  for (j = 0; j < size; j++) {
    INTN fy = ((2 * j + 1) * 48 * 128) / size - 128, y0 = fy >> 8, ty = fy & 255;
    INTN ya = cl(y0, 48), yb = cl(y0 + 1, 48);
    for (i = 0; i < size; i++) {
      INTN fx = ((2 * i + 1) * 48 * 128) / size - 128, x0 = fx >> 8, tx = fx & 255;
      INTN xa = cl(x0, 48), xb = cl(x0 + 1, 48);
      INTN a = (m[ya * 48 + xa] * (256 - tx) * (256 - ty) + m[ya * 48 + xb] * tx * (256 - ty) +
                m[yb * 48 + xa] * (256 - tx) * ty + m[yb * 48 + xb] * tx * ty) >> 16;
      px(x + i, y + j, c, (UINT32)a);
    }
  }
}

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

void draw_logo(int idx, INTN x, INTN y, INTN size) { draw_image(logos[idx], 64, x, y, size); }
void draw_icon(INTN x, INTN y, INTN size) { draw_image(icon_bootr, 128, x, y, size); }
