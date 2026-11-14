/*++

Module Name:

    ps_create_broadway.c

Abstract:

    Broadway/G3 minimal PsCreateSystemThread implementation.

    This port has no scheduler yet, so PsCreateSystemThread does not
    allocate a real kernel thread object or preemptively schedule it.
    Instead it validates parameters and returns success; the caller
    (KeInitializeKernelPpc) is expected to invoke the start routine
    directly on the current CPU as the initial system thread.  This keeps
    the shell REPL (CmdMain -> ConsoleReadLine blocking poll) running
    without needing interrupt-driven scheduling.

Environment:

    Kernel mode only.

--*/

#include "ppctx.h"
#include "wxptypes.h"

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

/* PsCreateSystemThread prototype mirrors ntos/inc/ps.h:1095.
 * Signature: NTSTATUS PsCreateSystemThread(PHANDLE, ULONG, POBJECT_ATTRIBUTES,
 *     HANDLE, PCLIENT_ID, PKSTART_ROUTINE, PVOID) */
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
    UNREFERENCED_PARAMETER(DesiredAccess);
    UNREFERENCED_PARAMETER(ObjectAttributes);
    UNREFERENCED_PARAMETER(ProcessHandle);
    UNREFERENCED_PARAMETER(ClientId);

    if (ThreadHandle != NULL) {
        *ThreadHandle = (HANDLE)1;  /* pseudo-handle for "shell thread" */
    }

    /*
     * No scheduler yet: do not block here.  KeInitializeKernelPpc calls
     * PsCreateSystemThread then returns to the idle loop in ntos_entry.S.
     * To make the shell actually run we invoke the start routine directly
     * on this CPU before returning, so CmdMain's ConsoleReadLine poll loop
     * gets control on real hardware.  (The start routine is VOID-returning,
     * matching PKSTART_ROUTINE.) */
    if (StartRoutine != NULL) {
        StartRoutine(StartContext);
    }

    return STATUS_SUCCESS;
}
