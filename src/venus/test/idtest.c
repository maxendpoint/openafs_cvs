/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID("$Header: /cvs/openafs/src/venus/test/idtest.c,v 1.4 2001/07/12 19:59:28 shadow Exp $");

main(argc, argv) {
    int uid;

    uid = geteuid();
    printf("My effective UID is %d, ", uid);
    uid = getuid();
    printf("and my real UID is %d.\n", uid);
    exit(0);
}

