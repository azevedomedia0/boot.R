/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Steven Azevedo */
#ifndef BM_H
#define BM_H
#include "uefi.h"

#define APP_NAME "boot.R"
#define MAXE 24
#define OPTS_MAX 480
#define C_BG    0x1b1b1b
#define C_PANEL 0x353b41
#define C_TILE  0x3f464d
#define C_BTN   0x444b52
#define C_ACC   0x669acc
#define C_TXT   0xf3f2f2
#define C_DARK  0x2e3338
#define C_DIM   0xb7b6b6

typedef struct Font Font;
struct Font { INTN cw, ch; const UINT8 *data; const UINT8 *adv; const Font *hi; };  /* hi: native 2x glyphs */
extern const Font font_small, font_med, font_big;
extern const UINT32 *const logos[41];
extern const UINT8 *const logos_hi[41];
extern const UINT32 logo_col[41];
extern const UINT32 icon_bootr[128 * 128];
extern const UINT8 *const masks[13];
enum { I_LEFT, I_RIGHT, I_UP, I_DOWN, I_BACK, I_REFRESH, I_SETTINGS, I_PENCIL, I_CHECK, I_EYE, I_EYEOFF, I_CPU, I_DISK };

typedef struct { char key[24]; char name[32]; UINT8 hidden, pad[3]; } SEnt;
typedef struct { UINT32 magic, timer, hide_hints; char def[24]; UINT32 n; SEnt e[MAXE]; } Settings;
typedef struct { char key[24]; char label[32]; INTN icon, kind; EFI_HANDLE dev; CHAR16 path[160]; CHAR16 opts[OPTS_MAX]; } Entry;
typedef struct { INTN ent, sidx; } VRow;

extern Settings cfg;
extern Entry ents[MAXE];
extern INTN nents;
void settings_load(void);
void settings_save(void);
void settings_sync(void);
void scan_all(EFI_HANDLE image);
INTN build_view(VRow *all);
const char *disp_name(VRow v);

extern INTN SW, SH;
EFI_STATUS gfx_init(void);
void gfx_flush(void);
void gfx_clip(INTN x, INTN y, INTN w, INTN h);
void gfx_noclip(void);
void fill_rect(INTN x, INTN y, INTN w, INTN h, UINT32 c);
void fill_rrect(INTN x, INTN y, INTN w, INTN h, INTN r, UINT32 c);
INTN text_w(const Font *f, const char *s, INTN sc);
void draw_text(const Font *f, INTN x, INTN y, const char *s, UINT32 c, INTN sc);
void draw_alpha(const UINT8 *m, INTN n, INTN x, INTN y, INTN size, UINT32 c);
void draw_mask(int icon, INTN x, INTN y, INTN size, UINT32 c);
void draw_logo(int idx, INTN x, INTN y, INTN size);
void draw_image(const UINT32 *m, INTN n, INTN x, INTN y, INTN size);
void draw_icon(INTN x, INTN y, INTN size);
void gfx_cursor_move(INTN x, INTN y);
void gfx_cursor_hide(void);

/* Mouse / touch. Draw code registers clickable rectangles; a click is turned into an action by the caller. */
enum { EV_KEY, EV_TICK, EV_CLICK, EV_WHEEL };
enum { H_TILE = 1, H_PREV, H_NEXT, H_REFRESH, H_SETTINGS, H_BACK, H_ROW, H_SEG, H_BTN, H_SCROLL };
typedef struct { INTN x, y, w, h, kind, a, b; } Hit;
typedef struct { UINT8 id, size, sign, rel; UINT16 page, usage; UINT32 off; INTN lmax; } HidField;
INTN hid_parse(const UINT8 *d, INTN n, HidField *out, INTN max);
INTN hid_get(const UINT8 *rep, INTN len, const HidField *f);
int i2chid_init(void);
int i2chid_poll(INTN *dx, INTN *dy, int *left, int *tap, INTN *wheel);
void diag_add(const char *s);
INTN diag_count(void);
const char *diag_line(INTN i);
void diag_stats(char *out);
void mouse_init(void);
void mouse_i2c_retry(void);
int wait_input(EFI_EVENT tmr, EFI_INPUT_KEY *k, INTN *cx, INTN *cy);
void hit_reset(void);
void hit_clip(INTN x, INTN y, INTN w, INTN h);
void hit_add(INTN x, INTN y, INTN w, INTN h, INTN kind, INTN a, INTN b);
const Hit *hit_find(INTN x, INTN y);
#endif
