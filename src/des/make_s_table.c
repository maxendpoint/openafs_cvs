/*
 * Copyright 1985, 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please
 * see the file <mit-cpyright.h>.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/des/make_s_table.c,v 1.7 2003/07/15 23:15:00 shadow Exp $");

#include <mit-cpyright.h>
#include <stdio.h>
#include <des.h>
#include "des_internal.h"
#include "des_prototypes.h"

#define WANT_S_TABLE
#include "tables.h"

char temp[8][64];
int des_debug;

void
gen(FILE * stream)
{
    register afs_uint32 i, j, k, l, m, n;

    /* rearrange the S table entries, and adjust for host bit order */

    fprintf(stream, "static unsigned char const S_adj[8][64] = {");
    fprintf(stream, "    /* adjusted */\n");

    for (i = 0; i <= 7; i++) {
	for (j = 0; j <= 63; j++) {
	    /*
	     * figure out which one to put in the new S[i][j]
	     *
	     * start by assuming the value of the input bits is "j" in
	     * host order, then figure out what it means in standard
	     * form.
	     */
	    k = swap_six_bits_to_ansi(j);
	    /* figure out the index for k */
	    l = (((k >> 5) & 01) << 5)
		+ ((k & 01) << 4) + ((k >> 1) & 0xf);
	    m = S[i][l];
	    /* restore in host order */
	    n = swap_four_bits_to_ansi(m);
	    if (des_debug)
		fprintf(stderr,
			"i = %ld, j = %ld, k = %ld, l = %ld, m = %ld, n = %ld\n",
			(long)i, (long)j, (long)k, (long)l, (long)m, (long)n);
	    temp[i][j] = n;
	}
    }

    for (i = 0; i <= 7; i++) {
	fprintf(stream, "\n{ ");
	k = 0;
	for (j = 0; j <= 3; j++) {
	    fprintf(stream, "\n");
	    for (m = 0; m <= 15; m++) {
		fprintf(stream, "%2d", temp[i][k]);
		if (k == 63) {
		    fprintf(stream, "\n}");
		}
		if ((k++ != 63) || (i != 7)) {
		    fprintf(stream, ", ");
		}
	    }
	}
    }

    fprintf(stream, "\n};\n");
}
