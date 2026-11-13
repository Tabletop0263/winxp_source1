/*++

Copyright (c) Microsoft Corporation

Module Name:

    obcreate.c

Abstract:

    Minimal object creation and memory primitives for the PowerPC
    (Broadway/G3) port of Windows XP (NT 5.1).  Self-contained: only
    depends on ppctx.h and wxptypes.h (no nt.h).  Implements:

      - WxpMemAlloc / WxpMemFree - bump allocator backed by a static pool.
      - WxpMemCopy / WxpMemSet - memory helpers declared in wxptypes.h.
      - ObCreateObject / ObReferenceObject / ObDereferenceObject - minimal
        object management stubs.

Author:

    (port)

Environment:

    Kernel mode only.

--*/

#include "ppctx.h"
#include "wxptypes.h"

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

/*--------------------------------------------------------------------------
    __memcpy_chk - memcpy with buffer-size checking.  Freestanding builds
    with -O2 may emit calls to this from cmregistry.c.  Provide a simple
    shim (no checking needed for the port's bump pool).
--------------------------------------------------------------------------*/
void *
__memcpy_chk(void *dest, const void *src, ULONG n, ULONG destlen)
{
    UNREFERENCED_PARAMETER(destlen);
    WxpMemCopy(dest, src, n);
    return dest;
}

/*--------------------------------------------------------------------------
    WxpMemoryPool - static backing store for the bump allocator.
    64 KiB is plenty for the minimal port's early-boot object usage.
--------------------------------------------------------------------------*/
#define WXP_MEMORY_POOL_SIZE 0x10000UL   /* 64 KiB */
#define WXP_MEMORY_ALIGN     4UL

static UCHAR WxpMemoryPool[WXP_MEMORY_POOL_SIZE];
static ULONG WxpMemoryPoolUsed = 0;

/*--------------------------------------------------------------------------
    WxpMemAlloc - allocate from the bump pool.

    Returns NULL if the pool is exhausted.  Free is a no-op (bump
    allocator); objects live until the system reboots, which is acceptable
    for this minimal port.
--------------------------------------------------------------------------*/
PVOID
WxpMemAlloc(
    IN ULONG Size
    )
{
    ULONG aligned;
    PVOID block;

    if (Size == 0) {
        Size = 1;
    }

    /* Round up to alignment. */
    aligned = (Size + (WXP_MEMORY_ALIGN - 1)) & ~(WXP_MEMORY_ALIGN - 1);

    if (WxpMemoryPoolUsed + aligned > WXP_MEMORY_POOL_SIZE) {
        return NULL;
    }

    block = (PVOID)&WxpMemoryPool[WxpMemoryPoolUsed];
    WxpMemoryPoolUsed += aligned;
    return block;
}

/*--------------------------------------------------------------------------
    WxpMemFree - release a block back to the pool.

    No-op for the bump allocator (block is reclaimed on reboot).
--------------------------------------------------------------------------*/
VOID
WxpMemFree(
    IN PVOID Block
    )
{
    UNREFERENCED_PARAMETER(Block);
    return;
}

/*--------------------------------------------------------------------------
    WxpMemCopy - copy Length bytes from Source to Destination.
--------------------------------------------------------------------------*/
VOID
WxpMemCopy(
    OUT PVOID Destination,
    IN  const VOID *Source,
    IN  ULONG Length
    )
{
    PUCHAR dst = (PUCHAR)Destination;
    const PUCHAR src = (const PUCHAR)Source;
    ULONG i;

    for (i = 0; i < Length; i++) {
        dst[i] = src[i];
    }
}

/*--------------------------------------------------------------------------
    WxpMemSet - fill Length bytes of Destination with FillByte.
--------------------------------------------------------------------------*/
VOID
WxpMemSet(
    OUT PVOID Destination,
    IN  UCHAR FillByte,
    IN  ULONG Length
    )
{
    PUCHAR dst = (PUCHAR)Destination;
    ULONG i;

    for (i = 0; i < Length; i++) {
        dst[i] = FillByte;
    }
}

/*--------------------------------------------------------------------------
    Minimal object header shared by all objects created via ObCreateObject.
--------------------------------------------------------------------------*/
typedef struct _WXP_OBJECT_HEADER {
    ULONG           Type;          /* OB_TYPE_* */
    ULONG           ReferenceCount;
    BOOLEAN         Deleted;
    PVOID           Body;          /* object-specific body */
} WXP_OBJECT_HEADER, *PWXP_OBJECT_HEADER;

/*--------------------------------------------------------------------------
    ObCreateObject - allocate an object of the given type and body size.

    Returns a pointer to the object body (the caller-usable portion), with
    the header hidden immediately before it.  Reference count starts at 1.
--------------------------------------------------------------------------*/
PVOID
ObCreateObject(
    IN ULONG ObjectType,
    IN ULONG BodySize,
    OUT PVOID *ObjectHeader OPTIONAL
    )
{
    PWXP_OBJECT_HEADER Header;
    PVOID Body;

    UNREFERENCED_PARAMETER(ObjectHeader);

    Header = (PWXP_OBJECT_HEADER)WxpMemAlloc(sizeof(WXP_OBJECT_HEADER) + BodySize);
    if (Header == NULL) {
        return NULL;
    }

    Header->Type = ObjectType;
    Header->ReferenceCount = 1;
    Header->Deleted = FALSE;
    Header->Body = NULL;

    Body = (PVOID)(Header + 1);
    Header->Body = Body;

    if (ObjectHeader != NULL) {
        *ObjectHeader = Header;
    }

    return Body;
}

/*--------------------------------------------------------------------------
    ObReferenceObject - increment an object's reference count.
--------------------------------------------------------------------------*/
VOID
ObReferenceObject(
    IN PVOID Object
    )
{
    PWXP_OBJECT_HEADER Header;

    UNREFERENCED_PARAMETER(Object);

    /* Recover the header from the body pointer (header sits right before
     * the body in the allocation). */
    Header = ((PWXP_OBJECT_HEADER)Object) - 1;
    Header->ReferenceCount++;
}

/*--------------------------------------------------------------------------
    ObDereferenceObject - decrement an object's reference count; if it
    reaches zero, mark it deleted.
--------------------------------------------------------------------------*/
VOID
ObDereferenceObject(
    IN PVOID Object
    )
{
    PWXP_OBJECT_HEADER Header;

    UNREFERENCED_PARAMETER(Object);

    Header = ((PWXP_OBJECT_HEADER)Object) - 1;
    if (Header->ReferenceCount > 0) {
        Header->ReferenceCount--;
    }

    if (Header->ReferenceCount == 0) {
        Header->Deleted = TRUE;
    }
}
