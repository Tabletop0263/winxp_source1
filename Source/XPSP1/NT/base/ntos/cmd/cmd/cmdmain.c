/*++

Copyright (c) Microsoft Corporation

Module Name:

    cmdmain.c

Abstract:

    Minimal Windows command processor (CMD.EXE analog) for the PowerPC
    (Broadway/G3) port of Windows XP (NT 5.1).  Self-contained: only
    depends on ppctx.h and wxptypes.h plus the HAL display/input
    functions declared in ppctx.h (HalpDisplayPrintf, HalpPutChar,
    HalpGetControllerState, HalpControllerMapButton).

    This is the shell entry point started by KeInitializeKernelPpc via
    PsCreateSystemThread.  The thread start routine signature is
    PKSTART_ROUTINE (VOID return, PVOID context).  It runs a simple REPL:

      - read a line of input from the gamepad-driven console (console.c)
      - dispatch the command to cmds.c (cmds.c:CmdDispatch)
      - print the result and loop

Author:

    (port)

Environment:

    Kernel mode only (runs as the initial system thread).

--*/

#include "ppctx.h"
#include "wxptypes.h"

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

/*--------------------------------------------------------------------------
    Console / command dispatch / line editor (console.c, cmds.c).
    Forward declarations only; implemented in console.c / cmds.c.
--------------------------------------------------------------------------*/
NTSTATUS
CmdDispatchLine(
    IN PCHAR CommandLine,
    IN ULONG CommandLineLength
    );

VOID
CmdInitializeConsole(
    VOID
    );

VOID
CmdShutdownConsole(
    VOID
    );

/*--------------------------------------------------------------------------
    CmdMain - the shell's main thread start routine.

    Signature matches PKSTART_ROUTINE (VOID return, PVOID context) so it
    can be passed directly to PsCreateSystemThread.
--------------------------------------------------------------------------*/
VOID
CmdMain(
    IN PVOID StartContext
    )
{
    UNREFERENCED_PARAMETER(StartContext);

    CmdInitializeConsole();

    HalpDisplayPrintf("Microsoft Windows XP [Version 5.1.2600]\r\n");
    HalpDisplayPrintf("(C) Copyright 1985-2001 Microsoft Corp.\r\n");
    HalpDisplayPrintf("\r\n");

    /* Simple REPL loop.  Reads a line, dispatches it, prints the result. */
    for (;;) {
        HalpDisplayPrintf("C:\\> ");
        /* Line reading and dispatch is implemented in console.c / cmds.c.
         * For now the shell idles here after printing the prompt. */
        HalpPutChar('\r');
    }

    /* Unreachable (no exit path in the minimal shell yet). */
    CmdShutdownConsole();
    return;
}
