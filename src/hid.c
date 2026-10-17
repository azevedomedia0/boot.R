/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Steven Azevedo */
#include "bm.h"

/* Walks a HID report descriptor and lists the input fields that matter for a pointer: buttons (usage page 9)
 * and generic-desktop X/Y/wheel (usage page 1). Bit offsets are counted from the first bit after the report ID. */
INTN hid_parse(const UINT8 *d, INTN n, HidField *out, INTN max) {
  INTN p = 0, nf = 0, nu = 0, i, lmin = 0, lmax = 0, nids = 0;
  UINT32 page = 0, rsize = 0, rcount = 0, rid = 0, usages[32], umin = 0, off[16], ids[16];
  int have_range = 0;
  while (p < n) {
    UINT8 b = d[p++]; INTN sz = b & 3, type = (b >> 2) & 3, tag = b >> 4, sv;
    UINT32 v = 0;
    if (b == 0xFE) { if (p < n) p += 2 + d[p]; continue; }
    if (sz == 3) sz = 4;
    for (i = 0; i < sz && p + i < n; i++) v |= (UINT32)d[p + i] << (8 * i);
    sv = sz == 1 ? (INTN)(INT8)v : sz == 2 ? (INTN)(INT16)v : (INTN)(INT32)v;
    p += sz;
    if (type == 1) {
      if (tag == 0) page = v; else if (tag == 1) lmin = sv; else if (tag == 2) lmax = sv; else if (tag == 7) rsize = v;
      else if (tag == 8) rid = v; else if (tag == 9) rcount = v;
    } else if (type == 2) {
      if (tag == 0 && nu < 32) usages[nu++] = v & 0xFFFF;
      else if (tag == 1) { umin = v & 0xFFFF; have_range = 1; }
    } else if (type == 0) {
      if (tag == 8) {
        INTN slot = -1;
        for (i = 0; i < nids; i++) if (ids[i] == rid) slot = i;
        if (slot < 0 && nids < 16) { ids[nids] = rid; off[nids] = 0; slot = nids++; }
        if (slot >= 0) {
          for (i = 0; i < (INTN)rcount; i++) {
            UINT32 u = nu > 0 ? usages[i < nu ? i : nu - 1] : have_range ? umin + (UINT32)i : 0;
            if (!(v & 1) && nf < max && (page == 1 || page == 9 || page == 0x0D)) {
              out[nf].id = (UINT8)rid; out[nf].off = off[slot] + (UINT32)i * rsize; out[nf].size = (UINT8)rsize;
              out[nf].sign = lmin < 0; out[nf].lmax = lmax; out[nf].rel = (v >> 2) & 1; out[nf].page = (UINT16)page; out[nf].usage = (UINT16)u;
              nf++;
            }
          }
          off[slot] += rcount * rsize;
        }
      }
      nu = 0; have_range = 0; umin = 0;
    }
  }
  return nf;
}

INTN hid_get(const UINT8 *rep, INTN len, const HidField *f) {
  INTN v = 0, b;
  for (b = 0; b < f->size; b++) {
    UINT32 bit = f->off + (UINT32)b;
    if ((INTN)(bit >> 3) >= len) return 0;
    v |= (INTN)((rep[bit >> 3] >> (bit & 7)) & 1) << b;
  }
  if (f->sign && f->size < 32 && (v >> (f->size - 1)) & 1) v -= (INTN)1 << f->size;
  return v;
}
