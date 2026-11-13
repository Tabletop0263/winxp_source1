/*++

Copyright (c) Microsoft Corporation

Module Name:

    lpc.c

Abstract:

    Minimal Local Inter-Process Communication (LPC) for the PowerPC
    (Broadway/G3) port of Windows XP (NT 5.1).  Self-contained: only
    depends on ppctx.h and wxptypes.h (no nt.h).  Implements:

      - LpcInitialize - initialize the LPC subsystem (port list).
      - LpcCreatePort - create a named LPC port object.
      - NtSendWaitReplyMessage - send a message and wait for a reply.
      - NtReplyWaitReceiveMessage - reply to a client and wait for the next
        message.

    This is a synchronous, single-message-slot implementation suitable for
    the port's early-boot shell (console <-> command dispatcher over LPC).
    Messages are copied into a per-port mailbox slot; there is no full
    LPC message queue or thread waiting yet.

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

/*--------------------------------------------------------------------------
    LPC port object (defined in lpc.h).
--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
    LpcpPortList - global list of created LPC ports.
--------------------------------------------------------------------------*/
static LIST_ENTRY LpcpPortList;
static ULONG LpcpInitialized = FALSE;

/*--------------------------------------------------------------------------
    LpcInitialize - initialize the LPC subsystem.

    Called once during boot (from KeInitializeKernelPpc or the shell
    startup thread).  Initializes the port list head.
--------------------------------------------------------------------------*/
NTSTATUS
LpcInitialize(
    VOID
    )
{
    if (LpcpInitialized) {
        return STATUS_SUCCESS;
    }

    InitializeListHead(&LpcpPortList);
    LpcpInitialized = TRUE;

    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    LpcCreatePort - create a named LPC port.

    Allocates an LPC_PORT body, copies the port name, inserts it into the
    global port list, and returns a handle.
--------------------------------------------------------------------------*/
NTSTATUS
LpcCreatePort(
    OUT PHANDLE PortHandle,
    IN  POBJECT_ATTRIBUTES ObjectAttributes,
    IN  ULONG MaxConnectionInfoLength,
    IN  ULONG MaxMessageLength,
    IN  ULONG MaxPoolUsage,
    IN  BOOLEAN Waitable
    )
{
    PLPC_PORT Port;
    PLIST_ENTRY Entry;

    UNREFERENCED_PARAMETER(MaxConnectionInfoLength);
    UNREFERENCED_PARAMETER(MaxMessageLength);
    UNREFERENCED_PARAMETER(MaxPoolUsage);
    UNREFERENCED_PARAMETER(Waitable);

    if (PortHandle == NULL || ObjectAttributes == NULL ||
        ObjectAttributes->ObjectName == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!LpcpInitialized) {
        NTSTATUS Status = LpcInitialize();
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
    }

    /* Reject duplicate port names. */
    Entry = LpcpPortList.Flink;
    while (Entry != &LpcpPortList) {
        PLPC_PORT Existing;
        Existing = (PLPC_PORT)CONTAINING_RECORD(Entry, LPC_PORT, Link);
        Entry = Entry->Flink;

        if (Existing->PortName.Length == ObjectAttributes->ObjectName->Length) {
            ULONG i;
            BOOLEAN match = TRUE;
            for (i = 0; i < (ObjectAttributes->ObjectName->Length / sizeof(WCHAR)); i++) {
                if (Existing->PortName.Buffer[i] != ObjectAttributes->ObjectName->Buffer[i]) {
                    match = FALSE;
                    break;
                }
            }
            if (match) {
                return STATUS_OBJECT_NAME_COLLISION;
            }
        }
    }

    Port = (PLPC_PORT)WxpMemAlloc(sizeof(LPC_PORT));
    if (Port == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    WxpZeroMemory(Port, sizeof(LPC_PORT));

    Port->PortName.Buffer = (PWSTR)WxpMemAlloc(ObjectAttributes->ObjectName->Length + sizeof(WCHAR));
    if (Port->PortName.Buffer == NULL) {
        WxpMemFree(Port);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    WxpMemCopy(Port->PortName.Buffer, ObjectAttributes->ObjectName->Buffer,
               ObjectAttributes->ObjectName->Length);
    Port->PortName.Buffer[ObjectAttributes->ObjectName->Length / sizeof(WCHAR)] = L'\0';
    Port->PortName.Length = ObjectAttributes->ObjectName->Length;
    Port->PortName.MaximumLength = ObjectAttributes->ObjectName->Length + sizeof(WCHAR);

    Port->PortHandle = (HANDLE)Port;
    Port->Connected = FALSE;
    Port->MessagePending = FALSE;
    Port->MessageLength = 0;
    Port->ReplyReady = FALSE;

    if (LpcpPortList.Flink == NULL && LpcpPortList.Blink == NULL) {
        InitializeListHead(&LpcpPortList);
    }
    InsertTailList(&LpcpPortList, &Port->Link);

    *PortHandle = Port->PortHandle;
    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    NtSendWaitReplyMessage - send a message to a port and wait for a reply.

    Minimal synchronous implementation: copies the request message into the
    target port's mailbox, sets MessagePending, and returns STATUS_SUCCESS
    (the reply is delivered via NtReplyWaitReceiveMessage on the server
    side).  No real blocking/waiting yet - this port has no scheduler.
--------------------------------------------------------------------------*/
NTSTATUS
NtSendWaitReplyMessage(
    IN  HANDLE PortHandle,
    IN  PVOID  RequestMessage,
    IN  ULONG  RequestMessageLength,
    OUT PVOID  ReplyMessage OPTIONAL,
    IN  ULONG  ReplyMessageLength,
    OUT PULONG ReplyLength OPTIONAL
    )
{
    PLPC_PORT Port = (PLPC_PORT)PortHandle;

    UNREFERENCED_PARAMETER(ReplyMessage);
    UNREFERENCED_PARAMETER(ReplyMessageLength);
    UNREFERENCED_PARAMETER(ReplyLength);

    if (Port == NULL || RequestMessage == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (RequestMessageLength > sizeof(Port->MessageBuffer)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    WxpMemCopy(Port->MessageBuffer, RequestMessage, RequestMessageLength);
    Port->MessageLength = RequestMessageLength;
    Port->MessagePending = TRUE;

    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    NtReplyWaitReceiveMessage - server-side: reply to the last client and
    wait for the next message on a port.

    Minimal implementation: clears the pending-message flag (reply consumed)
    and returns STATUS_SUCCESS with no new message (no scheduler, so no
    blocking wait).  Callers should poll.
--------------------------------------------------------------------------*/
NTSTATUS
NtReplyWaitReceiveMessage(
    IN  HANDLE PortHandle,
    OUT PVOID  ReplyMessage OPTIONAL,
    IN  ULONG  ReplyMessageLength,
    OUT PVOID  ReceiveMessage OPTIONAL,
    IN  ULONG  ReceiveMessageLength,
    OUT PULONG ReceiveLength OPTIONAL
    )
{
    PLPC_PORT Port = (PLPC_PORT)PortHandle;

    UNREFERENCED_PARAMETER(ReplyMessage);
    UNREFERENCED_PARAMETER(ReplyMessageLength);
    UNREFERENCED_PARAMETER(ReceiveMessage);
    UNREFERENCED_PARAMETER(ReceiveMessageLength);
    UNREFERENCED_PARAMETER(ReceiveLength);

    if (Port == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Consume the pending message (reply delivered). */
    Port->MessagePending = FALSE;
    Port->MessageLength = 0;

    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
    LpcpSendFree is defined in lpcport.c (message pool free).
--------------------------------------------------------------------------*/
VOID
LpcpSendFreePlaceholder(VOID)
{
    return;
}
