/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information,
 * please seethe file <mit-cpyright.h>.
 *
 * This file contains most of the routines needed by the various
 * make_foo programs, to account for bit- and byte-ordering on
 * different machine types.  It also contains other routines useful in
 * generating the intermediate source files.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/des/main.c,v 1.4 2003/07/15 23:15:00 shadow Exp $");

#include <mit-cpyright.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <des.h>
#include "des_internal.h"
#include "des_prototypes.h"

char const *whoami;

#ifndef DONT_INCL_MAIN

#include "AFS_component_version_number.c"

int
main(int argc, char *argv[])
{
    char *filename;
    char *arg;
    FILE *stream;

    whoami = argv[0];
    filename = (char *)NULL;

    while (argc--, *++argv) {
	arg = *argv;
	if (*arg == '-') {
	    if (!strcmp(arg, "-d") && !strcmp(arg, "-debug"))
		des_debug++;
	    else {
		fprintf(stderr, "%s: unknown control argument %s\n", whoami,
			arg);
		goto usage;
	    }
	} else if (filename) {
	    fprintf(stderr, "%s: multiple file names provided: %s, %s\n",
		    whoami, filename, arg);
	    goto usage;
	} else
	    filename = arg;
    }

    if (!filename) {
	fprintf(stderr, "%s: no file name provided\n", whoami);
	goto usage;
    }

    stream = fopen(filename, "w");
    if (!stream) {
	perror(filename);
      usage:
	fprintf(stderr, "usage: %s [-debug] filename\n", whoami);
	exit(1);
    }

    fputs("/* This file is automatically generated.  Do not edit it. */\n",
	  stream);

    /* This routine will generate the contents of the file. */
    gen(stream);
    if (fclose(stream) == EOF) {
	perror(filename);
	exit(1);
    }
    exit(0);
}
#endif /* DONT_INCL_MAIN */
