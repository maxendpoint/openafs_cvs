/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afsconfig.h>

RCSID("$Header: /cvs/openafs/src/ntp/Attic/mkntpconf.c,v 1.3 2001/07/05 15:20:38 shadow Exp $");

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <strings.h>
#include <sysexits.h>

#ifdef vax
#define PRECISION	-7	/* HZ == 100 */
#endif
#ifdef sun
#define PRECISION	-6	/* HZ == 50 */
#endif
#ifdef romp
#define PRECISION	-6	/* HZ == 64 */
#endif
#ifdef multimax
#define PRECISION	-3	/* HZ == 10 */
#endif

struct server {
	char	*s_name;
	char	*s_addr;
};

/*
 * primary servers
 */

static struct server dcn5	= { "dcn5.udel.edu",	"128.4.0.5" };
static struct server wwvb	= { "wwvb.isi.edu",	"128.9.2.129" };
static struct server sdsc	= { "sdsc-fuzz.nsf.net","192.12.207.1" };
static struct server umd1	= { "umd1.umd.edu",	"128.8.10.1" };

/*
 * secondary servers
 */

static struct server papaya	= { "papaya.srv.cs.cmu.edu", "128.2.222.199" };
static struct server guava	= { "guava.srv.cs.cmu.edu", "128.2.250.187" };
static struct server cluster1	= { "cluster1.fs.andrew.cmu.edu", "128.2.249.123" };

static char *name;

static char *headers1[] = {
	"",
	"  DO NOT EDIT THIS FILE MANUALLY.  It is maintained by mkntpconf.",
	"",
	"		Local clock parameters",
	"",
	"	Precision of the local clock to the nearest power of 2",
	"		ex.",
	"			60-HZ   = 2**-6",
	"			100-HZ  = 2**-7",
	"			1000-HZ = 2**-10",
	0
};

static char *headers2[] = {
	"",
	"	Peers",
	"",
	0
};

extern char *mktemp();
void peerline();


main(ac, av)
	int ac;
	char **av;
{
	register char *p, **v;
	register FILE *f, *g;
	char hostname[MAXHOSTNAMELEN+1];
	char tempfn[MAXPATHLEN+1], config[MAXPATHLEN+1];
	char line[BUFSIZ];
	struct server serv;

	name = (ac > 0) ? (ac--, *av++) : (ac = 0, "mkntpconf");
	(void) strcpy(tempfn, name);
	if ((p = rindex(tempfn, '/')) == 0)
		p = tempfn;
	else
		p += 1;
	*p = 0;
	(void) strcpy(config, tempfn);
	(void) strcat(config, "ntp.conf");
	(void) strcat(tempfn, "ntp.XXXXXX");
	(void) mktemp(tempfn);
	if (gethostname(hostname, MAXHOSTNAMELEN) == -1) {
		perror("gethostname");
		exit(EX_OSERR);
	}
	hostname[MAXHOSTNAMELEN] = 0;
	for (p = hostname; *p; p++)
		if (isupper(*p))
			*p = tolower(*p);
	if ((f = fopen(tempfn, "w")) == NULL) {
		perror(tempfn);
		exit(EX_OSERR);
	}

	for (v = headers1; *v; v++)
		fprintf(f, "#%s\n", *v);
	fprintf(f, "precision %d\n", PRECISION);
	for (v = headers2; *v; v++)
		fprintf(f, "#%s\n", *v);
	if (strcmp(hostname, papaya.s_name) == 0
	|| strcmp(hostname, guava.s_name) == 0) {
		if (strcmp(hostname, papaya.s_name) == 0) {
			peerline(dcn5, f);
			peerline(wwvb, f);
		} else {
			peerline(sdsc, f);
			peerline(umd1, f);
		}
		peerline(papaya, f);
		peerline(guava, f);
		peerline(cluster1, f);
	} else {
		if ((p = index(hostname, '.')) != 0
		&& strcmp(p+1, "srv.cs.cmu.edu") == 0) {
			peerline(papaya, f);
			peerline(guava, f);
		}
		serv.s_name = line;
		serv.s_addr = 0;
		if ((g = fopen("/etc/attributes", "r")) != NULL) {
			while (fgets(line, sizeof(line), g) != NULL) {
				if ((p = index(line, ':')) != 0)
					*p = 0;
				if (strcmp(line, papaya.s_name) == 0
				|| strcmp(line, guava.s_name) == 0)
					continue;
				if ((p = index(line, '.')) == 0
				|| strcmp(p+1, "srv.cs.cmu.edu") != 0)
					continue;
				peerline(serv, f);
			}
			(void) fclose(g);
		}
	}

	(void) fclose(f);
	if (rename(tempfn, config) == -1) {
		perror("rename");
		exit(EX_OSERR);
	}
	if (chmod(config, 0644) == -1) {
		perror("chmod");
		exit(EX_OSERR);
	}
	exit(EX_OK);
}


void
peerline(s, f)
	struct server s;
	register FILE *f;
{
	register struct hostent *hp;

	fprintf(f, "peer ");
	if ((hp = gethostbyname(s.s_name)) != 0 && hp->h_addrtype == AF_INET)
		fprintf(f, "%-15.15s # %s",
				inet_ntoa(* (struct in_addr *) hp->h_addr),
				s.s_name);
	else if (s.s_addr)
		fprintf(f, "%-15.15s # %s", s.s_addr, s.s_name);
	else
		fprintf(f, "%s", s.s_name);
	(void) fputc('\n', f);
}
