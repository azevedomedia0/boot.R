/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Steven Azevedo */
/* Minimal UEFI definitions for x86_64. Only what boot.R uses, laid out per UEFI 2.x.
   Replaces gnu-efi so the app builds with a bare mingw-w64 (or clang/lld-link) cross compiler.
   The x86_64 Windows ABI is the UEFI ABI, so protocol calls are plain C calls. */
#ifndef UEFI_H
#define UEFI_H
#include <stdint.h>
#include <stddef.h>

typedef uint8_t  UINT8;  typedef uint16_t UINT16; typedef uint32_t UINT32; typedef uint64_t UINT64;
typedef int8_t   INT8;   typedef int16_t  INT16;  typedef int32_t INT32; typedef int64_t  INTN;    typedef uint64_t UINTN;
typedef uint16_t CHAR16; typedef uint8_t  BOOLEAN;
typedef UINTN EFI_STATUS; typedef void *EFI_HANDLE; typedef void *EFI_EVENT;
typedef UINT64 EFI_PHYSICAL_ADDRESS;
#define TRUE 1
#define FALSE 0
#define EFIAPI

#define EFI_ERROR(s) (((INTN)(s)) < 0)
#define EFI_SUCCESS 0
#define EFIERR(n) ((EFI_STATUS)0x8000000000000000ULL | (n))
#define EFI_UNSUPPORTED     EFIERR(3)
#define EFI_OUT_OF_RESOURCES EFIERR(9)

typedef struct { UINT32 a; UINT16 b, c; UINT8 d[8]; } EFI_GUID;

typedef struct { UINT64 Signature; UINT32 Revision, HeaderSize, CRC32, Reserved; } EFI_TABLE_HEADER;

/* ---- console ---- */
typedef struct { UINT16 ScanCode; CHAR16 UnicodeChar; } EFI_INPUT_KEY;
#define SCAN_UP 1
#define SCAN_DOWN 2
#define SCAN_RIGHT 3
#define SCAN_LEFT 4
#define SCAN_ESC 0x17
#define CHAR_BACKSPACE 0x08
#define CHAR_CARRIAGE_RETURN 0x0d

typedef struct EFI_SIMPLE_TEXT_INPUT EFI_SIMPLE_TEXT_INPUT;
struct EFI_SIMPLE_TEXT_INPUT {
  void *Reset;
  EFI_STATUS (*ReadKeyStroke)(EFI_SIMPLE_TEXT_INPUT *, EFI_INPUT_KEY *);
  EFI_EVENT WaitForKey;
};
typedef struct EFI_SIMPLE_TEXT_OUTPUT EFI_SIMPLE_TEXT_OUTPUT;
struct EFI_SIMPLE_TEXT_OUTPUT {
  void *Reset;
  EFI_STATUS (*OutputString)(EFI_SIMPLE_TEXT_OUTPUT *, const CHAR16 *);
  void *TestString, *QueryMode, *SetMode, *SetAttribute, *ClearScreen, *SetCursorPosition;
  EFI_STATUS (*EnableCursor)(EFI_SIMPLE_TEXT_OUTPUT *, BOOLEAN);
  void *Mode;
};

/* ---- boot services ---- */
typedef enum { AllHandles, ByRegisterNotify, ByProtocol } EFI_LOCATE_SEARCH_TYPE;
typedef enum { EfiReservedMemoryType, EfiLoaderCode, EfiLoaderData } EFI_MEMORY_TYPE;
typedef enum { TimerCancel, TimerPeriodic, TimerRelative } EFI_TIMER_DELAY;
#define EVT_TIMER 0x80000000u
#define TPL_APPLICATION 4

typedef struct EFI_DEVICE_PATH { UINT8 Type, SubType; UINT8 Length[2]; } EFI_DEVICE_PATH;

typedef struct {
  EFI_TABLE_HEADER Hdr;
  void *RaiseTPL, *RestoreTPL, *AllocatePages, *FreePages, *GetMemoryMap;
  EFI_STATUS (*AllocatePool)(EFI_MEMORY_TYPE, UINTN, void **);
  EFI_STATUS (*FreePool)(void *);
  EFI_STATUS (*CreateEvent)(UINT32, UINTN, void *, void *, EFI_EVENT *);
  EFI_STATUS (*SetTimer)(EFI_EVENT, EFI_TIMER_DELAY, UINT64);
  EFI_STATUS (*WaitForEvent)(UINTN, EFI_EVENT *, UINTN *);
  void *SignalEvent, *CloseEvent, *CheckEvent;
  void *InstallProtocolInterface, *ReinstallProtocolInterface, *UninstallProtocolInterface;
  EFI_STATUS (*HandleProtocol)(EFI_HANDLE, EFI_GUID *, void **);
  void *Reserved, *RegisterProtocolNotify, *LocateHandle, *LocateDevicePath, *InstallConfigurationTable;
  EFI_STATUS (*LoadImage)(BOOLEAN, EFI_HANDLE, EFI_DEVICE_PATH *, void *, UINTN, EFI_HANDLE *);
  EFI_STATUS (*StartImage)(EFI_HANDLE, UINTN *, CHAR16 **);
  void *Exit, *UnloadImage, *ExitBootServices, *GetNextMonotonicCount;
  EFI_STATUS (*Stall)(UINTN);
  EFI_STATUS (*SetWatchdogTimer)(UINTN, UINT64, UINTN, CHAR16 *);
  EFI_STATUS (*ConnectController)(EFI_HANDLE, EFI_HANDLE *, EFI_DEVICE_PATH *, BOOLEAN);
  void *DisconnectController, *OpenProtocol, *CloseProtocol, *OpenProtocolInformation;
  void *ProtocolsPerHandle;
  EFI_STATUS (*LocateHandleBuffer)(EFI_LOCATE_SEARCH_TYPE, EFI_GUID *, void *, UINTN *, EFI_HANDLE **);
  EFI_STATUS (*LocateProtocol)(EFI_GUID *, void *, void **);
  void *InstallMultipleProtocolInterfaces, *UninstallMultipleProtocolInterfaces, *CalculateCrc32;
  void *CopyMem, *SetMem, *CreateEventEx;
} EFI_BOOT_SERVICES;

/* ---- runtime services ---- */
typedef enum { EfiResetCold, EfiResetWarm, EfiResetShutdown } EFI_RESET_TYPE;
#define EFI_VARIABLE_NON_VOLATILE 1
#define EFI_VARIABLE_BOOTSERVICE_ACCESS 2
#define EFI_VARIABLE_RUNTIME_ACCESS 4

typedef struct {
  EFI_TABLE_HEADER Hdr;
  void *GetTime, *SetTime, *GetWakeupTime, *SetWakeupTime, *SetVirtualAddressMap, *ConvertPointer;
  EFI_STATUS (*GetVariable)(const CHAR16 *, EFI_GUID *, UINT32 *, UINTN *, void *);
  void *GetNextVariableName;
  EFI_STATUS (*SetVariable)(const CHAR16 *, EFI_GUID *, UINT32, UINTN, void *);
  void *GetNextHighMonotonicCount;
  void (*ResetSystem)(EFI_RESET_TYPE, EFI_STATUS, UINTN, void *);
} EFI_RUNTIME_SERVICES;

typedef struct {
  EFI_TABLE_HEADER Hdr;
  CHAR16 *FirmwareVendor; UINT32 FirmwareRevision;
  EFI_HANDLE ConsoleInHandle; EFI_SIMPLE_TEXT_INPUT *ConIn;
  EFI_HANDLE ConsoleOutHandle; EFI_SIMPLE_TEXT_OUTPUT *ConOut;
  EFI_HANDLE StandardErrorHandle; EFI_SIMPLE_TEXT_OUTPUT *StdErr;
  EFI_RUNTIME_SERVICES *RuntimeServices; EFI_BOOT_SERVICES *BootServices;
  UINTN NumberOfTableEntries; void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

/* ---- graphics output ---- */
typedef struct { UINT8 Blue, Green, Red, Reserved; } EFI_GRAPHICS_OUTPUT_BLT_PIXEL;
typedef enum { EfiBltVideoFill, EfiBltVideoToBltBuffer, EfiBltBufferToVideo, EfiBltVideoToVideo } EFI_GRAPHICS_OUTPUT_BLT_OPERATION;
typedef struct { UINT32 Version, HorizontalResolution, VerticalResolution, PixelFormat, PixelInformation[4], PixelsPerScanLine; }
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;
typedef struct {
  UINT32 MaxMode, Mode; EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info; UINTN SizeOfInfo;
  EFI_PHYSICAL_ADDRESS FrameBufferBase; UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;
typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;
struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
  EFI_STATUS (*QueryMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL *, UINT32, UINTN *, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **);
  EFI_STATUS (*SetMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL *, UINT32);
  EFI_STATUS (*Blt)(EFI_GRAPHICS_OUTPUT_PROTOCOL *, EFI_GRAPHICS_OUTPUT_BLT_PIXEL *, EFI_GRAPHICS_OUTPUT_BLT_OPERATION,
                    UINTN, UINTN, UINTN, UINTN, UINTN, UINTN, UINTN);
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

/* ---- file system ---- */
#define EFI_FILE_MODE_READ 1ULL
#define EFI_FILE_DIRECTORY 0x10ULL
typedef struct { UINT16 Year; UINT8 Month, Day, Hour, Minute, Second, Pad1; UINT32 Nanosecond; INT16 TimeZone; UINT8 Daylight, Pad2; } EFI_TIME;
typedef struct {
  UINT64 Size, FileSize, PhysicalSize; EFI_TIME CreateTime, LastAccessTime, ModificationTime; UINT64 Attribute; CHAR16 FileName[1];
} EFI_FILE_INFO;
typedef struct EFI_FILE *EFI_FILE_HANDLE;
struct EFI_FILE {
  UINT64 Revision;
  EFI_STATUS (*Open)(EFI_FILE_HANDLE, EFI_FILE_HANDLE *, const CHAR16 *, UINT64, UINT64);
  EFI_STATUS (*Close)(EFI_FILE_HANDLE);
  void *Delete;
  EFI_STATUS (*Read)(EFI_FILE_HANDLE, UINTN *, void *);
  void *Write, *GetPosition, *SetPosition, *GetInfo, *SetInfo, *Flush;
};
typedef struct EFI_FILE_IO_INTERFACE EFI_FILE_IO_INTERFACE;
struct EFI_FILE_IO_INTERFACE { UINT64 Revision; EFI_STATUS (*OpenVolume)(EFI_FILE_IO_INTERFACE *, EFI_FILE_HANDLE *); };

typedef struct {
  UINT32 Revision; EFI_HANDLE ParentHandle; EFI_SYSTEM_TABLE *SystemTable; EFI_HANDLE DeviceHandle;
  EFI_DEVICE_PATH *FilePath; void *Reserved; UINT32 LoadOptionsSize; void *LoadOptions;
} EFI_LOADED_IMAGE;

/* ---- pointers ---- */
typedef struct { INT32 RelativeMovementX, RelativeMovementY, RelativeMovementZ; BOOLEAN LeftButton, RightButton; } EFI_SIMPLE_POINTER_STATE;
typedef struct EFI_SIMPLE_POINTER EFI_SIMPLE_POINTER;
struct EFI_SIMPLE_POINTER {
  EFI_STATUS (*Reset)(EFI_SIMPLE_POINTER *, BOOLEAN);
  EFI_STATUS (*GetState)(EFI_SIMPLE_POINTER *, EFI_SIMPLE_POINTER_STATE *);
  EFI_EVENT WaitForInput;
  void *Mode;
};
typedef struct { UINT64 CurrentX, CurrentY, CurrentZ; UINT32 ActiveButtons; } EFI_ABSOLUTE_POINTER_STATE;
typedef struct { UINT64 AbsoluteMinX, AbsoluteMinY, AbsoluteMinZ, AbsoluteMaxX, AbsoluteMaxY, AbsoluteMaxZ; UINT32 Attributes; } EFI_ABSOLUTE_POINTER_MODE;
typedef struct EFI_ABSOLUTE_POINTER EFI_ABSOLUTE_POINTER;
struct EFI_ABSOLUTE_POINTER {
  EFI_STATUS (*Reset)(EFI_ABSOLUTE_POINTER *, BOOLEAN);
  EFI_STATUS (*GetState)(EFI_ABSOLUTE_POINTER *, EFI_ABSOLUTE_POINTER_STATE *);
  EFI_EVENT WaitForInput;
  EFI_ABSOLUTE_POINTER_MODE *Mode;
};

/* ---- PCI I/O (only config-space access is used) ---- */
typedef struct EFI_PCI_IO EFI_PCI_IO;
struct EFI_PCI_IO {
  void *PollMem, *PollIo;
  struct { void *Read, *Write; } Mem, Io;
  struct {
    EFI_STATUS (*Read)(EFI_PCI_IO *, UINT32 Width, UINT32 Offset, UINTN Count, void *Buffer);
    EFI_STATUS (*Write)(EFI_PCI_IO *, UINT32 Width, UINT32 Offset, UINTN Count, void *Buffer);
  } Pci;
  void *CopyMem, *Map, *Unmap, *AllocateBuffer, *FreeBuffer, *Flush;
  EFI_STATUS (*GetLocation)(EFI_PCI_IO *, UINTN *Seg, UINTN *Bus, UINTN *Dev, UINTN *Fn);
  void *Attributes, *GetBarAttributes, *SetBarAttributes;
  UINT64 RomSize; void *RomImage;
};
#define EFI_PCI_IO_PROTOCOL_GUID {0x4cf5b200, 0x68b8, 0x4ca5, {0x9e, 0xec, 0xb2, 0x3e, 0x3f, 0x50, 0x02, 0x9a}}

/* ---- GUIDs ---- */
#define EFI_SIMPLE_POINTER_PROTOCOL_GUID   {0x31878c87, 0x0b75, 0x11d5, {0x9a, 0x4f, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}}
#define EFI_ABSOLUTE_POINTER_PROTOCOL_GUID {0x8d59d32b, 0xc655, 0x4ae9, {0x9b, 0x15, 0xf2, 0x59, 0x04, 0x99, 0x2a, 0x43}}
#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID {0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}}
#define SIMPLE_FILE_SYSTEM_PROTOCOL       {0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}
#define LOADED_IMAGE_PROTOCOL             {0x5b1b31a1, 0x9562, 0x11d2, {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}
#define DEVICE_PATH_PROTOCOL              {0x09576e91, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}
#define EFI_GLOBAL_VARIABLE               {0x8be4df61, 0x93ca, 0x11d2, {0xaa, 0x0d, 0x00, 0xe0, 0x98, 0x03, 0x2b, 0x8c}}

/* ---- runtime glue (src/rt.c) ---- */
extern EFI_SYSTEM_TABLE *ST;
extern EFI_BOOT_SERVICES *BS;
extern EFI_RUNTIME_SERVICES *RT;
#define uefi_call_wrapper(fn, n, ...) ((fn)(__VA_ARGS__))

void   InitializeLib(EFI_HANDLE image, EFI_SYSTEM_TABLE *st);
void  *AllocatePool(UINTN size);
void   FreePool(void *p);
void   ZeroMem(void *p, UINTN n);
UINTN  StrLen(const CHAR16 *s);
void   StrCpy(CHAR16 *d, const CHAR16 *s);
void   StrCat(CHAR16 *d, const CHAR16 *s);
void   Print(const CHAR16 *s);
EFI_DEVICE_PATH *FileDevicePath(EFI_HANDLE device, const CHAR16 *path);
#endif
