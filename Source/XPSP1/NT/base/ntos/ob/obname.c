/*++

Copyright (c) Microsoft Corporation

Module Name:

    obname.c

Abstract:

    Minimal object name / directory management for the PowerPC
    (Broadway/G3) port of Windows XP (NT 5.1).  Self-contained: only
    depends on ppctx.h and wxptypes.h (no nt.h).  Implements:

      - ObCreateDirectoryObject - create a named directory object.
      - ObLookupObjectByName - look up a named object in the namespace.
      - ObOpenObjectByName - open (or create) a named object.
      - ObCloseObject - dereference a named object handle.

    Objects are tracked in a global singly-linked list keyed by a
    UNICODE_STRING name.  This is sufficient for the port's early-boot
    shell (console, command dispatch) to name and locate objects such as
    LPC ports and devices.

Author:

    (port)

Environment:

    Kernel mode only.

--*/

#include "ppctx.h"
#include "wxptypes.h"

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

/*--------------------------------------------------------------------------
    WxpMemAlloc / WxpMemFree - implemented in obcreate.c; forward-declared
    here.  WxpMemCopy/WxpMemSet/WxpZeroMemory are already declared in
    wxptypes.h.
--------------------------------------------------------------------------*/
PVOID
WxpMemAlloc(
    IN ULONG Size
    );

VOID
WxpMemFree(
    IN PVOID Block
    );

/*--------------------------------------------------------------------------
    WXP_NAMED_OBJECT - a named object tracked in the global namespace.
--------------------------------------------------------------------------*/
typedef struct _WXP_NAMED_OBJECT {
    LIST_ENTRY          Link;          /* linkage in ObpNamespaceList */
    UNICODE_STRING      Name;          /* object name (copied) */
    ULONG               Type;          /* OB_TYPE_* */
    PVOID               Body;          /* object body */
    ULONG               ReferenceCount;
} WXP_NAMED_OBJECT, *PWXP_NAMED_OBJECT;

/*--------------------------------------------------------------------------
    ObpNamespaceList - global namespace of named objects.
--------------------------------------------------------------------------*/
static LIST_ENTRY ObpNamespaceList;

/*--------------------------------------------------------------------------
    ObpCompareUnicodeString - case-insensitive-ish compare of two
    UNICODE_STRINGs (exact length+content match).
--------------------------------------------------------------------------*/
static BOOLEAN
ObpCompareUnicodeString(
    IN PCUNICODE_STRING String1,
    IN PCUNICODE_STRING String2
    )
{
    ULONG i;

    if (String1->Length != String2->Length) {
        return FALSE;
    }

    for (i = 0; i < (String1->Length / sizeof(WCHAR)); i++) {
        if (String1->Buffer[i] != String2->Buffer[i]) {
            return FALSE;
        }
    }

    return TRUE;
}

/*--------------------------------------------------------------------------
    ObpDuplicateUnicodeString - allocate a copy of a UNICODE_STRING's
    buffer from WxpMemAlloc.
--------------------------------------------------------------------------*/
static PWSTR
ObpDuplicateUnicodeString(
    IN PCUNICODE_STRING Source
    )
{
    PWSTR Buffer;

    Buffer = (PWSTR)WxpMemAlloc(Source->Length + sizeof(WCHAR));
    if (Buffer == NULL) {
        return NULL;
    }

    WxpMemCopy(Buffer, Source->Buffer, Source->Length);
    Buffer[Source->Length / sizeof(WCHAR)] = L'\0';
    return Buffer;
}

/*--------------------------------------------------------------------------
    ObCreateDirectoryObject - create a named directory object in the
    namespace.

    If an object with the same name already exists, returns
    STATUS_OBJECT_NAME_COLLISION.
--------------------------------------------------------------------------*/
NTSTATUS
ObCreateDirectoryObject(
    OUT PHANDLE DirectoryHandle,
    IN  ACCESS_MASK DesiredAccess,
    IN  POBJECT_ATTRIBUTES ObjectAttributes,
    IN  PVOID DirectoryBody OPTIONAL
    )
{
    PWXP_NAMED_OBJECT Object;
    PLIST_ENTRY Entry;

    UNREFERENCED_PARAMETER(DesiredAccess);

    if (DirectoryHandle == NULL || ObjectAttributes == NULL ||
        ObjectAttributes->ObjectName == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Reject names that already exist. */
    Entry = ObpNamespaceList.Flink;
    while (Entry != &ObpNamespaceList) {
        PWXP_NAMED_OBJECT Existing;
        Existing = (PWXP_NAMED_OBJECT)CONTAINING_RECORD(Entry, WXP_NAMED_OBJECT, Link);
        Entry = Entry->Flink;

        if (ObpCompareUnicodeString(ObjectAttributes->ObjectName,
                                    &Existing->Name)) {
            return STATUS_OBJECT_NAME_COLLISION;
        }
    }

    Object = (PWXP_NAMED_OBJECT)WxpMemAlloc(sizeof(WXP_NAMED_OBJECT));
    if (Object == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    WxpZeroMemory(Object, sizeof(WXP_NAMED_OBJECT));

    Object->Name.Buffer = ObpDuplicateUnicodeString(ObjectAttributes->ObjectName);
    if (Object->Name.Buffer == NULL) {
        WxpMemFree(Object);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    Object->Name.Length = ObjectAttributes->ObjectName->Length;
    Object->Name.MaximumLength = ObjectAttributes->ObjectName->Length + sizeof(WCHAR);

    Object->Type = OB_TYPE_PROCESS;   /* directories use process type code */
    Object->Body = DirectoryBody;
    Object->ReferenceCount = 1;

    /* Insert into the global namespace list (initialize list head lazily). */
    if (ObpNamespaceList.Flink == NULL && ObpNamespaceList.Blink == NULL) {
        InitializeListHead(&ObpNamespaceList);
    }
    InsertTailList(&ObpNamespaceList, &Object->Link);

    *DirectoryHandle = (HANDLE)Object;
    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    ObLookupObjectByName - find a named object in the namespace.
--------------------------------------------------------------------------*/
NTSTATUS
ObLookupObjectByName(
    IN  POBJECT_ATTRIBUTES ObjectAttributes,
    OUT PVOID *ObjectBody OPTIONAL
    )
{
    PWXP_NAMED_OBJECT Object;
    PLIST_ENTRY Entry;

    UNREFERENCED_PARAMETER(ObjectBody);

    if (ObjectAttributes == NULL || ObjectAttributes->ObjectName == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (ObpNamespaceList.Flink == NULL && ObpNamespaceList.Blink == NULL) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    Entry = ObpNamespaceList.Flink;
    while (Entry != &ObpNamespaceList) {
        Object = (PWXP_NAMED_OBJECT)CONTAINING_RECORD(Entry, WXP_NAMED_OBJECT, Link);
        Entry = Entry->Flink;

        if (ObpCompareUnicodeString(ObjectAttributes->ObjectName,
                                    &Object->Name)) {
            Object->ReferenceCount++;
            if (ObjectBody != NULL) {
                *ObjectBody = Object->Body;
            }
            return STATUS_SUCCESS;
        }
    }

    return STATUS_OBJECT_NAME_NOT_FOUND;
}

/*--------------------------------------------------------------------------
    ObOpenObjectByName - open (or optionally create) a named object.

    If Create is TRUE and the object does not exist, a new directory
    object is created with no body.
--------------------------------------------------------------------------*/
NTSTATUS
ObOpenObjectByName(
    IN  POBJECT_ATTRIBUTES ObjectAttributes,
    IN  ACCESS_MASK DesiredAccess,
    IN  BOOLEAN Create,
    OUT PHANDLE Handle OPTIONAL
    )
{
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(DesiredAccess);

    if (ObjectAttributes == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    Status = ObLookupObjectByName(ObjectAttributes, NULL);
    if (NT_SUCCESS(Status)) {
        if (Handle != NULL) {
            /* Reuse the object pointer as the handle. */
            PWXP_NAMED_OBJECT Object;
            PLIST_ENTRY Entry;
            Entry = ObpNamespaceList.Flink;
            while (Entry != &ObpNamespaceList) {
                Object = (PWXP_NAMED_OBJECT)CONTAINING_RECORD(Entry, WXP_NAMED_OBJECT, Link);
                Entry = Entry->Flink;
                if (ObpCompareUnicodeString(ObjectAttributes->ObjectName,
                                            &Object->Name)) {
                    *Handle = (HANDLE)Object;
                    break;
                }
            }
        }
        return STATUS_SUCCESS;
    }

    if (Create && Status == STATUS_OBJECT_NAME_NOT_FOUND) {
        return ObCreateDirectoryObject(Handle, DesiredAccess, ObjectAttributes, NULL);
    }

    return Status;
}

/*--------------------------------------------------------------------------
    ObCloseObject - dereference a named object handle (no-op for the bump
    allocator; the object stays in the namespace until reboot).
--------------------------------------------------------------------------*/
VOID
ObCloseObject(
    IN HANDLE Handle
    )
{
    PWXP_NAMED_OBJECT Object = (PWXP_NAMED_OBJECT)Handle;

    UNREFERENCED_PARAMETER(Object);

    if (Object != NULL && Object->ReferenceCount > 0) {
        Object->ReferenceCount--;
    }
}
