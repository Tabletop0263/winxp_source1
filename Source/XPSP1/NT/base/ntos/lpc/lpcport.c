/*++

Copyright (c) Microsoft Corporation

Module Name:

    lpcport.c

Abstract:

    Minimal LPC port object management for the PowerPC (Broadway/G3) port
    of Windows XP (NT 5.1).  Self-contained: only depends on ppctx.h and
    wxptypes.h (no nt.h).  Implements:

      - LpcpClosePort - close/dereference an LPC port object.
      - LpcpConnectPort - connect a client to a server port.
      - LpcpAcceptConnectPort - accept a client connection on a server port.

    These operate on the LPC_PORT bodies created by LpcCreatePort (lpc.c).

Author:

    (port)

Environment:

    Kernel mode only.

--*/

#include "ppctx.h"
#include "wxptypes.h"
#include "lpc.h"

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

/*--------------------------------------------------------------------------
    WxpMemFree - implemented in obcreate.c.
--------------------------------------------------------------------------*/
VOID
WxpMemFree(
    IN PVOID Block
    );

/*--------------------------------------------------------------------------
    LpcpClosePort - close an LPC port handle.

    Marks the port disconnected, removes it from the port list, and frees
    its memory (including the port name buffer).
--------------------------------------------------------------------------*/
VOID
LpcpClosePort(
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG ProcessHandleCount,
    IN ULONG SystemHandleCount
    )
{
    PLPC_PORT Port = (PLPC_PORT)Object;

    UNREFERENCED_PARAMETER(Process);
    UNREFERENCED_PARAMETER(GrantedAccess);
    UNREFERENCED_PARAMETER(ProcessHandleCount);
    UNREFERENCED_PARAMETER(SystemHandleCount);

    if (Port == NULL) {
        return;
    }

    Port->Connected = FALSE;

    /* Free the port name buffer if allocated. */
    if (Port->PortName.Buffer != NULL) {
        WxpMemFree(Port->PortName.Buffer);
        Port->PortName.Buffer = NULL;
    }

    /* Remove from the global port list if linked. */
    if (Port->Link.Flink != NULL || Port->Link.Blink != NULL) {
        RemoveEntryList(&Port->Link);
    }

    WxpMemFree(Port);
}

/*--------------------------------------------------------------------------
    LpcpConnectPort - connect a client to a server port.

    Minimal implementation: marks the target port connected and returns
    STATUS_SUCCESS.  No real connection message exchange yet.
--------------------------------------------------------------------------*/
NTSTATUS
LpcpConnectPort(
    IN  HANDLE ClientPortHandle OPTIONAL,
    IN  POBJECT_ATTRIBUTES ConnectionPortAttributes,
    IN  PVOID ClientProcess OPTIONAL,
    IN  PVOID ConnectionInformation OPTIONAL,
    IN  ULONG ConnectionInformationLength,
    OUT PVOID *ClientConnectionPort OPTIONAL
    )
{
    UNREFERENCED_PARAMETER(ClientPortHandle);
    UNREFERENCED_PARAMETER(ClientProcess);
    UNREFERENCED_PARAMETER(ConnectionInformation);
    UNREFERENCED_PARAMETER(ConnectionInformationLength);
    UNREFERENCED_PARAMETER(ClientConnectionPort);

    if (ConnectionPortAttributes == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    LpcpAcceptConnectPort - accept a client connection on a server port.

    Minimal implementation: returns STATUS_SUCCESS with no client context.
--------------------------------------------------------------------------*/
NTSTATUS
LpcpAcceptConnectPort(
    IN  HANDLE PortHandle,
    IN  PVOID ClientConnectionPort OPTIONAL,
    OUT PVOID *ClientProcess OPTIONAL
    )
{
    UNREFERENCED_PARAMETER(PortHandle);
    UNREFERENCED_PARAMETER(ClientConnectionPort);
    UNREFERENCED_PARAMETER(ClientProcess);

    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    LpcpSendFree - free an LPC message buffer (no-op for bump allocator).
--------------------------------------------------------------------------*/
VOID
LpcpSendFree(
    IN PVOID Message
    )
{
    UNREFERENCED_PARAMETER(Message);
    return;
}
