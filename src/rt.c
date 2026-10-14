/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Steven Azevedo */
#include "uefi.h"

EFI_SYSTEM_TABLE *ST;
EFI_BOOT_SERVICES *BS;
EFI_RUNTIME_SERVICES *RT;

void InitializeLib(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) { (void)image; ST = st; BS = st->BootServices; RT = st->RuntimeServices; }

void *AllocatePool(UINTN size) { void *p = NULL; return EFI_ERROR(BS->AllocatePool(EfiLoaderData, size, &p)) ? NULL : p; }
void FreePool(void *p) { if (p) BS->FreePool(p); }
void ZeroMem(void *p, UINTN n) { UINT8 *b = p; while (n--) *b++ = 0; }

UINTN StrLen(const CHAR16 *s) { UINTN n = 0; while (s[n]) n++; return n; }
void StrCpy(CHAR16 *d, const CHAR16 *s) { while ((*d++ = *s++)) ; }
void StrCat(CHAR16 *d, const CHAR16 *s) { while (*d) d++; StrCpy(d, s); }
void Print(const CHAR16 *s) { ST->ConOut->OutputString(ST->ConOut, s); }

/* Device path of a file on a volume: the volume's path + a media file-path node + end node. */
EFI_DEVICE_PATH *FileDevicePath(EFI_HANDLE device, const CHAR16 *path) {
  EFI_GUID g = DEVICE_PATH_PROTOCOL; EFI_DEVICE_PATH *base = NULL, *n, *out;
  UINTN blen = 0, plen = (StrLen(path) + 1) * 2, nlen = 4 + plen, i;
  UINT8 *o;
  if (device && !EFI_ERROR(BS->HandleProtocol(device, &g, (void **)&base)) && base) {
    for (n = base; !(n->Type == 0x7f && n->SubType == 0xff); n = (EFI_DEVICE_PATH *)((UINT8 *)n + (n->Length[0] | (n->Length[1] << 8))))
      ;
    blen = (UINTN)((UINT8 *)n - (UINT8 *)base);
  }
  out = AllocatePool(blen + nlen + 4);
  if (!out) return NULL;
  o = (UINT8 *)out;
  for (i = 0; i < blen; i++) o[i] = ((UINT8 *)base)[i];
  o += blen;
  o[0] = 4; o[1] = 4; o[2] = (UINT8)nlen; o[3] = (UINT8)(nlen >> 8);
  for (i = 0; i < plen; i++) o[4 + i] = ((const UINT8 *)path)[i];
  o += nlen;
  o[0] = 0x7f; o[1] = 0xff; o[2] = 4; o[3] = 0;
  return out;
}

/* The compiler may emit these even in freestanding code. */
void *memset(void *d, int c, size_t n) { UINT8 *p = d; while (n--) *p++ = (UINT8)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { UINT8 *a = d; const UINT8 *b = s; while (n--) *a++ = *b++; return d; }
void *memmove(void *d, const void *s, size_t n) {
  UINT8 *a = d; const UINT8 *b = s;
  if (a < b) while (n--) *a++ = *b++; else { a += n; b += n; while (n--) *--a = *--b; }
  return d;
}
int memcmp(const void *x, const void *y, size_t n) {
  const UINT8 *a = x, *b = y;
  for (; n; n--, a++, b++) if (*a != *b) return *a - *b;
  return 0;
}
