/*++
Module Name:

    cmkey.c

Abstract:

    Windows XP (NT 5.1) Config Manager - Key creation/open/enumeration.
    PowerPC (Broadway/G3) port.

    All multi-byte cell fields are accessed byte-safe via the CmRd/CmWr
    helpers declared in cm.h,
    so the code is alignment-safe on PowerPC.

Environment:

    Kernel mode only.
--*/

#include "cm.h"
#include <string.h>

/*--------------------------------------------------------------------------
    CmFindSubKeyByName - find a subkey cell by name under a parent key cell.
    Delegates to cmregistry.c's CmpFindSubKey.
--------------------------------------------------------------------------*/
HCELL_INDEX
CmFindSubKeyByName(
    IN PHHIVE          Hive,
    IN HCELL_INDEX     ParentCell,
    IN PCUNICODE_STRING Name
    )
{
    HCELL_INDEX childCell;
    NTSTATUS status;

    status = CmpFindSubKey(Hive, ParentCell, (PUNICODE_STRING)Name, &childCell);
    if (!NT_SUCCESS(status)) {
        return HCELL_NIL;
    }
    return childCell;
}

/*--------------------------------------------------------------------------
    CmEnumerateSubKeys - enumerate subkeys of a cell by index.
--------------------------------------------------------------------------*/
NTSTATUS
CmEnumerateSubKeys(
    IN PHHIVE          Hive,
    IN HCELL_INDEX     ParentCell,
    IN ULONG           Index,
    OUT PUNICODE_STRING Name OPTIONAL,
    OUT PULONG NameLength OPTIONAL
    )
{
    PUCHAR parent;
    ULONG count;
    HCELL_INDEX listCell;
    PUCHAR idx;
    ULONG idxCount;
    HCELL_INDEX kidCell;
    PUCHAR kid;
    ULONG knLen;

    parent = CmPool + ParentCell;
    if (parent >= CmPool + CM_POOL_SIZE) {
        return STATUS_INVALID_HANDLE;
    }

    count = CmRdU32(parent, KN_oSubKeyCounts);
    if (Index >= count) {
        return STATUS_NO_MORE_ENTRIES;
    }

    /* Walk the subkey index list. */
    listCell = CmRdU32(parent, KN_oSubKeyLists);
    idx = (PUCHAR)CmpCellToPointer(NULL, listCell);
    if (idx == NULL) {
        return STATUS_NO_MORE_ENTRIES;
    }

    idxCount = CmRdU16(idx, KIX_oCount);
    if (Index >= idxCount) {
        return STATUS_NO_MORE_ENTRIES;
    }

    kidCell = CmRdU32(idx, KIX_oList + Index * 4);
    kid = (PUCHAR)CmpCellToPointer(NULL, kidCell);
    if (kid == NULL) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    knLen = CmRdU16(kid, KN_oNameLength);
    if (Name != NULL) {
        PWSTR knBuf = (PWSTR)(kid + KN_oName);

        if (Name->MaximumLength < knLen) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        CmMemCpy((UCHAR *)Name->Buffer, (const UCHAR *)knBuf, knLen);
        Name->Length = (USHORT)knLen;
    }

    if (NameLength) {
        *NameLength = knLen;
    }

    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    CmCreateKey - create or open a registry key.
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
    )
{
    PHHIVE hive;
    HCELL_INDEX rootCell;
    HCELL_INDEX cell;
    NTSTATUS status;
    PUCHAR parent;
    UNICODE_STRING remaining;
    UNICODE_STRING comp;
    BOOLEAN created;
    ULONG h;

    (void)DesiredAccess;
    (void)TitleIndex;
    (void)Class;
    (void)CreateOptions;

    if (KeyHandle == NULL || ObjectAttributes == NULL ||
        ObjectAttributes->ObjectName == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    hive = CmGetMachineHive();
    if (hive == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    rootCell = hive->RootCell;
    if (rootCell == HCELL_NIL) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    remaining = *ObjectAttributes->ObjectName;
    parent = CmPool + rootCell;
    if (parent >= CmPool + CM_POOL_SIZE) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    /* Strip leading backslash. */
    if (remaining.Length >= sizeof(WCHAR) && remaining.Buffer[0] == L'\\') {
        remaining.Buffer++;
        remaining.Length -= sizeof(WCHAR);
    }

    created = FALSE;
    cell = HCELL_NIL;

    while (remaining.Length > 0) {
        /* Extract the next path component. */
        {
            ULONG len = 0;
            while (len < remaining.Length / sizeof(WCHAR) &&
                   remaining.Buffer[len] != L'\\') {
                len++;
            }
            comp.Length        = (USHORT)(len * sizeof(WCHAR));
            comp.MaximumLength = comp.Length;
            comp.Buffer        = remaining.Buffer;

            remaining.Buffer += len;
            remaining.Length -= (USHORT)(len * sizeof(WCHAR));
            if (remaining.Length >= sizeof(WCHAR) && remaining.Buffer[0] == L'\\') {
                remaining.Buffer++;
                remaining.Length -= sizeof(WCHAR);
            }
        }

        /* Try to find the component. */
        cell = CmFindSubKeyByName(hive, CmPointerToCell(parent), &comp);
        if (cell == HCELL_NIL) {
            /* Not found: create it. */
            status = CmpCreateKeyNode(hive, &comp, CmPointerToCell(parent), &cell);
            if (!NT_SUCCESS(status)) {
                return status;
            }
            created = TRUE;
        }

        /* Descend into the child. */
        parent = CmPool + cell;
        if (parent >= CmPool + CM_POOL_SIZE) {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
    }

    /* Allocate a handle. */
    h = CmpAllocHandle(hive, cell);
    if (h == HCELL_NIL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    *KeyHandle = (HANDLE)(ULONG_PTR)h;

    if (Disposition) {
        *Disposition = created ? REG_CREATED_NEW_KEY : REG_OPENED_EXISTING_KEY;
    }
    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    CmOpenKey - open an existing registry key.
--------------------------------------------------------------------------*/
NTSTATUS
CmOpenKey(
    OUT PHANDLE KeyHandle,
    IN  ACCESS_MASK DesiredAccess,
    IN  POBJECT_ATTRIBUTES ObjectAttributes
    )
{
    PHHIVE hive;
    HCELL_INDEX rootCell;
    HCELL_INDEX cell;
    PUCHAR parent;
    UNICODE_STRING remaining;
    UNICODE_STRING comp;
    ULONG h;

    (void)DesiredAccess;

    if (KeyHandle == NULL || ObjectAttributes == NULL ||
        ObjectAttributes->ObjectName == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    hive = CmGetMachineHive();
    if (hive == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    rootCell = hive->RootCell;
    if (rootCell == HCELL_NIL) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    remaining = *ObjectAttributes->ObjectName;
    parent = CmPool + rootCell;
    if (parent >= CmPool + CM_POOL_SIZE) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    /* Strip leading backslash. */
    if (remaining.Length >= sizeof(WCHAR) && remaining.Buffer[0] == L'\\') {
        remaining.Buffer++;
        remaining.Length -= sizeof(WCHAR);
    }

    cell = HCELL_NIL;

    while (remaining.Length > 0) {
        /* Extract the next path component. */
        {
            ULONG len = 0;
            while (len < remaining.Length / sizeof(WCHAR) &&
                   remaining.Buffer[len] != L'\\') {
                len++;
            }
            comp.Length        = (USHORT)(len * sizeof(WCHAR));
            comp.MaximumLength = comp.Length;
            comp.Buffer        = remaining.Buffer;

            remaining.Buffer += len;
            remaining.Length -= (USHORT)(len * sizeof(WCHAR));
            if (remaining.Length >= sizeof(WCHAR) && remaining.Buffer[0] == L'\\') {
                remaining.Buffer++;
                remaining.Length -= sizeof(WCHAR);
            }
        }

        cell = CmFindSubKeyByName(hive, CmPointerToCell(parent), &comp);
        if (cell == HCELL_NIL) {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        parent = CmPool + cell;
        if (parent >= CmPool + CM_POOL_SIZE) {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
    }

    /* Allocate a handle. */
    h = CmpAllocHandle(hive, cell);
    if (h == HCELL_NIL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    *KeyHandle = (HANDLE)(ULONG_PTR)h;

    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    CmCloseKey - close a registry key handle.
--------------------------------------------------------------------------*/
NTSTATUS
CmCloseKey(
    IN HANDLE KeyHandle
    )
{
    NTSTATUS status;

    status = CmpFreeHandle((ULONG)(ULONG_PTR)KeyHandle) ? STATUS_SUCCESS :
                             STATUS_INVALID_HANDLE;
    return status;
}

/*--------------------------------------------------------------------------
    NtCreateKey - create or open a registry key (Nt API wrapper).
--------------------------------------------------------------------------*/
NTSTATUS
NtCreateKey(
    OUT PHANDLE KeyHandle,
    IN  ACCESS_MASK DesiredAccess,
    IN  POBJECT_ATTRIBUTES ObjectAttributes,
    IN  ULONG TitleIndex,
    IN  PUNICODE_STRING Class OPTIONAL,
    IN  ULONG CreateOptions,
    OUT PULONG Disposition OPTIONAL
    )
{
    return CmCreateKey(KeyHandle, DesiredAccess, ObjectAttributes,
                       TitleIndex, Class, CreateOptions, Disposition);
}

/*--------------------------------------------------------------------------
    NtOpenKey - open an existing registry key (Nt API wrapper).
--------------------------------------------------------------------------*/
NTSTATUS
NtOpenKey(
    OUT PHANDLE KeyHandle,
    IN  ACCESS_MASK DesiredAccess,
    IN  POBJECT_ATTRIBUTES ObjectAttributes
    )
{
    return CmOpenKey(KeyHandle, DesiredAccess, ObjectAttributes);
}

/*--------------------------------------------------------------------------
    NtCloseKey - close a registry key handle (Nt API wrapper).
--------------------------------------------------------------------------*/
NTSTATUS
NtCloseKey(
    IN HANDLE KeyHandle
    )
{
    return CmCloseKey(KeyHandle);
}

/*--------------------------------------------------------------------------
    NtEnumerateKey - enumerate subkeys of a key (Nt API wrapper).
--------------------------------------------------------------------------*/
NTSTATUS
NtEnumerateKey(
    IN  HANDLE KeyHandle,
    IN  ULONG  Index,
    OUT PUNICODE_STRING Name OPTIONAL,
    OUT PULONG NameLength OPTIONAL
    )
{
    PHHIVE hive;
    HCELL_INDEX cellIndex;
    NTSTATUS status;

    status = CmpLookupHandle((ULONG)(ULONG_PTR)KeyHandle, &hive, &cellIndex);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return CmEnumerateSubKeys(hive, cellIndex, Index, Name, NameLength);
}
