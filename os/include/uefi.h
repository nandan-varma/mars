#ifndef UEFI_H
#define UEFI_H

#include <stddef.h>
#include <stdint.h>

typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef int8_t INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;
typedef uint64_t UINTN;
typedef int64_t INTN;
typedef UINT16 CHAR16;
typedef void VOID;

typedef UINTN EFI_STATUS;
typedef VOID *EFI_HANDLE;
typedef UINT64 EFI_PHYSICAL_ADDRESS;
typedef UINT64 EFI_VIRTUAL_ADDRESS;
typedef UINT8 BOOLEAN;

typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8 Data4[8];
} EFI_GUID;

#if defined(__x86_64__)
#define EFIAPI __attribute__((ms_abi))
#else
#define EFIAPI
#endif

#define TRUE 1
#define FALSE 0

#define EFI_SUCCESS 0
#define EFI_NOT_READY ((EFI_STATUS)0x8000000000000006ULL)
#define EFI_BUFFER_TOO_SMALL ((EFI_STATUS)0x8000000000000005ULL)
#define EFI_ERROR(status) (((status) & 0x8000000000000000ULL) != 0)

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

typedef struct {
    UINT32 Type;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef enum {
    TimerCancel,
    TimerPeriodic,
    TimerRelative
} EFI_TIMER_DELAY;

typedef VOID *EFI_EVENT;
typedef UINTN EFI_TPL;
typedef UINTN EFI_ALLOCATE_TYPE;
typedef UINTN EFI_MEMORY_TYPE;
typedef VOID *EFI_DEVICE_PATH_PROTOCOL;

typedef enum {
    AllHandles,
    ByRegisterNotify,
    ByProtocol
} EFI_LOCATE_SEARCH_TYPE;

struct EFI_RUNTIME_SERVICES;
struct EFI_BOOT_SERVICES;
struct EFI_SYSTEM_TABLE;

typedef struct EFI_RUNTIME_SERVICES EFI_RUNTIME_SERVICES;
typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct EFI_SYSTEM_TABLE EFI_SYSTEM_TABLE;

typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

typedef struct {
    UINT16 Year;
    UINT8 Month;
    UINT8 Day;
    UINT8 Hour;
    UINT8 Minute;
    UINT8 Second;
    UINT8 Pad1;
    UINT32 Nanosecond;
    INT16 TimeZone;
    UINT8 Daylight;
    UINT8 Pad2;
} EFI_TIME;

typedef struct {
    UINT32 Resolution;
    UINT32 Accuracy;
    BOOLEAN SetsToZero;
} EFI_TIME_CAPABILITIES;

typedef struct {
    UINT32 KeyShiftState;
    UINT8 KeyToggleState;
} EFI_KEY_STATE;

typedef struct {
    EFI_INPUT_KEY Key;
    EFI_KEY_STATE KeyState;
} EFI_KEY_DATA;

typedef UINT8 EFI_KEY_TOGGLE_STATE;

typedef struct EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL;
struct EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL {
    EFI_STATUS(EFIAPI *Reset)(EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This, BOOLEAN ExtendedVerification);
    EFI_STATUS(EFIAPI *ReadKeyStrokeEx)(EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This, EFI_KEY_DATA *KeyData);
    EFI_EVENT WaitForKeyEx;
    EFI_STATUS(EFIAPI *SetState)(EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This, EFI_KEY_TOGGLE_STATE *KeyToggleState);
    EFI_STATUS(EFIAPI *RegisterKeyNotify)(EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This, EFI_KEY_DATA *KeyData, VOID *KeyNotificationFunction, VOID **NotifyHandle);
    EFI_STATUS(EFIAPI *UnregisterKeyNotify)(EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This, VOID *NotificationHandle);
};

typedef struct {
    UINT64 ResolutionX;
    UINT64 ResolutionY;
    UINT64 ResolutionZ;
    BOOLEAN LeftButton;
    BOOLEAN RightButton;
} EFI_SIMPLE_POINTER_MODE;

typedef struct {
    INT32 RelativeMovementX;
    INT32 RelativeMovementY;
    INT32 RelativeMovementZ;
    BOOLEAN LeftButton;
    BOOLEAN RightButton;
} EFI_SIMPLE_POINTER_STATE;

typedef struct EFI_SIMPLE_POINTER_PROTOCOL EFI_SIMPLE_POINTER_PROTOCOL;
struct EFI_SIMPLE_POINTER_PROTOCOL {
    EFI_STATUS(EFIAPI *Reset)(EFI_SIMPLE_POINTER_PROTOCOL *This, BOOLEAN ExtendedVerification);
    EFI_STATUS(EFIAPI *GetState)(EFI_SIMPLE_POINTER_PROTOCOL *This, EFI_SIMPLE_POINTER_STATE *State);
    EFI_EVENT WaitForInput;
    EFI_SIMPLE_POINTER_MODE *Mode;
};

typedef struct {
    UINT64 AbsoluteMinX;
    UINT64 AbsoluteMinY;
    UINT64 AbsoluteMinZ;
    UINT64 AbsoluteMaxX;
    UINT64 AbsoluteMaxY;
    UINT64 AbsoluteMaxZ;
    UINT32 Attributes;
} EFI_ABSOLUTE_POINTER_MODE;

typedef struct {
    UINT64 CurrentX;
    UINT64 CurrentY;
    UINT64 CurrentZ;
    UINT32 ActiveButtons;
} EFI_ABSOLUTE_POINTER_STATE;

typedef struct EFI_ABSOLUTE_POINTER_PROTOCOL EFI_ABSOLUTE_POINTER_PROTOCOL;
struct EFI_ABSOLUTE_POINTER_PROTOCOL {
    EFI_STATUS(EFIAPI *Reset)(EFI_ABSOLUTE_POINTER_PROTOCOL *This, BOOLEAN ExtendedVerification);
    EFI_STATUS(EFIAPI *GetState)(EFI_ABSOLUTE_POINTER_PROTOCOL *This, EFI_ABSOLUTE_POINTER_STATE *State);
    EFI_EVENT WaitForInput;
    EFI_ABSOLUTE_POINTER_MODE *Mode;
};

typedef struct {
    UINT32 RedMask;
    UINT32 GreenMask;
    UINT32 BlueMask;
    UINT32 ReservedMask;
} EFI_PIXEL_BITMASK;

typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    EFI_PIXEL_BITMASK PixelInformation;
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;
struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_STATUS(EFIAPI *QueryMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 ModeNumber, UINTN *SizeOfInfo, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info);
    EFI_STATUS(EFIAPI *SetMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 ModeNumber);
    VOID *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;
    VOID *RaiseTPL;
    VOID *RestoreTPL;
    VOID *AllocatePages;
    VOID *FreePages;
    EFI_STATUS(EFIAPI *GetMemoryMap)(UINTN *MemoryMapSize, EFI_MEMORY_DESCRIPTOR *MemoryMap, UINTN *MapKey, UINTN *DescriptorSize, UINT32 *DescriptorVersion);
    EFI_STATUS(EFIAPI *AllocatePool)(EFI_MEMORY_TYPE PoolType, UINTN Size, VOID **Buffer);
    EFI_STATUS(EFIAPI *FreePool)(VOID *Buffer);
    VOID *CreateEvent;
    VOID *SetTimer;
    EFI_STATUS(EFIAPI *WaitForEvent)(UINTN NumberOfEvents, EFI_EVENT *Event, UINTN *Index);
    VOID *SignalEvent;
    VOID *CloseEvent;
    EFI_STATUS(EFIAPI *CheckEvent)(EFI_EVENT Event);
    VOID *InstallProtocolInterface;
    VOID *ReinstallProtocolInterface;
    VOID *UninstallProtocolInterface;
    EFI_STATUS(EFIAPI *HandleProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol, VOID **Interface);
    VOID *Reserved;
    VOID *RegisterProtocolNotify;
    VOID *LocateHandle;
    VOID *LocateDevicePath;
    VOID *InstallConfigurationTable;
    VOID *LoadImage;
    VOID *StartImage;
    VOID *Exit;
    VOID *UnloadImage;
    EFI_STATUS(EFIAPI *ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);
    VOID *GetNextMonotonicCount;
    EFI_STATUS(EFIAPI *Stall)(UINTN Microseconds);
    VOID *SetWatchdogTimer;
    VOID *ConnectController;
    VOID *DisconnectController;
    VOID *OpenProtocol;
    VOID *CloseProtocol;
    VOID *OpenProtocolInformation;
    VOID *ProtocolsPerHandle;
    EFI_STATUS(EFIAPI *LocateHandleBuffer)(
        EFI_LOCATE_SEARCH_TYPE SearchType,
        EFI_GUID *Protocol,
        VOID *SearchKey,
        UINTN *NoHandles,
        EFI_HANDLE **Buffer
    );
    EFI_STATUS(EFIAPI *LocateProtocol)(EFI_GUID *Protocol, VOID *Registration, VOID **Interface);
    VOID *InstallMultipleProtocolInterfaces;
    VOID *UninstallMultipleProtocolInterfaces;
    VOID *CalculateCrc32;
    VOID *CopyMem;
    VOID *SetMem;
    VOID *CreateEventEx;
};

struct EFI_RUNTIME_SERVICES {
    EFI_TABLE_HEADER Hdr;
    EFI_STATUS(EFIAPI *GetTime)(EFI_TIME *Time, EFI_TIME_CAPABILITIES *Capabilities);
    VOID *SetTime;
    VOID *GetWakeupTime;
    VOID *SetWakeupTime;
    VOID *SetVirtualAddressMap;
    VOID *ConvertPointer;
    VOID *GetVariable;
    VOID *GetNextVariableName;
    VOID *SetVariable;
    VOID *GetNextHighMonotonicCount;
    VOID *ResetSystem;
    VOID *UpdateCapsule;
    VOID *QueryCapsuleCapabilities;
    VOID *QueryVariableInfo;
};

struct EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    VOID *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    VOID *ConOut;
    EFI_HANDLE StandardErrorHandle;
    VOID *StdErr;
    EFI_RUNTIME_SERVICES *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    VOID *ConfigurationTable;
};

#define EfiLoaderData ((EFI_MEMORY_TYPE)4)
#define EfiBootServicesCode ((EFI_MEMORY_TYPE)3)
#define EfiBootServicesData ((EFI_MEMORY_TYPE)4)
#define EfiConventionalMemory ((EFI_MEMORY_TYPE)7)

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    (EFI_GUID) { 0x9042A9DE, 0x23DC, 0x4A38, { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A } }

#define EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID \
    (EFI_GUID) { 0xDD9E7534, 0x7762, 0x4698, { 0x8C, 0x14, 0xF5, 0x85, 0x17, 0xA6, 0x25, 0xAA } }

#define EFI_SIMPLE_POINTER_PROTOCOL_GUID \
    (EFI_GUID) { 0x31878C87, 0x0B75, 0x11D5, { 0x9A, 0x4F, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } }

#define EFI_ABSOLUTE_POINTER_PROTOCOL_GUID \
    (EFI_GUID) { 0x8D59D32B, 0xC655, 0x4AE9, { 0x9B, 0x15, 0xF2, 0x59, 0x04, 0x99, 0x2A, 0x43 } }

#define EFI_ABSP_TouchActive 0x00000001

#define SCAN_UP 0x0001
#define SCAN_DOWN 0x0002
#define SCAN_RIGHT 0x0003
#define SCAN_LEFT 0x0004
#define SCAN_DELETE 0x0008


#endif
