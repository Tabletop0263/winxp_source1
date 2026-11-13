/*++

Copyright (c) Microsoft Corporation

Module Name:

    wxptypes.h

Abstract:

    Minimal, self-contained type definitions for the PowerPC (Broadway/G3)
    port's userspace/shell components (ps, ob, lpc, cmd).  This header is
    deliberately small and does not pull in the full NTOS headers, which
    have heavy interdependencies (PEPROCESS, PHANDLE_TABLE, etc.) and
    require nt.h which is not present in the port's include tree.

    Types already defined in ppctx.h are intentionally NOT redefined here
    (NTSTATUS, ULONG, UCHAR, USHORT, SHORT, LONG, VOID, PVOID, CHAR,
    BOOLEAN, KPROCESSOR_MODE, KTRAP_FRAME, KPCR, KPRCB, STATUS_SUCCESS,
    IN/OUT/OPTIONAL, NULL, TRUE, FALSE).

    Defines:
      - WCHAR / PWSTR / PCWSTR
      - HANDLE / PHANDLE / ACCESS_MASK
      - LIST_ENTRY / PLIST_ENTRY + list manipulation macros
      - UNICODE_STRING / PUNICODE_STRING / PCUNICODE_STRING
      - LARGE_INTEGER / PLARGE_INTEGER
      - OBJECT_ATTRIBUTES / POBJECT_ATTRIBUTES / PCOBJECT_ATTRIBUTES +
        InitializeObjectAttributes + OBJ_CASE_INSENSITIVE
      - CLIENT_ID / PCLIENT_ID
      - KSTART_ROUTINE / PKSTART_ROUTINE
      - Additional NTSTATUS values
      - WxpMemCopy / WxpMemSet (implemented in obcreate.c)
      - Object type codes and process/thread creation flags
      - Minimal EPROCESS / ETHREAD / PROCESS_THREAD_LIST body types

Author:

    (port)

Environment:

    Kernel mode only.

--*/

#ifndef _WXPTYPES_H_
#define _WXPTYPES_H_

#include "ppctx.h"

/*----------------------------------------------------------------------------
  Basic scalar types (not in ppctx.h)
----------------------------------------------------------------------------*/

typedef unsigned short WCHAR;
typedef WCHAR         *PWSTR;
typedef const WCHAR   *PCWSTR;

typedef VOID        *HANDLE;
typedef HANDLE       *PHANDLE;
typedef ULONG        ACCESS_MASK;
typedef CHAR         *PCHAR;
typedef UCHAR        *PUCHAR;
typedef ULONG        *PULONG;

/*----------------------------------------------------------------------------
  LIST_ENTRY / PLIST_ENTRY + manipulation macros
----------------------------------------------------------------------------*/

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

#define RemoveHeadList(ListHead) ((ListHead)->Flink)
#define RemoveTailList(ListHead) ((ListHead)->Blink)

#define IsListEmpty(ListHead) \
    ((ListHead)->Flink == (ListHead))

#define ForEachListEntry(ListHead, Entry, Type, Field) \
    for ((Entry) = (Type *)((ListHead)->Flink); \
         &(Entry)->Field != (PLIST_ENTRY)(ListHead); \
         (Entry) = (Type *)((Entry)->Field.Flink))

#define CONTAINING_RECORD(address, type, field) \
    ((type *)((PCHAR)(address) - (ULONG_PTR)&(((type *)0)->field)))

/*----------------------------------------------------------------------------
  UNICODE_STRING
----------------------------------------------------------------------------*/

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
typedef const UNICODE_STRING *PCUNICODE_STRING;

/*----------------------------------------------------------------------------
  Per-process thread list head.
----------------------------------------------------------------------------*/

typedef struct _PROCESS_THREAD_LIST {
    LIST_ENTRY  ThreadListHead;
    ULONG       ThreadCount;
} PROCESS_THREAD_LIST, *PPROCESS_THREAD_LIST;

/*----------------------------------------------------------------------------
  LARGE_INTEGER
----------------------------------------------------------------------------*/

typedef struct _LARGE_INTEGER {
    LONG LowPart;
    LONG HighPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

/*----------------------------------------------------------------------------
  OBJECT_ATTRIBUTES
----------------------------------------------------------------------------*/

typedef struct _OBJECT_ATTRIBUTES {
    ULONG                   Length;
    HANDLE                  RootDirectory;
    PUNICODE_STRING         ObjectName;
    ULONG                   Attributes;
    PVOID                   SecurityDescriptor;
    PVOID                   SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;
typedef const OBJECT_ATTRIBUTES *PCOBJECT_ATTRIBUTES;

#define InitializeObjectAttributes( p, n, a, r, s ) { \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES );          \
    (p)->RootDirectory = r;                             \
    (p)->Attributes = a;                                \
    (p)->ObjectName = n;                                \
    (p)->SecurityDescriptor = s;                        \
    (p)->SecurityQualityOfService = NULL;               \
    }

#define OBJ_CASE_INSENSITIVE 0x00000040L

/*----------------------------------------------------------------------------
  CLIENT_ID
----------------------------------------------------------------------------*/

typedef struct _CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

/*----------------------------------------------------------------------------
  KSTART_ROUTINE (thread start function)
----------------------------------------------------------------------------*/

typedef
VOID
(*PKSTART_ROUTINE) (
    IN PVOID StartContext
    );

/*----------------------------------------------------------------------------
  Process/thread body types (minimal definitions for the new PS modules).
  The full EPROCESS/ETHREAD definitions live in ps.h but cannot be used
  standalone here (they require nt.h).  These minimal definitions expose
  only the fields used by pscreate.c / psexit.c / pslist.c.
----------------------------------------------------------------------------*/

typedef struct _EPROCESS {
    /* Process ID. */
    HANDLE                      UniqueProcessId;
    /* Linkage in the global process list (PsProcessListHead). */
    LIST_ENTRY                  ProcessListEntry;
    /* Thread list head + count for threads belonging to this process. */
    PROCESS_THREAD_LIST         ThreadList;
    /* Object table pointer (stub). */
    PVOID                       ObjectTable;
    /* Quota block (stub). */
    PVOID                       QuotaBlock;
    /* Image file name (debug). */
    CHAR                        ImageFileName[16];
} EPROCESS, *PEPROCESS;

typedef struct _ETHREAD {
    /* Thread ID. */
    HANDLE                      ThreadId;
    /* Linkage in the process thread list. */
    LIST_ENTRY                  ThreadListEntry;
    /* Owning process. */
    PEPROCESS                   ThreadsProcess;
    /* Thread start routine / context. */
    PKSTART_ROUTINE             StartRoutine;
    PVOID                       StartContext;
    /* Thread priority / state (stub). */
    KPRIORITY                   Priority;
    KPROCESSOR_MODE             PreviousMode;
    /* Exit status. */
    NTSTATUS                    ExitStatus;
    BOOLEAN                     Terminated;
} ETHREAD, *PETHREAD;

/*--------------------------------------------------------------------------
    Controller state (mirrors the HAL's CONTROLLER_STATE defined in
    hals/halppc/ppc/pad.c).  Defined here so console.c can reference it
    without including HAL internals.
--------------------------------------------------------------------------*/
typedef struct _CONTROLLER_STATE {
    ULONG   Buttons;          /* PAD_BUTTON_* bitmask of currently pressed */
    USHORT  StickX;           /* left stick X (-128..127) */
    USHORT  StickY;           /* left stick Y (-128..127) */
    BOOLEAN Connected;        /* whether a controller is present */
} CONTROLLER_STATE, *PCONTROLLER_STATE;

/*----------------------------------------------------------------------------
  Additional NTSTATUS values used by new modules
----------------------------------------------------------------------------*/

#ifndef STATUS_UNSUCCESSFUL
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#endif
#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000DL)
#endif
#ifndef STATUS_INSUFFICIENT_RESOURCES
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xC000009AL)
#endif
#ifndef STATUS_OBJECT_NAME_NOT_FOUND
#define STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS)0xC0000034L)
#endif
#ifndef STATUS_OBJECT_NAME_COLLISION
#define STATUS_OBJECT_NAME_COLLISION ((NTSTATUS)0xC0000035L)
#endif
#ifndef STATUS_ACCESS_DENIED
#define STATUS_ACCESS_DENIED ((NTSTATUS)0xC0000022L)
#endif
#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#endif
#ifndef STATUS_BUFFER_OVERFLOW
#define STATUS_BUFFER_OVERFLOW ((NTSTATUS)0xC0000005L)
#endif
#ifndef STATUS_OBJECT_PATH_NOT_FOUND
#define STATUS_OBJECT_PATH_NOT_FOUND ((NTSTATUS)0xC0000039L)
#endif
#ifndef STATUS_NOT_IMPLEMENTED
#define STATUS_NOT_IMPLEMENTED ((NTSTATUS)0xC0000002L)
#endif

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

/*----------------------------------------------------------------------------
  Memory helpers (implemented in obcreate.c)
----------------------------------------------------------------------------*/

VOID
WxpMemCopy(
    OUT PVOID Destination,
    IN  const VOID *Source,
    IN  ULONG Length
    );

VOID
WxpMemSet(
    OUT PVOID Destination,
    IN  UCHAR FillByte,
    IN  ULONG Length
    );

#define WxpZeroMemory(Dest, Size) WxpMemSet((Dest), 0, (Size))

/*----------------------------------------------------------------------------
  Object type codes
----------------------------------------------------------------------------*/

#define OB_TYPE_PROCESS     1
#define OB_TYPE_THREAD      2
#define OB_TYPE_PORT        3

/*----------------------------------------------------------------------------
  Process/thread creation flags
----------------------------------------------------------------------------*/

#define PS_CREATE_THREAD_SUSPENDED 0x00000001UL

#endif /* _WXPTYPES_H_ */
