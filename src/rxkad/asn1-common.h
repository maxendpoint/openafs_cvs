/* $Id: asn1-common.h,v 1.2 2002/10/28 01:18:39 jhutz Exp $ */

#include <stddef.h>
#include <time.h>

#ifndef __asn1_common_definitions__
#define __asn1_common_definitions__

typedef struct octet_string {
    size_t length;
    void *data;
} octet_string;

typedef char *general_string;

typedef struct oid {
    size_t length;
    unsigned *components;
} oid;

#endif
