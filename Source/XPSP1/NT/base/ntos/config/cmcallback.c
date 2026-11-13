/*++

Copyright (c) Microsoft Corporation

Module Name:

    cmcallback.c

Abstract:

    Registry callback registration/invocation for the PowerPC config
    manager.  Callbacks are notified on create/open/delete/enumerate/query
    operations via the CmCallCallbacks dispatcher.

Environment:

    Kernel mode only.

--*/

#include "cm.h"

#include <string.h>

/*----------------------------------------------------------------------------
  Registered callback list (fixed capacity, no dynamic allocator).
----------------------------------------------------------------------------*/
#define CM_MAX_CALLBACKS  16

typedef struct _CM_CALLBACK_SLOT {
    LIST_ENTRY    List;
    PCM_CALLBACK  Function;
    PVOID         Context;
    LARGE_INTEGER Cookie;
    BOOLEAN       InUse;
} CM_CALLBACK_SLOT, *PCM_CALLBACK_SLOT;

static CM_CALLBACK_SLOT CmCallbackSlots[CM_MAX_CALLBACKS];
static LIST_ENTRY       CmCallbackListHead;
static LARGE_INTEGER    CmNextCookie;
static BOOLEAN          CmCallbackListInitialized = FALSE;

/*----------------------------------------------------------------------------
  CmRegisterCallback - register a registry change notification callback.
----------------------------------------------------------------------------*/
NTSTATUS
CmRegisterCallback(
    IN PCM_CALLBACK Function,
    IN PVOID Context,
    OUT PLARGE_INTEGER Cookie
    )
{
    ULONG i;

    if (Function == NULL || Cookie == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!CmCallbackListInitialized) {
        InitializeListHead(&CmCallbackListHead);
        CmNextCookie.QuadPart = 0;
        CmCallbackListInitialized = TRUE;
    }

    /* Find a free slot. */
    for (i = 0; i < CM_MAX_CALLBACKS; i++) {
        if (!CmCallbackSlots[i].InUse) {
            break;
        }
    }
    if (i >= CM_MAX_CALLBACKS) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    CmCallbackSlots[i].InUse = TRUE;
    CmCallbackSlots[i].Function = Function;
    CmCallbackSlots[i].Context = Context;

    /* Generate a monotonic cookie. */
    CmNextCookie.QuadPart++;
    CmCallbackSlots[i].Cookie = CmNextCookie;

    InitializeListHead(&CmCallbackSlots[i].List);
    InsertTailList(&CmCallbackListHead, &CmCallbackSlots[i].List);

    *Cookie = CmCallbackSlots[i].Cookie;
    return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------
  CmUnRegisterCallback - remove a registered callback by cookie.
----------------------------------------------------------------------------*/
NTSTATUS
CmUnRegisterCallback(
    IN LARGE_INTEGER Cookie
    )
{
    ULONG i;

    if (!CmCallbackListInitialized) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    for (i = 0; i < CM_MAX_CALLBACKS; i++) {
        if (CmCallbackSlots[i].InUse &&
            CmCallbackSlots[i].Cookie.QuadPart == Cookie.QuadPart) {
            CmCallbackSlots[i].InUse = FALSE;
            CmCallbackSlots[i].Function = NULL;
            CmCallbackSlots[i].Context = NULL;
            RemoveEntryList(&CmCallbackSlots[i].List);
            return STATUS_SUCCESS;
        }
    }
    return STATUS_OBJECT_NAME_NOT_FOUND;
}

/*----------------------------------------------------------------------------
  CmCallCallbacks - invoke all registered callbacks for a notify event.
----------------------------------------------------------------------------*/
VOID
CmCallCallbacks(
    IN REG_NOTIFY_CLASS Type,
    IN PVOID Argument
    )
{
    PLIST_ENTRY walk;

    if (!CmCallbackListInitialized) {
        return;
    }

    walk = CmCallbackListHead.Flink;
    while (walk != &CmCallbackListHead) {
        PCM_CALLBACK_SLOT slot;
        slot = CONTAINING_RECORD(walk, CM_CALLBACK_SLOT, List);
        if (slot->Function != NULL) {
            slot->Function(Type, Argument);
        }
        walk = walk->Flink;
    }
}
