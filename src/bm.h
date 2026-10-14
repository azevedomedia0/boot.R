/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Steven Azevedo */
#ifndef BM_H
#define BM_H
#include "uefi.h"

#define APP_NAME "boot.R"
#define MAXE 24
#define C_BG    0x1b1b1b
#define C_PANEL 0x353b41
#define C_TILE  0x3f464d
#define C_BTN   0x444b52
#define C_ACC   0x669acc
#define C_TXT   0xf3f2f2
#define C_DARK  0x2e3338
#define C_DIM   0xb7b6b6

typedef struct { INTN cw, ch; const UINT8 *data; const UINT8 *adv; } Font;
extern const Font font_small, font_med, font_big;
extern const UINT32 *const logos[41];
extern const UINT32 icon_bootr[128 * 128];
extern const UINT8 *const masks[13];
enum { I_LEFT, I_RIGHT, I_UP, I_DOWN, I_BACK, I_REFRESH, I_SETTINGS, I_PENCIL, I_CHECK, I_EYE, I_EYEOFF, I_CPU, I_DISK };

typedef struct { char key[24]; char name[32]; UINT8 hidden, pad[3]; } SEnt;
typedef struct { UINT32 magic, timer, hide_hints; char def[24]; UINT32 n; SEnt e[MAXE]; } Settings;
typedef struct { char key[24]; char label[32]; INTN icon, kind; EFI_HANDLE dev; CHAR16 path[160]; } Entry;
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
void draw_mask(int icon, INTN x, INTN y, INTN size, UINT32 c);
void draw_logo(int idx, INTN x, INTN y, INTN size);
void draw_image(const UINT32 *m, INTN n, INTN x, INTN y, INTN size);
void draw_icon(INTN x, INTN y, INTN size);
#endif
