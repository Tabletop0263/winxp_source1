/*++

Copyright (c) Microsoft Corporation

Module Name:

    cm.h

Abstract:

    Self-contained configuration manager (registry) header for the
    PowerPC (Broadway/G3) port of Windows XP (NT 5.1).

    This module defines the in-memory hive data structures and the
    byte-safe cell accessors used by the config manager.  All multi-byte
    cell fields are read/written with memcpy (byte-by-byte), never with
    unaligned casts, so the code is alignment-safe on PowerPC.

    The config manager is kernel-mode only and is self-contained: it
    implements its own cell-based allocator, handle table, and callback
    infrastructure, since the port has no object manager, memory manager,
    or disk/filesystem yet.

--*/

#ifndef _CM_CONFIG_H_
#define _CM_CONFIG_H_

#include "ppctx.h"

/*----------------------------------------------------------------------------
  Basic types the port headers do not define.
----------------------------------------------------------------------------*/

#ifndef _WCHAR_DEFINED
typedef unsigned short WCHAR;
typedef WCHAR         *PWSTR;
typedef const WCHAR   *PCWSTR;
#define _WCHAR_DEFINED
#endif

typedef unsigned char  UCHAR;
typedef UCHAR         *PUCHAR;
typedef char          *PCHAR;
typedef unsigned long  ULONG;
typedef ULONG         *PULONG;
typedef unsigned short USHORT;
typedef void           VOID;
typedef void          *PVOID;
typedef void          *HANDLE;
typedef HANDLE        *PHANDLE;
typedef long           NTSTATUS;
typedef int            BOOLEAN;
typedef ULONG         ACCESS_MASK;
#define CONST         const
typedef long long     LONGLONG;

typedef ULONG HCELL_INDEX, *PHCELL_INDEX;
#define HCELL_NIL ((HCELL_INDEX)0xFFFFFFFFUL)
#define CmOffsetOf(type, field) ((ULONG)&(((type *)0)->field))

extern UCHAR CmPool[];

/*----------------------------------------------------------------------------
  UNICODE_STRING / LIST_ENTRY / LARGE_INTEGER / OBJECT_ATTRIBUTES.
----------------------------------------------------------------------------*/

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
typedef const UNICODE_STRING *PCUNICODE_STRING;

typedef struct _LIST_ENTRY {
    struct _LIST_ENTRY *Flink;
    struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY;

#define InitializeListHead(ListHead) (\
    (ListHead)->Flink = (ListHead), \
    (ListHead)->Blink = (ListHead))

#define InsertHeadList(ListHead,Entry) {\
    PLIST_ENTRY _EXHead = (ListHead);\
    PLIST_ENTRY _EXEntry = (Entry);\
    _EXEntry->Flink = _EXHead->Flink;\
    _EXEntry->Blink = _EXHead;\
    _EXHead->Flink->Blink = _EXEntry;\
    _EXHead->Flink = _EXEntry;\
    }

#define InsertTailList(ListHead,Entry) {\
    PLIST_ENTRY _EXHead = (ListHead);\
    PLIST_ENTRY _EXEntry = (Entry);\
    _EXEntry->Flink = _EXHead;\
    _EXEntry->Blink = _EXHead->Blink;\
    _EXHead->Blink->Flink = _EXEntry;\
    _EXHead->Blink = _EXEntry;\
    }

#define RemoveEntryList(Entry) {\
    PLIST_ENTRY _EXPrev = (Entry)->Blink;\
    PLIST_ENTRY _EXNext = (Entry)->Flink;\
    _EXPrev->Flink = _EXNext;\
    _EXNext->Blink = _EXPrev;\
    }

#define IsListEmpty(ListHead) \
    ((ListHead)->Flink == (ListHead))

#define CONTAINING_RECORD(address, type, field) \
    ((type *)((PCHAR)(address) - (ULONG_PTR)&(((type *)0)->field)))

typedef struct _LARGE_INTEGER {
    LONG LowPart;
    LONG HighPart;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG                   Length;
    HANDLE                  RootDirectory;
    PUNICODE_STRING         ObjectName;
    ULONG                   Attributes;
    PVOID                   SecurityDescriptor;
    PVOID                   SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

#define InitializeObjectAttributes( p, n, a, r, s ) { \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES );          \
    (p)->RootDirectory = r;                             \
    (p)->Attributes = a;                                \
    (p)->ObjectName = n;                                \
    (p)->SecurityDescriptor = s;                        \
    (p)->SecurityQualityOfService = NULL;               \
    }

#define OBJ_CASE_INSENSITIVE 0x00000040L

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

#ifndef STATUS_OBJECT_NAME_NOT_FOUND
#define STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS)0xC0000034L)
#endif
#ifndef STATUS_OBJECT_NAME_COLLISION
#define STATUS_OBJECT_NAME_COLLISION ((NTSTATUS)0xC0000035L)
#endif
#ifndef STATUS_OBJECT_PATH_NOT_FOUND
#define STATUS_OBJECT_PATH_NOT_FOUND ((NTSTATUS)0xC0000039L)
#endif
#ifndef STATUS_ACCESS_DENIED
#define STATUS_ACCESS_DENIED ((NTSTATUS)0xC0000022L)
#endif
#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000DL)
#endif
#ifndef STATUS_INSUFFICIENT_RESOURCES
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xC000009AL)
#endif
#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#endif
#ifndef STATUS_BUFFER_OVERFLOW
#define STATUS_BUFFER_OVERFLOW ((NTSTATUS)0xC0000005L)
#endif
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif
#ifndef STATUS_NO_SUCH_KEY
#define STATUS_NO_SUCH_KEY ((NTSTATUS)0xC0000034L)
#endif
#ifndef STATUS_INVALID_HANDLE
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xC0000008L)
#endif
#ifndef STATUS_NO_MORE_ENTRIES
#define STATUS_NO_MORE_ENTRIES ((NTSTATUS)0xC000000AL)
#endif
#ifndef STATUS_UNSUCCESSFUL
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#endif

#define REG_CREATED_NEW_KEY     1
#define REG_OPENED_EXISTING_KEY 0

/*----------------------------------------------------------------------------
  Byte-safe cell field accessors (memcpy, never unaligned casts).
----------------------------------------------------------------------------*/

static __inline USHORT
CmRdU16(const UCHAR *base, ULONG off)
{
    USHORT v;
    const UCHAR *p = base + off;
    UCHAR *b = (UCHAR *)&v;
    b[0] = p[0]; b[1] = p[1];
    return v;
}

static __inline ULONG
CmRdU32(const UCHAR *base, ULONG off)
{
    ULONG v;
    const UCHAR *p = base + off;
    UCHAR *b = (UCHAR *)&v;
    b[0] = p[0]; b[1] = p[1]; b[2] = p[2]; b[3] = p[3];
    return v;
}

static __inline VOID
CmWrU16(UCHAR *base, ULONG off, USHORT v)
{
    UCHAR *p = base + off;
    const UCHAR *b = (const UCHAR *)&v;
    p[0] = b[0]; p[1] = b[1];
}

static __inline VOID
CmWrU32(UCHAR *base, ULONG off, ULONG v)
{
    UCHAR *p = base + off;
    const UCHAR *b = (const UCHAR *)&v;
    p[0] = b[0]; p[1] = b[1]; p[2] = b[2]; p[3] = b[3];
}

static __inline VOID
CmMemCpy(UCHAR *dst, const UCHAR *src, ULONG n)
{
    ULONG i;
    for (i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

static __inline VOID
CmMemSet(UCHAR *dst, UCHAR val, ULONG n)
{
    ULONG i;
    for (i = 0; i < n; i++) {
        dst[i] = val;
    }
}

static __inline LONG
CmReadI32(const UCHAR *base, ULONG off)
{
    LONG v;
    const UCHAR *p = base + off;
    UCHAR *b = (UCHAR *)&v;
    b[0] = p[0]; b[1] = p[1]; b[2] = p[2]; b[3] = p[3];
    return v;
}

static __inline VOID
CmWriteI32(UCHAR *base, ULONG off, LONG v)
{
    UCHAR *p = base + off;
    const UCHAR *b = (const UCHAR *)&v;
    p[0] = b[0]; p[1] = b[1]; p[2] = b[2]; p[3] = b[3];
}

/*----------------------------------------------------------------------------
  Cell-based hive definitions.
----------------------------------------------------------------------------*/

typedef ULONG HCELL_INDEX;
#define HCELL_NIL ((HCELL_INDEX)0xFFFFFFFFUL)

#define CM_CELL_KEY_NODE   1
#define CM_CELL_KEY_VALUE  2
#define CM_CELL_KEY_INDEX  3
#define CM_CELL_DATA       4

#define CM_CELL_HEADER_SIZE 8   // LONG Size + ULONG Type, 4-byte aligned

#define CM_HIVE_SIGNATURE   0x48495645UL  // "HIVE"
#define CM_HIVE_VERSION     3

#define CM_POOL_SIZE        (4 * 1024 * 1024)  // 4 MB reserved in-memory pool

#define CM_KEY_NODE_SIGNATURE     0x6b6e   // "kn"
#define CM_KEY_VALUE_SIGNATURE    0x6b76   // "kv"
#define CM_KEY_INDEX_ROOT         0x6972   // "ir"
#define CM_KEY_INDEX_LEAF         0x696c   // "il"
#define CM_KEY_FAST_LEAF          0x666c   // "fl"

#define KEY_VOLATILE      0x0001
#define KEY_NO_DELETE     0x0008
#define KEY_SYM_LINK      0x0010

#define REG_NONE          0
#define REG_SZ            1
#define REG_EXPAND_SZ     2
#define REG_BINARY        3
#define REG_DWORD         4
#define REG_MULTI_SZ      7
#define REG_QWORD         11

typedef enum _KEY_INFORMATION_CLASS {
    KeyBasicInformation = 0,
    KeyNodeInformation  = 1,
    KeyFullInformation  = 2,
    KeyNameInformation  = 3,
    MaxKeyInfoClass
} KEY_INFORMATION_CLASS;

typedef enum _KEY_VALUE_INFORMATION_CLASS {
    KeyValueBasicInformation = 0,
    KeyValueFullInformation  = 1,
    KeyValuePartialInformation = 2,
    MaxKeyValueInfoClass
} KEY_VALUE_INFORMATION_CLASS;

/* Cell header: LONG Size (+payload if allocated, -blocksize if free)
 *              ULONG Type (cell type tag)
 *              payload...
 */
typedef struct _CM_CELL {
    LONG  Size;
    ULONG Type;
} CM_CELL;

/*--------------------------------------------------------------------------
  HHIVE - in-memory hive descriptor.
--------------------------------------------------------------------------*/
typedef struct _HHIVE {
    ULONG               Signature;
    ULONG               Version;
    ULONG               PoolUsed;     // bytes consumed from the pool
    HCELL_INDEX         RootCell;     // root key node cell (\\REGISTRY\MACHINE)
    BOOLEAN             Dirty;        // set when in-memory hive contents change
} HHIVE, *PHHIVE;

/*--------------------------------------------------------------------------
  CM_KEY_NODE - key cell in a hive.
  Fixed part (all offsets 4-byte aligned except trailing USHORTs):
    0   USHORT  Signature
    2   USHORT  Flags
    4   LARGE_INTEGER LastWriteTime   (LowPart@4, HighPart@8)
   12   ULONG   Spare
   16   HCELL_INDEX Parent
   20   ULONG   SubKeyCounts[2]       (Stable@20, Volatile@24)
   28   HCELL_INDEX SubKeyLists[2]    (Stable@28, Volatile@32)
   36   ULONG   ValueListCount
   40   HCELL_INDEX ValueList
   44   HCELL_INDEX Security
   48   HCELL_INDEX Class
   52   ULONG   MaxNameLen
   56   ULONG   MaxClassLen
   60   ULONG   MaxValueNameLen
   64   ULONG   MaxValueDataLen
   68   ULONG   WorkVar
   72   USHORT NameLength
   74   USHORT ClassLength
   76   WCHAR Name[]
--------------------------------------------------------------------------*/
#define KN_SizeFixed 76
#define KN_oSignature     0
#define KN_oFlags         2
#define KN_oLastWriteLow  4
#define KN_oLastWriteHigh 8
#define KN_oSpare         12
#define KN_oParent        16
#define KN_oSubKeyCounts  20
#define KN_oSubKeyLists   28
#define KN_oValueCount    36
#define KN_oValueList     40
#define KN_oSecurity      44
#define KN_oClass         48
#define KN_oMaxNameLen    52
#define KN_oMaxClassLen   56
#define KN_oMaxValueNameLen 60
#define KN_oMaxValueDataLen 64
#define KN_oWorkVar       68
#define KN_oNameLength    72
#define KN_oClassLength   74
#define KN_oName          76

/*--------------------------------------------------------------------------
  CM_KEY_VALUE - value cell in a hive.
    0   USHORT Signature
    2   USHORT NameLength
    4   ULONG  DataLength
    8   HCELL_INDEX Data
   12   ULONG  Type
   16   USHORT Flags
   18   USHORT Spare
   20   WCHAR Name[]
--------------------------------------------------------------------------*/
#define KV_SizeFixed 20
#define KV_oSignature     0
#define KV_oNameLength    2
#define KV_oDataLength    4
#define KV_oData          8
#define KV_oType          12
#define KV_oFlags         16
#define KV_oSpare         18
#define KV_oName          20

/*--------------------------------------------------------------------------
  CM_KEY_INDEX / CM_FAST_LEAF - index cells.
  CM_KEY_INDEX:
    0   USHORT Signature  (CM_KEY_INDEX_LEAF "il")
    2   USHORT Count
    4   HCELL_INDEX List[]
  CM_FAST_LEAF:
    0   USHORT Signature  (CM_KEY_FAST_LEAF "fl")
    2   USHORT Count
    4   CM_INDEX List[]   where CM_INDEX = { UCHAR NameHint[4]; HCELL_INDEX Cell; } (8 bytes)
--------------------------------------------------------------------------*/
#define KIX_oSignature 0
#define KIX_oCount     2
#define KIX_oList      4

#define CM_FAST_INDEX_ENTRY 8   // UCHAR NameHint[4] + HCELL_INDEX Cell

/*--------------------------------------------------------------------------
  CMHIVE - CM wrapper around an HHIVE.
--------------------------------------------------------------------------*/
typedef struct _CMHIVE {
    HHIVE Hive;
} CMHIVE, *PCMHIVE;

/* Shared in-memory hive pool (defined in cmregistry.c). */
extern UCHAR CmPool[CM_POOL_SIZE];

/*--------------------------------------------------------------------------
  HMAPPING - handle table entry mapping a HANDLE to a key cell.
--------------------------------------------------------------------------*/
#define CM_MAPPING_SIGNATURE 0x4d415050UL  // "MAPP"

typedef struct _HMAPPING {
    ULONG           Signature;
    PCMHIVE         CmHive;
    HCELL_INDEX     KeyCell;
    BOOLEAN         Valid;
} HMAPPING, *PHMAPPING;

#define CM_MAX_HANDLES 256

/*--------------------------------------------------------------------------
  Registry callback infrastructure.
--------------------------------------------------------------------------*/
typedef enum _REG_NOTIFY_CLASS {
    RegNtDeleteKey,
    RegNtSetValueKey,
    RegNtDeleteValueKey,
    RegNtSetInformationKey,
    RegNtRenameKey,
    RegNtEnumerateKey,
    RegNtEnumerateValueKey,
    RegNtQueryKey,
    RegNtQueryValueKey,
    RegNtQueryMultipleValueKey,
    RegNtPreCreateKey,
    RegNtPostCreateKey,
    RegNtPreOpenKey,
    RegNtPostOpenKey,
    RegNtKeyHandleClose
} REG_NOTIFY_CLASS;

typedef NTSTATUS (*PCM_CALLBACK)(IN REG_NOTIFY_CLASS Type, IN PVOID Argument);

/* forward */
typedef struct _CM_CALLBACK_ENTRY CM_CALLBACK_ENTRY, *PCM_CALLBACK_ENTRY;

struct _CM_CALLBACK_ENTRY {
    LIST_ENTRY    List;
    PCM_CALLBACK  Function;
    PVOID         Context;
    LARGE_INTEGER Cookie;
};

/*--------------------------------------------------------------------------
  Key information structures returned to callers.
--------------------------------------------------------------------------*/

typedef struct _KEY_BASIC_INFORMATION {
    LARGE_INTEGER LastWriteTime;
    ULONG         TitleIndex;
    ULONG         NameLength;
    WCHAR         Name[1];
} KEY_BASIC_INFORMATION, *PKEY_BASIC_INFORMATION;

typedef struct _KEY_NODE_INFORMATION {
    LARGE_INTEGER LastWriteTime;
    ULONG         TitleIndex;
    ULONG         ClassOffset;
    ULONG         ClassLength;
    ULONG         NameLength;
    WCHAR         Name[1];
} KEY_NODE_INFORMATION, *PKEY_NODE_INFORMATION;

typedef struct _KEY_FULL_INFORMATION {
    LARGE_INTEGER LastWriteTime;
    ULONG         TitleIndex;
    ULONG         ClassOffset;
    ULONG         ClassLength;
    ULONG         SubKeys;
    ULONG         MaxNameLen;
    ULONG         MaxClassLen;
    ULONG         Values;
    ULONG         MaxValueNameLen;
    ULONG         MaxValueDataLen;
    WCHAR         Name[1];
} KEY_FULL_INFORMATION, *PKEY_FULL_INFORMATION;

typedef struct _KEY_VALUE_BASIC_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;
    ULONG NameLength;
    WCHAR Name[1];
} KEY_VALUE_BASIC_INFORMATION, *PKEY_VALUE_BASIC_INFORMATION;

typedef struct _KEY_VALUE_FULL_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;
    ULONG DataOffset;
    ULONG DataLength;
    ULONG NameLength;
    WCHAR Name[1];
} KEY_VALUE_FULL_INFORMATION, *PKEY_VALUE_FULL_INFORMATION;

typedef struct _KEY_VALUE_PARTIAL_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;
    ULONG DataLength;
    UCHAR Data[1];
} KEY_VALUE_PARTIAL_INFORMATION, *PKEY_VALUE_PARTIAL_INFORMATION;

/*--------------------------------------------------------------------------
  Nt API prototypes.
--------------------------------------------------------------------------*/

NTSTATUS
NtCreateKey(
    OUT PHANDLE KeyHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN ULONG TitleIndex,
    IN PUNICODE_STRING Class OPTIONAL,
    IN ULONG CreateOptions,
    OUT PULONG Disposition OPTIONAL
    );

NTSTATUS
NtOpenKey(
    OUT PHANDLE KeyHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes
    );

NTSTATUS
NtCloseKey(
    IN HANDLE KeyHandle
    );

NTSTATUS
NtEnumerateKey(
    IN HANDLE KeyHandle,
    IN ULONG Index,
    OUT PUNICODE_STRING Name OPTIONAL,
    OUT PULONG NameLength OPTIONAL
    );

NTSTATUS
NtEnumerateValueKey(
    IN HANDLE KeyHandle,
    IN ULONG Index,
    OUT PUNICODE_STRING ValueName OPTIONAL,
    OUT PULONG NameLength OPTIONAL
    );

NTSTATUS
NtSetValueKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING ValueName,
    IN ULONG TitleIndex OPTIONAL,
    IN ULONG Type,
    IN PVOID Data,
    IN ULONG DataSize
    );

NTSTATUS
NtQueryValueKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING ValueName,
    OUT PVOID  Data OPTIONAL,
    IN ULONG  DataLength,
    OUT PULONG ResultLength OPTIONAL
    );

NTSTATUS
NtDeleteValueKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING ValueName
    );

NTSTATUS
NtDeleteKey(
    IN HANDLE KeyHandle
    );

NTSTATUS
NtQueryKey(
    IN HANDLE KeyHandle,
    IN KEY_INFORMATION_CLASS KeyInformationClass,
    IN PVOID KeyInformation,
    IN ULONG Length,
    OUT PULONG ResultLength
    );

NTSTATUS
NtFlushKey(
    IN HANDLE KeyHandle
    );

/*--------------------------------------------------------------------------
  Config manager init entry point.
--------------------------------------------------------------------------*/

NTSTATUS
CmInitializeConfigRegistry(
    IN PVOID LoaderBlock
    );

/*--------------------------------------------------------------------------
  Internal Cm/Hv prototypes (implemented across cm*.c).
--------------------------------------------------------------------------*/

PHHIVE
CmCreateHive(
    VOID
    );

UCHAR *
HvGetCell(
    IN PHHIVE Hive,
    IN HCELL_INDEX Cell
    );

PVOID
CmpCellToPointer(
    IN PHHIVE Hive,
    IN HCELL_INDEX Cell
    );

HCELL_INDEX
CmAllocateCell(
    IN PHHIVE Hive,
    IN ULONG Type,
    IN ULONG PayloadSize
    );

UCHAR *
CmAllocatePayload(
    IN PHHIVE Hive,
    IN ULONG Type,
    IN ULONG PayloadSize,
    OUT PHCELL_INDEX CellIndex
    );

VOID
CmFreeCell(
    IN PHHIVE Hive,
    IN HCELL_INDEX Cell
    );

HCELL_INDEX
CmPointerToCell(
    IN PUCHAR Ptr
    );

/*--------------------------------------------------------------------------
  Handle table internals (implemented in cmregistry.c).
--------------------------------------------------------------------------*/
ULONG
CmpAllocHandle(
    IN PHHIVE Hive,
    IN HCELL_INDEX CellIndex
    );

BOOLEAN
CmpFreeHandle(
    IN ULONG Handle
    );

NTSTATUS
CmpLookupHandle(
    IN  ULONG   Handle,
    OUT PHHIVE  *Hive,
    OUT PULONG  CellIndex
    );

/*--------------------------------------------------------------------------
  Key node internals (implemented in cmregistry.c).
--------------------------------------------------------------------------*/
NTSTATUS
CmCreateKey(
    OUT PHANDLE KeyHandle,
    IN  ACCESS_MASK DesiredAccess,
    IN  POBJECT_ATTRIBUTES ObjectAttributes,
    IN  ULONG TitleIndex,
    IN  PUNICODE_STRING Class OPTIONAL,
    IN  ULONG CreateOptions,
    OUT PULONG Disposition OPTIONAL
    );

NTSTATUS
CmOpenKey(
    OUT PHANDLE KeyHandle,
    IN  ACCESS_MASK DesiredAccess,
    IN  POBJECT_ATTRIBUTES ObjectAttributes
    );

NTSTATUS
CmCloseKey(
    IN HANDLE KeyHandle
    );

HCELL_INDEX
CmFindSubKeyByName(
    IN PHHIVE Hive,
    IN HCELL_INDEX ParentCell,
    IN PCUNICODE_STRING Name
    );

NTSTATUS
CmpFindSubKey(
    IN PHHIVE          Hive,
    IN HCELL_INDEX     ParentCell,
    IN PUNICODE_STRING Name,
    OUT PHCELL_INDEX   ChildCell
    );

NTSTATUS
CmpCreateKeyNode(
    IN PHHIVE           Hive,
    IN PUNICODE_STRING  Name,
    IN HCELL_INDEX      ParentCell,
    OUT PHCELL_INDEX    NewCell
    );

NTSTATUS
CmEnumerateSubKeys(
    IN PHHIVE          Hive,
    IN HCELL_INDEX     ParentCell,
    IN ULONG           Index,
    OUT PUNICODE_STRING Name OPTIONAL,
    OUT PULONG NameLength OPTIONAL
    );

PHHIVE
CmGetMachineHive(
    VOID
    );

NTSTATUS
CmSetValueKey(
    IN PHHIVE Hive,
    IN HCELL_INDEX KeyCell,
    IN PUNICODE_STRING ValueName,
    IN ULONG Type,
    IN PVOID Data,
    IN ULONG DataSize
    );

NTSTATUS
CmQueryValueKey(
    IN PHHIVE Hive,
    IN HCELL_INDEX KeyCell,
    IN PUNICODE_STRING ValueName,
    IN KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    IN PVOID KeyValueInformation,
    IN ULONG Length,
    OUT PULONG ResultLength
    );

NTSTATUS
CmEnumerateValueKey(
    IN PHHIVE Hive,
    IN HCELL_INDEX KeyCell,
    IN ULONG Index,
    IN KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    IN PVOID KeyValueInformation,
    IN ULONG Length,
    OUT PULONG ResultLength
    );

NTSTATUS
CmDeleteValueKey(
    IN PHHIVE Hive,
    IN HCELL_INDEX KeyCell,
    IN PUNICODE_STRING ValueName
    );

NTSTATUS
CmDeleteKey(
    IN PHHIVE Hive,
    IN HCELL_INDEX KeyCell
    );

NTSTATUS
CmFlushKey(
    IN PHHIVE Hive
    );

/* Handle table */
HANDLE
CmAllocateHandle(
    IN PCMHIVE CmHive,
    IN HCELL_INDEX KeyCell
    );

PHMAPPING
CmLookupHandle(
    IN HANDLE Handle
    );

VOID
CmCloseHandle(
    IN HANDLE Handle
    );

/* Callbacks */
NTSTATUS
CmRegisterCallback(
    IN PCM_CALLBACK Function,
    IN PVOID Context,
    OUT PLARGE_INTEGER Cookie
    );

NTSTATUS
CmUnRegisterCallback(
    IN LARGE_INTEGER Cookie
    );

VOID
CmCallCallbacks(
    IN REG_NOTIFY_CLASS Type,
    IN PVOID Argument
    );

#endif /* _CM_CONFIG_H_ */
