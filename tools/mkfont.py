#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Steven Azevedo
"""Adds a native 2x glyph set to each font in src/assets.c.

On screens tall enough for UI scale 2, text used to be the 1x glyph bitmaps with every pixel doubled, which
looks soft. This rasterises the same cells again at twice the size (cell, baseline and size all doubled) so
text is drawn at its real resolution. Needs pycairo and a Helvetica-style bold face (default: Nimbus Sans).
Run from the repository root; safe to re-run, it replaces the 2x sets it wrote before.

    python3 tools/mkfont.py [font-family]
"""
import re
import sys
import cairo

FAMILY = sys.argv[1] if len(sys.argv) > 1 else "Nimbus Sans"
FONTS = (("small", 17, 21), ("med", 23, 29), ("big", 30, 38))
path = "src/assets.c"
src = open(path).read()


def arr(name):
    m = re.search(r"static const UINT8 %s\[\]=\{([^}]*)\}" % name, src)
    return [int(x) for x in m.group(1).split(",") if x.strip()]


def setup(ctx, size):
    ctx.select_font_face(FAMILY, cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
    o = cairo.FontOptions()
    o.set_antialias(cairo.ANTIALIAS_GRAY)
    o.set_hint_style(cairo.HINT_STYLE_NONE)
    o.set_hint_metrics(cairo.HINT_METRICS_OFF)
    ctx.set_font_options(o)
    ctx.set_font_size(size)


scratch = cairo.Context(cairo.ImageSurface(cairo.FORMAT_A8, 8, 8))
setup(scratch, 100)
cap_ratio = scratch.text_extents("H").height / 100.0

out = []
for name, cw, ch in FONTS:
    d, a = arr("fd_" + name), arr("fa_" + name)
    h = ord("H") - 32
    cell = d[h * cw * ch:(h + 1) * cw * ch]
    cols = [x for x in range(cw) if max(cell[y * cw + x] for y in range(ch)) >= 128]
    rows = [y for y in range(ch) if max(cell[y * cw:(y + 1) * cw]) >= 128]
    stem = cols[0] + 1
    cap = sum(cell[y * cw + stem] for y in range(ch)) / 255.0
    base = rows[-1] + 1
    size = cap / cap_ratio
    setup(scratch, size)
    ext = scratch.text_extents("H")
    origin = cols[0] - ext.x_bearing
    hw, hh = cw * 2, ch * 2
    data, adv = [], []
    for g in range(95):
        s = cairo.ImageSurface(cairo.FORMAT_A8, hw, hh)
        c = cairo.Context(s)
        setup(c, size * 2)
        c.set_source_rgba(0, 0, 0, 1)
        c.move_to(origin * 2, base * 2)
        c.show_text(chr(g + 32))
        s.flush()
        buf, stride = s.get_data(), s.get_stride()
        for y in range(hh):
            data.extend(buf[y * stride:y * stride + hw])
        adv.append(round(c.text_extents(chr(g + 32)).x_advance))
    print("%s: size %.2f baseline %d origin %.2f, adv(lo/hi) H %d/%d" % (name, size, base, origin, a[h], adv[h]),
          file=sys.stderr)
    out.append((name, cw, ch, data, adv))

for name, cw, ch, data, adv in out:
    src = re.sub(r"static const UINT8 (fd|fa)_%s2\[\]=\{[^}]*\};\n" % name, "", src)
    src = re.sub(r"static const Font font_%s2=[^\n]*\n" % name, "", src)
    new = ("static const UINT8 fd_%s2[]={%s};\nstatic const UINT8 fa_%s2[]={%s};\n"
           "static const Font font_%s2={%d,%d,fd_%s2,fa_%s2,0};\n"
           "const Font font_%s={%d,%d,fd_%s,fa_%s,&font_%s2};\n"
           % (name, ",".join(map(str, data)), name, ",".join(map(str, adv)), name, cw * 2, ch * 2, name, name,
              name, cw, ch, name, name, name))
    src = re.sub(r"const Font font_%s=[^\n]*\n" % name, lambda m: new, src, count=1)
open(path, "w").write(src)
