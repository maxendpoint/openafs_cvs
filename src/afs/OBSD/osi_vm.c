/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */


/* osi_vm.c implements:
 *
 * osi_VM_FlushVCache(avc, slept)
 * osi_ubc_flush_dirty_and_wait(vp, flags)
 * osi_VM_StoreAllSegments(avc)
 * osi_VM_TryToSmush(avc, acred, sync)
 * osi_VM_FlushPages(avc, credp)
 * osi_VM_Truncate(avc, alen, acred)
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID("$Header: /cvs/openafs/src/afs/OBSD/osi_vm.c,v 1.1 2002/10/28 21:28:27 rees Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afs/afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"  /* statistics */
/* #include <vm/vm_ubc.h> */
#include <limits.h>
#include <float.h>

/* Try to discard pages, in order to recycle a vcache entry.
 *
 * We also make some sanity checks:  ref count, open count, held locks.
 *
 * We also do some non-VM-related chores, such as releasing the cred pointer
 * (for AIX and Solaris) and releasing the gnode (for AIX).
 *
 * Locking:  afs_xvcache lock is held.  If it is dropped and re-acquired,
 *   *slept should be set to warn the caller.
 *
 * Formerly, afs_xvcache was dropped and re-acquired for Solaris, but now it
 * is not dropped and re-acquired for any platform.  It may be that *slept is
 * therefore obsolescent.
 *
 * OSF/1 Locking:  VN_LOCK has been called.
 */
int osi_VM_FlushVCache(struct vcache *avc, int *slept)
{
    return 0;
}

/* Try to store pages to cache, in order to store a file back to the server.
 *
 * Locking:  the vcache entry's lock is held.  It will usually be dropped and
 * re-obtained.
 */
void osi_VM_StoreAllSegments(struct vcache *avc)
{
}

/* Try to invalidate pages, for "fs flush" or "fs flushv"; or
 * try to free pages, when deleting a file.
 *
 * Locking:  the vcache entry's lock is held.  It may be dropped and
 * re-obtained.
 *
 * Since we drop and re-obtain the lock, we can't guarantee that there won't
 * be some pages around when we return, newly created by concurrent activity.
 */
void osi_VM_TryToSmush(struct vcache *avc, struct AFS_UCRED *acred, int sync)
{
}

/* Purge VM for a file when its callback is revoked.
 *
 * Locking:  No lock is held, not even the global lock.
 */
void osi_VM_FlushPages(struct vcache *avc, struct AFS_UCRED *credp)
{
}

/* Purge pages beyond end-of-file, when truncating a file.
 *
 * Locking:  no lock is held, not even the global lock.
 * activeV is raised.  This is supposed to block pageins, but at present
 * it only works on Solaris.
 */
void osi_VM_Truncate(struct vcache *avc, int alen, struct AFS_UCRED *acred)
{
}
