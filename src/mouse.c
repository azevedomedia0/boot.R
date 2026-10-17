/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Steven Azevedo */
#include "bm.h"

#define MAXP 4
static EFI_GUID GUARDG = {0x7d0b1c2e, 0x3a4f, 0x4b6e, {0x9a, 0x11, 0x5e, 0x2c, 0x70, 0xb1, 0x3d, 0x43}};
static EFI_GUID SPG = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
static EFI_GUID APG = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;
static EFI_SIMPLE_POINTER *sp[MAXP]; static EFI_ABSOLUTE_POINTER *ap[MAXP];
static INTN nsp, nap;
static UINT8 sp_down[MAXP], ap_down[MAXP], ap_moved[MAXP];
static INTN ap_sx[MAXP], ap_sy[MAXP];
static INTN mx, my;
static EFI_EVENT ptmr; static int have_i2c, i2c_down;

void mouse_init(void) {
  EFI_HANDLE *hs = NULL; UINTN n = 0, i;
  mx = SW / 2; my = SH / 2;
  /* Firmware often leaves USB and I2C input devices (trackpads included) unconnected until asked. */
  if (!EFI_ERROR(uefi_call_wrapper(BS->LocateHandleBuffer, 5, AllHandles, NULL, NULL, &n, &hs))) {
    for (i = 0; i < n; i++) uefi_call_wrapper(BS->ConnectController, 4, hs[i], NULL, NULL, (BOOLEAN)TRUE);
    FreePool(hs);
  }
  hs = NULL; n = 0;
  if (!EFI_ERROR(uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &SPG, NULL, &n, &hs))) {
    for (i = 0; i < n && nsp < MAXP; i++) {
      EFI_SIMPLE_POINTER *p = NULL;
      if (!EFI_ERROR(uefi_call_wrapper(BS->HandleProtocol, 3, hs[i], &SPG, (void **)&p)) && p) {
        uefi_call_wrapper(p->Reset, 2, p, (BOOLEAN)FALSE);
        sp[nsp++] = p;
      }
    }
    FreePool(hs);
  }
  hs = NULL; n = 0;
  if (!EFI_ERROR(uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &APG, NULL, &n, &hs))) {
    for (i = 0; i < n && nap < MAXP; i++) {
      EFI_ABSOLUTE_POINTER *p = NULL;
      if (!EFI_ERROR(uefi_call_wrapper(BS->HandleProtocol, 3, hs[i], &APG, (void **)&p)) && p) {
        uefi_call_wrapper(p->Reset, 2, p, (BOOLEAN)FALSE);
        ap[nap++] = p;
      }
    }
    FreePool(hs);
  }
  { char m[40] = "pointers: simple "; INTN k;
    for (k = 0; k < 1; k++) { m[17] = (char)('0' + nsp); m[18] = ' '; m[19] = 'a'; m[20] = 'b'; m[21] = 's'; m[22] = ' '; m[23] = (char)('0' + nap); m[24] = 0; }
    diag_add(m); }
  /* A guard variable protects against the trackpad probe hanging a machine: it is set while the probe runs and
   * removed when it finishes, so a probe that never finished is skipped next time (press T on the menu to retry). */
  { UINT8 g = 1; UINTN gs = 1;
    if (!EFI_ERROR(uefi_call_wrapper(RT->GetVariable, 5, L"BootRI2cGuard", &GUARDG, NULL, &gs, &g))) {
      diag_add("i2c: skipped, the last start did not finish (press T to retry next boot)");
      have_i2c = 0;
    } else {
      g = 1;
      uefi_call_wrapper(RT->SetVariable, 5, L"BootRI2cGuard", &GUARDG, (UINT32)(EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS), (UINTN)1, &g);
      have_i2c = i2chid_init();
      uefi_call_wrapper(RT->SetVariable, 5, L"BootRI2cGuard", &GUARDG, (UINT32)(EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS), (UINTN)0, &g);
    } }
  if (have_i2c && !EFI_ERROR(uefi_call_wrapper(BS->CreateEvent, 5, EVT_TIMER, TPL_APPLICATION, NULL, NULL, &ptmr)))
    uefi_call_wrapper(BS->SetTimer, 3, ptmr, TimerPeriodic, (UINT64)100000);
  else have_i2c = 0;
}

/* Relative devices (mice, trackpads) report small counts per event, so speed up bigger strokes. */
static INTN accel(INTN v, INTN sc) {
  INTN a = v < 0 ? -v : v;
  if (a > 48) { v = v < 0 ? -48 : 48; a = 48; }
  return v * (1 + a / 12) * sc;
}

/* Reads every pointer device and moves the cursor. Returns 1 if there was a click, 2 for a scroll step
 * (direction in *wheel), 0 otherwise. A press on a mouse/trackpad button clicks at once. A touch on an
 * absolute device (touchscreen, or a trackpad that reports positions) clicks when the finger lifts without
 * having moved, so dragging a finger to aim does not click. */
static int poll_pointers(INTN *wheel) {
  INTN i, sc = SH >= 1400 ? 2 : 1, ox = mx, oy = my; int ret = 0;
  *wheel = 0;
  for (i = 0; i < nsp; i++) {
    EFI_SIMPLE_POINTER_STATE st;
    if (EFI_ERROR(uefi_call_wrapper(sp[i]->GetState, 2, sp[i], &st))) continue;
    mx += accel(st.RelativeMovementX, sc); my += accel(st.RelativeMovementY, sc);
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    if (mx > SW - 1) mx = SW - 1;
    if (my > SH - 1) my = SH - 1;
    if (st.RelativeMovementZ) *wheel = st.RelativeMovementZ > 0 ? 1 : -1;
    if (st.LeftButton && !sp_down[i]) ret = 1;
    sp_down[i] = st.LeftButton;
  }
  for (i = 0; i < nap; i++) {
    EFI_ABSOLUTE_POINTER_STATE st; EFI_ABSOLUTE_POINTER_MODE *m = ap[i]->Mode; UINT64 rx, ry; int down;
    if (EFI_ERROR(uefi_call_wrapper(ap[i]->GetState, 2, ap[i], &st))) continue;
    down = st.ActiveButtons & 1;
    rx = m->AbsoluteMaxX - m->AbsoluteMinX; ry = m->AbsoluteMaxY - m->AbsoluteMinY;
    if (rx && ry && st.CurrentX >= m->AbsoluteMinX && st.CurrentY >= m->AbsoluteMinY) {
      mx = (INTN)((st.CurrentX - m->AbsoluteMinX) * (UINT64)(SW - 1) / rx);
      my = (INTN)((st.CurrentY - m->AbsoluteMinY) * (UINT64)(SH - 1) / ry);
      if (mx > SW - 1) mx = SW - 1;
      if (my > SH - 1) my = SH - 1;
    }
    if (down && !ap_down[i]) { ap_sx[i] = mx; ap_sy[i] = my; ap_moved[i] = 0; }
    if (down && (mx - ap_sx[i] > 24 * sc || ap_sx[i] - mx > 24 * sc || my - ap_sy[i] > 24 * sc || ap_sy[i] - my > 24 * sc))
      ap_moved[i] = 1;
    if (!down && ap_down[i] && !ap_moved[i]) ret = 1;
    ap_down[i] = (UINT8)down;
  }
  if (mx != ox || my != oy || ret) gfx_cursor_move(mx, my);
  return ret ? ret : (*wheel ? 2 : 0);
}

/* The I2C trackpad has no event of its own, so a 10 ms timer polls it. Returns 1 for a click, 2 for a scroll step. */
static int poll_i2c(INTN *wheel) {
  INTN dx, dy, w, ox = mx, oy = my, k; int left, tap, click = 0;
  *wheel = 0;
  for (k = 0; k < 4 && i2chid_poll(&dx, &dy, &left, &tap, &w); k++) {
    mx += dx; my += dy;
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    if (mx > SW - 1) mx = SW - 1;
    if (my > SH - 1) my = SH - 1;
    if ((left && !i2c_down) || tap) click = 1;
    i2c_down = left;
    if (w) *wheel = w;
  }
  if (mx != ox || my != oy || click) gfx_cursor_move(mx, my);
  return click ? 1 : (*wheel ? 2 : 0);
}

/* Blocks until a key press, a timer tick, a click or a scroll step. Moving the pointer only repaints it.
 * For EV_CLICK, *cx and *cy hold the position; for EV_WHEEL, *cx is +1 (down) or -1 (up). */
int wait_input(EFI_EVENT tmr, EFI_INPUT_KEY *k, INTN *cx, INTN *cy) {
  for (;;) {
    EFI_EVENT ev[3 + 2 * MAXP]; UINTN n = 0, idx; INTN i, wheel; int r;
    ev[n++] = ST->ConIn->WaitForKey;
    if (tmr) ev[n++] = tmr;
    for (i = 0; i < nsp; i++) ev[n++] = sp[i]->WaitForInput;
    for (i = 0; i < nap; i++) ev[n++] = ap[i]->WaitForInput;
    if (have_i2c) ev[n++] = ptmr;
    if (EFI_ERROR(uefi_call_wrapper(BS->WaitForEvent, 3, n, ev, &idx))) continue;
    if (idx == 0) {
      if (!EFI_ERROR(uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, k))) return EV_KEY;
    } else if (tmr && idx == 1) return EV_TICK;
    else if (have_i2c && ev[idx] == ptmr) {
      r = poll_i2c(&wheel);
      if (r == 1) { *cx = mx; *cy = my; return EV_CLICK; }
      if (r == 2) { *cx = wheel; *cy = 0; return EV_WHEEL; }
    } else {
      r = poll_pointers(&wheel);
      if (r == 1) { *cx = mx; *cy = my; return EV_CLICK; }
      if (r == 2) { *cx = wheel; *cy = 0; return EV_WHEEL; }
    }
  }
}

/* ---- clickable rectangles, filled in by the draw code ---- */
#define MAXH 192
static Hit hits[MAXH]; static INTN nhits, hx0, hy0, hx1, hy1;

void hit_reset(void) { nhits = 0; hit_clip(0, 0, SW, SH); }
void hit_clip(INTN x, INTN y, INTN w, INTN h) { hx0 = x; hy0 = y; hx1 = x + w; hy1 = y + h; }
void hit_add(INTN x, INTN y, INTN w, INTN h, INTN kind, INTN a, INTN b) {
  INTN x1 = x + w, y1 = y + h;
  if (x < hx0) x = hx0;
  if (y < hy0) y = hy0;
  if (x1 > hx1) x1 = hx1;
  if (y1 > hy1) y1 = hy1;
  if (x1 <= x || y1 <= y || nhits >= MAXH) return;
  hits[nhits].x = x; hits[nhits].y = y; hits[nhits].w = x1 - x; hits[nhits].h = y1 - y;
  hits[nhits].kind = kind; hits[nhits].a = a; hits[nhits].b = b;
  nhits++;
}
/* Later registrations sit on top of earlier ones. */
const Hit *hit_find(INTN x, INTN y) {
  INTN i;
  for (i = nhits - 1; i >= 0; i--)
    if (x >= hits[i].x && x < hits[i].x + hits[i].w && y >= hits[i].y && y < hits[i].y + hits[i].h) return &hits[i];
  return NULL;
}

/* Lets the next boot run the trackpad probe again after it was skipped. */
void mouse_i2c_retry(void) {
  UINT8 g = 1;
  uefi_call_wrapper(RT->SetVariable, 5, L"BootRI2cGuard", &GUARDG, (UINT32)(EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS), (UINTN)0, &g);
}
