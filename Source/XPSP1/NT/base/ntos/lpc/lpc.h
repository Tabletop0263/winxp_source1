/*++

Copyright (c) Microsoft Corporation

Module Name:

    lpc.h

Abstract:

    Minimal shared type definitions for the LPC subsystem in the PowerPC
    (Broadway/G3) port of Windows XP (NT 5.1).  Self-contained: only
    depends on wxptypes.h.

Author:

    (port)

Environment:

    Kernel mode only.

--*/

#ifndef _LPC_H_
#define _LPC_H_

#include "wxptypes.h"

/*--------------------------------------------------------------------------
    LPC port object.  A port is an object registered in the namespace
    (via obname.c) with an associated mailbox slot.
--------------------------------------------------------------------------*/
typedef struct _LPC_PORT {
    LIST_ENTRY          Link;          /* linkage in LpcpPortList */
    UNICODE_STRING      PortName;      /* port name (copied) */
    HANDLE              PortHandle;    /* handle returned to creator */
    BOOLEAN             Connected;     /* whether a client has connected */
    /* Single-message mailbox (synchronous LPC). */
    BOOLEAN             MessagePending;
    UCHAR               MessageBuffer[256];
    ULONG               MessageLength;
    /* Reply event - stubbed (no real event object yet). */
    BOOLEAN             ReplyReady;
} LPC_PORT, *PLPC_PORT;

#endif /* _LPC_H_ */
