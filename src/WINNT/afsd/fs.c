/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <winsock2.h>
#include <errno.h>
#include <assert.h>
#include <rx/rx_globals.h>

#include <osi.h>
#include <afsint.h>
#include <afs/cellconfig.h>
#include <afs/ptserver.h>
#include <afs/ptuser.h>
#include <afs/volser.h>
#include <WINNT\afsreg.h>

#include "fs.h"
#include "fs_utils.h"
#include "cmd.h"
#include "afsd.h"
#include "cm_ioctl.h"

#define	MAXHOSTS 13
#define	OMAXHOSTS 8
#define MAXCELLHOSTS 8
#define MAXNAME 100
#define	MAXSIZE	2048
#define MAXINSIZE 1300    /* pioctl complains if data is larger than this */
#define VMSGSIZE 128      /* size of msg buf in volume hdr */
#define MAXCELLCHARS		64
#define MAXHOSTCHARS		64

static char space[MAXSIZE];
static char tspace[1024];

static struct ubik_client *uclient;

static int GetClientAddrsCmd(struct cmd_syndesc *asp, char *arock);
static int SetClientAddrsCmd(struct cmd_syndesc *asp, char *arock);
static int FlushMountCmd(struct cmd_syndesc *asp, char *arock);
static int RxStatProcCmd(struct cmd_syndesc *asp, char *arock);
static int RxStatPeerCmd(struct cmd_syndesc *asp, char *arock);

extern struct cmd_syndesc *cmd_CreateSyntax();

static int MemDumpCmd(struct cmd_syndesc *asp, char *arock);
static int CSCPolicyCmd(struct cmd_syndesc *asp, char *arock);
static int MiniDumpCmd(struct cmd_syndesc *asp, char *arock);

static char pn[] = "fs";
static int rxInitDone = 0;

/*
 * Character to use between name and rights in printed representation for
 * DFS ACL's.
 */
#define DFS_SEPARATOR	' '

typedef char sec_rgy_name_t[1025];	/* A DCE definition */

struct Acl {
    int dfs;	        /* Originally true if a dfs acl; now also the type
                         * of the acl (1, 2, or 3, corresponding to object,
                         * initial dir, or initial object). */
    sec_rgy_name_t cell; /* DFS cell name */
    int nplus;
    int nminus;
    struct AclEntry *pluslist;
    struct AclEntry *minuslist;
};

struct AclEntry {
    struct AclEntry *next;
    char name[MAXNAME];
    afs_int32 rights;
};

static void 
ZapAcl (struct Acl *acl)
{
    if (!acl)
        return;

    ZapList(acl->pluslist);
    ZapList(acl->minuslist);
    free(acl);
}

/*
 * Mods for the AFS/DFS protocol translator.
 *
 * DFS rights. It's ugly to put these definitions here, but they 
 * *cannot* change, because they're part of the wire protocol.
 * In any event, the protocol translator will guarantee these
 * assignments for AFS cache managers.
 */
#define DFS_READ          0x01
#define DFS_WRITE         0x02
#define DFS_EXECUTE       0x04
#define DFS_CONTROL       0x08
#define DFS_INSERT        0x10
#define DFS_DELETE        0x20

/* the application definable ones (backwards from AFS) */
#define DFS_USR0 0x80000000      /* "A" bit */
#define DFS_USR1 0x40000000      /* "B" bit */
#define DFS_USR2 0x20000000      /* "C" bit */
#define DFS_USR3 0x10000000      /* "D" bit */
#define DFS_USR4 0x08000000      /* "E" bit */
#define DFS_USR5 0x04000000      /* "F" bit */
#define DFS_USR6 0x02000000      /* "G" bit */
#define DFS_USR7 0x01000000      /* "H" bit */
#define DFS_USRALL	(DFS_USR0 | DFS_USR1 | DFS_USR2 | DFS_USR3 |\
			 DFS_USR4 | DFS_USR5 | DFS_USR6 | DFS_USR7)

/*
 * Offset of -id switch in command structure for various commands.
 * The -if switch is the next switch always.
 */
static int parm_setacl_id, parm_copyacl_id, parm_listacl_id;

/*
 * Determine whether either the -id or -if switches are present, and
 * return 0, 1 or 2, as appropriate. Abort if both switches are present.
 */
/* int id; Offset of -id switch; -if is next switch */
static int 
getidf(struct cmd_syndesc *as, int id)
{
    int idf = 0;

    if (as->parms[id].items) {
	idf |= 1;
    }
    if (as->parms[id + 1].items) {
	idf |= 2;
    }
    if (idf == 3) {
	fprintf(stderr,
	     "%s: you may specify either -id or -if, but not both switches\n",
	     pn);
	exit(1);
    }
    return idf;
}

static int
PRights(afs_int32 arights, int dfs)
{
    if (!dfs) {
	if (arights & PRSFS_READ) 
            printf("r");
	if (arights & PRSFS_LOOKUP) 
            printf("l");
	if (arights & PRSFS_INSERT) 
            printf("i");
	if (arights & PRSFS_DELETE) 
            printf("d");
	if (arights & PRSFS_WRITE) 
            printf("w");
	if (arights & PRSFS_LOCK) 
            printf("k");
	if (arights & PRSFS_ADMINISTER) 
            printf("a");
	if (arights & PRSFS_USR0) 
            printf("A");
	if (arights & PRSFS_USR1) 
            printf("B");
	if (arights & PRSFS_USR2) 
            printf("C");
	if (arights & PRSFS_USR3) 
            printf("D");
	if (arights & PRSFS_USR4) 
            printf("E");
	if (arights & PRSFS_USR5) 
            printf("F");
	if (arights & PRSFS_USR6) 
            printf("G");
	if (arights & PRSFS_USR7) 
            printf("H");
    } else {
	if (arights & DFS_READ) 
            printf("r");
        else 
            printf("-");
	if (arights & DFS_WRITE) 
            printf("w"); 
        else 
            printf("-");
	if (arights & DFS_EXECUTE) 
            printf("x"); 
        else 
            printf("-");
	if (arights & DFS_CONTROL) 
            printf("c"); 
        else 
            printf("-");
	if (arights & DFS_INSERT) 
            printf("i"); 
        else 
            printf("-");
	if (arights & DFS_DELETE) 
            printf("d"); 
        else 
            printf("-");
	if (arights & (DFS_USRALL)) 
            printf("+");
	if (arights & DFS_USR0) 
            printf("A");
	if (arights & DFS_USR1) 
            printf("B");
	if (arights & DFS_USR2) 
            printf("C");
	if (arights & DFS_USR3) 
            printf("D");
	if (arights & DFS_USR4) 
            printf("E");
	if (arights & DFS_USR5) 
            printf("F");
	if (arights & DFS_USR6) 
            printf("G");
	if (arights & DFS_USR7) 
            printf("H");
    }	
    return 0;
}

/* this function returns TRUE (1) if the file is in AFS, otherwise false (0) */
static int 
InAFS(char *apath)
{
    struct ViceIoctl blob;
    afs_int32 code;

    blob.in_size = 0;
    blob.out_size = MAXSIZE;
    blob.out = space;

    code = pioctl(apath, VIOC_FILE_CELL_NAME, &blob, 1);
    if (code) {
	if ((errno == EINVAL) || (errno == ENOENT)) 
            return 0;
    }
    return 1;
}

static int 
IsFreelanceRoot(char *apath)
{
    struct ViceIoctl blob;
    afs_int32 code;

    blob.in_size = 0;
    blob.out_size = MAXSIZE;
    blob.out = space;

    code = pioctl(apath, VIOC_FILE_CELL_NAME, &blob, 1);
    if (code == 0)
        return !strcmp("Freelance.Local.Root",space);
    return 1;   /* assume it is because it is more restrictive that way */
}

/* return a static pointer to a buffer */
static char *
Parent(char *apath)
{
    char *tp;
    strcpy(tspace, apath);
    tp = strrchr(tspace, '\\');
    if (tp) {
	*(tp+1) = 0;	/* lv trailing slash so Parent("k:\foo") is "k:\" not "k:" */
    }
    else {
	fs_ExtractDriveLetter(apath, tspace);
    	strcat(tspace, ".");
    }
    return tspace;
}

enum rtype {add, destroy, deny};

static afs_int32 
Convert(char *arights, int dfs, enum rtype *rtypep)
{
    int i, len;
    afs_int32 mode;
    char tc;

    *rtypep = add;	/* add rights, by default */

    if (dfs) {
	if (!strcmp(arights, "null")) {
	    *rtypep = deny;
	    return 0;
	}
	if (!strcmp(arights,"read")) 
            return DFS_READ | DFS_EXECUTE;
	if (!strcmp(arights, "write")) 
            return DFS_READ | DFS_EXECUTE | DFS_INSERT | DFS_DELETE | 
                DFS_WRITE;
	if (!strcmp(arights, "all")) 
            return DFS_READ | DFS_EXECUTE | DFS_INSERT | DFS_DELETE | 
                DFS_WRITE | DFS_CONTROL;
    } else {
	if (!strcmp(arights,"read")) 
            return PRSFS_READ | PRSFS_LOOKUP;
	if (!strcmp(arights, "write")) 
            return PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE | 
                PRSFS_WRITE | PRSFS_LOCK;
	if (!strcmp(arights, "mail")) 
            return PRSFS_INSERT | PRSFS_LOCK | PRSFS_LOOKUP;
	if (!strcmp(arights, "all")) 
            return PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE | 
                PRSFS_WRITE | PRSFS_LOCK | PRSFS_ADMINISTER;
    }
    if (!strcmp(arights, "none")) {
	*rtypep = destroy; /* Remove entire entry */
	return 0;
    }
    len = (int)strlen(arights);
    mode = 0;
    for(i=0;i<len;i++) {
        tc = *arights++;
	if (dfs) {
	    if (tc == '-') 
                continue;
	    else if (tc == 'r') 
                mode |= DFS_READ;
	    else if (tc == 'w') 
                mode |= DFS_WRITE;
	    else if (tc == 'x') 
                mode |= DFS_EXECUTE;
	    else if (tc == 'c') 
                mode |= DFS_CONTROL;
	    else if (tc == 'i') 
                mode |= DFS_INSERT;
	    else if (tc == 'd') 
                mode |= DFS_DELETE;
	    else if (tc == 'A') 
                mode |= DFS_USR0;
	    else if (tc == 'B') 
                mode |= DFS_USR1;
	    else if (tc == 'C') 
                mode |= DFS_USR2;
	    else if (tc == 'D') 
                mode |= DFS_USR3;
	    else if (tc == 'E') 
                mode |= DFS_USR4;
	    else if (tc == 'F') 
                mode |= DFS_USR5;
	    else if (tc == 'G') 
                mode |= DFS_USR6;
	    else if (tc == 'H') 
                mode |= DFS_USR7;
	    else {
		fprintf(stderr, "%s: illegal DFS rights character '%c'.\n", 
                         pn, tc);
		exit(1);
	    }
	} else {
	    if (tc == 'r') 
                mode |= PRSFS_READ;
	    else if (tc == 'l') 
                mode |= PRSFS_LOOKUP;
	    else if (tc == 'i') 
                mode |= PRSFS_INSERT;
	    else if (tc == 'd') 
                mode |= PRSFS_DELETE;
	    else if (tc == 'w') 
                mode |= PRSFS_WRITE;
	    else if (tc == 'k') 
                mode |= PRSFS_LOCK;
	    else if (tc == 'a') 
                mode |= PRSFS_ADMINISTER;
	    else if (tc == 'A') 
                mode |= PRSFS_USR0;
	    else if (tc == 'B') 
                mode |= PRSFS_USR1;
	    else if (tc == 'C') 
                mode |= PRSFS_USR2;
	    else if (tc == 'D') 
                mode |= PRSFS_USR3;
	    else if (tc == 'E') 
                mode |= PRSFS_USR4;
	    else if (tc == 'F') 
                mode |= PRSFS_USR5;
	    else if (tc == 'G') 
                mode |= PRSFS_USR6;
	    else if (tc == 'H') 
                mode |= PRSFS_USR7;
	    else {
		fprintf(stderr, "%s: illegal rights character '%c'.\n", pn, 
                         tc);
		exit(1);
	    }
	}
    }
    return mode;
}

static struct AclEntry *
FindList (struct AclEntry *alist, char *aname)
{
    while (alist) {
        if (!strcasecmp(alist->name, aname)) 
            return alist;
        alist = alist->next;
    }
    return 0;
}

/* if no parm specified in a particular slot, set parm to be "." instead */
static void 
SetDotDefault(struct cmd_item **aitemp)
{
    struct cmd_item *ti;
    if (*aitemp) 
        return;	                /* already has value */
    /* otherwise, allocate an item representing "." */
    ti = (struct cmd_item *) malloc(sizeof(struct cmd_item));
    assert(ti);
    ti->next = (struct cmd_item *) 0;
    ti->data = (char *) malloc(2);
    assert(ti->data);
    strcpy(ti->data, ".");
    *aitemp = ti;
}

static void 
ChangeList (struct Acl *al, afs_int32 plus, char *aname, afs_int32 arights)
{
    struct AclEntry *tlist;
    tlist = (plus ? al->pluslist : al->minuslist);
    tlist = FindList (tlist, aname);
    if (tlist) {
        /* Found the item already in the list. */
        tlist->rights = arights;
        if (plus)
            al->nplus -= PruneList(&al->pluslist, al->dfs);
        else
            al->nminus -= PruneList(&al->minuslist, al->dfs);
        return;
    }
    /* Otherwise we make a new item and plug in the new data. */
    tlist = (struct AclEntry *) malloc(sizeof (struct AclEntry));
    assert(tlist);
    strcpy(tlist->name, aname);
    tlist->rights = arights;
    if (plus) {
        tlist->next = al->pluslist;
        al->pluslist = tlist;
        al->nplus++;
        if (arights == 0 || arights == -1)
	    al->nplus -= PruneList(&al->pluslist, al->dfs);
    } else {
        tlist->next = al->minuslist;
        al->minuslist = tlist;
        al->nminus++;
        if (arights == 0) 
            al->nminus -= PruneList(&al->minuslist, al->dfs);
    }
}

static void 
ZapList (struct AclEntry *alist)
{
    struct AclEntry *tp, *np;
    for (tp = alist; tp; tp = np) {
        np = tp->next;
        free(tp);
    }
}

static int 
PruneList (struct AclEntry **ae, int dfs)
{
    struct AclEntry **lp;
    struct AclEntry *te, *ne;
    afs_int32 ctr;
    ctr = 0;
    lp = ae;
    for(te = *ae;te;te=ne) {
        if ((!dfs && te->rights == 0) || te->rights == -1) {
            *lp = te->next;
            ne = te->next;
            free(te);
            ctr++;
	} else {
            ne = te->next;
            lp = &te->next;
	}
    }
    return ctr;
}

static char *
SkipLine (char *astr)
{
    while (*astr !='\n') 
        astr++;
    astr++;
    return astr;
}

/*
 * Create an empty acl, taking into account whether the acl pointed
 * to by astr is an AFS or DFS acl. Only parse this minimally, so we
 * can recover from problems caused by bogus ACL's (in that case, always
 * assume that the acl is AFS: for DFS, the user can always resort to
 * acl_edit, but for AFS there may be no other way out).
 */
static struct Acl *
EmptyAcl(char *astr)
{
    struct Acl *tp;
    int junk;

    tp = (struct Acl *)malloc(sizeof (struct Acl));
    assert(tp);
    tp->nplus = tp->nminus = 0;
    tp->pluslist = tp->minuslist = 0;
    tp->dfs = 0;
    sscanf(astr, "%d dfs:%d %s", &junk, &tp->dfs, tp->cell);
    return tp;
}

static struct Acl *
ParseAcl (char *astr)
{
    int nplus, nminus, i, trights;
    char tname[MAXNAME];
    struct AclEntry *first, *last, *tl;
    struct Acl *ta;

    ta = (struct Acl *) malloc (sizeof (struct Acl));
    assert(ta);
    ta->dfs = 0;
    sscanf(astr, "%d dfs:%d %s", &ta->nplus, &ta->dfs, ta->cell);
    astr = SkipLine(astr);
    sscanf(astr, "%d", &ta->nminus);
    astr = SkipLine(astr);

    nplus = ta->nplus;
    nminus = ta->nminus;

    last = 0;
    first = 0;
    for(i=0;i<nplus;i++) {
        sscanf(astr, "%100s %d", tname, &trights);
        astr = SkipLine(astr);
        tl = (struct AclEntry *) malloc(sizeof (struct AclEntry));
        assert(tl);
        if (!first) 
            first = tl;
        strcpy(tl->name, tname);
        tl->rights = trights;
        tl->next = 0;
        if (last) 
            last->next = tl;
        last = tl;
    }
    ta->pluslist = first;

    last = 0;
    first = 0;
    for(i=0;i<nminus;i++) {
        sscanf(astr, "%100s %d", tname, &trights);
        astr = SkipLine(astr);
        tl = (struct AclEntry *) malloc(sizeof (struct AclEntry));
        assert(tl);
        if (!first) 
            first = tl;
        strcpy(tl->name, tname);
        tl->rights = trights;
        tl->next = 0;
        if (last) 
            last->next = tl;
        last = tl;
    }
    ta->minuslist = first;

    return ta;
}

static int
PrintStatus(VolumeStatus *status, char *name, char *motd, char *offmsg)
{
    printf("Volume status for vid = %u named %s\n",status->Vid, name);
    if (*offmsg != 0)
	printf("Current offline message is %s\n",offmsg);
    if (*motd != 0)
	printf("Current message of the day is %s\n",motd);
    printf("Current disk quota is ");
    if (status->MaxQuota != 0) 
        printf("%d\n", status->MaxQuota);
    else 
        printf("unlimited\n");
    printf("Current blocks used are %d\n",status->BlocksInUse);
    printf("The partition has %d blocks available out of %d\n\n",
            status->PartBlocksAvail, status->PartMaxBlocks);
    return 0;
}

static int
QuickPrintStatus(VolumeStatus *status, char *name)
{
    double QuotaUsed =0.0;
    double PartUsed =0.0;
    int WARN = 0;
    printf("%-25.25s",name);

    if (status->MaxQuota != 0) {
	printf("%10d%10d", status->MaxQuota, status->BlocksInUse);
	QuotaUsed = ((((double)status->BlocksInUse)/status->MaxQuota) * 100.0);
    } else {
	printf("no limit%10d", status->BlocksInUse);
    }
    if (QuotaUsed > 90.0){
	printf(" %5.0f%%<<", QuotaUsed);
	WARN = 1;
    } else 
        printf(" %5.0f%%  ", QuotaUsed);
    PartUsed = (100.0 - ((((double)status->PartBlocksAvail)/status->PartMaxBlocks) * 100.0));
    if (PartUsed > 97.0){
	printf(" %9.0f%%<<", PartUsed);
	WARN = 1;
    } else 
        printf(" %9.0f%%  ", PartUsed);
    if (WARN){
	printf("  <<WARNING\n");
    } else 
        printf("\n");
    return 0;
}

static int
QuickPrintSpace(VolumeStatus *status, char *name)
{
    double PartUsed =0.0;
    int WARN = 0;
    printf("%-25.25s",name);

    printf("%10d%10d%10d", status->PartMaxBlocks, status->PartMaxBlocks - status->PartBlocksAvail, status->PartBlocksAvail);
	
    PartUsed = (100.0 - ((((double)status->PartBlocksAvail)/status->PartMaxBlocks) * 100.0));
    if (PartUsed > 90.0){
	printf(" %4.0f%%<<", PartUsed);
	WARN = 1;
    } else 
        printf(" %4.0f%%  ", PartUsed);
    if (WARN){
	printf("  <<WARNING\n");
    } else 
        printf("\n");
    return 0;
}

static char *
AclToString(struct Acl *acl)
{
    static char mydata[MAXSIZE];
    char tstring[MAXSIZE];
    char dfsstring[30];
    struct AclEntry *tp;
    
    if (acl->dfs) 
        sprintf(dfsstring, " dfs:%d %s", acl->dfs, acl->cell);
    else 
        dfsstring[0] = '\0';
    sprintf(mydata, "%d%s\n%d\n", acl->nplus, dfsstring, acl->nminus);
    for (tp = acl->pluslist;tp;tp=tp->next) {
        sprintf(tstring, "%s %d\n", tp->name, tp->rights);
        strcat(mydata, tstring);
    }
    for (tp = acl->minuslist;tp;tp=tp->next) {
        sprintf(tstring, "%s %d\n", tp->name, tp->rights);
        strcat(mydata, tstring);
    }
    return mydata;
}

static DWORD IsFreelance(void)
{
    HKEY  parmKey;
    DWORD code;
    DWORD dummyLen;
    DWORD enabled = 0;

    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                         0, KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        dummyLen = sizeof(cm_freelanceEnabled);
        code = RegQueryValueEx(parmKey, "FreelanceClient", NULL, NULL,
                            (BYTE *) &enabled, &dummyLen);
        RegCloseKey (parmKey);
    }
    return enabled;
}

static const char * NetbiosName(void)
{
    static char buffer[1024] = "AFS";
    HKEY  parmKey;
    DWORD code;
    DWORD dummyLen;
    DWORD enabled = 0;

    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                         0, KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        dummyLen = sizeof(buffer);
        code = RegQueryValueEx(parmKey, "NetbiosName", NULL, NULL,
			       buffer, &dummyLen);
        RegCloseKey (parmKey);
    } else {
	strcpy(buffer, "AFS");
    }
    return buffer;
}

#define AFSCLIENT_ADMIN_GROUPNAME "AFS Client Admins"

static BOOL IsAdmin (void)
{
    static BOOL fAdmin = FALSE;
    static BOOL fTested = FALSE;

    if (!fTested)
    {
        /* Obtain the SID for the AFS client admin group.  If the group does
         * not exist, then assume we have AFS client admin privileges.
         */
        PSID psidAdmin = NULL;
        DWORD dwSize, dwSize2;
        char pszAdminGroup[ MAX_COMPUTERNAME_LENGTH + sizeof(AFSCLIENT_ADMIN_GROUPNAME) + 2 ];
        char *pszRefDomain = NULL;
        SID_NAME_USE snu = SidTypeGroup;

        dwSize = sizeof(pszAdminGroup);

        if (!GetComputerName(pszAdminGroup, &dwSize)) {
            /* Can't get computer name.  We return false in this case.
               Retain fAdmin and fTested. This shouldn't happen.*/
            return FALSE;
        }

        dwSize = 0;
        dwSize2 = 0;

        strcat(pszAdminGroup,"\\");
        strcat(pszAdminGroup, AFSCLIENT_ADMIN_GROUPNAME);

        LookupAccountName(NULL, pszAdminGroup, NULL, &dwSize, NULL, &dwSize2, &snu);
        /* that should always fail. */

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            /* if we can't find the group, then we allow the operation */
            fAdmin = TRUE;
            return TRUE;
        }

        if (dwSize == 0 || dwSize2 == 0) {
            /* Paranoia */
            fAdmin = TRUE;
            return TRUE;
        }

        psidAdmin = (PSID)malloc(dwSize); memset(psidAdmin,0,dwSize);
        assert(psidAdmin);
        pszRefDomain = (char *)malloc(dwSize2);
        assert(pszRefDomain);

        if (!LookupAccountName(NULL, pszAdminGroup, psidAdmin, &dwSize, pszRefDomain, &dwSize2, &snu)) {
            /* We can't lookup the group now even though we looked it up earlier.  
               Could this happen? */
            fAdmin = TRUE;
        } else {
            /* Then open our current ProcessToken */
            HANDLE hToken;

            if (OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &hToken))
            {

                if (!CheckTokenMembership(hToken, psidAdmin, &fAdmin)) {
                    /* We'll have to allocate a chunk of memory to store the list of
                     * groups to which this user belongs; find out how much memory
                     * we'll need.
                     */
                    DWORD dwSize = 0;
                    PTOKEN_GROUPS pGroups;

                    GetTokenInformation (hToken, TokenGroups, NULL, dwSize, &dwSize);

                    pGroups = (PTOKEN_GROUPS)malloc(dwSize);
                    assert(pGroups);

                    /* Allocate that buffer, and read in the list of groups. */
                    if (GetTokenInformation (hToken, TokenGroups, pGroups, dwSize, &dwSize))
                    {
                        /* Look through the list of group SIDs and see if any of them
                         * matches the AFS Client Admin group SID.
                         */
                        size_t iGroup = 0;
                        for (; (!fAdmin) && (iGroup < pGroups->GroupCount); ++iGroup)
                        {
                            if (EqualSid (psidAdmin, pGroups->Groups[ iGroup ].Sid)) {
                                fAdmin = TRUE;
                            }
                        }
                    }

                    if (pGroups)
                        free(pGroups);
                }

                /* if do not have permission because we were not explicitly listed
                 * in the Admin Client Group let's see if we are the SYSTEM account
                 */
                if (!fAdmin) {
                    PTOKEN_USER pTokenUser;
                    SID_IDENTIFIER_AUTHORITY SIDAuth = SECURITY_NT_AUTHORITY;
                    PSID pSidLocalSystem = 0;
                    DWORD gle;

                    GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);

                    pTokenUser = (PTOKEN_USER)malloc(dwSize);
                    assert(pTokenUser);

                    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize))
                        gle = GetLastError();

                    if (AllocateAndInitializeSid( &SIDAuth, 1,
                                                  SECURITY_LOCAL_SYSTEM_RID,
                                                  0, 0, 0, 0, 0, 0, 0,
                                                  &pSidLocalSystem))
                    {
                        if (EqualSid(pTokenUser->User.Sid, pSidLocalSystem)) {
                            fAdmin = TRUE;
                        }

                        FreeSid(pSidLocalSystem);
                    }

                    if ( pTokenUser )
                        free(pTokenUser);
                }
            }
        }

        free(psidAdmin);
        free(pszRefDomain);

        fTested = TRUE;
    }

    return fAdmin;
}

static int
SetACLCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct Acl *ta = 0;
    struct cmd_item *ti, *ui;
    int plusp;
    afs_int32 rights;
    int clear;
    int idf = getidf(as, parm_setacl_id);

    int error = 0;

    if (as->parms[2].items)
        clear = 1;
    else
        clear = 0;
    plusp = !(as->parms[3].items);
    for(ti=as->parms[0].items; ti;ti=ti->next) {
	blob.out_size = MAXSIZE;
	blob.in_size = idf;
	blob.in = blob.out = space;
	code = pioctl(ti->data, VIOCGETAL, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
            continue;
	}
        if (ta)
            ZapAcl(ta);
	ta = ParseAcl(space);
	if (!plusp && ta->dfs) {
	    fprintf(stderr,
		    "fs: %s: you may not use the -negative switch with DFS acl's.\n%s",
		    ti->data,
		    "(you may specify \"null\" to revoke all rights, however)\n");
	    error = 1;
            continue;
	}
        if (ta)
            ZapAcl(ta);
	if (clear) 
            ta = EmptyAcl(space);
	else 
            ta = ParseAcl(space);
	CleanAcl(ta, ti->data);
	for(ui=as->parms[1].items; ui; ui=ui->next->next) {
	    enum rtype rtype;
	    if (!ui->next) {
		fprintf(stderr,
                        "%s: Missing second half of user/access pair.\n", pn);
                ZapAcl(ta);
		return 1;
	    }
	    rights = Convert(ui->next->data, ta->dfs, &rtype);
	    if (rtype == destroy && !ta->dfs) {
                struct AclEntry *tlist;

                tlist = (plusp ? ta->pluslist : ta->minuslist);
		if (!FindList(tlist, ui->data))
                    continue;
	    }
	    if (rtype == deny && !ta->dfs) 
                plusp = 0;
	    if (rtype == destroy && ta->dfs) 
                rights = -1;
	    ChangeList(ta, plusp, ui->data, rights);
	}
	blob.in = AclToString(ta);
	blob.out_size=0;
	blob.in_size = 1+(long)strlen(blob.in);
	code = pioctl(ti->data, VIOCSETAL, &blob, 1);
	if (code) {
	    if (errno == EINVAL) {
		if (ta->dfs) {
		    static char *fsenv = 0;
		    if (!fsenv) {
			fsenv = (char *)getenv("FS_EXPERT");
		    }
		    fprintf(stderr, "fs: \"Invalid argument\" was returned when you tried to store a DFS access list.\n");
		    if (!fsenv) {
			fprintf(stderr,
    "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
    "\nPossible reasons for this include:\n\n",			    
    " -You may have specified an inappropriate combination of rights.\n",
    "  For example, some DFS-supported filesystems may not allow you to\n",
    "  drop the \"c\" right from \"user_obj\".\n\n",
    " -A mask_obj may be required (it is likely required by the underlying\n",
    "  filesystem if you try to set anything other than the basic \"user_obj\"\n",
    "  \"mask_obj\", or \"group_obj\" entries). Unlike acl_edit, the fs command\n",
    "  does not automatically create or update the mask_obj. Try setting\n",
    "  the rights \"mask_obj all\" with \"fs sa\" before adding any explicit\n",
    "  users or groups. You can do this with a single command, such as\n",
    "  \"fs sa mask_obj all user:somename read\"\n\n",
    " -A specified user or group may not exist.\n\n",
    " -You may have tried to delete \"user_obj\", \"group_obj\", or \"other_obj\".\n",
    "  This is probably not allowed by the underlying file system.\n\n",
    " -If you add a user or group to a DFS ACL, remember that it must be\n",
    "  fully specified as \"user:username\" or \"group:groupname\". In addition, there\n",
    "  may be local requirements on the format of the user or group name.\n",
    "  Check with your cell administrator.\n\n",			    
    " -Or numerous other possibilities. It would be great if we could be more\n",
    "  precise about the actual problem, but for various reasons, this is\n",
    "  impractical via this interface.  If you can't figure it out, you\n",
    "  might try logging into a DCE-equipped machine and use acl_edit (or\n",
    "  whatever is provided). You may get better results. Good luck!\n\n",
    " (You may inhibit this message by setting \"FS_EXPERT\" in your environment)\n");
		    }
		} else {
		    fprintf(stderr,
                            "%s: Invalid argument, possible reasons include:\n", 
                             pn);
		    fprintf(stderr,"\t-File not in AFS\n");
		    fprintf(stderr,
                            "\t-Too many users on access control list\n");
		    fprintf(stderr,
                            "\t-Tried to add non-existent user to access control list\n");
		}
            } else {
		Die(errno, ti->data);
            }
            error = 1;
        }
    }
    if (ta)
        ZapAcl(ta);
    return error;
}

static int 
CopyACLCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct Acl *fa, *ta = 0;
    struct AclEntry *tp;
    struct cmd_item *ti;
    int clear;
    int idf = getidf(as, parm_copyacl_id);
    int error = 0;

    if (as->parms[2].items) 
        clear=1;
    else 
        clear=0;
    blob.out_size = MAXSIZE;
    blob.in_size = idf;
    blob.in = blob.out = space;
    code = pioctl(as->parms[0].items->data, VIOCGETAL, &blob, 1);
    if (code) {
	Die(errno, as->parms[0].items->data);
	return 1;
    }
    fa = ParseAcl(space);
    CleanAcl(fa, as->parms[0].items->data);
    for (ti=as->parms[1].items; ti;ti=ti->next) {
	blob.out_size = MAXSIZE;
	blob.in_size = idf;
	blob.in = blob.out = space;
	code = pioctl(ti->data, VIOCGETAL, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
            continue;
	}
        if (ta)
            ZapAcl(ta);
	if (clear) 
            ta = EmptyAcl(space);
	else 
            ta = ParseAcl(space);
	CleanAcl(ta, ti->data);
	if (ta->dfs != fa->dfs) {
	    fprintf(stderr, 
                    "%s: incompatible file system types: acl not copied to %s; aborted\n", 
                    pn, ti->data);
	    error = 1;
            continue;
	}
	if (ta->dfs) {
	    if (! clear && strcmp(ta->cell, fa->cell) != 0) {
		fprintf(stderr, 
                        "%s: default DCE cell differs for file %s: use \"-clear\" switch; acl not merged\n", 
                        pn, ti->data);
                error = 1;
		continue;
	    }
	    strcpy(ta->cell, fa->cell);
	}
	for (tp = fa->pluslist;tp;tp=tp->next) 
	    ChangeList(ta, 1, tp->name, tp->rights);
	for (tp = fa->minuslist;tp;tp=tp->next) 
	    ChangeList(ta, 0, tp->name, tp->rights);
	blob.in = AclToString(ta);
	blob.out_size=0;
	blob.in_size = 1+(long)strlen(blob.in);
	code = pioctl(ti->data, VIOCSETAL, &blob, 1);
	if (code) {
	    if (errno == EINVAL) {
		fprintf(stderr,
                        "%s: Invalid argument, possible reasons include:\n", pn);
		fprintf(stderr,"\t-File not in AFS\n");
	    } else {
		Die(errno, ti->data);
	    }
            error = 1;
	}
    } 
    if (ta)
	ZapAcl(ta);
    ZapAcl(fa);
    return error;
}

/* pioctl() call to get the cellname of a pathname */
static afs_int32
GetCell(char *fname, char *cellname)
{
    afs_int32 code;
    struct ViceIoctl blob;

    blob.in_size = 0;
    blob.out_size = MAXCELLCHARS;
    blob.out = cellname;

    code = pioctl(fname, VIOC_FILE_CELL_NAME, &blob, 1);
    return code ? errno : 0;
}

/* Check if a username is valid: If it contains only digits (or a
 * negative sign), then it might be bad.  We then query the ptserver
 * to see.
 */
static int
BadName(char *aname, char *fname)
{
    afs_int32 tc, code, id;
    char *nm;
    char cell[MAXCELLCHARS];

    for ( nm = aname; tc = *nm; nm++) {
	/* all must be '-' or digit to be bad */
	if (tc != '-' && (tc < '0' || tc > '9'))
            return 0;
    }

    /* Go to the PRDB and see if this all number username is valid */
    code = GetCell(fname, cell);
    if (code)
        return 0;

    pr_Initialize(1, AFSDIR_CLIENT_ETC_DIRPATH, cell);
    code = pr_SNameToId(aname, &id);
    pr_End();

    /* 1=>Not-valid; 0=>Valid */
    return ((!code && (id == ANONYMOUSID)) ? 1 : 0);
}


/* clean up an access control list of its bad entries; return 1 if we made
   any changes to the list, and 0 otherwise */
static int 
CleanAcl(struct Acl *aa, char *fname)
{
    struct AclEntry *te, **le, *ne;
    int changes;

    /* Don't correct DFS ACL's for now */
    if (aa->dfs)
	return 0;

    /* prune out bad entries */
    changes = 0;	    /* count deleted entries */
    le = &aa->pluslist;
    for(te = aa->pluslist; te; te=ne) {
	ne = te->next;
	if (BadName(te->name, fname)) {
	    /* zap this dude */
	    *le = te->next;
	    aa->nplus--;
	    free(te);
	    changes++;
	} else {
	    le = &te->next;
	}
    }
    le = &aa->minuslist;
    for(te = aa->minuslist; te; te=ne) {
	ne = te->next;
	if (BadName(te->name, fname)) {
	    /* zap this dude */
	    *le = te->next;
	    aa->nminus--;
	    free(te);
	    changes++;
	} else {
	    le = &te->next;
	}
    }
    return changes;
}


/* clean up an acl to not have bogus entries */
static int 
CleanACLCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct Acl *ta = 0;
    struct ViceIoctl blob;
    int changes;
    struct cmd_item *ti;
    struct AclEntry *te;
    int error = 0;

    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	blob.out_size = MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETAL, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
            error = 1;
	    continue;
	}
        if (ta)
            ZapAcl(ta);
	ta = ParseAcl(space);

	if (ta->dfs) {
	    fprintf(stderr,
		    "%s: cleanacl is not supported for DFS access lists.\n",
		    pn);
	    error = 1;
            continue;
	}

	changes = CleanAcl(ta, ti->data);

	if (changes) {
	    /* now set the acl */
	    blob.in=AclToString(ta);
	    blob.in_size = (long)strlen(blob.in)+1;
	    blob.out_size = 0;
	    code = pioctl(ti->data, VIOCSETAL, &blob, 1);
	    if (code) {
		if (errno == EINVAL) {
		    fprintf(stderr,
                            "%s: Invalid argument, possible reasons include\n", 
                             pn);
		    fprintf(stderr,"%s: File not in vice or\n", pn);
		    fprintf(stderr,
                            "%s: Too many users on access control list or\n", 
                            pn);
		} else {
		    Die(errno, ti->data);
		}
                error = 1;
                continue;
	    }

	    /* now list the updated acl */
	    printf("Access list for %s is now\n", ti->data);
	    if (ta->nplus > 0) {
		if (!ta->dfs) 
                    printf("Normal rights:\n");
		for(te = ta->pluslist;te;te=te->next) {
		    printf("  %s ", te->name);
		    PRights(te->rights, ta->dfs);
		    printf("\n");
		}
	    }
	    if (ta->nminus > 0) {
		printf("Negative rights:\n");
		for(te = ta->minuslist;te;te=te->next) {
		    printf("  %s ", te->name);
		    PRights(te->rights, ta->dfs);
		    printf("\n");
		}
	    }
	    if (ti->next) 
                printf("\n");
	} else
	    printf("Access list for %s is fine.\n", ti->data);
    }
    if (ta)
        ZapAcl(ta);
    return error;
}

static int 
ListACLCmd(struct cmd_syndesc *as, char *arock) 
{
    afs_int32 code;
    struct Acl *ta = 0;
    struct ViceIoctl blob;
    struct AclEntry *te;
    struct cmd_item *ti;
    int idf = getidf(as, parm_listacl_id);
    int error = 0;

    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	char separator;
	blob.out_size = MAXSIZE;
	blob.in_size = idf;
	blob.in = blob.out = space;
	code = pioctl(ti->data, VIOCGETAL, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
            error = 1;
	    continue;
	}
	ta = ParseAcl(space);
	switch (ta->dfs) {
	  case 0:
	    printf("Access list for %s is\n", ti->data);
	    break;
	  case 1:
	    printf("DFS access list for %s is\n", ti->data);
	    break;
	  case 2:
	    printf("DFS initial directory access list of %s is\n", ti->data);
	    break;
	  case 3:
	    printf("DFS initial file access list of %s is\n", ti->data);
	    break;
	}
	if (ta->dfs) {
	    printf("  Default cell = %s\n", ta->cell);
	}
	separator = ta->dfs? DFS_SEPARATOR : ' ';
	if (ta->nplus > 0) {
	    if (!ta->dfs) 
                printf("Normal rights:\n");
	    for(te = ta->pluslist;te;te=te->next) {
		printf("  %s%c", te->name, separator);
		PRights(te->rights, ta->dfs);
		printf("\n");
	    }
	}
	if (ta->nminus > 0) {
	    printf("Negative rights:\n");
	    for(te = ta->minuslist;te;te=te->next) {
		printf("  %s ", te->name);
		PRights(te->rights, ta->dfs);
		printf("\n");
	    }
	}
	if (ti->next) 
            printf("\n");
        ZapAcl(ta);
    }
    return error;
}

static int
FlushAllCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;

    blob.in_size = blob.out_size = 0;
    code = pioctl(NULL, VIOC_FLUSHALL, &blob, 0);
    if (code) {
	fprintf(stderr, "Error flushing all ");
	return 1;
    }
    return 0;
}

static int
FlushVolumeCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    int error = 0;

    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	blob.in_size = blob.out_size = 0;
	code = pioctl(ti->data, VIOC_FLUSHVOLUME, &blob, 0);
	if (code) {
	    fprintf(stderr, "Error flushing volume ");
            perror(ti->data);
            error = 1;
	    continue;
	}
    }
    return error;
}

static int 
FlushCmd(struct cmd_syndesc *as, char *arock) 
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;

    int error = 0;

    for(ti=as->parms[0].items; ti; ti=ti->next) {
	blob.in_size = blob.out_size = 0;
	code = pioctl(ti->data, VIOCFLUSH, &blob, 0);
	if (code) {
	    if (errno == EMFILE) {
		fprintf(stderr, "%s: Can't flush active file %s\n", pn, 
                        ti->data);
	    } else {
		fprintf(stderr, "%s: Error flushing file ", pn);
		perror(ti->data);
	    }
            error = 1;
            continue;
	}
    }
    return error;
}

/* all this command does is repackage its args and call SetVolCmd */
static int
SetQuotaCmd(struct cmd_syndesc *as, char *arock) {
    struct cmd_syndesc ts;

    /* copy useful stuff from our command slot; we may later have to reorder */
    memcpy(&ts, as, sizeof(ts));	/* copy whole thing */
    return SetVolCmd(&ts, arock);
}

static int
SetVolCmd(struct cmd_syndesc *as, char *arock) {
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    struct VolumeStatus *status;
    char *motd, *offmsg, *input;
    int error = 0;

    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	blob.out_size = MAXSIZE;
	blob.in_size = sizeof(*status) + 3;	/* for the three terminating nulls */
	blob.out = space;
	blob.in = space;
	status = (VolumeStatus *)space;
	status->MinQuota = status->MaxQuota = -1;
	motd = offmsg = NULL;
	if (as->parms[1].items) {
	    code = util_GetInt32(as->parms[1].items->data, &status->MaxQuota);
	    if (code) {
		fprintf(stderr,"%s: bad integer specified for quota.\n", pn);
		error = 1;
                continue;
	    }
	}
	if (as->parms[2].items) 
            motd = as->parms[2].items->data;
	if (as->parms[3].items) 
            offmsg = as->parms[3].items->data;
	input = (char *)status + sizeof(*status);
	*(input++) = '\0';	/* never set name: this call doesn't change vldb */
	if(offmsg) {
            if (strlen(offmsg) >= VMSGSIZE) {
                fprintf(stderr,"%s: message must be shorter than %d characters\n",
                         pn, VMSGSIZE);
                error = 1;
                continue;
            }
	    strcpy(input,offmsg);
	    blob.in_size += (long)strlen(offmsg);
	    input += strlen(offmsg) + 1;
	} else 
            *(input++) = '\0';
	if(motd) {
            if (strlen(motd) >= VMSGSIZE) {
                fprintf(stderr,"%s: message must be shorter than %d characters\n",
                         pn, VMSGSIZE);
                return code;
            }
	    strcpy(input,motd);
	    blob.in_size += (long)strlen(motd);
	    input += strlen(motd) + 1;
	} else 
            *(input++) = '\0';
	code = pioctl(ti->data,VIOCSETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	}
    }
    return error;
}

#ifndef WIN32
/* 
 * Why is VenusFid declared in the kernel-only section of afs.h, 
 * if it's the exported interface of the (UNIX) cache manager?
 */
struct VenusFid {
    afs_int32 Cell;
    AFSFid Fid;
};
#endif /* WIN32 */

static int 
ExamineCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    struct VolumeStatus *status;
    char *name, *offmsg, *motd;
    int error = 0;
    
    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
        cm_fid_t fid;
	afs_uint32 owner[2];
	char cell[MAXCELLCHARS];

	code = GetCell(ti->data, cell);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}

	/* once per file */
	blob.in_size = 0;

        blob.out_size = sizeof(cm_fid_t);
        blob.out = (char *) &fid;
        if (0 == pioctl(ti->data, VIOCGETFID, &blob, 1)) {
            printf("File %s (%u.%u.%u) contained in cell %s\n",
                    ti->data, fid.volume, fid.vnode, fid.unique,
                    cell);
        }

	blob.out_size = 2 * sizeof(afs_uint32);
        blob.out = (char *) &owner;
	if (0 == pioctl(ti->data, VIOCGETOWNER, &blob, 1)) {
	    char oname[PR_MAXNAMELEN] = "(unknown)";

	    /* Go to the PRDB and see if this all number username is valid */
	    pr_Initialize(1, AFSDIR_CLIENT_ETC_DIRPATH, cell);
	    pr_SIdToName(owner[0], oname);
	    printf("Owner %s (%u) Group %u\n", oname, owner[0], owner[1]);
        }
	
	blob.out = space;
	blob.out_size = MAXSIZE;
	code = pioctl(ti->data, VIOCGETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}
	status = (VolumeStatus *)space;
	name = (char *)status + sizeof(*status);
	offmsg = name + strlen(name) + 1;
	motd = offmsg + strlen(offmsg) + 1;

	PrintStatus(status, name, motd, offmsg);
    }
    return error;
}

static int
ListQuotaCmd(struct cmd_syndesc *as, char *arock) 
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    struct VolumeStatus *status;
    char *name;

    int error = 0;
    
    printf("%-25s%-10s%-10s%-7s%-13s\n", 
           "Volume Name", "     Quota", "      Used", "  %Used", "    Partition");
    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	blob.out_size = MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
            error = 1;
	    continue;
	}
	status = (VolumeStatus *)space;
	name = (char *)status + sizeof(*status);
	QuickPrintStatus(status, name);
    }
    return error;
}

static int
WhereIsCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    int j;
    afs_int32 *hosts;
    char *tp;
    int error = 0;
    
    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	blob.out_size = MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	memset(space, 0, sizeof(space));
	code = pioctl(ti->data, VIOCWHEREIS, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
            error = 1;
	    continue;
	}
	hosts = (afs_int32 *) space;
	printf("File %s is on host%s ", ti->data, 
                (hosts[0] && !hosts[1]) ? "": "s");
	for(j=0; j<MAXHOSTS; j++) {
	    if (hosts[j] == 0) 
                break;
	    tp = hostutil_GetNameByINet(hosts[j]);
	    printf("%s ", tp);
	}
	printf("\n");
    }
    return error;
}


static int
DiskFreeCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char *name;
    struct VolumeStatus *status;
    int error = 0;
    
    printf("%-25s%-10s%-10s%-10s%-6s\n", "Volume Name", "    kbytes",
	   "      used", "     avail", " %used");
    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	blob.out_size = MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
            error = 1;
	    continue;
	}
	status = (VolumeStatus *)space;
	name = (char *)status + sizeof(*status);
	QuickPrintSpace(status, name);
    }
    return error;
}

static int
QuotaCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    double quotaPct;
    struct VolumeStatus *status;
    int error = 0;
    
    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	blob.out_size = MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	code = pioctl(ti->data, VIOCGETVOLSTAT, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
            error = 1;
	    continue;
	}
	status = (VolumeStatus *)space;
	if (status->MaxQuota) 
            quotaPct = ((((double)status->BlocksInUse)/status->MaxQuota) * 100.0);
	else 
            quotaPct = 0.0;
	printf("%2.0f%% of quota used.\n", quotaPct);
    }
    return error;
}

static int
ListMountCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char orig_name[1024];		/*Original name, may be modified*/
    char true_name[1024];		/*``True'' dirname (e.g., symlink target)*/
    char parent_dir[1024];		/*Parent directory of true name*/
    char *last_component;	/*Last component of true name*/
#ifndef WIN32
    struct stat statbuff;		/*Buffer for status info*/
#endif /* not WIN32 */
#ifndef WIN32
    int link_chars_read;		/*Num chars read in readlink()*/
#endif /* not WIN32 */
    int	thru_symlink;			/*Did we get to a mount point via a symlink?*/
    
    int error = 0;
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	thru_symlink = 0;
#ifdef WIN32
	strcpy(orig_name, ti->data);
#else /* not WIN32 */
	sprintf(orig_name, "%s%s",
		(ti->data[0] == '/') ? "" : "./",
		ti->data);
#endif /* not WIN32 */

#ifndef WIN32
	if (lstat(orig_name, &statbuff) < 0) {
	    /* if lstat fails, we should still try the pioctl, since it
             * may work (for example, lstat will fail, but pioctl will
             * work if the volume of offline (returning ENODEV). */
	    statbuff.st_mode = S_IFDIR; /* lie like pros */
	}

	/*
	 * The lstat succeeded.  If the given file is a symlink, substitute
	 * the file name with the link name.
	 */
	if ((statbuff.st_mode & S_IFMT) == S_IFLNK) {
	    thru_symlink = 1;
	    /*
	     * Read name of resolved file.
	     */
	    link_chars_read = readlink(orig_name, true_name, 1024);
	    if (link_chars_read <= 0) {
		fprintf(stderr,
                        "%s: Can't read target name for '%s' symbolic link!\n",
		       pn, orig_name);
		error = 1;
                continue;
	    }

	    /*
	     * Add a trailing null to what was read, bump the length.
	     */
	    true_name[link_chars_read++] = 0;

	    /*
	     * If the symlink is an absolute pathname, we're fine.  Otherwise, we
	     * have to create a full pathname using the original name and the
	     * relative symlink name.  Find the rightmost slash in the original
	     * name (we know there is one) and splice in the symlink value.
	     */
	    if (true_name[0] != '\\') {
		last_component = (char *) strrchr(orig_name, '\\');
		strcpy(++last_component, true_name);
		strcpy(true_name, orig_name);
	    }
	} else
	    strcpy(true_name, orig_name);
#else	/* WIN32 */
	strcpy(true_name, orig_name);
#endif /* WIN32 */

	/*
	 * Find rightmost slash, if any.
	 */
#ifdef WIN32
	last_component = (char *) strrchr(true_name, '\\');
	if (!last_component)
#endif /* WIN32 */
	    last_component = (char *) strrchr(true_name, '/');
	if (last_component) {
	    /*
	     * Found it.  Designate everything before it as the parent directory,
	     * everything after it as the final component.
	     */
	    strncpy(parent_dir, true_name, last_component - true_name + 1);
	    parent_dir[last_component - true_name + 1] = 0;
	    last_component++;   /*Skip the slash*/
#ifdef WIN32
	    if (!InAFS(parent_dir)) {
		const char * nbname = NetbiosName();
		int len = (int)strlen(nbname);

		if (parent_dir[0] == '\\' && parent_dir[1] == '\\' &&
		    parent_dir[len+2] == '\\' &&
		    parent_dir[len+3] == '\0' &&
		    !strnicmp(nbname,&parent_dir[2],len))
		{
		    sprintf(parent_dir,"\\\\%s\\all\\", nbname);
		}
	    }
#endif
	} else {
	    /*
	     * No slash appears in the given file name.  Set parent_dir to the current
	     * directory, and the last component as the given name.
	     */
	    fs_ExtractDriveLetter(true_name, parent_dir);
	    strcat(parent_dir, ".");
	    last_component = true_name;
            fs_StripDriveLetter(true_name, true_name, sizeof(true_name));
	}

	if (strcmp(last_component, ".") == 0 || strcmp(last_component, "..") == 0) {
	    fprintf(stderr,"%s: you may not use '.' or '..' as the last component\n",pn);
	    fprintf(stderr,"%s: of a name in the 'fs lsmount' command.\n",pn);
	    error = 1;
	    continue;
	}

	blob.in = last_component;
	blob.in_size = (long)strlen(last_component)+1;
	blob.out_size = MAXSIZE;
	blob.out = space;
	memset(space, 0, MAXSIZE);

	code = pioctl(parent_dir, VIOC_AFS_STAT_MT_PT, &blob, 1);

	if (code == 0) {
	    printf("'%s' is a %smount point for volume '%s'\n",
		   ti->data,
		   (thru_symlink ? "symbolic link, leading to a " : ""),
		   space);

	} else {
	    if (errno == EINVAL) {
		fprintf(stderr,"'%s' is not a mount point.\n", ti->data);
	    } else {
		Die(errno, (ti->data ? ti->data : parent_dir));
	    }
	    error = 1;
	}
    }
    return error;
}

static int
MakeMountCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    char *cellName, *volName, *tmpName;
#ifdef WIN32
    char localCellName[128];
#endif
    char path[1024] = "";
    struct afsconf_cell info;
    struct vldbentry vldbEntry;
    struct ViceIoctl blob;
    char * parent;

    if (as->parms[2].items)	/* cell name specified */
	cellName = as->parms[2].items->data;
    else
	cellName = NULL;
    volName = as->parms[1].items->data;

    if (strlen(volName) >= 64) {
	fprintf(stderr,"%s: volume name too long (length must be < 64 characters)\n", pn);
	return 1;
    }

    /* Check for a cellname in the volume specification, and complain
     * if it doesn't match what was specified with -cell */
    if (tmpName = strchr(volName, ':')) {
        *tmpName = '\0';
        if (cellName) {
            if (strcasecmp(cellName,volName)) {
                fprintf(stderr,"fs: cellnames do not match.\n");
                return 1;
            }
        }
        cellName = volName;
        volName = ++tmpName;
    }

    parent = Parent(as->parms[0].items->data);
    if (!InAFS(parent)) {
#ifdef WIN32
	const char * nbname = NetbiosName();
	int len = (int)strlen(nbname);

	if (parent[0] == '\\' && parent[1] == '\\' &&
	    parent[len+2] == '\\' &&
	    parent[len+3] == '\0' &&
	    !strnicmp(nbname,&parent[2],len))
	{
	    sprintf(path,"%sall\\%s", parent, &as->parms[0].items->data[strlen(parent)]);
	    parent = Parent(path);
	    if (!InAFS(parent)) {
		fprintf(stderr,"%s: mount points must be created within the AFS file system\n", pn);
		return 1;
	    }
	} else 
#endif
	{
	    fprintf(stderr,"%s: mount points must be created within the AFS file system\n", pn);
	    return 1;
	}
    }

    if ( strlen(path) == 0 )
	strcpy(path, as->parms[0].items->data);

    if ( IsFreelanceRoot(parent) ) {
	if ( !IsAdmin() ) {
	    fprintf(stderr,"%s: Only AFS Client Administrators may alter the root.afs volume\n", pn);
	    return 1;
	}

	if (!cellName) {
	    blob.in_size = 0;
	    blob.out_size = sizeof(localCellName);
	    blob.out = localCellName;
	    code = pioctl(parent, VIOC_GET_WS_CELL, &blob, 1);
	    if (!code)
		cellName = localCellName;
	}
    } else {
	if (!cellName)
	    GetCell(parent,space);
    }

    code = GetCellName(cellName?cellName:space, &info);
    if (code) {
	return 1;
    }
    if (!(as->parms[4].items)) {
      /* not fast, check which cell the mountpoint is being created in */
      code = 0;
	/* not fast, check name with VLDB */
      if (!code)
	code = VLDBInit(1, &info);
      if (code == 0) {
	  /* make the check.  Don't complain if there are problems with init */
	  code = ubik_VL_GetEntryByNameO(uclient, 0, volName, &vldbEntry);
	  if (code == VL_NOENT) {
	      fprintf(stderr,"%s: warning, volume %s does not exist in cell %s.\n",
		      pn, volName, cellName ? cellName : space);
	  }
      }
    }

    if (as->parms[3].items)	/* if -rw specified */
	strcpy(space, "%");
    else
	strcpy(space, "#");
    if (cellName) {
	/* cellular mount point, prepend cell prefix */
	strcat(space, info.name);
	strcat(space, ":");
    }
    strcat(space, volName);	/* append volume name */
    strcat(space, ".");		/* stupid convention; these end with a period */
#ifdef WIN32
    /* create symlink with a special pioctl for Windows NT, since it doesn't
     * have a symlink system call.
     */
    blob.out_size = 0;
    blob.in_size = 1 + (long)strlen(space);
    blob.in = space;
    blob.out = NULL;
    code = pioctl(path, VIOC_AFS_CREATE_MT_PT, &blob, 0);
#else /* not WIN32 */
    code = symlink(space, path);
#endif /* not WIN32 */
    if (code) {
	Die(errno, path);
	return 1;
    }
    return 0;
}

/*
 * Delete AFS mount points.  Variables are used as follows:
 *       tbuffer: Set to point to the null-terminated directory name of the mount point
 *	    (or ``.'' if none is provided)
 *      tp: Set to point to the actual name of the mount point to nuke.
 */
static int
RemoveMountCmd(struct cmd_syndesc *as, char *arock) {
    afs_int32 code=0;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char tbuffer[1024];
    char lsbuffer[1024];
    char *tp;
    int error = 0;
    
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	tp = (char *) strrchr(ti->data, '\\');
	if (!tp)
	    tp = (char *) strrchr(ti->data, '/');
	if (tp) {
	    strncpy(tbuffer, ti->data, code=(afs_int32)(tp-ti->data+1));  /* the dir name */
            tbuffer[code] = 0;
	    tp++;   /* skip the slash */

#ifdef WIN32
	    if (!InAFS(tbuffer)) {
		const char * nbname = NetbiosName();
		int len = (int)strlen(nbname);

		if (tbuffer[0] == '\\' && tbuffer[1] == '\\' &&
		    tbuffer[len+2] == '\\' &&
		    tbuffer[len+3] == '\0' &&
		    !strnicmp(nbname,&tbuffer[2],len))
		{
		    sprintf(tbuffer,"\\\\%s\\all\\", nbname);
		}
	    }
#endif
	} else {
	    fs_ExtractDriveLetter(ti->data, tbuffer);
	    strcat(tbuffer, ".");
	    tp = ti->data;
            fs_StripDriveLetter(tp, tp, 0);
	}
	blob.in = tp;
	blob.in_size = (long)strlen(tp)+1;
	blob.out = lsbuffer;
	blob.out_size = sizeof(lsbuffer);
	code = pioctl(tbuffer, VIOC_AFS_STAT_MT_PT, &blob, 0);
	if (code) {
	    if (errno == EINVAL) {
		fprintf(stderr,"%s: '%s' is not a mount point.\n", pn, ti->data);
            } else {
		Die(errno, ti->data);
	    }
            error = 1;
	    continue;	/* don't bother trying */
	}

        if ( IsFreelanceRoot(tbuffer) && !IsAdmin() ) {
            fprintf(stderr,"%s: Only AFS Client Administrators may alter the root.afs volume\n", pn);
            error = 1;
            continue;   /* skip */
        }

        blob.out_size = 0;
	blob.in = tp;
	blob.in_size = (long)strlen(tp)+1;
	code = pioctl(tbuffer, VIOC_AFS_DELETE_MT_PT, &blob, 0);
	if (code) {
	    Die(errno, ti->data);
            error = 1;
	}
    }
    return error;
}

/*
*/

static int
CheckServersCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    afs_int32 j;
    afs_int32 temp;
    char *tp;
    struct afsconf_cell info;
    struct chservinfo checkserv;

    memset(&checkserv, 0, sizeof(struct chservinfo));
    blob.in_size=sizeof(struct chservinfo);
    blob.in=(caddr_t)&checkserv;

    blob.out_size = MAXSIZE;
    blob.out = space;
    memset(space, 0, sizeof(afs_int32));	/* so we assure zero when nothing is copied back */

    /* prepare flags for checkservers command */
    temp = 2;	/* default to checking local cell only */
    if (as->parms[2].items) 
        temp |= 1;	/* set fast flag */
    if (as->parms[1].items) 
        temp &= ~2;	/* turn off local cell check */
    
    checkserv.magic = 0x12345678;	/* XXX */
    checkserv.tflags=temp;

    /* now copy in optional cell name, if specified */
    if (as->parms[0].items) {
	code = GetCellName(as->parms[0].items->data, &info);
	if (code) {
	    return 1;
	}
	strcpy(checkserv.tbuffer,info.name);
	checkserv.tsize=(int)strlen(info.name)+1;
    } else {
        strcpy(checkserv.tbuffer,"\0");
        checkserv.tsize=0;
    }

    if(as->parms[3].items) {
        checkserv.tinterval=atol(as->parms[3].items->data);

        /* sanity check */
        if(checkserv.tinterval<0) {
            printf("Warning: The negative -interval is ignored; treated as an inquiry\n");
            checkserv.tinterval=0;
        } else if(checkserv.tinterval> 600) {
            printf("Warning: The maximum -interval value is 10 mins (600 secs)\n");
            checkserv.tinterval=600;	/* 10 min max interval */
        }       
    } else {
        checkserv.tinterval = -1;	/* don't change current interval */
    }

    if ( checkserv.tinterval != 0 ) {
#ifdef WIN32
        if ( !IsAdmin() ) {
            fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
            return EACCES;
        }
#else /* WIN32 */
        if (geteuid()) {
            fprintf (stderr,"Permission denied: requires root access.\n");
            return EACCES;
        }
#endif /* WIN32 */
    }

    code = pioctl(0, VIOCCKSERV, &blob, 1);
    if (code) {
	if ((errno == EACCES) && (checkserv.tinterval > 0)) {
	    printf("Must be root to change -interval\n");
	    return code;
	}
	Die(errno, 0);
        return 1;
    }
    memcpy(&temp, space, sizeof(afs_int32));
    if (checkserv.tinterval >= 0) {
	if (checkserv.tinterval > 0) 
	    printf("The new down server probe interval (%d secs) is now in effect (old interval was %d secs)\n", 
		   checkserv.tinterval, temp);
	else 
	    printf("The current down server probe interval is %d secs\n", temp);
	return 0;
    }
    if (temp == 0) {
	printf("All servers are running.\n");
    } else {
	printf("These servers unavailable due to network or server problems: ");
	for(j=0; j < MAXHOSTS; j++) {
	    memcpy(&temp, space + j*sizeof(afs_int32), sizeof(afs_int32));
	    if (temp == 0) 
                break;
	    tp = hostutil_GetNameByINet(temp);
	    printf(" %s", tp);
	}
	printf(".\n");
	code = 1;	/* XXX */
    }
    return code;
}

static int
MessagesCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code=0;
    struct ViceIoctl blob;
    struct gaginfo gagflags;
    struct cmd_item *show;
    
    memset(&gagflags, 0, sizeof(struct gaginfo));
    blob.in_size = sizeof(struct gaginfo);
    blob.in = (caddr_t ) &gagflags;
    blob.out_size = MAXSIZE;
    blob.out = space;
    memset(space, 0, sizeof(afs_int32));	/* so we assure zero when nothing is copied back */

    if (show = as->parms[0].items) {
        if (!strcasecmp (show->data, "user"))
            gagflags.showflags |= GAGUSER;
        else if (!strcasecmp (show->data, "console"))
            gagflags.showflags |= GAGCONSOLE;
        else if (!strcasecmp (show->data, "all"))
            gagflags.showflags |= GAGCONSOLE | GAGUSER;
        else if (!strcasecmp (show->data, "none"))
            /* do nothing */ ;
        else {
            fprintf(stderr, 
                     "unrecognized flag %s: must be in {user,console,all,none}\n", 
                     show->data);
            code = EINVAL;
        }
    }
 
    if (code)
        return 1;

    code = pioctl(0, VIOC_GAG, &blob, 1);
    if (code) {
	Die(errno, 0);
        return 1;
    }
    return 0;
}

static int
CheckVolumesCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    
    blob.in_size = 0;
    blob.out_size = 0;
    code = pioctl(0, VIOCCKBACK, &blob, 1);
    if (code) {
	Die(errno, 0);
	return 1;
    }
    printf("All volumeID/name mappings checked.\n");
    
    return 0;
}

static int
SetCacheSizeCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    afs_int32 temp;
    
#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#else /* WIN32 */
    if (geteuid()) {
        fprintf (stderr,"Permission denied: requires root access.\n");
        return EACCES;
    }
#endif /* WIN32 */

    if (!as->parms[0].items && !as->parms[1].items) {
	fprintf(stderr,"%s: syntax error in set cache size cmd.\n", pn);
	return 1;
    }
    if (as->parms[0].items) {
	code = util_GetInt32(as->parms[0].items->data, &temp);
	if (code) {
	    fprintf(stderr,"%s: bad integer specified for cache size.\n", pn);
	    return 1;
	}
    } else
	temp = 0;
    blob.in = (char *) &temp;
    blob.in_size = sizeof(afs_int32);
    blob.out_size = 0;
    code = pioctl(0, VIOCSETCACHESIZE, &blob, 1);
    if (code) {
	Die(errno, (char *) 0);
        return 1;
    } 
      
    printf("New cache size set.\n");
    return 0;
}

#define MAXGCSIZE	16
static int
GetCacheParmsCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    afs_uint64 parms[MAXGCSIZE];

    memset(parms, 0, sizeof(parms));
    blob.in = NULL;
    blob.in_size = 0;
    blob.out_size = sizeof(parms);
    blob.out = (char *) parms;
    code = pioctl(0, VIOCGETCACHEPARMS, &blob, 1);
    if (code) {
	Die(errno, NULL);
        return 1;
    }
     
    printf("AFS using %d of the cache's available %d 1K byte blocks.\n",
           parms[1], parms[0]);
    if (parms[1] > parms[0])
        printf("[Cache guideline temporarily deliberately exceeded; it will be adjusted down but you may wish to increase the cache size.]\n");
    return 0;
}

static int
ListCellsCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    afs_int32 i, j, *lp, magic, size;
    char *tp;
    afs_int32 addr, maxa = OMAXHOSTS;
    struct ViceIoctl blob;
    int resolve;

    resolve = !(as->parms[0].items);    /* -numeric */
    
    for(i=0;i<1000;i++) {
	tp = space;
	memcpy(tp, &i, sizeof(afs_int32));
	tp = (char *)(space + sizeof(afs_int32));
	lp = (afs_int32 *)tp;
	*lp++ = 0x12345678;
	size = sizeof(afs_int32) + sizeof(afs_int32);
	blob.out_size = MAXSIZE;
	blob.in_size = sizeof(afs_int32);
	blob.in = space;
	blob.out = space;
	code = pioctl(0, VIOCGETCELL, &blob, 1);
	if (code < 0) {
	    if (errno == EDOM) 
                break;	/* done with the list */
            Die(errno, 0);
            return 1;
	}       
	tp = space;
	memcpy(&magic, tp, sizeof(afs_int32));	
	if (magic == 0x12345678) {
	    maxa = MAXHOSTS;
	    tp += sizeof(afs_int32);
	}
	printf("Cell %s on hosts", tp+maxa*sizeof(afs_int32));
	for(j=0; j < maxa; j++) {
            char *name, tbuffer[20];

	    memcpy(&addr, tp + j*sizeof(afs_int32), sizeof(afs_int32));
	    if (addr == 0) 
                break;

            if (resolve) {
                name = hostutil_GetNameByINet(addr);
            } else {
                addr = ntohl(addr);
                sprintf(tbuffer, "%d.%d.%d.%d", (addr >> 24) & 0xff,
                         (addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff);
                name = tbuffer;
            }
	    printf(" %s", name);
	}
	printf(".\n");
    }
    return 0;
}

#ifndef WIN32
static int
ListAliasesCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code, i;
    char *tp, *aliasName, *realName;
    struct ViceIoctl blob;

    for (i = 0;; i++) {
	tp = space;
	memcpy(tp, &i, sizeof(afs_int32));
	blob.out_size = MAXSIZE;
	blob.in_size = sizeof(afs_int32);
	blob.in = space;
	blob.out = space;
	code = pioctl(0, VIOC_GETALIAS, &blob, 1);
	if (code < 0) {
	    if (errno == EDOM)
		break;		/* done with the list */
	    Die(errno, 0);
	    return 1;
	}
	tp = space;
	aliasName = tp;
	tp += strlen(aliasName) + 1;
	realName = tp;
	printf("Alias %s for cell %s\n", aliasName, realName);
    }
    return 0;
}

static int
CallBackRxConnCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    afs_int32 hostAddr;
    struct hostent *thp;
    char *tp;
    int setp;
    
    ti = as->parms[0].items;
    setp = 1;
    if (ti) {
        thp = hostutil_GetHostByName(ti->data);
	if (!thp) {
	    fprintf(stderr, "host %s not found in host table.\n", ti->data);
	    return 1;
	}
	else memcpy(&hostAddr, thp->h_addr, sizeof(afs_int32));
    } else {
        hostAddr = 0;   /* means don't set host */
	setp = 0;       /* aren't setting host */
    }
    
    /* now do operation */
    blob.in_size = sizeof(afs_int32);
    blob.out_size = sizeof(afs_int32);
    blob.in = (char *) &hostAddr;
    blob.out = (char *) &hostAddr;
    
    code = pioctl(0, VIOC_CBADDR, &blob, 1);
    if (code < 0) {
	Die(errno, 0);
	return 1;
    }
    return 0;
}
#endif /* WIN32 */

static int
NewCellCmd(struct cmd_syndesc *as, char *arock)
{
#ifndef WIN32
    afs_int32 code, linkedstate=0, size=0, *lp;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char *tp, *cellname=0;
    struct hostent *thp;
    afs_int32 fsport = 0, vlport = 0;

    memset(space, 0, MAXHOSTS * sizeof(afs_int32));
    tp = space;
    lp = (afs_int32 *)tp;
    *lp++ = 0x12345678;
    tp += sizeof(afs_int32);
    for(ti=as->parms[1].items; ti; ti=ti->next) {
	thp = hostutil_GetHostByName(ti->data);
	if (!thp) {
	    fprintf(stderr,"%s: Host %s not found in host table, skipping it.\n",
		   pn, ti->data);
	}
	else {
	    memcpy(tp, thp->h_addr, sizeof(afs_int32));
	    tp += sizeof(afs_int32);
	}
    }
    if (as->parms[2].items) {
	/*
	 * Link the cell, for the purposes of volume location, to the specified
	 * cell.
	 */
	cellname = as->parms[2].items->data;
	linkedstate = 1;
    }
#ifdef FS_ENABLE_SERVER_DEBUG_PORTS
    if (as->parms[3].items) {
	code = util_GetInt32(as->parms[3].items->data, &vlport);
	if (code) {
	    fprintf(stderr,"fs: bad integer specified for the fileserver port.\n");
	    return code;
	}
    }
    if (as->parms[4].items) {
	code = util_GetInt32(as->parms[4].items->data, &fsport);
	if (code) {
	    fprintf(stderr,"fs: bad integer specified for the vldb server port.\n");
	    return code;
	}
    }
#endif
    tp = (char *)(space + (MAXHOSTS+1) *sizeof(afs_int32));
    lp = (afs_int32 *)tp;    
    *lp++ = fsport;
    *lp++ = vlport;
    *lp = linkedstate;
    strcpy(space +  ((MAXHOSTS+4) * sizeof(afs_int32)), as->parms[0].items->data);
    size = ((MAXHOSTS+4) * sizeof(afs_int32)) + strlen(as->parms[0].items->data) + 1 /* for null */;
    tp = (char *)(space + size);
    if (linkedstate) {
	strcpy(tp, cellname);
	size += strlen(cellname) + 1;
    }
    blob.in_size = size;
    blob.in = space;
    blob.out_size = 0;
    code = pioctl(0, VIOCNEWCELL, &blob, 1);
    if (code < 0)
	Die(errno, 0);
    return 0;
#else /* WIN32 */
    afs_int32 code;
    struct ViceIoctl blob;
    
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }

    blob.in_size = 0;
    blob.in = (char *) 0;
    blob.out_size = MAXSIZE;
    blob.out = space;

    code = pioctl((char *) 0, VIOCNEWCELL, &blob, 1);

    if (code) {
        Die(errno, (char *) 0);
        return 1;
    }
    
    printf("Cell servers information refreshed\n");
    return 0;
#endif /* WIN32 */
}

#ifndef WIN32
static int
NewAliasCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    char *tp;
    char *aliasName, *realName;

    /* Setup and do the NEWALIAS pioctl call */
    aliasName = as->parms[0].items->data;
    realName = as->parms[1].items->data;
    tp = space;
    strcpy(tp, aliasName);
    tp += strlen(aliasName) + 1;
    strcpy(tp, realName);
    tp += strlen(realName) + 1;

    blob.in_size = tp - space;
    blob.in = space;
    blob.out_size = 0;
    blob.out = space;
    code = pioctl(0, VIOC_NEWALIAS, &blob, 1);
    if (code < 0) {
	if (errno == EEXIST) {
	    fprintf(stderr,
		    "%s: cell name `%s' in use by an existing cell.\n", pn,
		    aliasName);
	} else {
	    Die(errno, 0);
	}
	return 1;
    }
    return 0;
}
#endif /* WIN32 */

static int
WhichCellCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct cmd_item *ti;
    int error = 0;
    char cell[MAXCELLCHARS]="";
    
    SetDotDefault(&as->parms[0].items);
    for(ti=as->parms[0].items; ti; ti=ti->next) {
        code = GetCell(ti->data, cell);
	if (code) {
	    if (errno == ENOENT)
		fprintf(stderr,"%s: no such cell as '%s'\n", pn, ti->data);
	    else
		Die(errno, ti->data);
	    error = 1;
	    continue;
	}
        printf("File %s lives in cell '%s'\n", ti->data, cell);
    }
    return error;
}

static int
WSCellCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    
    blob.in_size = 0;
    blob.in = NULL;
    blob.out_size = MAXSIZE;
    blob.out = space;

    code = pioctl(NULL, VIOC_GET_WS_CELL, &blob, 1);

    if (code) {
	Die(errno, NULL);
        return 1;
    }

    printf("This workstation belongs to cell '%s'\n", space);
    return 0;
}

/*
static int
PrimaryCellCmd(struct cmd_syndesc *as, char *arock)
{
    fprintf(stderr,"This command is obsolete, as is the concept of a primary token.\n");
    return 0;
}
*/

static int
MonitorCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    afs_int32 hostAddr;
    struct hostent *thp;
    char *tp;
    int setp;
    
    ti = as->parms[0].items;
    setp = 1;
    if (ti) {
	/* set the host */
	if (!strcmp(ti->data, "off")) {
	    hostAddr = 0xffffffff;
	} else {
	    thp = hostutil_GetHostByName(ti->data);
	    if (!thp) {
		if (!strcmp(ti->data, "localhost")) {
		    fprintf(stderr,"localhost not in host table, assuming 127.0.0.1\n");
		    hostAddr = htonl(0x7f000001);
		} else {
		    fprintf(stderr,"host %s not found in host table.\n", ti->data);
		    return 1;
		}
	    } else {
                memcpy(&hostAddr, thp->h_addr, sizeof(afs_int32));
            }
	}
    } else {
	hostAddr = 0;	/* means don't set host */
	setp = 0;	/* aren't setting host */
    }

    /* now do operation */
    blob.in_size = sizeof(afs_int32);
    blob.out_size = sizeof(afs_int32);
    blob.in = (char *) &hostAddr;
    blob.out = (char *) &hostAddr;
    code = pioctl(0, VIOC_AFS_MARINER_HOST, &blob, 1);
    if (code) {
	Die(errno, 0);
	return 1;
    }
    if (setp) {
	printf("%s: new monitor host set.\n", pn);
    } else {
	/* now decode old address */
	if (hostAddr == 0xffffffff) {
	    printf("Cache monitoring is currently disabled.\n");
	} else {
	    tp = hostutil_GetNameByINet(hostAddr);
	    printf("Using host %s for monitor services.\n", tp);
	}
    }
    return 0;
}

static int
SysNameCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char *input = space;
    afs_int32 setp = 0;
    
    ti = as->parms[0].items;
    if (ti) {
#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#else /* WIN32 */
    if (geteuid()) {
        fprintf (stderr,"Permission denied: requires root access.\n");
        return EACCES;
    }
#endif /* WIN32 */
    }

    blob.in = space;
    blob.out = space;
    blob.out_size = MAXSIZE;
    blob.in_size = sizeof(afs_int32);
    memcpy(input, &setp, sizeof(afs_int32));
    input += sizeof(afs_int32);
    for (; ti; ti = ti->next) {
        setp++;
        blob.in_size += (long)strlen(ti->data) + 1;
        if (blob.in_size > MAXSIZE) {
            fprintf(stderr, "%s: sysname%s too long.\n", pn,
                     setp > 1 ? "s" : "");
            return 1;
        }
        strcpy(input, ti->data);
        input += strlen(ti->data);
        *(input++) = '\0';
    }
    memcpy(space, &setp, sizeof(afs_int32));
    code = pioctl(0, VIOC_AFS_SYSNAME, &blob, 1);
    if (code) {
        Die(errno, 0);
        return 1;
    }    
    if (setp) {
        printf("%s: new sysname%s set.\n", pn, setp > 1 ? " list" : "");
        return 0;
    }

    input = space;
    memcpy(&setp, input, sizeof(afs_int32));
    input += sizeof(afs_int32);
    if (!setp) {
        fprintf(stderr,"No sysname name value was found\n");
        return 1;
    } 
    
    printf("Current sysname%s is", setp > 1 ? " list" : "");
    for (; setp > 0; --setp ) {
        printf(" \'%s\'", input);
        input += strlen(input) + 1;
    }
    printf("\n");
    return 0;
}

static char *exported_types[] = {"null", "nfs", ""};
static int ExportAfsCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    int export = 0, type = 0, mode = 0, exp = 0, gstat = 0;
    int exportcall, pwsync = 0, smounts = 0;
    
#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#else /* WIN32 */
    if (geteuid()) {
        fprintf (stderr,"Permission denied: requires root access.\n");
        return EACCES;
    }
#endif /* WIN32 */

    ti = as->parms[0].items;
    if (strcmp(ti->data, "nfs")	== 0) 
        type = 0x71; /* NFS */
    else {
	fprintf(stderr,
                "Invalid exporter type, '%s', Only the 'nfs' exporter is currently supported\n", ti->data);
	return 1;
    }
    ti = as->parms[1].items;
    if (ti) {
	if (strcmp(ti->data, "on") == 0) 
            export = 3;
	else if (strcmp(ti->data, "off") == 0) 
            export = 2;
	else {
	    fprintf(stderr, "Illegal argument %s\n", ti->data);
	    return 1;
	}
	exp = 1;
    }
    if (ti = as->parms[2].items) {	/* -noconvert */
	if (strcmp(ti->data, "on") == 0) 
            mode = 2;
	else if (strcmp(ti->data, "off") == 0) 
            mode = 3;
	else {
	    fprintf(stderr, "Illegal argument %s\n", ti->data);
	    return 1;
	}
    }
    if (ti = as->parms[3].items) {	/* -uidcheck */
	if (strcmp(ti->data, "on") == 0) 
            pwsync = 3;
	else if (strcmp(ti->data, "off") == 0) 
            pwsync = 2;
	else {
	    fprintf(stderr, "Illegal argument %s\n", ti->data);
	    return 1;
	}
    }
    if (ti = as->parms[4].items) {	/* -submounts */
	if (strcmp(ti->data, "on") == 0) 
            smounts = 3;
	else if (strcmp(ti->data, "off") == 0) 
            smounts = 2;
	else {
	    fprintf(stderr, "Illegal argument %s\n", ti->data);
	    return 1;
	}
    }
    exportcall =  (type << 24) | (mode << 6) | (pwsync << 4) | (smounts << 2) | export;
    type &= ~0x70;
    /* make the call */
    blob.in = (char *) &exportcall;
    blob.in_size = sizeof(afs_int32);
    blob.out = (char *) &exportcall;
    blob.out_size = sizeof(afs_int32);
    code = pioctl(0, VIOC_EXPORTAFS, &blob, 1);
    if (code) {
	if (errno == ENODEV) {
	    fprintf(stderr,
                    "Sorry, the %s-exporter type is currently not supported on this AFS client\n", exported_types[type]);
	} else {
	    Die(errno, 0);
	}
        return 1;
    } else {
	if (!gstat) {
	    if (exportcall & 1) {
		printf("'%s' translator is enabled with the following options:\n\tRunning in %s mode\n\tRunning in %s mode\n\t%s\n", 
		       exported_types[type], (exportcall & 2 ? "strict unix" : "convert owner mode bits to world/other"),
		       (exportcall & 4 ? "strict 'passwd sync'" : "no 'passwd sync'"),
		       (exportcall & 8 ? "Allow mounts of /afs/.. subdirs" : "Only mounts to /afs allowed"));
	    } else {
		printf("'%s' translator is disabled\n", exported_types[type]);
	    }
	}
    }
    return 0;
}


static int
GetCellCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct afsconf_cell info;
    struct cmd_item *ti;
    struct a {
	afs_int32 stat;
	afs_int32 junk;
    } args;
    int error = 0;

    memset(&args, 0, sizeof(args));      /* avoid Purify UMR error */
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per cell */
	blob.out_size = sizeof(args);
	blob.out = (caddr_t) &args;
	code = GetCellName(ti->data, &info);
	if (code) {
            error = 1;
	    continue;
	}
	blob.in_size = 1+(long)strlen(info.name);
	blob.in = info.name;
	code = pioctl(0, VIOC_GETCELLSTATUS, &blob, 1);
	if (code) {
	    if (errno == ENOENT)
		fprintf(stderr,"%s: the cell named '%s' does not exist\n", pn, info.name);
	    else
		Die(errno, info.name);
	    error = 1;
            continue;
	}
	printf("Cell %s status: ", info.name);
#ifdef notdef
	if (args.stat & 1) 
            printf("primary ");
#endif
	if (args.stat & 2) 
            printf("no setuid allowed");
	else 
            printf("setuid allowed");
	if (args.stat & 4) 
            printf(", using old VLDB");
	printf("\n");
    }
    return error;
}

static int SetCellCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct afsconf_cell info;
    struct cmd_item *ti;
    struct a {
	afs_int32 stat;
	afs_int32 junk;
	char cname[64];
    } args;
    int error = 0;

    /* Check arguments. */
    if (as->parms[1].items && as->parms[2].items) {
        fprintf(stderr, "Cannot specify both -suid and -nosuid.\n");
        return 1;
    }

    /* figure stuff to set */
    args.stat = 0;
    args.junk = 0;

#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#else /* WIN32 */
    if (geteuid()) {
        fprintf (stderr,"Permission denied: requires root access.\n");
        return EACCES;
    }
#endif /* WIN32 */

    if (! as->parms[1].items) 
        args.stat |= CM_SETCELLFLAG_SUID; /* default to -nosuid */

    /* set stat for all listed cells */
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per cell */
	code = GetCellName(ti->data, &info);
	if (code) {
            error = 1;
	    continue;
	}
	strcpy(args.cname, info.name);
	blob.in_size = sizeof(args);
	blob.in = (caddr_t) &args;
	blob.out_size = 0;
	blob.out = (caddr_t) 0;
	code = pioctl(0, VIOC_SETCELLSTATUS, &blob, 1);
	if (code) {
	    Die(errno, info.name);      /* XXX added cell name to Die() call */
            error = 1;
	}
    }
    return error;
}

#ifdef WIN32
static int
GetCellName(char *cellNamep, struct afsconf_cell *infop)
{
    strcpy(infop->name, cellNamep);
    return 0;
}
#else
static int
GetCellName(char *cellName, struct afsconf_cell *info)
{
    struct afsconf_dir *tdir;
    int code;

    tdir = afsconf_Open(AFSDIR_CLIENT_ETC_CLIENTNAME);
    if (!tdir) {
	fprintf(stderr,
                "Could not process files in configuration directory (%s).\n",
                 AFSDIR_CLIENT_ETC_CLIENTNAME);
	return -1;
    }

    code = afsconf_GetCellInfo(tdir, cellName, AFSCONF_VLDBSERVICE, info);
    if (code) {
	fprintf(stderr,"fs: cell %s not in %s/CellServDB\n", cellName, 
                AFSDIR_CLIENT_ETC_CLIENTNAME);
	return code;
    }

    return 0;
}
#endif /* not WIN32 */

static int
VLDBInit(int noAuthFlag, struct afsconf_cell *info)
{
    afs_int32 code;

    code = ugen_ClientInit(noAuthFlag, AFSDIR_CLIENT_ETC_DIRPATH, 
			   info->name, 0, &uclient, 
                           NULL, pn, rxkad_clear,
                           VLDB_MAXSERVERS, AFSCONF_VLDBSERVICE, 50,
                           0, 0, USER_SERVICE_ID);
    rxInitDone = 1;
    return code;
}

static struct ViceIoctl gblob;
static int debug = 0;
/* 
 * here follow some routines in suport of the setserverprefs and
 * getserverprefs commands.  They are:
 * SetPrefCmd  "top-level" routine
 * addServer   adds a server to the list of servers to be poked into the
 *             kernel.  Will poke the list into the kernel if it threatens
 *             to get too large.
 * pokeServers pokes the existing list of servers and ranks into the kernel
 * GetPrefCmd  reads the Cache Manager's current list of server ranks
 */

#ifdef WIN32
static int 
pokeServers(void)
{
    int code;
    cm_SSetPref_t *ssp;
    code = pioctl(0, VIOC_SETSPREFS, &gblob, 1);

    ssp = (cm_SSetPref_t *)space;
    gblob.in_size = (long)(((char *)&(ssp->servers[0])) - (char *)ssp);
    gblob.in = space;
    return code;
}
#else
/*
 * returns -1 if error message printed,
 * 0 on success,
 * errno value if error and no error message printed
 */
static int
pokeServers(void)
{
    int code;

    code = pioctl(0, VIOC_SETSPREFS, &gblob, 1);
    if (code && (errno == EINVAL)) {
	struct setspref *ssp;
	ssp = (struct setspref *)gblob.in;
	if (!(ssp->flags & DBservers)) {
	    gblob.in = (void *)&(ssp->servers[0]);
	    gblob.in_size -= ((char *)&(ssp->servers[0])) - (char *)ssp;
	    code = pioctl(0, VIOC_SETSPREFS33, &gblob, 1);
	    return code ? errno : 0;
	}
	fprintf(stderr,
		"This cache manager does not support VL server preferences.\n");
	return -1;
    }

    return code ? errno : 0;
}
#endif /* WIN32 */

#ifdef WIN32
static int
addServer(char *name, unsigned short rank)
{  
    int code;
    cm_SSetPref_t *ssp;
    cm_SPref_t *sp;
    struct hostent *thostent;

#ifndef MAXUSHORT
#ifdef MAXSHORT
#define MAXUSHORT ((unsigned short) 2*MAXSHORT+1)  /* assumes two's complement binary system */
#else
#define MAXUSHORT ((unsigned short) ~0)
#endif
#endif

    code = 0;
    thostent = hostutil_GetHostByName(name);
    if (!thostent) {
        fprintf (stderr, "%s: couldn't resolve name.\n", name);
        return EINVAL;
    }

    ssp = (cm_SSetPref_t *)(gblob.in);

    if (gblob.in_size > MAXINSIZE - sizeof(cm_SPref_t)) {
        code = pokeServers();
        ssp->num_servers = 0;
    }

    sp = (cm_SPref_t *)((char*)gblob.in + gblob.in_size);
    memcpy (&(sp->host.s_addr), thostent->h_addr, sizeof(afs_uint32));
    sp->rank = (rank > MAXUSHORT ? MAXUSHORT : rank);
    gblob.in_size += sizeof(cm_SPref_t);
    ssp->num_servers++;

    if (debug) fprintf(stderr, "adding server %s, rank %d, ip addr 0x%lx\n",name,sp->rank,sp->host.s_addr);

    return code;
}
#else
/*
 * returns -1 if error message printed,
 * 0 on success,
 * errno value if error and no error message printed
 */
static int
addServer(char *name, afs_int32 rank)
{
    int t, code;
    struct setspref *ssp;
    struct spref *sp;
    struct hostent *thostent;
    afs_uint32 addr;
    int error = 0;

#ifndef MAXUSHORT
#ifdef MAXSHORT
#define MAXUSHORT ((unsigned short) 2*MAXSHORT+1)	/* assumes two's complement binary system */
#else
#define MAXUSHORT ((unsigned short) ~0)
#endif
#endif

    thostent = hostutil_GetHostByName(name);
    if (!thostent) {
	fprintf(stderr, "%s: couldn't resolve name.\n", name);
	return -1;
    }

    ssp = (struct setspref *)(gblob.in);

    for (t = 0; thostent->h_addr_list[t]; t++) {
	if (gblob.in_size > MAXINSIZE - sizeof(struct spref)) {
	    code = pokeServers();
	    if (code)
		error = code;
	    ssp->num_servers = 0;
	}

	sp = (struct spref *)(gblob.in + gblob.in_size);
	memcpy(&(sp->server.s_addr), thostent->h_addr_list[t],
	       sizeof(afs_uint32));
	sp->rank = (rank > MAXUSHORT ? MAXUSHORT : rank);
	gblob.in_size += sizeof(struct spref);
	ssp->num_servers++;

	if (debug)
	    fprintf(stderr, "adding server %s, rank %d, ip addr 0x%lx\n",
		    name, sp->rank, sp->server.s_addr);
    }

    return error;
}
#endif /* WIN32 */

#ifdef WIN32
static BOOL IsWindowsNT (void)
{
    static BOOL fChecked = FALSE;
    static BOOL fIsWinNT = FALSE;

    if (!fChecked)
    {
        OSVERSIONINFO Version;

        fChecked = TRUE;

        memset (&Version, 0x00, sizeof(Version));
        Version.dwOSVersionInfoSize = sizeof(Version);

        if (GetVersionEx (&Version))
        {
            if (Version.dwPlatformId == VER_PLATFORM_WIN32_NT)
                fIsWinNT = TRUE;
        }
    }
    return fIsWinNT;
}
#endif /* WIN32 */

#ifdef WIN32
static int
SetPrefCmd(struct cmd_syndesc *as, char * arock)
{
    FILE *infd;
    afs_int32 code;
    struct cmd_item *ti;
    char name[80];
    afs_int32 rank;
    cm_SSetPref_t *ssp;
    
    ssp = (cm_SSetPref_t *)space;
    ssp->flags = 0;
    ssp->num_servers = 0;
    gblob.in_size = (long)(((char*)&(ssp->servers[0])) - (char *)ssp);
    gblob.in = space;
    gblob.out = space;
    gblob.out_size = MAXSIZE;

    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }

    code = 0;

    ti = as->parms[2].items;  /* -file */
    if (ti) {
        if (debug) fprintf(stderr,"opening file %s\n",ti->data);
        if (!(infd = fopen(ti->data,"r" ))) {
            code = errno;
            Die(errno,ti->data);
        }
        else
            while ( fscanf(infd, "%79s%ld", name, &rank) != EOF) {
                code = addServer (name, (unsigned short) rank);
            }
    }

    ti = as->parms[3].items;  /* -stdin */
    if (ti) {
        while ( scanf("%79s%ld", name, &rank) != EOF) {
            code = addServer (name, (unsigned short) rank);
        }
    }

    for (ti = as->parms[0].items;ti;ti=ti->next) {/*list of servers, ranks */
        if (ti) {
            if (!ti->next) {
                break;
            }
            code = addServer (ti->data, (unsigned short) atol(ti->next->data));
            if (debug)
                printf("set fs prefs %s %s\n", ti->data, ti->next->data);
            ti=ti->next;
        }
    }
    code = pokeServers();
    if (debug) 
        printf("now working on vlservers, code=%d, errno=%d\n",code,errno);

    ssp = (cm_SSetPref_t *)space;
    gblob.in_size = (long)(((char*)&(ssp->servers[0])) - (char *)ssp);
    gblob.in = space;
    ssp->flags = CM_SPREF_VLONLY;
    ssp->num_servers = 0;

    for (ti = as->parms[1].items;ti;ti=ti->next) { /* list of dbservers, ranks */
        if (ti) {
            if (!ti->next) {
                break;
            }
            code = addServer (ti->data, (unsigned short) atol(ti->next->data));
            if (debug) 
                printf("set vl prefs %s %s\n", ti->data, ti->next->data);
            ti=ti->next;
        }
    }

    if (as->parms[1].items) {
        if (debug) 
            printf("now poking vlservers\n");
        code = pokeServers();
    }

    if (code) 
        Die(errno,0);

    return code;
}
#else
static int
SetPrefCmd(struct cmd_syndesc *as, char *arock)
{
    FILE *infd;
    afs_int32 code;
    struct cmd_item *ti;
    char name[80];
    afs_int32 rank;
    struct setspref *ssp;
    int error = 0;		/* -1 means error message printed,
				 * >0 means errno value for unprinted message */

    ssp = (struct setspref *)space;
    ssp->flags = 0;
    ssp->num_servers = 0;
    gblob.in_size = ((char *)&(ssp->servers[0])) - (char *)ssp;
    gblob.in = space;
    gblob.out = space;
    gblob.out_size = MAXSIZE;


    if (geteuid()) {
	fprintf(stderr, "Permission denied: requires root access.\n");
	return 1;
    }

    ti = as->parms[2].items;	/* -file */
    if (ti) {
	if (debug)
	    fprintf(stderr, "opening file %s\n", ti->data);
	if (!(infd = fopen(ti->data, "r"))) {
	    perror(ti->data);
	    error = -1;
	} else {
	    while (fscanf(infd, "%79s%ld", name, &rank) != EOF) {
		code = addServer(name, (unsigned short)rank);
		if (code)
		    error = code;
	    }
	}
    }

    ti = as->parms[3].items;	/* -stdin */
    if (ti) {
	while (scanf("%79s%ld", name, &rank) != EOF) {
	    code = addServer(name, (unsigned short)rank);
	    if (code)
		error = code;
	}
    }

    for (ti = as->parms[0].items; ti; ti = ti->next) {	/* list of servers, ranks */
	if (ti) {
	    if (!ti->next) {
		break;
	    }
	    code = addServer(ti->data, (unsigned short)atol(ti->next->data));
	    if (code)
		error = code;
	    if (debug)
		printf("set fs prefs %s %s\n", ti->data, ti->next->data);
	    ti = ti->next;
	}
    }
    code = pokeServers();
    if (code)
	error = code;
    if (debug)
	printf("now working on vlservers, code=%d\n", code);

    ssp = (struct setspref *)space;
    ssp->flags = DBservers;
    ssp->num_servers = 0;
    gblob.in_size = ((char *)&(ssp->servers[0])) - (char *)ssp;
    gblob.in = space;

    for (ti = as->parms[1].items; ti; ti = ti->next) {	/* list of dbservers, ranks */
	if (ti) {
	    if (!ti->next) {
		break;
	    }
	    code = addServer(ti->data, (unsigned short)atol(ti->next->data));
	    if (code)
		error = code;
	    if (debug)
		printf("set vl prefs %s %s\n", ti->data, ti->next->data);
	    ti = ti->next;
	}
    }

    if (as->parms[1].items) {
	if (debug)
	    printf("now poking vlservers\n");
	code = pokeServers();
	if (code)
	    error = code;
    }

    if (error > 0)
	Die(error, 0);

    return error ? 1 : 0;
}
#endif /* WIN32 */

#ifdef WIN32
static int 
GetPrefCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct cmd_item *ti;
    char *name, tbuffer[20];
    afs_int32 addr;
    FILE * outfd;
    int resolve;
    int vlservers;
    struct ViceIoctl blob;
    struct cm_SPrefRequest *in;
    struct cm_SPrefInfo *out;
    int i;
    
    code = 0;
    ti = as->parms[0].items;  /* -file */
    if (ti) {
        if (debug) fprintf(stderr,"opening file %s\n",ti->data);
        if (!(outfd = freopen(ti->data,"w",stdout))) {
            Die(errno,ti->data);
            return errno;
        }
    }

    ti = as->parms[1].items;  /* -numeric */
    resolve = !(ti);
    ti = as->parms[2].items;  /* -vlservers */
    vlservers = (ti ? CM_SPREF_VLONLY : 0);
    /*  ti = as->parms[3].items;   -cell */

    in = (struct cm_SPrefRequest *)space;
    in->offset = 0;

    do {
        blob.in_size=sizeof(struct cm_SPrefRequest);
        blob.in = (char *)in;
        blob.out = space;
        blob.out_size = MAXSIZE;

        in->num_servers = (MAXSIZE - 2*sizeof(short))/sizeof(struct cm_SPref);
        in->flags = vlservers; 

        code = pioctl(0, VIOC_GETSPREFS, &blob, 1);
        if (code){
            perror("getserverprefs pioctl");
            Die (errno,0);
        }
        else {
            out = (struct cm_SPrefInfo *) blob.out;

            for (i=0;i<out->num_servers;i++) {
                if (resolve) {
                    name = hostutil_GetNameByINet(out->servers[i].host.s_addr);
                }
                else {
                    addr = ntohl(out->servers[i].host.s_addr);
                    sprintf(tbuffer, "%d.%d.%d.%d", (addr>>24) & 0xff, (addr>>16) & 0xff,
                             (addr>>8) & 0xff, addr & 0xff);
                    name=tbuffer;
                }
                printf ("%-50s %5u\n",name,out->servers[i].rank);      
            }

            in->offset = out->next_offset;
        }
    } while (!code && out->next_offset > 0);

    return code;
}
#else
static int
GetPrefCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct cmd_item *ti;
    char *name, tbuffer[20];
    afs_int32 rank, addr;
    FILE *outfd;
    int resolve;
    int vlservers = 0;
    struct ViceIoctl blob;
    struct sprefrequest *in;
    struct sprefinfo *out;
    int i;

    ti = as->parms[0].items;	/* -file */
    if (ti) {
	if (debug)
	    fprintf(stderr, "opening file %s\n", ti->data);
	if (!(outfd = freopen(ti->data, "w", stdout))) {
	    perror(ti->data);
	    return 1;
	}
    }

    ti = as->parms[1].items;	/* -numeric */
    resolve = !(ti);
    ti = as->parms[2].items;	/* -vlservers */
    vlservers |= (ti ? DBservers : 0);
    /*  ti = as->parms[3].items;   -cell */

    in = (struct sprefrequest *)space;
    in->offset = 0;

    do {
	blob.in_size = sizeof(struct sprefrequest);
	blob.in = (char *)in;
	blob.out = space;
	blob.out_size = MAXSIZE;

	in->num_servers =
	    (MAXSIZE - 2 * sizeof(short)) / sizeof(struct spref);
	in->flags = vlservers;

	code = pioctl(0, VIOC_GETSPREFS, &blob, 1);
	if (code) {
	    perror("getserverprefs pioctl");
	    return 1;
	}

	out = (struct sprefinfo *)blob.out;

	for (i = 0; i < out->num_servers; i++) {
	    if (resolve) {
		name = hostutil_GetNameByINet(out->servers[i].server.s_addr);
	    } else {
		addr = ntohl(out->servers[i].server.s_addr);
		sprintf(tbuffer, "%d.%d.%d.%d", (addr >> 24) & 0xff,
			(addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff);
		name = tbuffer;
	    }
	    printf("%-50s %5u\n", name, out->servers[i].rank);
	}

	in->offset = out->next_offset;
    } while (out->next_offset > 0);

    return 0;
}
#endif /* WIN32 */

static int
UuidCmd(struct cmd_syndesc *asp, char *arock)
{
    long code;
    long inValue;
    afsUUID outValue;
    struct ViceIoctl blob;
    char * uuidstring = NULL;

#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#else
    if (geteuid()) {
        fprintf (stderr, "Permission denied: requires root access.\n");
        return EACCES;
    }
#endif

    if (asp->parms[0].items) {
        inValue = 1;            /* generate new UUID */
    } else {
        inValue = 0;            /* just show the current UUID */
    }

    blob.in_size = sizeof(inValue);
    blob.in = (char *) &inValue;
    blob.out_size = sizeof(outValue);
    blob.out = (char *) &outValue;

    code = pioctl(NULL, VIOC_UUIDCTL, &blob, 1);
    if (code) {
        Die(errno, NULL);
        return code;
    }

    UuidToString((UUID *) &outValue, &uuidstring);

    printf("%sUUID: %s",
           ((inValue == 1)?"New ":""),
           uuidstring);

    if (uuidstring)
        RpcStringFree(&uuidstring);

    return 0;
}

static int
TraceCmd(struct cmd_syndesc *asp, char *arock)
{
    long code;
    struct ViceIoctl blob;
    long inValue;
    long outValue;
    
#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#else /* WIN32 */
        if (geteuid()) {
            fprintf (stderr,"Permission denied: requires root access.\n");
            return EACCES;
        }
#endif /* WIN32 */

    if ((asp->parms[0].items && asp->parms[1].items)) {
        fprintf(stderr, "fs trace: must use at most one of '-off' or '-on'\n");
        return EINVAL;
    }
        
    /* determine if we're turning this tracing on or off */
    inValue = 0;
    if (asp->parms[0].items)
        inValue = 3;		/* enable */
    else if (asp->parms[1].items) 
        inValue = 2;	/* disable */
    if (asp->parms[2].items) 
        inValue |= 4;		/* do reset */
    if (asp->parms[3].items) 
        inValue |= 8;		/* dump */
        
    blob.in_size = sizeof(long);
    blob.in = (char *) &inValue;
    blob.out_size = sizeof(long);
    blob.out = (char *) &outValue;
        
    code = pioctl(NULL, VIOC_TRACECTL, &blob, 1);
    if (code) {
        Die(errno, NULL);
        return code;
    }

    if (outValue) 
        printf("AFS tracing enabled.\n");
    else 
        printf("AFS tracing disabled.\n");

    return 0;
}

static void sbusage(void)
{
    fprintf(stderr, "example usage: %s storebehind -files *.o -kb 99999 -default 0\n", pn);
    fprintf(stderr, "               %s sb 50000 *.[ao] -default 10\n", pn);
}       

/* fs sb -kbytes 9999 -files *.o -default 64 */
static int
StoreBehindCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code = 0;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    struct sbstruct tsb, tsb2;
    int verbose = 0;
    afs_int32 allfiles;
    char *t;
    int error = 0;

#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");  
        return EACCES;
    }
#endif /* WIN32 */

    tsb.sb_thisfile = -1;
    ti = as->parms[0].items;	/* -kbytes */
    if (ti) {
	if (!as->parms[1].items) {
	    fprintf(stderr, "%s: you must specify -files with -kbytes.\n",
		    pn);
	    return 1;
	}
	tsb.sb_thisfile = strtol(ti->data, &t, 10) * 1024;
	if ((tsb.sb_thisfile < 0) || (t != ti->data + strlen(ti->data))) {
	    fprintf(stderr, "%s: %s must be 0 or a positive number.\n", pn,
		    ti->data);
	    return 1;
	}
    }

    allfiles = tsb.sb_default = -1;	/* Don't set allfiles yet */
    ti = as->parms[2].items;	/* -allfiles */
    if (ti) {
	allfiles = strtol(ti->data, &t, 10) * 1024;
	if ((allfiles < 0) || (t != ti->data + strlen(ti->data))) {
	    fprintf(stderr, "%s: %s must be 0 or a positive number.\n", pn,
		    ti->data);
	    return 1;
	}
    }

    /* -verbose or -file only or no options */
    if (as->parms[3].items || (as->parms[1].items && !as->parms[0].items)
	|| (!as->parms[0].items && !as->parms[1].items
	    && !as->parms[2].items))
	verbose = 1;

    blob.in = (char *)&tsb;
    blob.out = (char *)&tsb2;
    blob.in_size = blob.out_size = sizeof(struct sbstruct);
    memset(&tsb2, 0, sizeof(tsb2));

    /* once per -file */
    for (ti = as->parms[1].items; ti; ti = ti->next) {
	/* Do this solely to see if the file is there */
	code = pioctl(ti->data, VIOCWHEREIS, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}

	code = pioctl(ti->data, VIOC_STOREBEHIND, &blob, 1);
	if (code) {
	    Die(errno, ti->data);
	    error = 1;
	    continue;
	}

	if (verbose && (blob.out_size == sizeof(tsb2))) {
	    if (tsb2.sb_thisfile == -1) {
		fprintf(stdout, "Will store %s according to default.\n",
			ti->data);
	    } else {
		fprintf(stdout,
			"Will store up to %d kbytes of %s asynchronously.\n",
			(tsb2.sb_thisfile / 1024), ti->data);
	    }
	}
    }

    /* If no files - make at least one pioctl call, or
     * set the allfiles default if we need to.
     */
    if (!as->parms[1].items || (allfiles != -1)) {
	tsb.sb_default = allfiles;
	code = pioctl(0, VIOC_STOREBEHIND, &blob, 1);
	if (code) {
	    Die(errno, ((allfiles == -1) ? 0 : "-allfiles"));
	    error = 1;
	}
    }

    /* Having no arguments also reports the default store asynchrony */
    if (verbose && (blob.out_size == sizeof(tsb2))) {
	fprintf(stdout, "Default store asynchrony is %d kbytes.\n",
		(tsb2.sb_default / 1024));
    }

    return error;
}

static afs_int32 
SetCryptCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code = 0, flag;
    struct ViceIoctl blob;
    char *tp;
 
#ifdef WIN32
    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }
#endif /* WIN32 */

    tp = as->parms[0].items->data;
    if (strcmp(tp, "on") == 0)
      flag = 1;
    else if (strcmp(tp, "off") == 0)
      flag = 0;
    else {
      fprintf (stderr, "%s: %s must be \"on\" or \"off\".\n", pn, tp);
      return EINVAL;
    }

    blob.in = (char *) &flag;
    blob.in_size = sizeof(flag);
    blob.out_size = 0;
    code = pioctl(0, VIOC_SETRXKCRYPT, &blob, 1);
    if (code)
        Die(code, NULL);
    return 0;
}

static afs_int32 
GetCryptCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code = 0, flag;
    struct ViceIoctl blob;
    char *tp;
 
    blob.in = NULL;
    blob.in_size = 0;
    blob.out_size = sizeof(flag);
    blob.out = space;

    code = pioctl(0, VIOC_GETRXKCRYPT, &blob, 1);

    if (code) 
        Die(code, NULL);
    else {
      tp = space;
      memcpy(&flag, tp, sizeof(afs_int32));
      printf("Security level is currently ");
      if (flag == 1)
        printf("crypt (data security).\n");
      else
        printf("clear.\n");
    }
    return 0;
}

static int
MemDumpCmd(struct cmd_syndesc *asp, char *arock)
{
    long code;
    struct ViceIoctl blob;
    long inValue = 0;
    long outValue;

    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }

    if ((asp->parms[0].items && asp->parms[1].items)) {
        fprintf(stderr, "%s trace: must use at most one of '-begin' or '-end'\n", pn);
        return EINVAL;
    }

    /* determine if we're turning this tracing on or off */
    if (asp->parms[0].items)
        inValue = 1;            /* begin */
    else if (asp->parms[1].items)
        inValue = 0;            /* end */


    blob.in_size = sizeof(long);
    blob.in = (char *) &inValue;
    blob.out_size = sizeof(long);
    blob.out = (char *) &outValue;

    code = pioctl(NULL, VIOC_TRACEMEMDUMP, &blob, 1);
    if (code) {
        Die(errno, NULL);
        return code;
    }

    if (!outValue) { 
        printf("AFS memdump created.\n");
        return 0;
    } else {
        printf("AFS memdump failed.\n");
        return -1;
    }
}

static int
MiniDumpCmd(struct cmd_syndesc *asp, char *arock)
{
    BOOL success = 0;
    SERVICE_STATUS status;
    SC_HANDLE hManager = NULL;
    SC_HANDLE hService = NULL;

    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }

    hManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hManager)
        goto failure;

    hService = OpenService(hManager, "TransarcAFSDaemon", SERVICE_USER_DEFINED_CONTROL);
    if (!hService)
        goto failure;

    success = ControlService(hService, SERVICE_CONTROL_CUSTOM_DUMP, &status);

    if (success) {
        CloseServiceHandle(hService);
        CloseServiceHandle(hManager);

        printf("AFS minidump generated.\n");
        return 0;
    }

  failure: 
    if (hService)
        CloseServiceHandle(hService);
    if (hManager)
        CloseServiceHandle(hManager);

    printf("AFS minidump failed.\n");
    return -1;
}

static int
CSCPolicyCmd(struct cmd_syndesc *asp, char *arock)
{
    struct cmd_item *ti;
    char *share = NULL;
    HKEY hkCSCPolicy;

    if ( !IsAdmin() ) {
        fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
        return EACCES;
    }

    for(ti=asp->parms[0].items; ti;ti=ti->next) {
        share = ti->data;
        if (share)
        {
            break;
        }
    }

    if (share)
    {
        char *policy;

        RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                         AFSREG_CLT_OPENAFS_SUBKEY "\\CSCPolicy",
                        0, 
                        "AFS", 
                        REG_OPTION_NON_VOLATILE,
                        KEY_WRITE,
                        NULL, 
                        &hkCSCPolicy,
                        NULL );

        if ( hkCSCPolicy == NULL ) {
            fprintf (stderr,"Permission denied: requires Administrator access.\n");
            return EACCES;
        }

        if ( !IsAdmin() ) {
            fprintf (stderr,"Permission denied: requires AFS Client Administrator access.\n");
            RegCloseKey(hkCSCPolicy);
            return EACCES;
        }

        policy = "manual";
		
        if (asp->parms[1].items)
            policy = "manual";
        if (asp->parms[2].items)
            policy = "programs";
        if (asp->parms[3].items)
            policy = "documents";
        if (asp->parms[4].items)
            policy = "disable";
		
        RegSetValueEx( hkCSCPolicy, share, 0, REG_SZ, policy, (DWORD)strlen(policy)+1);
		
        printf("CSC policy on share \"%s\" changed to \"%s\".\n\n", share, policy);
        printf("Close all applications that accessed files on this share or restart AFS Client for the change to take effect.\n"); 
    }
    else
    {
        DWORD dwIndex, dwPolicies;
        char policyName[256];
        DWORD policyNameLen;
        char policy[256];
        DWORD policyLen;
        DWORD dwType;

        /* list current csc policies */

        RegCreateKeyEx( HKEY_LOCAL_MACHINE, 
                        AFSREG_CLT_OPENAFS_SUBKEY "\\CSCPolicy",
                        0, 
                        "AFS", 
                        REG_OPTION_NON_VOLATILE,
                        KEY_READ|KEY_QUERY_VALUE,
                        NULL, 
                        &hkCSCPolicy,
                        NULL );

        RegQueryInfoKey( hkCSCPolicy,
                         NULL,  /* lpClass */
                         NULL,  /* lpcClass */
                         NULL,  /* lpReserved */
                         NULL,  /* lpcSubKeys */
                         NULL,  /* lpcMaxSubKeyLen */
                         NULL,  /* lpcMaxClassLen */
                         &dwPolicies, /* lpcValues */
                         NULL,  /* lpcMaxValueNameLen */
                         NULL,  /* lpcMaxValueLen */
                         NULL,  /* lpcbSecurityDescriptor */
                         NULL   /* lpftLastWriteTime */
                         );
		
        printf("Current CSC policies:\n");
        for ( dwIndex = 0; dwIndex < dwPolicies; dwIndex ++ ) {

            policyNameLen = sizeof(policyName);
            policyLen = sizeof(policy);
            RegEnumValue( hkCSCPolicy, dwIndex, policyName, &policyNameLen, NULL,
                          &dwType, policy, &policyLen);

            printf("  %s = %s\n", policyName, policy);
        }
    }

    RegCloseKey(hkCSCPolicy);
    return (0);
}

#ifndef WIN32
/* get clients interface addresses */
static int
GetClientAddrsCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct cmd_item *ti;
    char *name;
    struct ViceIoctl blob;
    struct sprefrequest *in;
    struct sprefinfo *out;

    in = (struct sprefrequest *)space;
    in->offset = 0;

    do {
	blob.in_size = sizeof(struct sprefrequest);
	blob.in = (char *)in;
	blob.out = space;
	blob.out_size = MAXSIZE;

	in->num_servers =
	    (MAXSIZE - 2 * sizeof(short)) / sizeof(struct spref);
	/* returns addr in network byte order */
	code = pioctl(0, VIOC_GETCPREFS, &blob, 1);
	if (code) {
	    perror("getClientInterfaceAddr pioctl");
	    return 1;
	}

	{
	    int i;
	    out = (struct sprefinfo *)blob.out;
	    for (i = 0; i < out->num_servers; i++) {
		afs_int32 addr;
		char tbuffer[32];
		addr = ntohl(out->servers[i].server.s_addr);
		sprintf(tbuffer, "%d.%d.%d.%d", (addr >> 24) & 0xff,
			(addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff);
		printf("%-50s\n", tbuffer);
	    }
	    in->offset = out->next_offset;
	}
    } while (out->next_offset > 0);

    return 0;
}

static int
SetClientAddrsCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code, addr;
    struct cmd_item *ti;
    char name[80];
    struct ViceIoctl blob;
    struct setspref *ssp;
    int sizeUsed = 0, i, flag;
    afs_int32 existingAddr[1024];	/* existing addresses on this host */
    int existNu;
    int error = 0;

    ssp = (struct setspref *)space;
    ssp->num_servers = 0;
    blob.in = space;
    blob.out = space;
    blob.out_size = MAXSIZE;

    if (geteuid()) {
	fprintf(stderr, "Permission denied: requires root access.\n");
	return 1;
    }

    /* extract all existing interface addresses */
    existNu = rx_getAllAddr(existingAddr, 1024);
    if (existNu < 0)
	return 1;

    sizeUsed = sizeof(struct setspref);	/* space used in ioctl buffer */
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	if (sizeUsed >= sizeof(space)) {
	    fprintf(stderr, "No more space\n");
	    return 1;
	}
	addr = extractAddr(ti->data, 20);	/* network order */
	if ((addr == AFS_IPINVALID) || (addr == AFS_IPINVALIDIGNORE)) {
	    fprintf(stderr, "Error in specifying address: %s..ignoring\n",
		    ti->data);
	    error = 1;
	    continue;
	}
	/* see if it is an address that really exists */
	for (flag = 0, i = 0; i < existNu; i++)
	    if (existingAddr[i] == addr) {
		flag = 1;
		break;
	    }
	if (!flag) {		/* this is an nonexistent address */
	    fprintf(stderr, "Nonexistent address: 0x%08x..ignoring\n", addr);
	    error = 1;
	    continue;
	}
	/* copy all specified addr into ioctl buffer */
	(ssp->servers[ssp->num_servers]).server.s_addr = addr;
	printf("Adding 0x%08x\n", addr);
	ssp->num_servers++;
	sizeUsed += sizeof(struct spref);
    }
    if (ssp->num_servers < 1) {
	fprintf(stderr, "No addresses specified\n");
	return 1;
    }
    blob.in_size = sizeUsed - sizeof(struct spref);

    code = pioctl(0, VIOC_SETCPREFS, &blob, 1);	/* network order */
    if (code) {
	Die(errno, 0);
	error = 1;
    }

    return error;
}

static int
FlushMountCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char orig_name[1024];	/*Original name, may be modified */
    char true_name[1024];	/*``True'' dirname (e.g., symlink target) */
    char parent_dir[1024];	/*Parent directory of true name */
    char *last_component;	/*Last component of true name */
    struct stat statbuff;	/*Buffer for status info */
    int link_chars_read;	/*Num chars read in readlink() */
    int thru_symlink;		/*Did we get to a mount point via a symlink? */
    int error = 0;

    for (ti = as->parms[0].items; ti; ti = ti->next) {
	/* once per file */
	thru_symlink = 0;
	sprintf(orig_name, "%s%s", (ti->data[0] == '/') ? "" : "./",
		ti->data);

	if (lstat(orig_name, &statbuff) < 0) {
	    /* if lstat fails, we should still try the pioctl, since it
	     * may work (for example, lstat will fail, but pioctl will
	     * work if the volume of offline (returning ENODEV). */
	    statbuff.st_mode = S_IFDIR;	/* lie like pros */
	}

	/*
	 * The lstat succeeded.  If the given file is a symlink, substitute
	 * the file name with the link name.
	 */
	if ((statbuff.st_mode & S_IFMT) == S_IFLNK) {
	    thru_symlink = 1;
	    /*
	     * Read name of resolved file.
	     */
	    link_chars_read = readlink(orig_name, true_name, 1024);
	    if (link_chars_read <= 0) {
		fprintf(stderr,
			"%s: Can't read target name for '%s' symbolic link!\n",
			pn, orig_name);
		error = 1;
		continue;
	    }

	    /*
	     * Add a trailing null to what was read, bump the length.
	     */
	    true_name[link_chars_read++] = 0;

	    /*
	     * If the symlink is an absolute pathname, we're fine.  Otherwise, we
	     * have to create a full pathname using the original name and the
	     * relative symlink name.  Find the rightmost slash in the original
	     * name (we know there is one) and splice in the symlink value.
	     */
	    if (true_name[0] != '/') {
		last_component = (char *)strrchr(orig_name, '/');
		strcpy(++last_component, true_name);
		strcpy(true_name, orig_name);
	    }
	} else
	    strcpy(true_name, orig_name);

	/*
	 * Find rightmost slash, if any.
	 */
	last_component = (char *)strrchr(true_name, '/');
	if (last_component) {
	    /*
	     * Found it.  Designate everything before it as the parent directory,
	     * everything after it as the final component.
	     */
	    strncpy(parent_dir, true_name, last_component - true_name);
	    parent_dir[last_component - true_name] = 0;
	    last_component++;	/*Skip the slash */
	} else {
	    /*
	     * No slash appears in the given file name.  Set parent_dir to the current
	     * directory, and the last component as the given name.
	     */
	    strcpy(parent_dir, ".");
	    last_component = true_name;
	}

	if (strcmp(last_component, ".") == 0
	    || strcmp(last_component, "..") == 0) {
	    fprintf(stderr,
		    "%s: you may not use '.' or '..' as the last component\n",
		    pn);
	    fprintf(stderr, "%s: of a name in the 'fs flushmount' command.\n",
		    pn);
	    error = 1;
	    continue;
	}

	blob.in = last_component;
	blob.in_size = strlen(last_component) + 1;
	blob.out_size = 0;
	memset(space, 0, MAXSIZE);

	code = pioctl(parent_dir, VIOC_AFS_FLUSHMOUNT, &blob, 1);

	if (code != 0) {
	    if (errno == EINVAL) {
		fprintf(stderr, "'%s' is not a mount point.\n", ti->data);
	    } else {
		Die(errno, (ti->data ? ti->data : parent_dir));
	    }
	    error = 1;
	}
    }
    return error;
}
#endif /* WIN32 */

static int
RxStatProcCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    afs_int32 flags = 0;
    struct ViceIoctl blob;

    if (as->parms[0].items) {	/* -enable */
	flags |= AFSCALL_RXSTATS_ENABLE;
    }
    if (as->parms[1].items) {	/* -disable */
	flags |= AFSCALL_RXSTATS_DISABLE;
    }
    if (as->parms[2].items) {	/* -clear */
	flags |= AFSCALL_RXSTATS_CLEAR;
    }
    if (flags == 0) {
	fprintf(stderr, "You must specify at least one argument\n");
	return 1;
    }

    blob.in = (char *)&flags;
    blob.in_size = sizeof(afs_int32);
    blob.out_size = 0;

    code = pioctl(NULL, VIOC_RXSTAT_PROC, &blob, 1);
    if (code != 0) {
	Die(errno, NULL);
	return 1;
    }

    return 0;
}

static int
RxStatPeerCmd(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    afs_int32 flags = 0;
    struct ViceIoctl blob;

    if (as->parms[0].items) {	/* -enable */
	flags |= AFSCALL_RXSTATS_ENABLE;
    }
    if (as->parms[1].items) {	/* -disable */
	flags |= AFSCALL_RXSTATS_DISABLE;
    }
    if (as->parms[2].items) {	/* -clear */
	flags |= AFSCALL_RXSTATS_CLEAR;
    }
    if (flags == 0) {
	fprintf(stderr, "You must specify at least one argument\n");
	return 1;
    }

    blob.in = (char *)&flags;
    blob.in_size = sizeof(afs_int32);
    blob.out_size = 0;

    code = pioctl(NULL, VIOC_RXSTAT_PEER, &blob, 1);
    if (code != 0) {
	Die(errno, NULL);
	return 1;
    }

    return 0;
}

#ifndef WIN32
#include "AFS_component_version_number.c"
#endif

main(int argc, char **argv)
{
    afs_int32 code;
    struct cmd_syndesc *ts;

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

#ifdef WIN32
    WSADATA WSAjunk;
    WSAStartup(0x0101, &WSAjunk);
#endif /* WIN32 */

    /* try to find volume location information */
    osi_Init();

#ifndef WIN32
    ts = cmd_CreateSyntax("getclientaddrs", GetClientAddrsCmd, 0,
			  "get client network interface addresses");
    cmd_CreateAlias(ts, "gc");

    ts = cmd_CreateSyntax("setclientaddrs", SetClientAddrsCmd, 0,
			  "set client network interface addresses");
    cmd_AddParm(ts, "-address", CMD_LIST, CMD_OPTIONAL | CMD_EXPANDS,
                "client network interfaces");
    cmd_CreateAlias(ts, "sc");
#endif /* WIN32 */

    ts = cmd_CreateSyntax("setserverprefs", SetPrefCmd, 0, "set server ranks");
    cmd_AddParm(ts, "-servers", CMD_LIST, CMD_OPTIONAL|CMD_EXPANDS, "fileserver names and ranks");
    cmd_AddParm(ts, "-vlservers", CMD_LIST, CMD_OPTIONAL|CMD_EXPANDS, "VL server names and ranks");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL, "input from named file");
    cmd_AddParm(ts, "-stdin", CMD_FLAG, CMD_OPTIONAL, "input from stdin");
    cmd_CreateAlias(ts, "sp");

    ts = cmd_CreateSyntax("getserverprefs", GetPrefCmd, 0, "get server ranks");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL, "output to named file");
    cmd_AddParm(ts, "-numeric", CMD_FLAG, CMD_OPTIONAL, "addresses only");
    cmd_AddParm(ts, "-vlservers", CMD_FLAG, CMD_OPTIONAL, "VL servers");
    /* cmd_AddParm(ts, "-cell", CMD_FLAG, CMD_OPTIONAL, "cellname"); */
    cmd_CreateAlias(ts, "gp");

    ts = cmd_CreateSyntax("setacl", SetACLCmd, 0, "set access control list");
    cmd_AddParm(ts, "-dir", CMD_LIST, 0, "directory");
    cmd_AddParm(ts, "-acl", CMD_LIST, 0, "access list entries");
    cmd_AddParm(ts, "-clear", CMD_FLAG, CMD_OPTIONAL, "clear access list");
    cmd_AddParm(ts, "-negative", CMD_FLAG, CMD_OPTIONAL, "apply to negative rights");
    parm_setacl_id = ts->nParms;
    cmd_AddParm(ts, "-id", CMD_FLAG, CMD_OPTIONAL, "initial directory acl (DFS only)");
    cmd_AddParm(ts, "-if", CMD_FLAG, CMD_OPTIONAL, "initial file acl (DFS only)");
    cmd_CreateAlias(ts, "sa");
    
    ts = cmd_CreateSyntax("listacl", ListACLCmd, 0, "list access control list");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    parm_listacl_id = ts->nParms;
    cmd_AddParm(ts, "-id", CMD_FLAG, CMD_OPTIONAL, "initial directory acl");
    cmd_AddParm(ts, "-if", CMD_FLAG, CMD_OPTIONAL, "initial file acl");
    cmd_CreateAlias(ts, "la");
    
    ts = cmd_CreateSyntax("cleanacl", CleanACLCmd, 0, "clean up access control list");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    
    ts = cmd_CreateSyntax("copyacl", CopyACLCmd, 0, "copy access control list");
    cmd_AddParm(ts, "-fromdir", CMD_SINGLE, 0, "source directory (or DFS file)");
    cmd_AddParm(ts, "-todir", CMD_LIST, 0, "destination directory (or DFS file)");
    cmd_AddParm(ts, "-clear", CMD_FLAG, CMD_OPTIONAL, "first clear dest access list");
    parm_copyacl_id = ts->nParms;
    cmd_AddParm(ts, "-id", CMD_FLAG, CMD_OPTIONAL, "initial directory acl");
    cmd_AddParm(ts, "-if", CMD_FLAG, CMD_OPTIONAL, "initial file acl");
    
    cmd_CreateAlias(ts, "ca");

    ts = cmd_CreateSyntax("flush", FlushCmd, 0, "flush file from cache");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    
#ifndef WIN32
    ts = cmd_CreateSyntax("flushmount", FlushMountCmd, 0,
                           "flush mount symlink from cache");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
#endif

    ts = cmd_CreateSyntax("setvol", SetVolCmd, 0, "set volume status");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    cmd_AddParm(ts, "-max", CMD_SINGLE, CMD_OPTIONAL, "disk space quota in 1K units");
#ifdef notdef
    cmd_AddParm(ts, "-min", CMD_SINGLE, CMD_OPTIONAL, "disk space guaranteed");
#endif
    cmd_AddParm(ts, "-motd", CMD_SINGLE, CMD_OPTIONAL, "message of the day");
    cmd_AddParm(ts, "-offlinemsg", CMD_SINGLE, CMD_OPTIONAL, "offline message");
    cmd_CreateAlias(ts, "sv");
    
    ts = cmd_CreateSyntax("messages", MessagesCmd, 0, "control Cache Manager messages");
    cmd_AddParm(ts, "-show", CMD_SINGLE, CMD_OPTIONAL, "[user|console|all|none]");

    ts = cmd_CreateSyntax("examine", ExamineCmd, 0, "display file/volume status");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    cmd_CreateAlias(ts, "lv");
    cmd_CreateAlias(ts, "listvol");
    
    ts = cmd_CreateSyntax("listquota", ListQuotaCmd, 0, "list volume quota");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    cmd_CreateAlias(ts, "lq");
    
    ts = cmd_CreateSyntax("diskfree", DiskFreeCmd, 0, "show server disk space usage");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    cmd_CreateAlias(ts, "df");
    
    ts = cmd_CreateSyntax("quota", QuotaCmd, 0, "show volume quota usage");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");
    
    ts = cmd_CreateSyntax("lsmount", ListMountCmd, 0, "list mount point");    
    cmd_AddParm(ts, "-dir", CMD_LIST, 0, "directory");
    
    ts = cmd_CreateSyntax("mkmount", MakeMountCmd, 0, "make mount point");
    cmd_AddParm(ts, "-dir", CMD_SINGLE, 0, "directory");
    cmd_AddParm(ts, "-vol", CMD_SINGLE, 0, "volume name");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(ts, "-rw", CMD_FLAG, CMD_OPTIONAL, "force r/w volume");
    cmd_AddParm(ts, "-fast", CMD_FLAG, CMD_OPTIONAL, "don't check name with VLDB");

    /*
     *
     * defect 3069
     * 
    cmd_AddParm(ts, "-root", CMD_FLAG, CMD_OPTIONAL, "create cellular mount point");
    */

    
    ts = cmd_CreateSyntax("rmmount", RemoveMountCmd, 0, "remove mount point");
    cmd_AddParm(ts, "-dir", CMD_LIST, 0, "directory");
    
    ts = cmd_CreateSyntax("checkservers", CheckServersCmd, 0, "check local cell's servers");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell to check");
    cmd_AddParm(ts, "-all", CMD_FLAG, CMD_OPTIONAL, "check all cells");
    cmd_AddParm(ts, "-fast", CMD_FLAG, CMD_OPTIONAL, "just list, don't check");
    cmd_AddParm(ts,"-interval",CMD_SINGLE,CMD_OPTIONAL,"seconds between probes");
    
    ts = cmd_CreateSyntax("checkvolumes", CheckVolumesCmd,0, "check volumeID/name mappings");
    cmd_CreateAlias(ts, "checkbackups");

    
    ts = cmd_CreateSyntax("setcachesize", SetCacheSizeCmd, 0, "set cache size");
    cmd_AddParm(ts, "-blocks", CMD_SINGLE, CMD_OPTIONAL, "size in 1K byte blocks (0 => reset)");
    cmd_CreateAlias(ts, "cachesize");

    cmd_AddParm(ts, "-reset", CMD_FLAG, CMD_OPTIONAL, "reset size back to boot value");
    
    ts = cmd_CreateSyntax("getcacheparms", GetCacheParmsCmd, 0, "get cache usage info");

    ts = cmd_CreateSyntax("listcells", ListCellsCmd, 0, "list configured cells");
    cmd_AddParm(ts, "-numeric", CMD_FLAG, CMD_OPTIONAL, "addresses only");
    
    ts = cmd_CreateSyntax("setquota", SetQuotaCmd, 0, "set volume quota");
    cmd_AddParm(ts, "-path", CMD_SINGLE, CMD_OPTIONAL, "dir/file path");
    cmd_AddParm(ts, "-max", CMD_SINGLE, 0, "max quota in kbytes");
#ifdef notdef
    cmd_AddParm(ts, "-min", CMD_SINGLE, CMD_OPTIONAL, "min quota in kbytes");
#endif
    cmd_CreateAlias(ts, "sq");

    ts = cmd_CreateSyntax("newcell", NewCellCmd, 0, "configure new cell");
#ifndef WIN32
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "cell name");
    cmd_AddParm(ts, "-servers", CMD_LIST, CMD_REQUIRED, "primary servers");
    cmd_AddParm(ts, "-linkedcell", CMD_SINGLE, CMD_OPTIONAL, "linked cell name");

#ifdef FS_ENABLE_SERVER_DEBUG_PORTS
    /*
     * Turn this on only if you wish to be able to talk to a server which is listening
     * on alternative ports. This is not intended for general use and may not be
     * supported in the cache manager. It is not a way to run two servers at the
     * same host, since the cache manager cannot properly distinguish those two hosts.
     */
    cmd_AddParm(ts, "-fsport", CMD_SINGLE, CMD_OPTIONAL, "cell's fileserver port");
    cmd_AddParm(ts, "-vlport", CMD_SINGLE, CMD_OPTIONAL, "cell's vldb server port");
#endif

    ts = cmd_CreateSyntax("newalias", NewAliasCmd, 0,
			  "configure new cell alias");
    cmd_AddParm(ts, "-alias", CMD_SINGLE, 0, "alias name");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "real name of cell");
#endif

    ts = cmd_CreateSyntax("whichcell", WhichCellCmd, 0, "list file's cell");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");

    ts = cmd_CreateSyntax("whereis", WhereIsCmd, 0, "list file's location");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");

    ts = cmd_CreateSyntax("wscell", WSCellCmd, 0, "list workstation's cell");
    
    /*
     ts = cmd_CreateSyntax("primarycell", PrimaryCellCmd, 0, "obsolete (listed primary cell)");
     */
    
    ts = cmd_CreateSyntax("monitor", MonitorCmd, 0, "set cache monitor host address");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL, "host name or 'off'");
    cmd_CreateAlias(ts, "mariner");
    
   
    ts = cmd_CreateSyntax("getcellstatus", GetCellCmd, 0, "get cell status");
    cmd_AddParm(ts, "-cell", CMD_LIST, 0, "cell name");
    
    ts = cmd_CreateSyntax("setcell", SetCellCmd, 0, "set cell status");
    cmd_AddParm(ts, "-cell", CMD_LIST, 0, "cell name");
    cmd_AddParm(ts, "-suid", CMD_FLAG, CMD_OPTIONAL, "allow setuid programs");
    cmd_AddParm(ts, "-nosuid", CMD_FLAG, CMD_OPTIONAL, "disallow setuid programs");

    ts = cmd_CreateSyntax("flushall", FlushAllCmd, 0, "flush all data");

    ts = cmd_CreateSyntax("flushvolume", FlushVolumeCmd, 0, "flush all data in volume");
    cmd_AddParm(ts, "-path", CMD_LIST, CMD_OPTIONAL, "dir/file path");

    ts = cmd_CreateSyntax("sysname", SysNameCmd, 0, "get/set sysname (i.e. @sys) value");
    cmd_AddParm(ts, "-newsys", CMD_LIST, CMD_OPTIONAL, "new sysname");

    ts = cmd_CreateSyntax("exportafs", ExportAfsCmd, 0, "enable/disable translators to AFS");
    cmd_AddParm(ts, "-type", CMD_SINGLE, 0, "exporter name");
    cmd_AddParm(ts, "-start", CMD_SINGLE, CMD_OPTIONAL, "start/stop translator ('on' or 'off')");
    cmd_AddParm(ts, "-convert", CMD_SINGLE, CMD_OPTIONAL, "convert from afs to unix mode ('on or 'off')");
    cmd_AddParm(ts, "-uidcheck", CMD_SINGLE, CMD_OPTIONAL, "run on strict 'uid check' mode ('on' or 'off')");
    cmd_AddParm(ts, "-submounts", CMD_SINGLE, CMD_OPTIONAL, "allow nfs mounts to subdirs of /afs/.. ('on' or 'off')");


    ts = cmd_CreateSyntax("storebehind", StoreBehindCmd, 0, 
			  "store to server after file close");
    cmd_AddParm(ts, "-kbytes", CMD_SINGLE, CMD_OPTIONAL, "asynchrony for specified names");
    cmd_AddParm(ts, "-files", CMD_LIST, CMD_OPTIONAL, "specific pathnames");
    cmd_AddParm(ts, "-allfiles", CMD_SINGLE, CMD_OPTIONAL, "new default (KB)");
    cmd_CreateAlias(ts, "sb");

    ts = cmd_CreateSyntax("setcrypt", SetCryptCmd, 0, "set cache manager encryption flag");
    cmd_AddParm(ts, "-crypt", CMD_SINGLE, 0, "on or off");

    ts = cmd_CreateSyntax("getcrypt", GetCryptCmd, 0, "get cache manager encryption flag");

    ts = cmd_CreateSyntax("rxstatproc", RxStatProcCmd, 0,
			  "Manage per process RX statistics");
    cmd_AddParm(ts, "-enable", CMD_FLAG, CMD_OPTIONAL, "Enable RX stats");
    cmd_AddParm(ts, "-disable", CMD_FLAG, CMD_OPTIONAL, "Disable RX stats");
    cmd_AddParm(ts, "-clear", CMD_FLAG, CMD_OPTIONAL, "Clear RX stats");

    ts = cmd_CreateSyntax("rxstatpeer", RxStatPeerCmd, 0,
			  "Manage per peer RX statistics");
    cmd_AddParm(ts, "-enable", CMD_FLAG, CMD_OPTIONAL, "Enable RX stats");
    cmd_AddParm(ts, "-disable", CMD_FLAG, CMD_OPTIONAL, "Disable RX stats");
    cmd_AddParm(ts, "-clear", CMD_FLAG, CMD_OPTIONAL, "Clear RX stats");

#ifndef WIN32
    ts = cmd_CreateSyntax("setcbaddr", CallBackRxConnCmd, 0, "configure callback connection address");
    cmd_AddParm(ts, "-addr", CMD_SINGLE, CMD_OPTIONAL, "host name or address");
#endif

    ts = cmd_CreateSyntax("trace", TraceCmd, 0, "enable or disable CM tracing");
    cmd_AddParm(ts, "-on", CMD_FLAG, CMD_OPTIONAL, "enable tracing");
    cmd_AddParm(ts, "-off", CMD_FLAG, CMD_OPTIONAL, "disable tracing");
    cmd_AddParm(ts, "-reset", CMD_FLAG, CMD_OPTIONAL, "reset log contents");
    cmd_AddParm(ts, "-dump", CMD_FLAG, CMD_OPTIONAL, "dump log contents");
    cmd_CreateAlias(ts, "tr");

    ts = cmd_CreateSyntax("uuid", UuidCmd, 0, "manage the UUID for the cache manager");
    cmd_AddParm(ts, "-generate", CMD_FLAG, CMD_OPTIONAL, "generate a new UUID");

    ts = cmd_CreateSyntax("memdump", MemDumpCmd, 0, "dump memory allocs in debug builds");
    cmd_AddParm(ts, "-begin", CMD_FLAG, CMD_OPTIONAL, "set a memory checkpoint");
    cmd_AddParm(ts, "-end", CMD_FLAG, CMD_OPTIONAL, "dump memory allocs");
    
    ts = cmd_CreateSyntax("cscpolicy", CSCPolicyCmd, 0, "change client side caching policy for AFS shares");
    cmd_AddParm(ts, "-share", CMD_SINGLE, CMD_OPTIONAL, "AFS share");
    cmd_AddParm(ts, "-manual", CMD_FLAG, CMD_OPTIONAL, "manual caching of documents");
    cmd_AddParm(ts, "-programs", CMD_FLAG, CMD_OPTIONAL, "automatic caching of programs and documents");
    cmd_AddParm(ts, "-documents", CMD_FLAG, CMD_OPTIONAL, "automatic caching of documents");
    cmd_AddParm(ts, "-disable", CMD_FLAG, CMD_OPTIONAL, "disable caching");

    ts = cmd_CreateSyntax("minidump", MiniDumpCmd, 0, "Generate MiniDump of current service state");

    code = cmd_Dispatch(argc, argv);

    if (rxInitDone) 
        rx_Finalize();
    
    return code;
}

static void 
Die(int code, char *filename)
{ /*Die*/

    if (code == EINVAL) {
	if (filename)
	    fprintf(stderr,"%s: Invalid argument; it is possible that %s is not in AFS.\n", pn, filename);
	else 
            fprintf(stderr,"%s: Invalid argument.\n", pn);
    }
    else if (code == ENOENT) {
	if (filename) 
            fprintf(stderr,"%s: File '%s' doesn't exist\n", pn, filename);
	else 
            fprintf(stderr,"%s: no such file returned\n", pn);
    }
    else if (code == EROFS)  
        fprintf(stderr,"%s: You can not change a backup or readonly volume\n", pn);
    else if (code == EACCES || code == EPERM) {
	if (filename) 
            fprintf(stderr,"%s: You don't have the required access rights on '%s'\n", pn, filename);
	else 
            fprintf(stderr,"%s: You do not have the required rights to do this operation\n", pn);
    }
    else if (code == ENODEV) {
	fprintf(stderr,"%s: AFS service may not have started.\n", pn);
    }
    else if (code == ESRCH) {
	fprintf(stderr,"%s: Cell name not recognized.\n", pn);
    }
    else if (code == EPIPE) {
	fprintf(stderr,"%s: Volume name or ID not recognized.\n", pn);
    }
    else if (code == EFBIG) {
	fprintf(stderr,"%s: Cache size too large.\n", pn);
    }
    else if (code == ETIMEDOUT) {
	if (filename)
	    fprintf(stderr,"%s:'%s': Connection timed out", pn, filename);
	else
	    fprintf(stderr,"%s: Connection timed out", pn);
    }
    else {
	if (filename) 
            fprintf(stderr,"%s:'%s'", pn, filename);
	else 
            fprintf(stderr,"%s", pn);
#ifdef WIN32
	fprintf(stderr, ": code 0x%x\n", code);
#else /* not WIN32 */
	fprintf(stderr,": %s\n", error_message(code));
#endif /* not WIN32 */
    }
} /*Die*/

