/*++
Module Name:

    cmvalue.c

Abstract:

    Windows XP (NT 5.1) Config Manager - Registry Value Operations.
    PowerPC (Broadway/G3) port.

    Implements NtSetValueKey, NtQueryValueKey, NtEnumerateValueKey,
    NtDeleteValueKey, NtFlushKey.  All multi-byte field access is
    byte-safe via the CmRd/CmWr helpers in cm.h.

Environment:

    Kernel mode only.
--*/

#include "cm.h"
#include <string.h>

/*--------------------------------------------------------------------------
    NtSetValueKey - set or create a registry value.
--------------------------------------------------------------------------*/
NTSTATUS
NtSetValueKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING ValueName,
    IN ULONG TitleIndex,
    IN ULONG Type,
    IN PVOID Data OPTIONAL,
    IN ULONG DataSize
    )
{
    PHHIVE hive;
    ULONG  cellIndex;
    NTSTATUS status;
    PUCHAR parent;
    HCELL_INDEX valueCell;
    PUCHAR payload;
    ULONG payloadSize;
    ULONG nameBytes;
    ULONG dataOffset;

    (void)TitleIndex;

    if (ValueName == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    status = CmpLookupHandle((ULONG)(ULONG_PTR)KeyHandle, &hive, &cellIndex);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    parent = CmPool + cellIndex;
    if (parent >= CmPool + CM_POOL_SIZE) {
        return STATUS_INVALID_HANDLE;
    }

    nameBytes = ValueName->Length;
    payloadSize = KV_SizeFixed + nameBytes + DataSize;

    /* Allocate a CM_KEY_VALUE cell holding name + inline data. */
    valueCell = CmAllocateCell(hive, CM_CELL_KEY_VALUE, payloadSize);
    if (valueCell == HCELL_NIL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    payload = CmAllocatePayload(hive, CM_CELL_KEY_VALUE, payloadSize,
                                &valueCell);
    if (payload == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Fill the value cell fields (byte-safe). */
    CmWrU16(payload, KV_oSignature, 0x766b);   /* "vk" */
    CmWrU16(payload, KV_oNameLength, (USHORT)nameBytes);
    CmWrU32(payload, KV_oDataLength, DataSize);
    dataOffset = KV_oName + nameBytes;
    CmWrU32(payload, KV_oData, dataOffset);
    CmWrU32(payload, KV_oType, Type);
    CmWrU16(payload, KV_oFlags, 0);

    if (nameBytes > 0 && ValueName->Buffer != NULL) {
        CmMemCpy(payload + KV_oName, (CONST UCHAR *)ValueName->Buffer,
                 nameBytes);
    }
    if (DataSize > 0 && Data != NULL) {
        CmMemCpy(payload + dataOffset, (CONST UCHAR *)Data, DataSize);
    }

    /* Store the value cell index in the key node's ValueList field. */
    CmWrU32(parent, KN_oValueList, valueCell);
    hive->Dirty = TRUE;

    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    NtQueryValueKey - query a registry value.
--------------------------------------------------------------------------*/
NTSTATUS
NtQueryValueKey(
    IN  HANDLE KeyHandle,
    IN  PUNICODE_STRING ValueName,
    OUT PVOID  Data OPTIONAL,
    IN  ULONG  DataLength,
    OUT PULONG ResultLength OPTIONAL
    )
{
    PHHIVE hive;
    ULONG  cellIndex;
    NTSTATUS status;
    PUCHAR parent;
    HCELL_INDEX valueCell;
    PUCHAR value;
    ULONG storedSize;
    PUCHAR dataPtr;

    (void)ValueName;

    status = CmpLookupHandle((ULONG)(ULONG_PTR)KeyHandle, &hive, &cellIndex);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    parent = CmPool + cellIndex;
    if (parent >= CmPool + CM_POOL_SIZE) {
        return STATUS_INVALID_HANDLE;
    }

    valueCell = CmRdU32(parent, KN_oValueList);
    if (valueCell == 0 || valueCell == HCELL_NIL) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    value = CmPool + valueCell;
    if (value >= CmPool + CM_POOL_SIZE) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    storedSize = CmRdU32(value, KV_oDataLength);
    if (ResultLength) {
        *ResultLength = storedSize;
    }

    dataPtr = value + KV_oName + CmRdU16(value, KV_oNameLength);
    if (Data != NULL && DataLength > 0) {
        ULONG copySize = storedSize < DataLength ? storedSize : DataLength;
        CmMemCpy(Data, dataPtr, copySize);
    }

    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    NtEnumerateValueKey - enumerate values of a key.
--------------------------------------------------------------------------*/
NTSTATUS
NtEnumerateValueKey(
    IN  HANDLE KeyHandle,
    IN  ULONG  Index,
    OUT PUNICODE_STRING ValueName OPTIONAL,
    OUT PULONG NameLength OPTIONAL
    )
{
    PHHIVE hive;
    ULONG  cellIndex;
    NTSTATUS status;
    PUCHAR parent;
    HCELL_INDEX valueCell;
    PUCHAR value;
    ULONG nameLen;

    status = CmpLookupHandle((ULONG)(ULONG_PTR)KeyHandle, &hive, &cellIndex);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    parent = CmPool + cellIndex;
    if (parent >= CmPool + CM_POOL_SIZE) {
        return STATUS_INVALID_HANDLE;
    }

    valueCell = CmRdU32(parent, KN_oValueList);
    if (valueCell == 0 || valueCell == HCELL_NIL) {
        return STATUS_NO_MORE_ENTRIES;
    }

    if (Index != 0) {
        /* Single inline value only; index 0 is the only entry. */
        return STATUS_NO_MORE_ENTRIES;
    }

    value = CmPool + valueCell;
    if (value >= CmPool + CM_POOL_SIZE) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    nameLen = CmRdU16(value, KV_oNameLength);
    if (ValueName != NULL) {
        PWSTR valBuf = (PWSTR)(value + KV_oName);

        if (ValueName->MaximumLength < nameLen) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        CmMemCpy((UCHAR *)ValueName->Buffer, (CONST UCHAR *)valBuf, nameLen);
        ValueName->Length = (USHORT)nameLen;
    }

    if (NameLength) {
        *NameLength = nameLen;
    }

    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    NtDeleteValueKey - delete a registry value.
--------------------------------------------------------------------------*/
NTSTATUS
NtDeleteValueKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING ValueName
    )
{
    PHHIVE hive;
    ULONG  cellIndex;
    NTSTATUS status;
    PUCHAR parent;

    (void)ValueName;

    status = CmpLookupHandle((ULONG)(ULONG_PTR)KeyHandle, &hive, &cellIndex);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    parent = CmPool + cellIndex;
    if (parent >= CmPool + CM_POOL_SIZE) {
        return STATUS_INVALID_HANDLE;
    }

    /* Clear the value list pointer (delete the single inline value). */
    CmWrU32(parent, KN_oValueList, HCELL_NIL);
    hive->Dirty = TRUE;

    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    NtFlushKey - flush a key's changes (no-op for in-memory hive).
--------------------------------------------------------------------------*/
NTSTATUS
NtFlushKey(
    IN HANDLE KeyHandle
    )
{
    PHHIVE hive;
    ULONG  cellIndex;
    NTSTATUS status;

    status = CmpLookupHandle((ULONG)(ULONG_PTR)KeyHandle, &hive,
                             &cellIndex);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    hive->Dirty = FALSE;
    return STATUS_SUCCESS;
}
