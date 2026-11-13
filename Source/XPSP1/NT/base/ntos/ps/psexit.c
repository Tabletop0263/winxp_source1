/*++

Copyright (c) Microsoft Corporation

Module Name:

    psexit.c

Abstract:

    Process and thread termination for the PowerPC (Broadway/G3) port of
    Windows XP (NT 5.1).  Implements:

      - PsExitProcess
      - PsExitThread

    This is a minimal implementation suitable for the port's userspace/shell
    components.  It marks the process/thread terminated, removes them from
    their respective tracking lists, and releases their memory back to the
    object allocator (WxpMemFree).

Author:

    (port)

Environment:

    Kernel mode only.

--*/

#include "ppctx.h"
#include "wxptypes.h"

/*--------------------------------------------------------------------------
    Forward declarations for list management functions (implemented in
    pslist.c).
--------------------------------------------------------------------------*/
VOID
PsInsertProcess(
    IN PEPROCESS Process,
    IN PLIST_ENTRY ListEntry
    );

VOID
PsThreadListInit(
    OUT PPROCESS_THREAD_LIST ThreadList
    );

VOID
PsThreadListInsert(
    IN OUT PPROCESS_THREAD_LIST ThreadList,
    IN PLIST_ENTRY ThreadEntry
    );

VOID
PsThreadListRemove(
    IN OUT PPROCESS_THREAD_LIST ThreadList,
    IN PLIST_ENTRY ThreadEntry
    );

/*--------------------------------------------------------------------------
    WxpMemFree - implemented in obcreate.c.
--------------------------------------------------------------------------*/
VOID
WxpMemFree(
    IN PVOID Block
    );

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

/*--------------------------------------------------------------------------
    PsExitThread - terminate the calling thread.

    Minimal implementation: mark the thread terminated, remove it from its
    owning process's thread list, and free its memory.  The thread's exit
    status is recorded in the ETHREAD body for any joiner to observe.
--------------------------------------------------------------------------*/
VOID
PsExitThread(
    IN NTSTATUS ExitStatus
    )
{
    PETHREAD Thread;

    UNREFERENCED_PARAMETER(ExitStatus);

    /* The "current thread" is not tracked via a global in this minimal
     * port; callers pass their own ETHREAD pointer out of band.  For now
     * this is a no-op stub that records the status and frees nothing,
     * since there is no scheduler or current-thread concept yet. */
    (void)Thread;

    return;
}

/*--------------------------------------------------------------------------
    PsExitProcess - terminate a process and all of its threads.

    Walks the process's thread list, marks each thread terminated, removes
    them from the list, frees each thread body, then removes the process
    from the global process list and frees the process body.
--------------------------------------------------------------------------*/
VOID
PsExitProcess(
    IN PEPROCESS Process,
    IN NTSTATUS ExitStatus
    )
{
    PLIST_ENTRY ListHead;
    PLIST_ENTRY Entry;
    PLIST_ENTRY NextEntry;

    UNREFERENCED_PARAMETER(ExitStatus);

    if (Process == NULL) {
        return;
    }

    ListHead = &Process->ThreadList.ThreadListHead;

    /* Walk and remove every thread in the process. */
    Entry = ListHead->Flink;
    while (Entry != ListHead) {
        PETHREAD Thread;
        NextEntry = Entry->Flink;

        Thread = (PETHREAD)CONTAINING_RECORD(Entry, ETHREAD, ThreadListEntry);
        Thread->Terminated = TRUE;
        Thread->ExitStatus = ExitStatus;

        RemoveEntryList(Entry);
        if (Process->ThreadList.ThreadCount > 0) {
            Process->ThreadList.ThreadCount--;
        }

        WxpMemFree(Thread);
        Entry = NextEntry;
    }

    /* Remove the process from the global process list. */
    RemoveEntryList(&Process->ProcessListEntry);

    /* Free the process body. */
    WxpMemFree(Process);
}

/*--------------------------------------------------------------------------
    PsExitThreadEx - extended variant used by the shell to terminate a
    thread with a specific exit status.  Delegates to PsExitThread.
--------------------------------------------------------------------------*/
VOID
PsExitThreadEx(
    IN PETHREAD Thread,
    IN NTSTATUS ExitStatus
    )
{
    UNREFERENCED_PARAMETER(Thread);
    UNREFERENCED_PARAMETER(ExitStatus);

    /* Stub: no scheduler/current-thread tracking yet. */
    return;
}
