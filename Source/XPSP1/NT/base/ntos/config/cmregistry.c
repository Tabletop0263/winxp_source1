/*++
Module Name:

    cmregistry.c

Abstract:

    Windows XP (NT 5.1) Config Manager - In-memory hive cell management.
    PowerPC (Broadway/G3) port.

    Cell-based, fully self-contained hive manager backed by a reserved
    4 MB pool. All multi-byte cell fields are accessed byte-safe via the
    CmRd/CmWr helpers declared in cm.h, so the code is alignment-safe
    on PowerPC.

Environment:

    Kernel mode only.
--*/

#include "cm.h"

/*--------------------------------------------------------------------------
  Reserved in-memory hive pool (4 MB).
--------------------------------------------------------------------------*/
static CMHIVE CmMachineHive;
static BOOLEAN CmHiveInitialized = FALSE;

/* In-memory hive pool, shared across cm*.c files. */
UCHAR CmPool[CM_POOL_SIZE];

/*--------------------------------------------------------------------------
    CmCreateHive - create an empty hive backed by the reserved pool.
--------------------------------------------------------------------------*/
PHHIVE
CmCreateHive(
    VOID
    )
{
    PHHIVE Hive;
    ULONG zero = 0;

    Hive = (PHHIVE)&CmMachineHive.Hive;

    /* Zero the pool so the first cell starts clean. */
    CmMemSet(CmPool, 0, CM_POOL_SIZE);

    Hive->Signature = CM_HIVE_SIGNATURE;
    Hive->Version   = CM_HIVE_VERSION;
    Hive->PoolUsed  = CM_CELL_HEADER_SIZE;  /* reserve the header block */
    Hive->RootCell  = HCELL_NIL;

    /* Write an empty (free) header cell at offset 0 so the pool starts
       with a well-formed cell. */
    CmWriteI32(CmPool, 0, (LONG)CM_CELL_HEADER_SIZE);
    CmWrU32(CmPool, CM_CELL_HEADER_SIZE, CM_CELL_DATA);
    CmWrU32(CmPool, CM_CELL_HEADER_SIZE + 4, zero);

    CmHiveInitialized = TRUE;
    return Hive;
}

/*--------------------------------------------------------------------------
    CmAllocateCell - allocate a cell of the given type and payload size
    (payload excludes the 8-byte header) from the pool using first-fit.
--------------------------------------------------------------------------*/
HCELL_INDEX
CmAllocateCell(
    IN PHHIVE Hive,
    IN ULONG Type,
    IN ULONG PayloadSize
    )
{
    ULONG total;
    ULONG off;
    LONG  size;
    PUCHAR p;

    (void)Type;

    total = CM_CELL_HEADER_SIZE + PayloadSize;

    /* Align total to 4 bytes. */
    total = (total + 3) & ~3UL;

    off = CM_CELL_HEADER_SIZE;   /* skip reserved header block */

    while (off + total <= CM_POOL_SIZE) {
        p = CmPool + off;
        size = CmReadI32(p, 0);

        if (size > 0) {
            /* This cell is in use; skip past it. */
            off += (ULONG)size;
            continue;
        }

        /* Free cell found: size is negative (free block size). */
        size = -size;
        if ((ULONG)size >= total) {
            /* Mark allocated portion. */
            CmWriteI32(p, 0, (LONG)total);
            CmWrU32(p, CM_CELL_HEADER_SIZE, Type);

            if ((ULONG)size > total) {
                /* Carve out remainder as a new free cell. */
                PUCHAR rest = p + total;
                CmWriteI32(rest, 0, (LONG)(-((LONG)((ULONG)size - total))));
                CmWrU32(rest, CM_CELL_HEADER_SIZE, CM_CELL_DATA);
            }

            Hive->PoolUsed = off + total;
            return (HCELL_INDEX)off;
        }

        /* Not big enough, advance. */
        off += (ULONG)size;
    }

    return HCELL_NIL;
}

/*--------------------------------------------------------------------------
    CmFreeCell - mark a cell free and coalesce with the next free cell.
--------------------------------------------------------------------------*/
VOID
CmFreeCell(
    IN PHHIVE Hive,
    IN HCELL_INDEX Cell
    )
{
    ULONG off;
    ULONG poolUsed;
    PUCHAR p;
    LONG  size;
    ULONG nextOff;
    LONG  nextSize;
    PUCHAR nbase;

    off = Cell;
    p = CmPool + off;
    if (p >= CmPool + CM_POOL_SIZE) {
        return;
    }

    size = CmReadI32(p, 0);
    if (size < 0) {
        /* Already free. */
        return;
    }

    CmWriteI32(p, 0, -size);
    CmWrU32(p, CM_CELL_HEADER_SIZE, CM_CELL_DATA);

    /* Coalesce with the next cell if it is free. */
    nextOff = off + (ULONG)size;
    poolUsed = Hive->PoolUsed;
    if (nextOff < poolUsed) {
        nbase = CmPool + nextOff;
        if (nbase < CmPool + CM_POOL_SIZE) {
            nextSize = CmReadI32(nbase, 0);
            if (nextSize < 0) {
                CmWriteI32(p, 0, -((LONG)(-size + -nextSize)));
                CmWrU32(p, CM_CELL_HEADER_SIZE, CM_CELL_DATA);
            }
        }
    }
}

/*--------------------------------------------------------------------------
    CmAllocatePayload - allocate raw bytes in a new cell and return its
    pointer, writing back the cell index via CellIndex.
--------------------------------------------------------------------------*/
UCHAR *
CmAllocatePayload(
    IN PHHIVE Hive,
    IN ULONG Type,
    IN ULONG PayloadSize,
    OUT PHCELL_INDEX CellIndex
    )
{
    HCELL_INDEX cell;

    cell = CmAllocateCell(Hive, Type, PayloadSize);
    if (cell == HCELL_NIL) {
        *CellIndex = HCELL_NIL;
        return NULL;
    }

    *CellIndex = cell;
    return CmPool + cell + CM_CELL_HEADER_SIZE;
}

/*--------------------------------------------------------------------------
    HvGetCell - translate a cell index into a byte address in the pool.
--------------------------------------------------------------------------*/
UCHAR *
HvGetCell(
    IN PHHIVE Hive,
    IN HCELL_INDEX Cell
    )
{
    (void)Hive;
    if (Cell == HCELL_NIL || Cell >= CM_POOL_SIZE) {
        return NULL;
    }
    return CmPool + Cell;
}

/*--------------------------------------------------------------------------
    CmpCellToPointer - translate a cell index into a byte address.
--------------------------------------------------------------------------*/
PVOID
CmpCellToPointer(
    IN PHHIVE Hive,
    IN HCELL_INDEX Cell
    )
{
    return (PVOID)HvGetCell(Hive, Cell);
}

/*--------------------------------------------------------------------------
    Handle table (simple slot array).
--------------------------------------------------------------------------*/
#define CM_MAX_HANDLES  256

typedef struct _CM_HANDLE_ENTRY {
    PHHIVE      Hive;
    HCELL_INDEX Cell;
    BOOLEAN     InUse;
} CM_HANDLE_ENTRY;

static CM_HANDLE_ENTRY KeyHandleTable[CM_MAX_HANDLES];

/*--------------------------------------------------------------------------
    CmpAllocHandle - allocate a handle slot for a hive cell.
    Returns the handle index, or HCELL_NIL on failure.
--------------------------------------------------------------------------*/
ULONG
CmpAllocHandle(
    IN PHHIVE Hive,
    IN HCELL_INDEX CellIndex
    )
{
    ULONG i;

    (void)Hive;

    for (i = 0; i < CM_MAX_HANDLES; i++) {
        if (!KeyHandleTable[i].InUse) {
            KeyHandleTable[i].InUse   = TRUE;
            KeyHandleTable[i].Hive    = Hive;
            KeyHandleTable[i].Cell    = CellIndex;
            return i;
        }
    }
    return HCELL_NIL;
}

/*--------------------------------------------------------------------------
    CmpFreeHandle - release a handle slot. Returns TRUE if freed, FALSE
    if the handle was invalid.
--------------------------------------------------------------------------*/
BOOLEAN
CmpFreeHandle(
    IN ULONG Handle
    )
{
    if (Handle < CM_MAX_HANDLES && KeyHandleTable[Handle].InUse) {
        KeyHandleTable[Handle].InUse = FALSE;
        return TRUE;
    }
    return FALSE;
}

/*--------------------------------------------------------------------------
    CmpLookupHandle - resolve a handle to a hive + cell index.
--------------------------------------------------------------------------*/
NTSTATUS
CmpLookupHandle(
    IN  ULONG   Handle,
    OUT PHHIVE  *Hive,
    OUT PULONG  CellIndex
    )
{
    if (Handle >= CM_MAX_HANDLES || !KeyHandleTable[Handle].InUse) {
        return STATUS_INVALID_HANDLE;
    }
    *Hive     = KeyHandleTable[Handle].Hive;
    *CellIndex = KeyHandleTable[Handle].Cell;
    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    CmPointerToCell - translate a byte address in the pool back to a cell
    index.
--------------------------------------------------------------------------*/
HCELL_INDEX
CmPointerToCell(
    IN PUCHAR Ptr
    )
{
    if (Ptr < CmPool || Ptr >= CmPool + CM_POOL_SIZE) {
        return HCELL_NIL;
    }
    return (HCELL_INDEX)(Ptr - CmPool);
}

/*--------------------------------------------------------------------------
    CmpFindSubKey - find a subkey cell by name under a parent key cell.
--------------------------------------------------------------------------*/
NTSTATUS
CmpFindSubKey(
    IN PHHIVE          Hive,
    IN HCELL_INDEX     ParentCell,
    IN PUNICODE_STRING Name,
    OUT PHCELL_INDEX   ChildCell
    )
{
    PUCHAR parent;
    ULONG  count;
    ULONG  i;

    (void)Hive;

    if (ChildCell) *ChildCell = HCELL_NIL;

    parent = CmPool + ParentCell;
    if (parent >= CmPool + CM_POOL_SIZE) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    count = CmRdU32(parent, KN_oSubKeyCounts);
    if (count == 0) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    /* Walk the subkey index list. */
    {
        HCELL_INDEX listCell = CmRdU32(parent, KN_oValueList + 4);
        /* KN_oSubKeyLists array: Stable@28, Volatile@32. Use stable list. */
        listCell = CmRdU32(parent, KN_oSubKeyLists);
        PUCHAR idx = (PUCHAR)CmpCellToPointer(NULL, listCell);
        if (idx == NULL) {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        for (i = 0; i < CmRdU16(idx, KIX_oCount); i++) {
            HCELL_INDEX kidCell = CmRdU32(idx, KIX_oList + i * 4);
            PUCHAR kid = (PUCHAR)CmpCellToPointer(NULL, kidCell);
            UNICODE_STRING knName;

            if (kid == NULL) {
                continue;
            }

            knName.Length        = CmRdU16(kid, KN_oNameLength);
            knName.MaximumLength = knName.Length;
            knName.Buffer        = (PWSTR)(kid + KN_oName);

            if (knName.Length == Name->Length) {
                BOOLEAN match = TRUE;
                ULONG j;
                for (j = 0; j < knName.Length / sizeof(WCHAR); j++) {
                    WCHAR c1 = knName.Buffer[j];
                    WCHAR c2 = Name->Buffer[j];
                    if (c1 >= L'a' && c1 <= L'z') c1 = (WCHAR)(c1 - L'a' + L'A');
                    if (c2 >= L'a' && c2 <= L'z') c2 = (WCHAR)(c2 - L'a' + L'A');
                    if (c1 != c2) { match = FALSE; break; }
                }
                if (match) {
                    if (ChildCell) *ChildCell = kidCell;
                    return STATUS_SUCCESS;
                }
            }
        }
    }

    return STATUS_OBJECT_NAME_NOT_FOUND;
}

/*--------------------------------------------------------------------------
    CmpCreateKeyNode - create a new key node cell under a parent.
--------------------------------------------------------------------------*/
NTSTATUS
CmpCreateKeyNode(
    IN PHHIVE           Hive,
    IN PUNICODE_STRING  Name,
    IN HCELL_INDEX      ParentCell,
    OUT PHCELL_INDEX    NewCell
    )
{
    PUCHAR payload;
    HCELL_INDEX cell;
    ULONG nameBytes;

    if (NewCell) *NewCell = HCELL_NIL;

    nameBytes = Name->Length;
    if (nameBytes == 0 || nameBytes > 256) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Allocate a key-node cell: fixed header + name. */
    cell = CmAllocateCell(Hive, CM_CELL_KEY_NODE,
                          KN_SizeFixed + nameBytes);
    if (cell == HCELL_NIL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    payload = CmAllocatePayload(Hive, CM_CELL_KEY_NODE,
                                KN_SizeFixed + nameBytes, &cell);
    if (payload == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Fill the key node fields (byte-safe). */
    CmWrU16(payload, KN_oSignature, CM_KEY_NODE_SIGNATURE);   /* "nk" */
    CmWrU16(payload, KN_oFlags, 0);
    CmWrU32(payload, KN_oParent, ParentCell);
    CmWrU32(payload, KN_oSubKeyCounts, 0);
    CmWrU32(payload, KN_oSubKeyLists, HCELL_NIL);
    CmWrU32(payload, KN_oValueList, HCELL_NIL);
    CmWrU32(payload, KN_oClass, HCELL_NIL);
    CmWrU32(payload, KN_oNameLength, (USHORT)nameBytes);
    CmMemCpy(payload + KN_oName, (const UCHAR *)Name->Buffer, nameBytes);

    /* Link into parent's subkey list. */
    {
        PUCHAR pParent = (PUCHAR)CmpCellToPointer(NULL, ParentCell);
        if (pParent != NULL) {
            ULONG count = CmRdU32(pParent, KN_oSubKeyCounts);
            CmWrU32(pParent, KN_oSubKeyCounts, count + 1);
            CmWrU32(pParent, KN_oSubKeyLists, cell);
        }
    }

    if (NewCell) *NewCell = cell;
    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    CmInitializeConfigRegistry - initialize the config manager and create
    HKLM\HARDWARE, HKLM\SYSTEM, HKLM\SOFTWARE under \REGISTRY\MACHINE.
--------------------------------------------------------------------------*/
NTSTATUS
CmInitializeConfigRegistry(
    IN PVOID LoaderBlock
    )
{
    PHHIVE hive;
    NTSTATUS status;
    HCELL_INDEX rootCell;
    UNICODE_STRING name;
    UCHAR hwName[] = { 0x48,0x00, 0x41,0x00, 0x52,0x00, 0x44,0x00,
                       0x57,0x00, 0x41,0x00, 0x52,0x00, 0x45,0x00, 0x00,0x00 };
    UCHAR sysName[] = { 0x53,0x00, 0x59,0x00, 0x53,0x00, 0x54,0x00,
                        0x45,0x00, 0x4d,0x00, 0x00,0x00 };
    UCHAR swName[]  = { 0x53,0x00, 0x4f,0x00, 0x46,0x00, 0x54,0x00,
                        0x57,0x00, 0x41,0x00, 0x52,0x00, 0x45,0x00, 0x00,0x00 };
    ULONG hwLen = 16;
    ULONG sysLen = 12;
    ULONG swLen  = 16;

    (void)LoaderBlock;

    hive = CmCreateHive();
    if (hive == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Create root key cell ("\" - empty name) under nothing. */
    {
        UNICODE_STRING empty;
        empty.Length        = 0;
        empty.MaximumLength = 0;
        empty.Buffer        = NULL;

        status = CmpCreateKeyNode(hive, &empty, HCELL_NIL, &rootCell);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        hive->RootCell = rootCell;
    }

    /* Create HARDWARE under root. */
    name.Length        = (USHORT)hwLen;
    name.MaximumLength = (USHORT)hwLen;
    name.Buffer        = (PWSTR)hwName;

    status = CmpCreateKeyNode(hive, &name, rootCell, &rootCell);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Create SYSTEM under root. */
    name.Length        = (USHORT)sysLen;
    name.MaximumLength = (USHORT)sysLen;
    name.Buffer        = (PWSTR)sysName;

    status = CmpCreateKeyNode(hive, &name, hive->RootCell, &rootCell);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Create SOFTWARE under root. */
    name.Length        = (USHORT)swLen;
    name.MaximumLength = (USHORT)swLen;
    name.Buffer        = (PWSTR)swName;

    status = CmpCreateKeyNode(hive, &name, hive->RootCell, &rootCell);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    CmGetMachineHive - return the machine hive pointer.
--------------------------------------------------------------------------*/
PHHIVE
CmGetMachineHive(
    VOID
    )
{
    if (!CmHiveInitialized) {
        return NULL;
    }
    return (PHHIVE)&CmMachineHive.Hive;
}
