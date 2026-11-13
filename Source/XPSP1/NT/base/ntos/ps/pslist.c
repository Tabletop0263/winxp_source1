/*++

Copyright (c) Microsoft Corporation

Module Name:

    pslist.c

Abstract:

    Process and thread tracking lists for the PowerPC (Broadway/G3) port
    of Windows XP (NT 5.1).  Maintains the global process list and
    per-process thread lists used by the process/thread manager (PS).

    This module provides the list heads and insertion/removal helpers
    used by pscreate.c and psexit.c.  All lists are protected by the
    caller (PS spin lock is a stub for this minimal port).

Author:

    (port)

Environment:

    Kernel mode only.

--*/

#include "ppctx.h"
#include "wxptypes.h"

/*--------------------------------------------------------------------------
    Global process list head.  All EPROCESS bodies are linked through
    PsProcessListEntry (embedded at a fixed offset in EPROCESS).
--------------------------------------------------------------------------*/
static LIST_ENTRY PsProcessListHead;

/*--------------------------------------------------------------------------
    Global process count.
--------------------------------------------------------------------------*/
static ULONG PsProcessCount = 0;

/*--------------------------------------------------------------------------
    PsInsertProcess - insert a process into the global process list.
--------------------------------------------------------------------------*/
VOID
PsInsertProcess(
    IN PEPROCESS Process,
    IN PLIST_ENTRY ListEntry
    )
{
    InsertTailList(&PsProcessListHead, ListEntry);
    PsProcessCount++;
}

/*--------------------------------------------------------------------------
    PsRemoveProcess - remove a process from the global process list.
--------------------------------------------------------------------------*/
VOID
PsRemoveProcess(
    IN PLIST_ENTRY ListEntry
    )
{
    if (!IsListEmpty(&PsProcessListHead)) {
        RemoveEntryList(ListEntry);
        if (PsProcessCount > 0) {
            PsProcessCount--;
        }
    }
}

/*--------------------------------------------------------------------------
    PsProcessList - return the address of the global process list head.
--------------------------------------------------------------------------*/
PLIST_ENTRY
PsProcessList(
    VOID
    )
{
    return &PsProcessListHead;
}

/*--------------------------------------------------------------------------
    PsProcessCountGet - return the number of tracked processes.
--------------------------------------------------------------------------*/
ULONG
PsProcessCountGet(
    VOID
    )
{
    return PsProcessCount;
}

/*--------------------------------------------------------------------------
    Per-process thread list helpers.  Each process body contains a
    PROCESS_THREAD_LIST (defined in wxptypes.h) with a thread list head
    and thread count.
--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
    PsThreadListInit - initialize a process's thread list head.
--------------------------------------------------------------------------*/
VOID
PsThreadListInit(
    OUT PPROCESS_THREAD_LIST ThreadList
    )
{
    InitializeListHead(&ThreadList->ThreadListHead);
    ThreadList->ThreadCount = 0;
}

/*--------------------------------------------------------------------------
    PsThreadListInsert - insert a thread into a process's thread list.
--------------------------------------------------------------------------*/
VOID
PsThreadListInsert(
    IN OUT PPROCESS_THREAD_LIST ThreadList,
    IN PLIST_ENTRY ThreadEntry
    )
{
    InsertTailList(&ThreadList->ThreadListHead, ThreadEntry);
    ThreadList->ThreadCount++;
}

/*--------------------------------------------------------------------------
    PsThreadListRemove - remove a thread from a process's thread list.
--------------------------------------------------------------------------*/
VOID
PsThreadListRemove(
    IN OUT PPROCESS_THREAD_LIST ThreadList,
    IN PLIST_ENTRY ThreadEntry
    )
{
    if (!IsListEmpty(&ThreadList->ThreadListHead)) {
        RemoveEntryList(ThreadEntry);
        if (ThreadList->ThreadCount > 0) {
            ThreadList->ThreadCount--;
        }
    }
}

/*--------------------------------------------------------------------------
    PsThreadListHeadAddr - return address of a process's thread list head.
--------------------------------------------------------------------------*/
PLIST_ENTRY
PsThreadListHeadAddr(
    IN PPROCESS_THREAD_LIST ThreadList
    )
{
    return &ThreadList->ThreadListHead;
}

/*--------------------------------------------------------------------------
    PsThreadCountGet - return number of threads in a process's thread list.
--------------------------------------------------------------------------*/
ULONG
PsThreadCountGet(
    IN PPROCESS_THREAD_LIST ThreadList
    )
{
    return ThreadList->ThreadCount;
}
