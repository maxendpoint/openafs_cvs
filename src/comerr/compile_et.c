/*
 *
 * Copyright 1986, 1987, 1988
 * by MIT Student Information Processing Board.
 *
 * For copyright info, see "mit-sipb-cr.h".
 *
 */

#undef MEMORYLEAK
#include <afsconfig.h>
#include <afs/param.h>

RCSID("$Header: /cvs/openafs/src/comerr/compile_et.c,v 1.6 2001/07/12 19:58:32 shadow Exp $");

#include <stdio.h>
#include <stdlib.h>

#ifndef AFS_NT40_ENV
#include <sys/file.h>
#include <sys/param.h>
#endif

#include <errno.h>
#include <string.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include "mit-sipb-cr.h"
#include "compiler.h"

#ifndef lint
static const char copyright[] =
    "Copyright 1987,1988 by MIT Student Information Processing Board";
#endif

extern char *gensym();
extern char *current_token;
extern int table_number, current;
char buffer[BUFSIZ];
char *table_name = (char *)NULL;
FILE *hfile, *cfile, *msfile;
int version = 1;
int use_msf = 0;

/* lex stuff */
extern FILE *yyin;
extern FILE *yyout;
extern int yylineno;

char * xmalloc (size) unsigned int size; {
    char * p = malloc (size);
    if (!p) {
	perror (whoami);
	exit (1);
    }
    return p;
}

static int check_arg (str_list, arg) char const *const *str_list, *arg; {
    while (*str_list)
	if (!strcmp(arg, *str_list++))
	    return 1;
    return 0;
}

static const char *const debug_args[] = {
    "d",
    "debug",
    0,
};

static const char *const lang_args[] = {
    "lang",
    "language",
    0,
};

static const char *const language_names[] = {
    "C",
    "K&R C",
    "C++",
    0,
};

static const char * const c_src_prolog[] = {
    "#include <afs/param.h>\n",
    "#include <afs/error_table.h>\n",
    "static const char * const text[] = {\n",
    0,
};

static const char * const krc_src_prolog[] = {
    "#ifdef __STDC__\n",
    "#define NOARGS void\n",
    "#else\n",
    "#define NOARGS\n",
    "#define const\n",
    "#endif\n\n",
    "#include <afs/param.h>\n",
    "#include <afs/error_table.h>\n",
    "static const char * const text[] = {\n",
    0,
};

static const char warning[] =
    "/*\n * %s:\n * This file is automatically generated; please do not edit it.\n */\n";

static const char msf_warning[] =
    "$ \n$ %s:\n$ This file is automatically generated; please do not edit it.\n$ \n$set 1\n";

/* pathnames */
char c_file[MAXPATHLEN];	/* output file */
char h_file[MAXPATHLEN];	/* output */
char msf_file[MAXPATHLEN];

static void usage () {
    fprintf (stderr, "%s: usage: %s ERROR_TABLE [-debug] [-language LANG] [-h INCLUDE] [-v version]\n",
	     whoami, whoami);
    exit (1);
}

static void dup_err (type, one, two) char const *type, *one, *two; {
    fprintf (stderr, "%s: multiple %s specified: `%s' and `%s'\n",
	     whoami, type, one, two);
    usage ();
}

#include "AFS_component_version_number.c"

int main (argc, argv) int argc; char **argv; {
    char *p, *ename;
    char const * const *cpp;
    int got_language = 0;
    char *got_include = 0;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a 
     * crash (i.e. core is generated) we can include the user's data section 
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;
    
    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    /* argument parsing */
    debug = 0;
    filename = 0;
    whoami = argv[0];
    p = strrchr (whoami, '/');
    if (p)
	whoami = p+1;
    while (argv++, --argc) {
	char *arg = *argv;
	if (arg[0] != '-') {
	    if (filename)
		dup_err ("filenames", filename, arg);
	    filename = arg;
	}
	else {
	    arg++;
	    if (check_arg (debug_args, arg))
		debug++;
	    else if (check_arg (lang_args, arg)) {
		got_language++;
		arg = *++argv, argc--;
		if (!arg)
		    usage ();
		if (language)
		    dup_err ("languanges", language_names[(int)language], arg);
#define check_lang(x,v) else if (!strcasecmp(arg,x)) language = v
		check_lang ("c", lang_C);
		check_lang ("ansi_c", lang_C);
		check_lang ("ansi-c", lang_C);
		check_lang ("krc", lang_KRC);
		check_lang ("kr_c", lang_KRC);
		check_lang ("kr-c", lang_KRC);
		check_lang ("k&r-c", lang_KRC);
		check_lang ("k&r_c", lang_KRC);
		check_lang ("c++", lang_CPP);
		check_lang ("cplusplus", lang_CPP);
		check_lang ("c-plus-plus", lang_CPP);
#undef check_lang
		else {
		    fprintf (stderr, "%s: unknown language name `%s'\n",
			     whoami, arg);
		    fprintf (stderr, "\tpick one of: C K&R-C\n");
		    exit (1);
		}
	    }
	    else if (strcmp (arg, "h") == 0) {
		arg = *++argv, argc--;
		if (!arg) usage ();
		got_include = arg;
	    }
	    else if (strcmp (arg, "v") == 0) {
		arg = *++argv, argc--;
		version = atoi(arg);
		if (version != 1 && version != 2) {
		    fprintf (stderr, "%s: unknown control argument -`%s'\n",
			     whoami, arg);
		    usage ();
		    exit(1);
		}
		if (version == 2) use_msf = 1;
	    }
	    else {
		fprintf (stderr, "%s: unknown control argument -`%s'\n",
			 whoami, arg);
		usage ();
	    }
	}
    }
    if (!filename)
	usage ();
    if (!got_language)
	language = lang_C;
    else if (language == lang_CPP) {
	fprintf (stderr, "%s: Sorry, C++ support is not yet finished.\n",
		 whoami);
	exit (1);
    }


    p = strrchr(filename, '/');
    if (p == (char *)NULL)
	p = filename;
    else
	p++;
    ename = xmalloc (strlen(p) + 5);
    strcpy (ename, p);

    /* Now, flush .et suffix if it exists.   */
    p = strrchr(ename, '.');
    if (p != NULL) {
	if (strcmp (p, ".et") == 0)
	    *p = 0;
    }

    if (use_msf) {
	sprintf(msf_file, "%s.msf", ename);
    }  else {
	sprintf(c_file, "%s.c", ename);
    }
    if (got_include) {
        sprintf(h_file, "%s.h", got_include);
    } else {
        sprintf(h_file, "%s.h", ename);
    }
    p = strrchr(filename, '.');
    if (p == NULL)
    {
        p = xmalloc (strlen(filename) + 4);
        sprintf(p, "%s.et", filename);
        filename = p;
    }

    yyin = fopen(filename, "r");
    if (!yyin) {
	perror(filename);
	exit(1);
    }

    /* on NT, yyout is not initialized to stdout */
    if (!yyout) {
	yyout = stdout;
    }

    hfile = fopen(h_file, "w");
    if (hfile == (FILE *)NULL) {
	perror(h_file);
	exit(1);
    }
    fprintf (hfile, warning, h_file);
    if (got_include) {
	char  buffer[BUFSIZ];
	char  prolog_h_file[MAXPATHLEN];
	FILE *prolog_hfile;
	int   count, written;

	strcpy (prolog_h_file, got_include);
	strcat (prolog_h_file, ".p.h");
	prolog_hfile = fopen(prolog_h_file, "r");
	if (prolog_hfile) {
	    fprintf (stderr, "Including %s at beginning of %s file.\n", prolog_h_file, h_file);
	    fprintf (hfile, "/* Including %s at beginning of %s file. */\n\n",
		     prolog_h_file, h_file);
	    do {
		count = fread (buffer, sizeof(char), sizeof(buffer), prolog_hfile);
		if (count == EOF) {
		    perror(prolog_h_file);
		    exit(1);
		}
		written = fwrite (buffer, sizeof(char), count, hfile);
		if (count != written) {
		    perror(prolog_h_file);
		    exit(1);
		}
	    } while (count > 0);
	    fprintf (hfile, "\n/* End of prolog file %s. */\n\n", prolog_h_file);
	}
    }

    if (use_msf) {
	msfile = fopen(msf_file, "w");
	if (msfile == (FILE *)NULL) {
	    perror(msf_file);
	    exit(1);
	}
	fprintf(msfile, msf_warning, msf_file);
    } else {
	cfile = fopen(c_file, "w");
	if (cfile == (FILE *)NULL) {
	    perror(c_file);
	    exit(1);
	}
	fprintf (cfile, warning, c_file);

	/* prologue */
	if (language == lang_C)
	    cpp = c_src_prolog;
	else if (language == lang_KRC)
	    cpp = krc_src_prolog;
	else
	    abort ();
	while (*cpp)
	    fputs (*cpp++, cfile);
    }

    /* parse it */
    yyparse();
    fclose(yyin);		/* bye bye input file */

    if (!use_msf) {
	fputs ("    0\n};\n\n", cfile);
	fprintf(cfile,
		"static const struct error_table et = { text, %ldL, %d };\n\n",
		(long int) table_number, current);
	fputs("static struct et_list etlink = { 0, &et};\n\n", cfile);
	fprintf(cfile, "void initialize_%s_error_table (%s) {\n",
		table_name, (language == lang_C) ? "void" : "NOARGS");
	fputs("    add_to_error_table(&etlink);\n", cfile);
	fputs("}\n", cfile);
	fclose(cfile);


	fprintf (hfile, "extern void initialize_%s_error_table ();\n",
		 table_name);
    } else {
	fprintf (hfile, "#define initialize_%s_error_table()\n",
		 table_name);
    }

    fprintf (hfile, "#define ERROR_TABLE_BASE_%s (%ldL)\n",
	     table_name, (long int) table_number);
    /* compatibility... */
    fprintf (hfile, "\n/* for compatibility with older versions... */\n");
    fprintf (hfile, "#define init_%s_err_tbl initialize_%s_error_table\n",
	     table_name, table_name);
    fprintf (hfile, "#define %s_err_base ERROR_TABLE_BASE_%s\n", table_name,
	     table_name);
    fclose(hfile);		/* bye bye include file */
    if (use_msf)
	fclose(msfile);
    return 0;
}

void yyerror(const char *s) {
    fputs(s, stderr);
    fprintf(stderr, "\nLine number %d; last token was '%s'\n",
	    yylineno, current_token);
}
