/*++

Copyright (c) Microsoft Corporation

Module Name:

    cmds.c

Abstract:

    Minimal command dispatch for the Windows command processor analog in
    the PowerPC (Broadway/G3) port of Windows XP (NT 5.1).  Self-contained:
    only depends on ppctx.h and wxptypes.h plus HAL output
    (HalpDisplayPrintf).

    Built-in commands:
      - ver         - print OS version
      - help        - list built-in commands
      - echo <text> - print text
      - cls         - clear the screen
      - exit        - halt (stub: loops forever)

    Unknown commands print "'cmd' is not recognized..." (CMD-style).

Author:

    (port)

Environment:

    Kernel mode only.

--*/

#include "ppctx.h"
#include "wxptypes.h"

/*--------------------------------------------------------------------------
    Process management declarations (ntos/ps/pslist.c).
    Declared here to avoid pulling in the full header tree.
--------------------------------------------------------------------------*/
PLIST_ENTRY PsProcessList(VOID);
ULONG       PsProcessCountGet(VOID);

/* EPROCESS body minimal fields are defined in wxptypes.h (already included).
 */

/*--------------------------------------------------------------------------
    FAT filesystem declarations (hals/halppc/ppc/fat.c).
    Declared here to avoid pulling in the full HAL header tree.
--------------------------------------------------------------------------*/
BOOLEAN HalpFatIsMounted(VOID);
ULONG   HalpFatListDirectory(IN const CHAR *Path);
BOOLEAN HalpFatOpen(IN const CHAR *Path, OUT PULONG FileSize, OUT PULONG StartCluster);
ULONG   HalpFatRead(IN ULONG StartCluster, IN ULONG FileSize,
                    IN ULONG ByteOffset, IN ULONG ByteCount, OUT PVOID Buffer);

#define CMD_FILE_BUF_SIZE 512

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

/*--------------------------------------------------------------------------
    CmdDispatchLine - dispatch a single command line.

    Parses the command name (first whitespace-delimited token) and
    invokes the matching built-in handler.  Returns STATUS_SUCCESS for
    handled commands, STATUS_NOT_IMPLEMENTED for unknown commands.
--------------------------------------------------------------------------*/
NTSTATUS
CmdDispatchLine(
    IN PCHAR CommandLine,
    IN ULONG CommandLineLength
    )
{
    PCHAR Cmd;
    ULONG CmdLen;
    ULONG i;

    UNREFERENCED_PARAMETER(CommandLineLength);

    if (CommandLine == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Skip leading whitespace. */
    Cmd = CommandLine;
    while (*Cmd == ' ' || *Cmd == '\t' || *Cmd == '\r' || *Cmd == '\n') {
        Cmd++;
    }

    /* Empty line - nothing to do. */
    if (*Cmd == '\0') {
        return STATUS_SUCCESS;
    }

    /* Find the end of the command name. */
    CmdLen = 0;
    while (Cmd[CmdLen] != '\0' &&
           Cmd[CmdLen] != ' ' &&
           Cmd[CmdLen] != '\t' &&
           Cmd[CmdLen] != '\r' &&
           Cmd[CmdLen] != '\n') {
        CmdLen++;
    }

    /* ver */
    if (CmdLen == 3 && Cmd[0] == 'v' && Cmd[1] == 'e' && Cmd[2] == 'r') {
        HalpDisplayPrintf("Microsoft Windows XP [Version 5.1.2600]\r\n");
        return STATUS_SUCCESS;
    }

    /* help */
    if (CmdLen == 4 && Cmd[0] == 'h' && Cmd[1] == 'e' && Cmd[2] == 'l' &&
        Cmd[3] == 'p') {
        HalpDisplayPrintf("Built-in commands:\r\n");
        HalpDisplayPrintf("  ver     - display OS version\r\n");
        HalpDisplayPrintf("  help    - list built-in commands\r\n");
        HalpDisplayPrintf("  echo    - display a message\r\n");
        HalpDisplayPrintf("  cls     - clear the screen\r\n");
        HalpDisplayPrintf("  dir     - list a directory (FAT SD card)\r\n");
        HalpDisplayPrintf("  type    - display a file's contents\r\n");
        HalpDisplayPrintf("  ps      - list running processes\r\n");
        HalpDisplayPrintf("  exit    - halt the shell\r\n");
        return STATUS_SUCCESS;
    }

    /* echo */
    if (CmdLen == 4 && Cmd[0] == 'e' && Cmd[1] == 'c' && Cmd[2] == 'h' &&
        Cmd[3] == 'o') {
        PCHAR Arg = Cmd + CmdLen;
        while (*Arg == ' ' || *Arg == '\t') {
            Arg++;
        }
        HalpDisplayPrintf(Arg);
        HalpDisplayPrintf("\r\n");
        return STATUS_SUCCESS;
    }

    /* cls */
    if (CmdLen == 3 && Cmd[0] == 'c' && Cmd[1] == 'l' && Cmd[2] == 's') {
        HalpClearScreen();
        return STATUS_SUCCESS;
    }

    /* exit */
    if (CmdLen == 4 && Cmd[0] == 'e' && Cmd[1] == 'x' && Cmd[2] == 'i' &&
        Cmd[3] == 't') {
        HalpDisplayPrintf("Halting shell...\r\n");
        for (;;) {
            /* Halt. */
        }
        return STATUS_SUCCESS; /* unreachable */
    }

    /* dir */
    if (CmdLen == 3 && Cmd[0] == 'd' && Cmd[1] == 'i' && Cmd[2] == 'r') {
        PCHAR Arg = Cmd + CmdLen;
        while (*Arg == ' ' || *Arg == '\t') {
            Arg++;
        }
        if (!HalpFatIsMounted()) {
            HalpDisplayPrintf("Filesystem not mounted.\r\n");
            return STATUS_SUCCESS;
        }
        HalpDisplayPrintf(" Directory of ");
        HalpDisplayPrintf(*Arg == '\0' ? "\\" : Arg);
        HalpDisplayPrintf("\r\n\r\n");
        HalpFatListDirectory(*Arg == '\0' ? "\\" : Arg);
        return STATUS_SUCCESS;
    }

    /* type */
    if (CmdLen == 4 && Cmd[0] == 't' && Cmd[1] == 'y' && Cmd[2] == 'p' &&
        Cmd[3] == 'e') {
        PCHAR Arg = Cmd + CmdLen;
        ULONG fileSize, startCluster;
        UCHAR buf[CMD_FILE_BUF_SIZE];
        ULONG totalRead = 0;

        while (*Arg == ' ' || *Arg == '\t') {
            Arg++;
        }

        if (*Arg == '\0') {
            HalpDisplayPrintf("Missing filename.\r\n");
            return STATUS_SUCCESS;
        }

        if (!HalpFatIsMounted()) {
            HalpDisplayPrintf("Filesystem not mounted.\r\n");
            return STATUS_SUCCESS;
        }

        if (!HalpFatOpen(Arg, &fileSize, &startCluster)) {
            HalpDisplayPrintf("File not found: ");
            HalpDisplayPrintf(Arg);
            HalpDisplayPrintf("\r\n");
            return STATUS_SUCCESS;
        }

        while (totalRead < fileSize) {
            ULONG toRead = fileSize - totalRead;
            if (toRead > CMD_FILE_BUF_SIZE) {
                toRead = CMD_FILE_BUF_SIZE;
            }
            ULONG read = HalpFatRead(startCluster, fileSize, totalRead, toRead, buf);
            if (read == 0) {
                break;
            }
            HalpDisplayPrintf((const CHAR *)buf);
            totalRead += read;
        }

        HalpDisplayPrintf("\r\n");
        return STATUS_SUCCESS;
    }

    /* ps - list running processes. */
    if (CmdLen == 2 && Cmd[0] == 'p' && Cmd[1] == 's') {
        PLIST_ENTRY ListHead = PsProcessList();
        PEPROCESS Entry;
        ULONG count = PsProcessCountGet();
        HalpDisplayPrintf("Active processes: ");
        HalpDisplayPrintf("%lu\r\n", count);
        ForEachListEntry(ListHead, Entry, EPROCESS, ProcessListEntry) {
            HalpDisplayPrintf("  PID %lu  %s  threads=%lu\r\n",
                              (ULONG)(ULONG_PTR)Entry->UniqueProcessId,
                              Entry->ImageFileName,
                              Entry->ThreadList.ThreadCount);
        }
        return STATUS_SUCCESS;
    }

    /* kill <pid> - terminate a process (stub: removes from list). */
    if (CmdLen == 4 && Cmd[0] == 'k' && Cmd[1] == 'i' && Cmd[2] == 'l' &&
        Cmd[3] == 'l') {
        PCHAR Arg = Cmd + CmdLen;
        while (*Arg == ' ' || *Arg == '\t') {
            Arg++;
        }
        if (*Arg == '\0') {
            HalpDisplayPrintf("Usage: kill <pid>\r\n");
            return STATUS_SUCCESS;
        }
        HalpDisplayPrintf("kill is not implemented in this port.\r\n");
        return STATUS_NOT_IMPLEMENTED;
    }

    /* Unknown command - CMD-style error. */
    HalpDisplayPrintf("'");
    for (i = 0; i < CmdLen; i++) {
        HalpPutChar(Cmd[i]);
    }
    HalpDisplayPrintf("' is not recognized as an internal or external command,\r\n");
    HalpDisplayPrintf("operable program or batch file.\r\n");
    return STATUS_NOT_IMPLEMENTED;
}
