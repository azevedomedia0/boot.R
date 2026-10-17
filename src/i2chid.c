/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Steven Azevedo */
#include "bm.h"

/* Trackpads on modern laptops (Synaptics, Elan, ...) sit on an Intel serial-IO I2C controller and speak
 * HID over I2C. Firmware has no driver for them, so UEFI offers no pointer for them. This drives the
 * controller (a Synaptics DesignWare I2C block) directly by polling, finds the HID device on its own, and
 * reads the mouse-mode reports a touchpad sends until the host switches it to precision-touchpad mode. */

static EFI_GUID PIG = EFI_PCI_IO_PROTOCOL_GUID;
#define R32(a) (*(volatile UINT32 *)(UINTN)(a))

/* DesignWare I2C registers */
enum { DW_CON = 0x00, DW_TAR = 0x04, DW_DATA = 0x10, DW_INTR_MASK = 0x30, DW_RAW = 0x34, DW_CLR_INTR = 0x40,
       DW_CLR_ABRT = 0x54, DW_ENABLE = 0x6C, DW_STATUS = 0x70, DW_TXFLR = 0x74, DW_RXFLR = 0x78,
       DW_ABRT_SRC = 0x80, DW_EN_STATUS = 0x9C, DW_PARAM1 = 0xF4, DW_COMP_TYPE = 0xFC };

/* ---- diagnostics shown on the menu (press I) ---- */
#define DLINES 16
static char dl[DLINES][84]; static INTN ndl;
void diag_add(const char *s) {
  INTN i = 0;
  if (ndl >= DLINES) return;
  for (; s[i] && i < 83; i++) dl[ndl][i] = s[i];
  dl[ndl][i] = 0; ndl++;
}
INTN diag_count(void) { return ndl; }
const char *diag_line(INTN i) { return dl[i]; }

static INTN slen(const char *s) { INTN n = 0; while (s[n]) n++; return n; }
static void cat(char *d, const char *s) { while (*d) d++; while ((*d++ = *s++)) ; }
static void hex(char *d, UINT64 v, int digits) {
  int i;
  while (*d) d++;
  for (i = digits - 1; i >= 0; i--) *d++ = "0123456789abcdef"[(v >> (4 * i)) & 15];
  *d = 0;
}
static void dec(char *d, UINT64 v) {
  char t[21]; int n = 0, i;
  while (*d) d++;
  if (!v) t[n++] = '0';
  while (v) { t[n++] = (char)('0' + v % 10); v /= 10; }
  for (i = 0; i < n; i++) *d++ = t[n - 1 - i];
  *d = 0;
}

/* ---- controller ---- */
static UINT64 base; static INTN txd = 8;
static UINT8 addr;

static void dw_off(void) {
  INTN i;
  R32(base + DW_ENABLE) = 0;
  for (i = 0; i < 1000 && (R32(base + DW_EN_STATUS) & 1); i++) uefi_call_wrapper(BS->Stall, 1, (UINTN)10);
}

/* One I2C transaction: write wlen bytes, then (after a repeated start) read rlen bytes.
 * Returns 1 on success, 0 if the device did not answer (NACK), -1 if the controller never finished. */
static int xfer(UINT8 a, const UINT8 *w, INTN wlen, UINT8 *r, INTN rlen) {
  INTN sent = 0, got = 0, total = wlen + rlen, t;
  dw_off();
  R32(base + DW_TAR) = a;
  R32(base + DW_INTR_MASK) = 0;
  (void)R32(base + DW_CLR_INTR); (void)R32(base + DW_CLR_ABRT);
  R32(base + DW_ENABLE) = 1;
  for (t = 0; t < 2000 + total * 60; t++) {
    if (R32(base + DW_RAW) & 0x40) { (void)R32(base + DW_CLR_ABRT); dw_off(); return 0; }
    while (sent < total && (INTN)R32(base + DW_TXFLR) < txd) {
      UINT32 cmd;
      if (sent < wlen) cmd = w[sent];
      else cmd = 0x100 | ((sent == wlen && wlen) ? 0x400 : 0);
      if (sent == total - 1) cmd |= 0x200;
      R32(base + DW_DATA) = cmd; sent++;
    }
    while (got < rlen && R32(base + DW_RXFLR)) r[got++] = (UINT8)R32(base + DW_DATA);
    if (got >= rlen && sent >= total && !R32(base + DW_TXFLR) && !(R32(base + DW_STATUS) & 1)) return 1;
    uefi_call_wrapper(BS->Stall, 1, (UINTN)3);
  }
  dw_off();
  return -1;
}

/* Writes a 2-byte register address, then reads n bytes after a repeated start. */
static int read_reg(UINT16 reg, UINT8 *buf, INTN n) {
  UINT8 b[2]; b[0] = (UINT8)reg; b[1] = (UINT8)(reg >> 8);
  return xfer(addr, b, 2, buf, n) == 1;
}

/* ---- the HID device ---- */
static int ok; static UINT16 maxin; static UINT8 mid; static HidField fx, fy, fb;
/* Precision-touchpad mode: absolute finger positions plus a scan time that tells fresh reports from repeats. */
static int ptp; static UINT8 pid; static HidField px_, py_, ptip, pconf, pcnt, pscan, pbtn;
static UINT32 last_scan, down_scan; static int have_scan, was_down, scrolled; static INTN lastx, lasty, moved, sacc;
static UINT8 lastrep[8]; static int have_lastrep;
static UINT32 npoll, nrep, nfail;

void diag_stats(char *out) {
  out[0] = 0;
  if (!ok) return;
  cat(out, "i2c-hid polls "); dec(out, npoll); cat(out, " reports "); dec(out, nrep); cat(out, " failed "); dec(out, nfail);
}

static UINT16 le16(const UINT8 *b) { return (UINT16)(b[0] | (b[1] << 8)); }

/* HID-over-I2C SET_REPORT for a feature report: command register, report type/ID and opcode, data register,
 * length, then the report ID and payload. */
static void set_feature(UINT16 cmd, UINT16 dreg, UINT8 id, const UINT8 *data, INTN n) {
  UINT8 b[16]; INTN k = 0, size = 2 + 1 + n, i;
  b[k++] = (UINT8)cmd; b[k++] = (UINT8)(cmd >> 8);
  b[k++] = (UINT8)(0x30 | id); b[k++] = 0x03;
  b[k++] = (UINT8)dreg; b[k++] = (UINT8)(dreg >> 8);
  b[k++] = (UINT8)size; b[k++] = (UINT8)(size >> 8);
  b[k++] = id;
  for (i = 0; i < n; i++) b[k++] = data[i];
  (void)xfer(addr, b, k, NULL, 0);
}

/* Tries to bring up an I2C HID pointer at the current controller and address. */
static int try_hid(void) {
  static const UINT16 regs[] = { 0x0020, 0x0001, 0x0000, 0x0002, 0x0100 };
  static UINT8 rd[1400]; static HidField f[64];
  UINT8 d[30]; INTN i, nf, rl; UINT16 reg = 0, cmd, rdreg; int found = 0;
  char m[84];
  for (i = 0; i < (INTN)(sizeof(regs) / sizeof(regs[0])); i++) {
    if (!read_reg(regs[i], d, 30)) continue;
    if (le16(d) == 30 && le16(d + 2) == 0x0100) { reg = regs[i]; found = 1; break; }
  }
  if (!found) return 0;
  rl = le16(d + 4); rdreg = le16(d + 6); maxin = le16(d + 10); cmd = le16(d + 16);
  m[0] = 0; cat(m, "  hid dev at 0x"); hex(m, addr, 2); cat(m, " desc reg 0x"); hex(m, reg, 4); cat(m, " vid:pid ");
  hex(m, le16(d + 20), 4); cat(m, ":"); hex(m, le16(d + 22), 4); diag_add(m);
  m[0] = 0; cat(m, "  report desc "); dec(m, (UINT64)rl); cat(m, " bytes, max input "); dec(m, maxin); diag_add(m);
  if (rl <= 0 || rl > (INTN)sizeof(rd) || maxin < 4 || maxin > 250) { diag_add("  descriptor sizes out of range"); return 0; }
  if (!read_reg(rdreg, rd, rl)) { diag_add("  could not read report descriptor"); return 0; }
  nf = hid_parse(rd, rl, f, 64);
  {
    int hx = 0, hy = 0, hb = 0; UINT8 id = 0, tid = 0;
    for (i = 0; i < nf; i++) if (f[i].page == 0x0D && f[i].usage == 0x56) tid = f[i].id;
    if (tid) {
      int g[7] = { 0, 0, 0, 0, 0, 0, 0 };
      for (i = 0; i < nf; i++) {
        if (f[i].id != tid) continue;
        if (f[i].page == 1 && f[i].usage == 0x30 && !g[0]) { px_ = f[i]; g[0] = 1; }
        else if (f[i].page == 1 && f[i].usage == 0x31 && !g[1]) { py_ = f[i]; g[1] = 1; }
        else if (f[i].page == 0x0D && f[i].usage == 0x42 && !g[2]) { ptip = f[i]; g[2] = 1; }
        else if (f[i].page == 0x0D && f[i].usage == 0x47 && !g[3]) { pconf = f[i]; g[3] = 1; }
        else if (f[i].page == 0x0D && f[i].usage == 0x54 && !g[4]) { pcnt = f[i]; g[4] = 1; }
        else if (f[i].page == 0x0D && f[i].usage == 0x56 && !g[5]) { pscan = f[i]; g[5] = 1; }
        else if (f[i].page == 9 && f[i].usage == 1 && !g[6]) { pbtn = f[i]; g[6] = 1; }
      }
      if (g[0] && g[1] && g[2] && g[3] && g[4] && g[5] && px_.lmax > 0 && py_.lmax > 0) {
        ptp = 1; pid = tid; if (!g[6]) pbtn.size = 0;
        m[0] = 0; cat(m, "  touchpad reports: id "); dec(m, tid); cat(m, " x max "); dec(m, (UINT64)px_.lmax);
        cat(m, " y max "); dec(m, (UINT64)py_.lmax); diag_add(m);
      }
    }
    for (i = 0; i < nf; i++) if (f[i].rel && f[i].page == 1 && f[i].usage == 0x30 && !hx) { fx = f[i]; id = f[i].id; hx = 1; }
    for (i = 0; i < nf; i++) if (f[i].rel && f[i].page == 1 && f[i].usage == 0x31 && f[i].id == id && !hy) { fy = f[i]; hy = 1; }
    for (i = 0; i < nf; i++) if (f[i].page == 9 && f[i].usage == 1 && f[i].id == id && !hb) { fb = f[i]; hb = 1; }
    if (hx && hy) {
      if (!hb) { fb = fx; fb.size = 0; }
      mid = id;
      m[0] = 0; cat(m, "  mouse reports: id "); dec(m, id); cat(m, " x@"); dec(m, fx.off); cat(m, " y@"); dec(m, fy.off);
      cat(m, " btn "); cat(m, hb ? "yes" : "no"); diag_add(m);
    } else if (!ptp) { diag_add("  no pointer reports in the report descriptor"); return 0; }
  }
  /* SET_POWER(on), then RESET, as the HID-over-I2C spec asks; the answer to RESET is an empty report. */
  { UINT8 c[4]; c[0] = (UINT8)cmd; c[1] = (UINT8)(cmd >> 8); c[2] = 0x00; c[3] = 0x08; (void)xfer(addr, c, 4, NULL, 0); }
  uefi_call_wrapper(BS->Stall, 1, (UINTN)20000);
  { UINT8 c[4]; c[0] = (UINT8)cmd; c[1] = (UINT8)(cmd >> 8); c[2] = 0x00; c[3] = 0x01; (void)xfer(addr, c, 4, NULL, 0); }
  uefi_call_wrapper(BS->Stall, 1, (UINTN)150000);
  if (ptp) {
    UINT16 dreg = le16(d + 18); UINT8 v;
    v = 3; set_feature(cmd, dreg, 4, &v, 1);    /* Input Mode: Windows precision touchpad */
    v = 3; set_feature(cmd, dreg, 6, &v, 1);    /* Surface and button switch: on */
    uefi_call_wrapper(BS->Stall, 1, (UINTN)30000);
  }
  { UINT8 junk[256]; for (i = 0; i < 4; i++) (void)xfer(addr, NULL, 0, junk, maxin); }
  return 1;
}

static UINT32 cfg_read(EFI_PCI_IO *p, UINT32 width, UINT32 off) {
  UINT32 v = 0;
  uefi_call_wrapper(p->Pci.Read, 5, p, width, off, (UINTN)1, &v);
  return v;
}

int i2chid_init(void) {
  EFI_HANDLE *hs = NULL; UINTN n = 0, h; char m[84];
  if (EFI_ERROR(uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &PIG, NULL, &n, &hs))) {
    diag_add("i2c: no PCI I/O protocol"); return 0;
  }
  for (h = 0; h < n && !ok; h++) {
    EFI_PCI_IO *p = NULL; UINT32 id, cls, bar, cmdr; UINT64 b; UINTN seg, bus, dev, fn; INTN a, to;
    if (EFI_ERROR(uefi_call_wrapper(BS->HandleProtocol, 3, hs[h], &PIG, (void **)&p)) || !p) continue;
    id = cfg_read(p, 2, 0); cls = cfg_read(p, 2, 8) >> 8;
    if ((id & 0xFFFF) != 0x8086 || cls != 0x0C8000) continue;
    bar = cfg_read(p, 2, 0x10); b = bar & ~0xFULL;
    if ((bar & 6) == 4) b |= (UINT64)cfg_read(p, 2, 0x14) << 32;
    if (!b || bar == 0xFFFFFFFFu) continue;
    /* Memory decoding on, device out of D3 (config space is poked via two 16-bit accesses). */
    cmdr = cfg_read(p, 1, 4);
    if (!(cmdr & 2)) { UINT16 w = (UINT16)(cmdr | 2); uefi_call_wrapper(p->Pci.Write, 5, p, (UINT32)1, (UINT32)4, (UINTN)1, &w); }
    if (cfg_read(p, 1, 6) & 0x10) {
      UINT32 cp = cfg_read(p, 0, 0x34) & 0xFC; INTN guard;
      for (guard = 0; cp && guard < 24; guard++) {
        if (cfg_read(p, 0, cp) == 1) {
          UINT32 pm = cfg_read(p, 1, cp + 4);
          if (pm & 3) { UINT16 w = (UINT16)(pm & ~3u); uefi_call_wrapper(p->Pci.Write, 5, p, (UINT32)1, cp + 4, (UINTN)1, &w); uefi_call_wrapper(BS->Stall, 1, (UINTN)10000); }
          break;
        }
        cp = cfg_read(p, 0, cp + 1) & 0xFC;
      }
    }
    base = b;
    R32(base + 0x204) = 7;                      /* intel-lpss: take the function and DMA out of reset */
    uefi_call_wrapper(BS->Stall, 1, (UINTN)1000);
    if (R32(base + DW_COMP_TYPE) != 0x44570140) continue;   /* not a DesignWare I2C block */
    uefi_call_wrapper(p->GetLocation, 5, p, &seg, &bus, &dev, &fn);
    { UINT32 pr = R32(base + DW_PARAM1); txd = (INTN)(((pr >> 16) & 0xFF) + 1); if (txd < 4) txd = 4; }
    R32(base + DW_ENABLE) = 0;
    R32(base + DW_CON) = 0x63;                  /* master, standard speed, restart enabled, slave disabled */
    m[0] = 0; cat(m, "i2c ctl "); hex(m, bus, 2); cat(m, ":"); hex(m, dev, 2); cat(m, "."); hex(m, fn, 1);
    cat(m, " "); hex(m, id & 0xFFFF, 4); cat(m, ":"); hex(m, id >> 16, 4); cat(m, " bar "); hex(m, b, 10); diag_add(m);
    m[0] = 0; cat(m, "  acks:");
    for (a = 0x08, to = 0; a <= 0x77 && !ok && to < 3; a++) {
      UINT8 byte; int r;
      addr = (UINT8)a;
      r = xfer(addr, NULL, 0, &byte, 1);
      if (r < 0) to++;
      if (r != 1) continue;
      if (slen(m) < 70) { cat(m, " "); hex(m, (UINT64)a, 2); }
      if (try_hid()) { ok = 1; break; }
    }
    if (to >= 3 && slen(m) < 50) cat(m, " (controller not responding)");
    diag_add(m);
  }
  FreePool(hs);
  if (!ok) diag_add("i2c: no HID pointer found");
  return ok;
}

static INTN iabs(INTN v) { return v < 0 ? -v : v; }

/* Counts of finger travel on the pad, turned into screen pixels: the pad width maps to about 60% of the screen,
 * and quick strokes go further. */
static INTN pad_px(INTN d, INTN lx) {
  INTN a = iabs(d), mult = 4 + (a > 30 ? 30 : a) / 3, v = d * SW * mult / (lx * 8);
  if (!v && d) v = d < 0 ? -1 : 1;
  return v;
}

/* One precision-touchpad report. Returns 0 for a repeat of the last one (the device keeps handing out the latest
 * report when polled with nothing new, so the scan time is what says whether it is fresh). */
static int ptp_report(const UINT8 *rep, INTN rl, INTN *dx, INTN *dy, int *left, int *tap, INTN *wheel) {
  UINT32 st = (UINT32)hid_get(rep, rl, &pscan) & 0xFFFF; INTN cnt = hid_get(rep, rl, &pcnt), x = hid_get(rep, rl, &px_), y = hid_get(rep, rl, &py_);
  int down = (hid_get(rep, rl, &ptip) & 1) && (hid_get(rep, rl, &pconf) & 1);
  if (have_scan && st == last_scan) return 0;
  have_scan = 1; last_scan = st;
  *left = pbtn.size ? (int)(hid_get(rep, rl, &pbtn) & 1) : 0;
  if (down) {
    if (!was_down) { down_scan = st; moved = 0; scrolled = 0; sacc = 0; }
    else if (cnt >= 2) {
      sacc += y - lasty; scrolled = 1;
      if (sacc <= -30) { *wheel = 1; sacc = 0; } else if (sacc >= 30) { *wheel = -1; sacc = 0; }
    } else if (!scrolled) {
      *dx = pad_px(x - lastx, px_.lmax); *dy = pad_px(y - lasty, py_.lmax);
      moved += iabs(x - lastx) + iabs(y - lasty);
    }
    lastx = x; lasty = y;
  } else if (was_down && !scrolled && moved < 80 && ((st - down_scan) & 0xFFFF) < 2500) *tap = 1;
  was_down = down;
  return 1;
}

/* Reads one pending report, if any. dx/dy come back as screen pixels, *left is the pad's physical button,
 * *tap a tap on the pad, *wheel a two-finger scroll step. */
int i2chid_poll(INTN *dx, INTN *dy, int *left, int *tap, INTN *wheel) {
  UINT8 buf[256]; INTN len, rl; const UINT8 *rep;
  if (!ok) return 0;
  npoll++;
  *dx = *dy = *wheel = 0; *tap = 0; *left = 0;
  if (!xfer(addr, NULL, 0, buf, maxin)) { nfail++; return 0; }
  len = le16(buf);
  if (len < 3 || len > maxin) return 0;
  if (ptp && buf[2] == pid) {
    rep = buf + 3; rl = len - 3;
    if (!ptp_report(rep, rl, dx, dy, left, tap, wheel)) return 0;
    nrep++;
    return 1;
  }
  if (mid) { if (buf[2] != mid) return 0; rep = buf + 3; rl = len - 3; }
  else { rep = buf + 2; rl = len - 2; }
  /* Mouse mode: no timestamp, so an identical report is taken to be a repeat. */
  { INTN i, same = have_lastrep; for (i = 0; i < 8 && i < rl; i++) { if (lastrep[i] != rep[i]) same = 0; lastrep[i] = rep[i]; } have_lastrep = 1; if (same) return 0; }
  nrep++;
  { INTN rx = hid_get(rep, rl, &fx), ry = hid_get(rep, rl, &fy), a, b;
    a = iabs(rx); b = iabs(ry);
    *dx = rx * (1 + (a > 48 ? 48 : a) / 12) * (SH >= 1400 ? 2 : 1); *dy = ry * (1 + (b > 48 ? 48 : b) / 12) * (SH >= 1400 ? 2 : 1); }
  *left = fb.size ? (int)(hid_get(rep, rl, &fb) & 1) : 0;
  return 1;
}
