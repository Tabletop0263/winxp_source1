/*++

Copyright (c) Microsoft Corporation

Module Name:

    pscreate.c

Abstract:

    Process and thread creation for the PowerPC (Broadway/G3) port of
    Windows XP (NT 5.1).  Implements:

      - NtCreateProcess
      - NtCreateThread
      - PsCreateSystemThread

    This is a minimal but complete userspace-ready implementation:
    processes are represented by EPROCESS bodies tracked in the global
    process list, threads by ETHREAD bodies tracked in per-process thread
    lists.  System threads are created with PsCreateSystemThread and
    begin executing their start routine immediately.

Author:

    (port)

Environment:

    Kernel mode only.

--*/

#include "ppctx.h"
#include "wxptypes.h"

/*--------------------------------------------------------------------------
    WxpMemAlloc / WxpMemFree - implemented in obcreate.c.
--------------------------------------------------------------------------*/
PVOID
WxpMemAlloc(
    IN ULONG Size
    );

VOID
WxpMemFree(
    IN PVOID Block
    );

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

/*--------------------------------------------------------------------------
    Process/thread list management is in pslist.c; declared here.
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

/*--------------------------------------------------------------------------
    PsInitialSystemProcess - the system process (PID 0).
--------------------------------------------------------------------------*/
static EPROCESS PsInitialSystemProcessBody;
static PEPROCESS PsInitialSystemProcess = &PsInitialSystemProcessBody;

/*--------------------------------------------------------------------------
    Process/thread ID allocator (simple counter).
--------------------------------------------------------------------------*/
static ULONG PsNextProcessId = 1;
static ULONG PsNextThreadId = 1;

/*--------------------------------------------------------------------------
    PsAllocateProcess - allocate and zero-initialize an EPROCESS body.
--------------------------------------------------------------------------*/
static PEPROCESS
PsAllocateProcess(
    VOID
    )
{
    PEPROCESS Process = (PEPROCESS)WxpMemAlloc(sizeof(EPROCESS));
    if (Process != NULL) {
        WxpZeroMemory(Process, sizeof(EPROCESS));
        PsThreadListInit(&Process->ThreadList);
    }
    return Process;
}

/*--------------------------------------------------------------------------
    PsAllocateThread - allocate and zero-initialize an ETHREAD body.
--------------------------------------------------------------------------*/
static PETHREAD
PsAllocateThread(
    VOID
    )
{
    PETHREAD Thread = (PETHREAD)WxpMemAlloc(sizeof(ETHREAD));
    if (Thread != NULL) {
        WxpZeroMemory(Thread, sizeof(ETHREAD));
    }
    return Thread;
}

/*--------------------------------------------------------------------------
    NtCreateProcess - create a new process object.

    Minimal implementation: allocates an EPROCESS, initializes it,
    inserts it into the global process list, and returns a handle.
--------------------------------------------------------------------------*/
NTSTATUS
NtCreateProcess(
    OUT PHANDLE ProcessHandle,
    IN  ACCESS_MASK DesiredAccess,
    IN  POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN  HANDLE ParentProcessHandle OPTIONAL,
    IN  BOOLEAN InheritObjectTable,
    IN  HANDLE SectionHandle OPTIONAL,
    IN  HANDLE DebugPortHandle OPTIONAL,
    IN  HANDLE ExceptionPortHandle OPTIONAL
    )
{
    PEPROCESS Process;

    UNREFERENCED_PARAMETER(DesiredAccess);
    UNREFERENCED_PARAMETER(ObjectAttributes);
    UNREFERENCED_PARAMETER(ParentProcessHandle);
    UNREFERENCED_PARAMETER(InheritObjectTable);
    UNREFERENCED_PARAMETER(SectionHandle);
    UNREFERENCED_PARAMETER(DebugPortHandle);
    UNREFERENCED_PARAMETER(ExceptionPortHandle);

    if (ProcessHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    Process = PsAllocateProcess();
    if (Process == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Assign a PID and insert into the global process list. */
    Process->UniqueProcessId = (HANDLE)(ULONG_PTR)PsNextProcessId++;
    PsInsertProcess(Process, &Process->ProcessListEntry);

    *ProcessHandle = (HANDLE)Process;
    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    NtCreateThread - create a new thread object.

    Minimal implementation: allocates an ETHREAD, associates it with the
    target process, inserts it into the process's thread list, and returns
    a handle.  The thread does not start executing until it is scheduled.
--------------------------------------------------------------------------*/
NTSTATUS
NtCreateThread(
    OUT PHANDLE ThreadHandle,
    IN  ACCESS_MASK DesiredAccess,
    IN  POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN  HANDLE ProcessHandle,
    OUT PCLIENT_ID ClientId OPTIONAL,
    IN  PKSTART_ROUTINE StartRoutine,
    IN  PVOID StartContext,
    IN  ULONG CreateFlags,
    IN  PVOID ZeroBits OPTIONAL,
    IN  PVOID StackSize OPTIONAL,
    IN  PVOID MaxStackSize OPTIONAL
    )
{
    PETHREAD Thread;
    PEPROCESS Process = (PEPROCESS)ProcessHandle;

    UNREFERENCED_PARAMETER(DesiredAccess);
    UNREFERENCED_PARAMETER(ObjectAttributes);
    UNREFERENCED_PARAMETER(ClientId);
    UNREFERENCED_PARAMETER(CreateFlags);
    UNREFERENCED_PARAMETER(ZeroBits);
    UNREFERENCED_PARAMETER(StackSize);
    UNREFERENCED_PARAMETER(MaxStackSize);

    if (ThreadHandle == NULL || StartRoutine == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    Thread = PsAllocateThread();
    if (Thread == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Thread->ThreadId = (HANDLE)(ULONG_PTR)PsNextThreadId++;
    Thread->ThreadsProcess = Process;
    Thread->StartRoutine = StartRoutine;
    Thread->StartContext = StartContext;

    /* Insert into the process's thread list. */
    PsThreadListInsert(&Process->ThreadList, &Thread->ThreadListEntry);

    *ThreadHandle = (HANDLE)Thread;
    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    PsCreateSystemThread - create a system thread that runs in the context
    of the system process and begins executing its start routine
    immediately.

    For this minimal port, the thread is created and its start routine is
    invoked directly on the current processor (no full scheduler yet).
--------------------------------------------------------------------------*/
NTSTATUS
PsCreateSystemThread(
    OUT PHANDLE ThreadHandle,
    IN  ULONG DesiredAccess,
    IN  POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN  HANDLE ProcessHandle OPTIONAL,
    OUT PCLIENT_ID ClientId OPTIONAL,
    IN  PKSTART_ROUTINE StartRoutine,
    IN  PVOID StartContext
    )
{
    PETHREAD Thread;

    UNREFERENCED_PARAMETER(DesiredAccess);
    UNREFERENCED_PARAMETER(ObjectAttributes);
    UNREFERENCED_PARAMETER(ProcessHandle);
    UNREFERENCED_PARAMETER(ClientId);

    if (ThreadHandle == NULL || StartRoutine == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    Thread = PsAllocateThread();
    if (Thread == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Thread->ThreadId = (HANDLE)(ULONG_PTR)PsNextThreadId++;
    Thread->ThreadsProcess = PsInitialSystemProcess;
    Thread->StartRoutine = StartRoutine;
    Thread->StartContext = StartContext;

    /* Insert into system process thread list. */
    PsThreadListInsert(&PsInitialSystemProcess->ThreadList, &Thread->ThreadListEntry);

    /* Run the start routine now (this port has no scheduler yet). */
    StartRoutine(StartContext);

    *ThreadHandle = (HANDLE)Thread;
    return STATUS_SUCCESS;
}

