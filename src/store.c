/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Steven Azevedo */
#include "bm.h"

Settings cfg;
Entry ents[MAXE];
INTN nents;

static EFI_GUID BMG = {0x7d0b1c2e, 0x3a4f, 0x4b6e, {0x9a, 0x11, 0x5e, 0x2c, 0x70, 0xb1, 0x3d, 0x42}};
static EFI_GUID FSG = SIMPLE_FILE_SYSTEM_PROTOCOL;
static EFI_GUID LIG = LOADED_IMAGE_PROTOCOL;
static EFI_GUID GVG = EFI_GLOBAL_VARIABLE;
#define MAGIC 0x424D4E31

static INTN slen(const char *s) { INTN n = 0; while (s[n]) n++; return n; }
static int seq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static void scpy(char *d, const char *s, INTN n) { INTN i = 0; for (; s[i] && i < n - 1; i++) d[i] = s[i]; d[i] = 0; }
static int has(const char *h, const char *n) {
  INTN hl = slen(h), nl = slen(n), i, j;
  for (i = 0; i + nl <= hl; i++) { for (j = 0; j < nl && h[i + j] == n[j]; j++) ; if (j == nl) return 1; }
  return 0;
}
static void narrow(const CHAR16 *w, char *o, INTN n, int lower) {
  INTN i = 0;
  for (; w[i] && i < n - 1; i++) {
    CHAR16 c = w[i]; char a = (c >= 32 && c < 127) ? (char)c : '_';
    if (lower && a >= 'A' && a <= 'Z') a += 32;
    o[i] = a;
  }
  o[i] = 0;
}

void settings_load(void) {
  Settings t; UINTN sz = sizeof(t);
  EFI_STATUS s = uefi_call_wrapper(RT->GetVariable, 5, L"BootMenuSettings", &BMG, NULL, &sz, &t);
  if (!EFI_ERROR(s) && sz == sizeof(t) && t.magic == MAGIC) { cfg = t; return; }
  ZeroMem(&cfg, sizeof(cfg));
  cfg.magic = MAGIC;
}

void settings_save(void) {
  uefi_call_wrapper(RT->SetVariable, 5, L"BootMenuSettings", &BMG,
                    EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                    sizeof(cfg), &cfg);
}

static void add_entry(const char *key, const char *label, INTN icon, INTN kind, EFI_HANDLE dev, const CHAR16 *path,
                      const CHAR16 *opts) {
  INTN i;
  if (nents >= MAXE) return;
  for (i = 0; i < nents; i++) if (seq(ents[i].key, key)) return;
  scpy(ents[nents].key, key, 24);
  scpy(ents[nents].label, label, 32);
  ents[nents].icon = icon; ents[nents].kind = kind; ents[nents].dev = dev;
  if (path) StrCpy(ents[nents].path, (CHAR16 *)path); else ents[nents].path[0] = 0;
  if (opts) StrCpy(ents[nents].opts, (CHAR16 *)opts); else ents[nents].opts[0] = 0;
  nents++;
}

static int exists(EFI_FILE_HANDLE root, CHAR16 *path) {
  EFI_FILE_HANDLE f;
  if (EFI_ERROR(uefi_call_wrapper(root->Open, 5, root, &f, path, EFI_FILE_MODE_READ, (UINT64)0))) return 0;
  uefi_call_wrapper(f->Close, 1, f);
  return 1;
}

static const struct { const char *n, *label; INTN icon; } dtab[] = {
  {"kubuntu","Kubuntu",16},{"xubuntu","Xubuntu",17},{"lubuntu","Lubuntu",18},{"ubuntu-mate","Ubuntu MATE",19},
  {"ubuntumate","Ubuntu MATE",19},{"budgie","Ubuntu Budgie",20},{"ubuntu-studio","Ubuntu Studio",21},
  {"ubuntustudio","Ubuntu Studio",21},{"cinnamon","Ubuntu Cinnamon",22},{"edubuntu","Edubuntu",23},
  {"ubuntu","Ubuntu",0},{"microsoft","Windows",1},{"debian","Debian",2},{"mint","Linux Mint",6},
  {"fedora","Fedora",5},{"arch","Arch Linux",7},{"manjaro","Manjaro",8},{"opensuse","openSUSE",9},
  {"suse","openSUSE",9},{"zorin","Zorin OS",10},{"deepin","Deepin",11},{"elementary","elementary OS",12},
  {"endeavour","EndeavourOS",13},{"raspberry","Raspberry Pi OS",14},{"rpi","Raspberry Pi OS",14},
  {"neon","KDE neon",15},{"apple","macOS",4},{"macos","macOS",4},{"opencore","macOS",4},
  {"mx-linux","MX Linux",24},{"mxlinux","MX Linux",24},{"vanilla","Vanilla OS",25},{"nixos","NixOS",26},{"mx","MX Linux",24},
  {"freebsd","FreeBSD",27},{"redhat","Red Hat",28},{"rhel","Red Hat",28},
  {"garuda","Garuda Linux",29},{"bazzite","Bazzite",30},{"android","Android x86",31},{"solus","Solus",32},
  {"cachy","CachyOS",33},{"rocky","Rocky Linux",34},{"gentoo","Gentoo",35},{"void","Void Linux",36},{"kali","Kali Linux",37},
  {"parrot","ParrotOS",38},{"alpine","Alpine Linux",39},{"slackware","Slackware",40}
};
static const CHAR16 *cands[] = { L"shimx64.efi", L"grubx64.efi", L"Boot\\bootmgfw.efi", L"systemd-bootx64.efi",
                                 L"bootx64.efi", L"refind_x64.efi", L"OpenCore.efi", L"loader.efi", L"elilo.efi" };
static const char *skip_efi[] = { "mmx64.efi", "fbx64.efi", "fwupx64.efi", "fwupia32.efi", "fallback.efi" };

static void classify(const char *low, const char *raw, char *key, const char **label, INTN *icon) {
  INTN q;
  *label = raw; *icon = -1;
  scpy(key, low, 24);
  if (low[0] == 'p' && low[1] == 'o' && low[2] == 'p') { *label = "Pop!_OS"; *icon = 3; scpy(key, "pop", 24); }
  else if (seq(low, "oc")) { *label = "macOS"; *icon = 4; scpy(key, "macos", 24); }
  else
    for (q = 0; q < (INTN)(sizeof(dtab) / sizeof(dtab[0])); q++)
      if (has(low, dtab[q].n)) { *label = dtab[q].label; *icon = dtab[q].icon; scpy(key, dtab[q].n, 24); break; }
}

/* A bare EFI-stub kernel has no boot loader to hand it a command line, so without one it starts with no
 * root= and no initramfs and panics. systemd-boot style entries (\loader\entries\*.conf, which kernelstub
 * writes) carry that information, so look up the entry that points at this kernel and turn its
 * initrd/options lines into the load options the stub reads. Returns 0 when no entry matches. */
static INTN read_text(EFI_FILE_HANDLE root, const CHAR16 *path, char *buf, UINTN max) {
  EFI_FILE_HANDLE f; UINTN sz = max - 1;
  if (EFI_ERROR(uefi_call_wrapper(root->Open, 5, root, &f, path, EFI_FILE_MODE_READ, (UINT64)0))) return -1;
  if (EFI_ERROR(uefi_call_wrapper(f->Read, 3, f, &sz, buf))) sz = 0;
  uefi_call_wrapper(f->Close, 1, f);
  buf[sz] = 0;
  return (INTN)sz;
}

static void sapp(char *d, INTN max, const char *s, int bs) {
  INTN n = slen(d);
  for (; *s && n < max - 1; s++) d[n++] = (bs && *s == '/') ? '\\' : *s;
  d[n] = 0;
}

static int conf_opts(char *txt, const char *want, char *out, INTN omax) {
  char ini[200], opt[300]; int match = 0; char *p = txt;
  ini[0] = opt[0] = 0;
  while (*p) {
    char *k, *v, *e; char kw[12]; INTN n = 0;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    k = p;
    while (*p && *p != '\n') p++;
    e = p; if (*p) p++;
    while (e > k && (e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) e--;
    *e = 0;
    while (k[n] && k[n] != ' ' && k[n] != '\t' && n < 11) { kw[n] = k[n]; n++; }
    kw[n] = 0;
    v = k + n; while (*v == ' ' || *v == '\t') v++;
    if (seq(kw, "linux")) {
      char low[160]; INTN i = 0;
      for (; v[i] && i < 159; i++) { char c = v[i]; low[i] = (c == '\\') ? '/' : (c >= 'A' && c <= 'Z') ? c + 32 : c; }
      low[i] = 0;
      if (has(low, want)) match = 1;
    } else if (seq(kw, "initrd")) {
      if (ini[0]) sapp(ini, 200, " ", 0);
      sapp(ini, 200, "initrd=", 0); sapp(ini, 200, v, 1);
    } else if (seq(kw, "options")) {
      if (opt[0]) sapp(opt, 300, " ", 0);
      sapp(opt, 300, v, 0);
    }
  }
  if (!match || !opt[0]) return 0;
  out[0] = 0;
  if (ini[0] && !has(opt, "initrd=")) { sapp(out, omax, ini, 0); sapp(out, omax, " ", 0); }
  sapp(out, omax, opt, 0);
  return 1;
}

static int stub_opts(EFI_FILE_HANDLE root, const CHAR16 *kpath, CHAR16 *out) {
  EFI_FILE_HANDLE dir; UINT64 buf[64]; char want[160], txt[2048], o[OPTS_MAX]; int ok = 0; INTN i;
  narrow(kpath, want, 160, 1);
  for (i = 0; want[i]; i++) if (want[i] == '\\') want[i] = '/';
  if (EFI_ERROR(uefi_call_wrapper(root->Open, 5, root, &dir, L"\\loader\\entries", EFI_FILE_MODE_READ, (UINT64)0))) return 0;
  while (!ok) {
    UINTN sz = sizeof(buf); EFI_FILE_INFO *fi = (EFI_FILE_INFO *)buf; char low[40]; CHAR16 cp[160];
    if (EFI_ERROR(uefi_call_wrapper(dir->Read, 3, dir, &sz, buf)) || sz == 0) break;
    if ((fi->Attribute & EFI_FILE_DIRECTORY) || StrLen(fi->FileName) > 100) continue;
    narrow(fi->FileName, low, 40, 1);
    if (!has(low, ".conf")) continue;
    StrCpy(cp, L"\\loader\\entries\\"); StrCat(cp, fi->FileName);
    if (read_text(root, cp, txt, sizeof(txt)) <= 0) continue;
    if (conf_opts(txt, want, o, OPTS_MAX)) {
      for (i = 0; o[i]; i++) out[i] = (CHAR16)(UINT8)o[i];
      out[i] = 0; ok = 1;
    }
  }
  uefi_call_wrapper(dir->Close, 1, dir);
  return ok;
}

/* Some distros (e.g. Pop!_OS's kernelstub) boot the kernel's EFI stub directly, with no
 * grub/shim/systemd-boot in sight. If none of the known loader names matched, take any other
 * .efi file in the directory that a boot entry gives a command line for, skipping shim's own helper
 * binaries. A directory can hold more than one, e.g. a normal kernel plus a recovery one (Pop!_OS) or
 * several generations (NixOS); prefer one that doesn't look like a recovery/rescue kernel, so that isn't
 * what boots by default. */
static int any_efi(EFI_FILE_HANDLE root, const CHAR16 *dname, CHAR16 *out, CHAR16 *opts) {
  EFI_FILE_HANDLE dir; CHAR16 dpath[140], full[160], fallback[160], fopts[OPTS_MAX]; UINT64 buf[64]; int have_fallback = 0;
  StrCpy(dpath, L"\\EFI\\"); StrCat(dpath, dname);
  if (EFI_ERROR(uefi_call_wrapper(root->Open, 5, root, &dir, dpath, EFI_FILE_MODE_READ, (UINT64)0))) return 0;
  for (;;) {
    UINTN sz = sizeof(buf); EFI_FILE_INFO *fi = (EFI_FILE_INFO *)buf;
    char low[32]; INTN q; int bad = 0;
    if (EFI_ERROR(uefi_call_wrapper(dir->Read, 3, dir, &sz, buf)) || sz == 0) break;
    if ((fi->Attribute & EFI_FILE_DIRECTORY) || StrLen(dpath) + 1 + StrLen(fi->FileName) >= 150) continue;
    narrow(fi->FileName, low, 32, 1);
    if (!has(low, ".efi")) continue;
    for (q = 0; q < (INTN)(sizeof(skip_efi) / sizeof(skip_efi[0])); q++) if (seq(low, skip_efi[q])) { bad = 1; break; }
    if (bad) continue;
    StrCpy(full, dpath); StrCat(full, L"\\"); StrCat(full, fi->FileName);
    if (!stub_opts(root, full, opts)) continue;
    if (!has(low, "recovery") && !has(low, "rescue")) {
      StrCpy(out, full);
      uefi_call_wrapper(dir->Close, 1, dir);
      return 1;
    }
    if (!have_fallback) { StrCpy(fallback, full); StrCpy(fopts, opts); have_fallback = 1; }
  }
  uefi_call_wrapper(dir->Close, 1, dir);
  if (have_fallback) { StrCpy(out, fallback); StrCpy(opts, fopts); return 1; }
  return 0;
}

static void scan_vol(EFI_HANDLE h, INTN idx, int isself) {
  EFI_FILE_IO_INTERFACE *fs; EFI_FILE_HANDLE root, dir; INTN found = 0;
  if (EFI_ERROR(uefi_call_wrapper(BS->HandleProtocol, 3, h, &FSG, (void **)&fs))) return;
  if (EFI_ERROR(uefi_call_wrapper(fs->OpenVolume, 2, fs, &root))) return;
  if (!EFI_ERROR(uefi_call_wrapper(root->Open, 5, root, &dir, L"\\EFI", EFI_FILE_MODE_READ, (UINT64)0))) {
    UINT64 buf[64];
    for (;;) {
      UINTN sz = sizeof(buf); EFI_FILE_INFO *fi = (EFI_FILE_INFO *)buf;
      char key[24], raw[32], low[32]; CHAR16 path[160], opts[OPTS_MAX]; INTN k;
      if (EFI_ERROR(uefi_call_wrapper(dir->Read, 3, dir, &sz, buf)) || sz == 0) break;
      if (!(fi->Attribute & EFI_FILE_DIRECTORY) || StrLen(fi->FileName) > 100) continue;
      narrow(fi->FileName, raw, 32, 0);
      narrow(fi->FileName, low, 32, 1);
      if (seq(low, ".") || seq(low, "..") || seq(low, "boot") || seq(low, "bootr")) continue;
      for (k = 0; k < 9; k++) {
        StrCpy(path, L"\\EFI\\"); StrCat(path, fi->FileName); StrCat(path, L"\\"); StrCat(path, (CHAR16 *)cands[k]);
        if (exists(root, path)) {
          const char *label; INTN icon;
          classify(low, raw, key, &label, &icon);
          add_entry(key, label, icon, 0, h, path, NULL);
          found++;
          break;
        }
      }
      if (k == 9 && any_efi(root, fi->FileName, path, opts)) {
        const char *label; INTN icon;
        classify(low, raw, key, &label, &icon);
        add_entry(key, label, icon, 0, h, path, opts);
        found++;
      }
    }
    uefi_call_wrapper(dir->Close, 1, dir);
  }
  if (!found && !isself && exists(root, L"\\EFI\\BOOT\\BOOTX64.EFI")) {
    char key[24] = "disk0", label[32] = "Boot disk ";
    key[4] = '0' + (char)(idx % 10); label[10] = '1' + (char)(idx % 9); label[11] = 0;
    add_entry(key, label, -1, 0, h, L"\\EFI\\BOOT\\BOOTX64.EFI", NULL);
  }
  if (exists(root, L"\\System\\Library\\CoreServices\\boot.efi"))
    add_entry("macos", "macOS", 4, 0, h, L"\\System\\Library\\CoreServices\\boot.efi", NULL);
  uefi_call_wrapper(root->Close, 1, root);
}

void scan_all(EFI_HANDLE image) {
  EFI_LOADED_IMAGE *li = NULL; EFI_HANDLE self = NULL, *hs = NULL; UINTN n = 0, i, sz = 8; UINT64 sup = 0;
  nents = 0;
  if (!EFI_ERROR(uefi_call_wrapper(BS->HandleProtocol, 3, image, &LIG, (void **)&li)) && li) self = li->DeviceHandle;
  if (!EFI_ERROR(uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &FSG, NULL, &n, &hs))) {
    for (i = 0; i < n; i++) scan_vol(hs[i], (INTN)i, hs[i] == self);
    FreePool(hs);
  }
  if (!EFI_ERROR(uefi_call_wrapper(RT->GetVariable, 5, L"OsIndicationsSupported", &GVG, NULL, &sz, &sup)) && (sup & 1))
    add_entry("fw", "UEFI Firmware", -1, 1, NULL, NULL, NULL);
}

void settings_sync(void) {
  INTN i; UINT32 j;
  for (i = 0; i < nents; i++) {
    int f = 0;
    for (j = 0; j < cfg.n; j++) if (seq(cfg.e[j].key, ents[i].key)) { f = 1; break; }
    if (!f && cfg.n < MAXE) { scpy(cfg.e[cfg.n].key, ents[i].key, 24); cfg.e[cfg.n].name[0] = 0; cfg.e[cfg.n].hidden = 0; cfg.n++; }
  }
  if (!cfg.def[0] && cfg.n) scpy(cfg.def, cfg.e[0].key, 24);
}

INTN build_view(VRow *all) {
  INTN n = 0, i; UINT32 j;
  for (j = 0; j < cfg.n; j++)
    for (i = 0; i < nents; i++) if (seq(cfg.e[j].key, ents[i].key)) { all[n].ent = i; all[n].sidx = (INTN)j; n++; break; }
  return n;
}

const char *disp_name(VRow v) { return cfg.e[v.sidx].name[0] ? cfg.e[v.sidx].name : ents[v.ent].label; }
