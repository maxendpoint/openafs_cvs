/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This program will parse the output generated by afsmonitor (with the -output
 * switch) and print it in a readable form. It does not make any statistical
 * analysis of the data.
 *
 * Most of the code here is cloned from afsmon-output.c. It is made as a 
 * separate file so that it can be independently given to customers.
 *
 *-------------------------------------------------------------------------*/

#include <stdio.h>
#include <afs/param.h>
#include <afsconfig.h>

RCSID("$Header: /cvs/openafs/src/afsmonitor/afsmon-parselog.c,v 1.3 2001/07/05 15:20:09 shadow Exp $");

#include <afs/xstat_fs.h>  
#include <afs/xstat_cm.h>
#include <malloc.h>


/* Number of items of CM & FS statistics collected per probe.
   These constants MUST be changed if the xstat structures are modified */

#define XSTAT_CM_FULLPERF_RESULTS_LEN 740
#define XSTAT_FS_FULLPERF_RESULTS_LEN 424


/* structures used by FS & CM stats print routines */

static char *fsOpNames[] = {
    "FetchData",
    "FetchACL",
    "FetchStatus",
    "StoreData",
    "StoreACL",
    "StoreStatus",
    "RemoveFile",
    "CreateFile",
    "Rename",
    "Symlink",
    "Link",
    "MakeDir",
    "RemoveDir",
    "SetLock",
    "ExtendLock",
    "ReleaseLock",
    "GetStatistics",
    "GiveUpCallbacks",
    "GetVolumeInfo",
    "GetVolumeStatus",
    "SetVolumeStatus",
    "GetRootVolume",
    "CheckToken",
    "GetTime",
    "NGetVolumeInfo",
    "BulkStatus",
    "XStatsVersion",
    "GetXStats"
};

static char *cmOpNames[] = {
    "CallBack",
    "InitCallBackState",
    "Probe",
    "GetLock",
    "GetCE",
    "XStatsVersion",
    "GetXStats"
};

static char *xferOpNames[] = {
    "FetchData",
    "StoreData"
};

/*________________________________________________________________________
				FS STATS ROUTINES 
 *_______________________________________________________________________*/

/*------------------------------------------------------------------------
 * Print_fs_OverallPerfInfo
 *
 * Description:
 *	Print out overall performance numbers.
 *
 * Arguments:
 *	a_ovP : Ptr to the overall performance numbers.
 *
 *------------------------------------------------------------------------*/

void Print_fs_OverallPerfInfo(a_ovP)
    struct afs_PerfStats *a_ovP;

{ /*Print_fs_OverallPerfInfo*/

    printf("\t%10d numPerfCalls\n\n", a_ovP->numPerfCalls);


    /*
     * Vnode cache section.
     */
    printf("\t%10d vcache_L_Entries\n", a_ovP->vcache_L_Entries);
    printf("\t%10d vcache_L_Allocs\n",  a_ovP->vcache_L_Allocs);
    printf("\t%10d vcache_L_Gets\n",	  a_ovP->vcache_L_Gets);
    printf("\t%10d vcache_L_Reads\n",	  a_ovP->vcache_L_Reads);
    printf("\t%10d vcache_L_Writes\n\n",a_ovP->vcache_L_Writes);

    printf("\t%10d vcache_S_Entries\n", a_ovP->vcache_S_Entries);
    printf("\t%10d vcache_S_Allocs\n",  a_ovP->vcache_S_Allocs);
    printf("\t%10d vcache_S_Gets\n",	  a_ovP->vcache_S_Gets);
    printf("\t%10d vcache_S_Reads\n",	  a_ovP->vcache_S_Reads);
    printf("\t%10d vcache_S_Writes\n\n",a_ovP->vcache_S_Writes);

    printf("\t%10d vcache_H_Entries\n", a_ovP->vcache_H_Entries);
    printf("\t%10d vcache_H_Gets\n",	  a_ovP->vcache_H_Gets);
    printf("\t%10d vcache_H_Replacements\n\n",a_ovP->vcache_H_Replacements);

    /*
     * Directory package section.
     */
    printf("\t%10d dir_Buffers\n", a_ovP->dir_Buffers);
    printf("\t%10d dir_Calls\n", a_ovP->dir_Calls);
    printf("\t%10d dir_IOs\n\n", a_ovP->dir_IOs);

    /*
     * Rx section.
     */
    printf("\t%10d rx_packetRequests\n", a_ovP->rx_packetRequests);
    printf("\t%10d rx_noPackets_RcvClass\n", a_ovP->rx_noPackets_RcvClass);
    printf("\t%10d rx_noPackets_SendClass\n",	a_ovP->rx_noPackets_SendClass);
    printf("\t%10d rx_noPackets_SpecialClass\n", a_ovP->rx_noPackets_SpecialClass);
    printf("\t%10d rx_socketGreedy\n", a_ovP->rx_socketGreedy);
    printf("\t%10d rx_bogusPacketOnRead\n", a_ovP->rx_bogusPacketOnRead);
    printf("\t%10d rx_bogusHost\n",	a_ovP->rx_bogusHost);
    printf("\t%10d rx_noPacketOnRead\n", a_ovP->rx_noPacketOnRead);
    printf("\t%10d rx_noPacketBuffersOnRead\n", a_ovP->rx_noPacketBuffersOnRead);
    printf("\t%10d rx_selects\n", a_ovP->rx_selects);
    printf("\t%10d rx_sendSelects\n",	a_ovP->rx_sendSelects);
    printf("\t%10d rx_packetsRead_RcvClass\n", a_ovP->rx_packetsRead_RcvClass);
    printf("\t%10d rx_packetsRead_SendClass\n", a_ovP->rx_packetsRead_SendClass);
    printf("\t%10d rx_packetsRead_SpecialClass\n", a_ovP->rx_packetsRead_SpecialClass);
    printf("\t%10d rx_dataPacketsRead\n", a_ovP->rx_dataPacketsRead);
    printf("\t%10d rx_ackPacketsRead\n", a_ovP->rx_ackPacketsRead);
    printf("\t%10d rx_dupPacketsRead\n", a_ovP->rx_dupPacketsRead);
    printf("\t%10d rx_spuriousPacketsRead\n",	a_ovP->rx_spuriousPacketsRead);
    printf("\t%10d rx_packetsSent_RcvClass\n", a_ovP->rx_packetsSent_RcvClass);
    printf("\t%10d rx_packetsSent_SendClass\n", a_ovP->rx_packetsSent_SendClass);
    printf("\t%10d rx_packetsSent_SpecialClass\n", a_ovP->rx_packetsSent_SpecialClass);
    printf("\t%10d rx_ackPacketsSent\n", a_ovP->rx_ackPacketsSent);
    printf("\t%10d rx_pingPacketsSent\n", a_ovP->rx_pingPacketsSent);
    printf("\t%10d rx_abortPacketsSent\n",a_ovP->rx_abortPacketsSent);
    printf("\t%10d rx_busyPacketsSent\n", a_ovP->rx_busyPacketsSent);
    printf("\t%10d rx_dataPacketsSent\n", a_ovP->rx_dataPacketsSent);
    printf("\t%10d rx_dataPacketsReSent\n", a_ovP->rx_dataPacketsReSent);
    printf("\t%10d rx_dataPacketsPushed\n",	a_ovP->rx_dataPacketsPushed);
    printf("\t%10d rx_ignoreAckedPacket\n",	a_ovP->rx_ignoreAckedPacket);
    printf("\t%10d rx_totalRtt_Sec\n", a_ovP->rx_totalRtt_Sec);
    printf("\t%10d rx_totalRtt_Usec\n", a_ovP->rx_totalRtt_Usec);
    printf("\t%10d rx_minRtt_Sec\n",	a_ovP->rx_minRtt_Sec);
    printf("\t%10d rx_minRtt_Usec\n",		a_ovP->rx_minRtt_Usec);
    printf("\t%10d rx_maxRtt_Sec\n",		a_ovP->rx_maxRtt_Sec);
    printf("\t%10d rx_maxRtt_Usec\n",		a_ovP->rx_maxRtt_Usec);
    printf("\t%10d rx_nRttSamples\n",		a_ovP->rx_nRttSamples);
    printf("\t%10d rx_nServerConns\n",	a_ovP->rx_nServerConns);
    printf("\t%10d rx_nClientConns\n",	a_ovP->rx_nClientConns);
    printf("\t%10d rx_nPeerStructs\n",	a_ovP->rx_nPeerStructs);
    printf("\t%10d rx_nCallStructs\n",	a_ovP->rx_nCallStructs);
    printf("\t%10d rx_nFreeCallStructs\n\n",	a_ovP->rx_nFreeCallStructs);

    /*
     * Host module fields.
     */
    printf("\t%10d host_NumHostEntries\n",	a_ovP->host_NumHostEntries);
    printf("\t%10d host_HostBlocks\n",	a_ovP->host_HostBlocks);
    printf("\t%10d host_NonDeletedHosts\n",	a_ovP->host_NonDeletedHosts);
    printf("\t%10d host_HostsInSameNetOrSubnet\n", a_ovP->host_HostsInSameNetOrSubnet);
    printf("\t%10d host_HostsInDiffSubnet\n",	a_ovP->host_HostsInDiffSubnet);
    printf("\t%10d host_HostsInDiffNetwork\n",a_ovP->host_HostsInDiffNetwork);
    printf("\t%10d host_NumClients\n",	a_ovP->host_NumClients);
    printf("\t%10d host_ClientBlocks\n\n",	a_ovP->host_ClientBlocks);

} /*Print_fs_OverallPerfInfo*/


/*------------------------------------------------------------------------
 * Print_fs_OpTiming
 *
 * Description:
 *	Print out the contents of an RPC op timing structure.
 *
 * Arguments:
 *	a_opIdx   : Index of the AFS operation we're printing number on.
 *	a_opTimeP : Ptr to the op timing structure to print.
 *
 *------------------------------------------------------------------------*/

void Print_fs_OpTiming(a_opIdx, a_opTimeP)
    int a_opIdx;
    struct fs_stats_opTimingData *a_opTimeP;

{ /*Print_fs_OpTiming*/

    printf("%15s: %d ops (%d OK); sum=%d.%06d, min=%d.%06d, max=%d.%06d\n",
	   fsOpNames[a_opIdx],
	   a_opTimeP->numOps, a_opTimeP->numSuccesses,
	   a_opTimeP->sumTime.tv_sec, a_opTimeP->sumTime.tv_usec,
	   a_opTimeP->minTime.tv_sec, a_opTimeP->minTime.tv_usec,
	   a_opTimeP->maxTime.tv_sec, a_opTimeP->maxTime.tv_usec);

} /*Print_fs_OpTiming*/


/*------------------------------------------------------------------------
 * Print_fs_XferTiming
 *
 * Description:
 *	Print out the contents of a data transfer structure.
 *
 * Arguments:
 *	a_opIdx : Index of the AFS operation we're printing number on.
 *	a_xferP : Ptr to the data transfer structure to print.
 *
 *------------------------------------------------------------------------*/

void Print_fs_XferTiming(a_opIdx, a_xferP)
    int a_opIdx;
    struct fs_stats_xferData *a_xferP;

{ /*Print_fs_XferTiming*/

    printf("%s: %d xfers (%d OK), time sum=%d.%06d, min=%d.%06d, max=%d.%06d\n",
	   xferOpNames[a_opIdx],
	   a_xferP->numXfers, a_xferP->numSuccesses,
	   a_xferP->sumTime.tv_sec, a_xferP->sumTime.tv_usec,
	   a_xferP->minTime.tv_sec, a_xferP->minTime.tv_usec,
	   a_xferP->maxTime.tv_sec, a_xferP->maxTime.tv_usec);
    printf("\t[bytes: sum=%d, min=%d, max=%d]\n",
	   a_xferP->sumBytes, a_xferP->minBytes, a_xferP->maxBytes);
    printf("\t[buckets: 0: %d, 1: %d, 2: %d, 3: %d, 4: %d, 5: %d 6: %d 7:%d 8:%d]\n",
	   a_xferP->count[0],
	   a_xferP->count[1],
	   a_xferP->count[2],
	   a_xferP->count[3],
	   a_xferP->count[4],
	   a_xferP->count[5],
	   a_xferP->count[6],
	   a_xferP->count[7],
	   a_xferP->count[8]);

} /*Print_fs_XferTiming*/


/*------------------------------------------------------------------------
 * Print_fs_DetailedPerfInfo
 *
 * Description:
 *	Print out a set of detailed performance numbers.
 *
 * Arguments:
 *	a_detP : Ptr to detailed perf numbers to print.
 *
 *------------------------------------------------------------------------*/

void Print_fs_DetailedPerfInfo(a_detP)
    struct fs_stats_DetailedStats *a_detP;

{ /*Print_fs_DetailedPerfInfo*/

    int currIdx;	/*Loop variable*/

    printf("\t%10d epoch\n", a_detP->epoch);

    for (currIdx = 0; currIdx < FS_STATS_NUM_RPC_OPS; currIdx++)
	Print_fs_OpTiming(currIdx, &(a_detP->rpcOpTimes[currIdx]));

    for (currIdx = 0; currIdx < FS_STATS_NUM_XFER_OPS; currIdx++)
	Print_fs_XferTiming(currIdx, &(a_detP->xferOpTimes[currIdx]));

} /*Print_fs_DetailedPerfInfo*/


/*------------------------------------------------------------------------
 * Print_fs_FullPerfInfo
 *
 * Description:
 *	Print out the AFS_XSTATSCOLL_FULL_PERF_INFO collection we just
 *	received.
 *
 * Arguments:
 *	None.
 *
 *------------------------------------------------------------------------*/

void Print_fs_FullPerfInfo(a_fs_Results)
struct xstat_fs_ProbeResults *a_fs_Results;		/* ptr to fs results */
{ /*Print_fs_FullPerfInfo*/

    static char rn[] = "Print_fs_FullPerfInfo";	      /*Routine name*/
    static long fullPerfLongs =
	(sizeof(struct fs_stats_FullPerfStats) >> 2); /*Correct # longs to rcv*/
    long numLongs;				      /*# longwords received*/
    struct fs_stats_FullPerfStats *fullPerfP;	      /*Ptr to full perf stats*/
    char *printableTime;			/*Ptr to printable time string*/


    numLongs = a_fs_Results->data.AFS_CollData_len;
    if (numLongs != fullPerfLongs) {
	printf(" ** Data size mismatch in full performance collection!\n");
	printf(" ** Expecting %d, got %d\n", fullPerfLongs, numLongs);
	return;
    }

    printableTime = ctime(&(a_fs_Results->probeTime));
    printableTime[strlen(printableTime)-1] = '\0';
    fullPerfP = (struct fs_stats_FullPerfStats *)
	(a_fs_Results->data.AFS_CollData_val);

    printf("AFS_XSTATSCOLL_FULL_PERF_INFO (coll %d) for FS %s\n[Probe %d, %s]\n\n",
	   a_fs_Results->collectionNumber,
	   a_fs_Results->connP->hostName,
	   a_fs_Results->probeNum,
	   printableTime);

    Print_fs_OverallPerfInfo(&(fullPerfP->overall));
    Print_fs_DetailedPerfInfo(&(fullPerfP->det));

} /*Print_fs_FullPerfInfo*/


   
/*___________________________________________________________________________
			CM STATS
 *__________________________________________________________________________*/



/*------------------------------------------------------------------------
 * Print_cm_UpDownStats
 *
 * Description:
 *	Print the up/downtime stats for the given class of server records
 *	provided.
 *
 * Arguments:
 *	a_upDownP : Ptr to the server up/down info.
 *
 *------------------------------------------------------------------------*/

void Print_cm_UpDownStats(a_upDownP)
    struct afs_stats_SrvUpDownInfo *a_upDownP;	/*Ptr to server up/down info*/

{ /*Print_cm_UpDownStats*/

    /*
     * First, print the simple values.
     */
    printf("\t\t%10d numTtlRecords\n",		a_upDownP->numTtlRecords);
    printf("\t\t%10d numUpRecords\n",		a_upDownP->numUpRecords);
    printf("\t\t%10d numDownRecords\n",		a_upDownP->numDownRecords);
    printf("\t\t%10d sumOfRecordAges\n",	a_upDownP->sumOfRecordAges);
    printf("\t\t%10d ageOfYoungestRecord\n",	a_upDownP->ageOfYoungestRecord);
    printf("\t\t%10d ageOfOldestRecord\n",	a_upDownP->ageOfOldestRecord);
    printf("\t\t%10d numDowntimeIncidents\n",	a_upDownP->numDowntimeIncidents);
    printf("\t\t%10d numRecordsNeverDown\n",	a_upDownP->numRecordsNeverDown);
    printf("\t\t%10d maxDowntimesInARecord\n",	a_upDownP->maxDowntimesInARecord);
    printf("\t\t%10d sumOfDowntimes\n",		a_upDownP->sumOfDowntimes);
    printf("\t\t%10d shortestDowntime\n",	a_upDownP->shortestDowntime);
    printf("\t\t%10d longestDowntime\n",	a_upDownP->longestDowntime);

    /*
     * Now, print the array values.
     */
    printf("\t\tDowntime duration distribution:\n");
    printf("\t\t\t%8d: 0 min .. 10 min\n",  a_upDownP->downDurations[0]);
    printf("\t\t\t%8d: 10 min .. 30 min\n", a_upDownP->downDurations[1]);
    printf("\t\t\t%8d: 30 min .. 1 hr\n",   a_upDownP->downDurations[2]);
    printf("\t\t\t%8d: 1 hr .. 2 hr\n",     a_upDownP->downDurations[3]);
    printf("\t\t\t%8d: 2 hr .. 4 hr\n",     a_upDownP->downDurations[4]);
    printf("\t\t\t%8d: 4 hr .. 8 hr\n",     a_upDownP->downDurations[5]);
    printf("\t\t\t%8d: > 8 hr\n",           a_upDownP->downDurations[6]);

    printf("\t\tDowntime incident distribution:\n");
    printf("\t\t\t%8d: 0 times\n",        a_upDownP->downIncidents[0]);
    printf("\t\t\t%8d: 1 time\n",         a_upDownP->downIncidents[1]);
    printf("\t\t\t%8d: 2 .. 5 times\n",   a_upDownP->downIncidents[2]);
    printf("\t\t\t%8d: 6 .. 10 times\n",  a_upDownP->downIncidents[3]);
    printf("\t\t\t%8d: 10 .. 50 times\n", a_upDownP->downIncidents[4]);
    printf("\t\t\t%8d: > 50 times\n",     a_upDownP->downIncidents[5]);

} /*Print_cm_UpDownStats*/


/*------------------------------------------------------------------------
 * Print_cm_OverallPerfInfo
 *
 * Description:
 *	Print out overall performance numbers.
 *
 * Arguments:
 *	a_ovP : Ptr to the overall performance numbers.
 *
 *------------------------------------------------------------------------*/

void Print_cm_OverallPerfInfo(a_ovP)
    struct afs_stats_CMPerf *a_ovP;

{ /*Print_cm_OverallPerfInfo*/

    printf("\t%10d numPerfCalls\n",		a_ovP->numPerfCalls);

    printf("\t%10d epoch\n",			a_ovP->epoch);
    printf("\t%10d numCellsVisible\n",	a_ovP->numCellsVisible);
    printf("\t%10d numCellsContacted\n",   	a_ovP->numCellsContacted);
    printf("\t%10d dlocalAccesses\n",		a_ovP->dlocalAccesses);
    printf("\t%10d vlocalAccesses\n",		a_ovP->vlocalAccesses);
    printf("\t%10d dremoteAccesses\n",	a_ovP->dremoteAccesses);
    printf("\t%10d vremoteAccesses\n",	a_ovP->vremoteAccesses);
    printf("\t%10d cacheNumEntries\n",	a_ovP->cacheNumEntries);
    printf("\t%10d cacheBlocksTotal\n",	a_ovP->cacheBlocksTotal);
    printf("\t%10d cacheBlocksInUse\n",	a_ovP->cacheBlocksInUse);
    printf("\t%10d cacheBlocksOrig\n",	a_ovP->cacheBlocksOrig);
    printf("\t%10d cacheMaxDirtyChunks\n",a_ovP->cacheMaxDirtyChunks);
    printf("\t%10d cacheCurrDirtyChunks\n",	a_ovP->cacheCurrDirtyChunks);
    printf("\t%10d dcacheHits\n",		a_ovP->dcacheHits);
    printf("\t%10d vcacheHits\n",		a_ovP->vcacheHits);
    printf("\t%10d dcacheMisses\n",		a_ovP->dcacheMisses);
    printf("\t%10d vcacheMisses\n",		a_ovP->vcacheMisses);
    printf("\t%10d cacheFilesReused\n",	a_ovP->cacheFilesReused);
    printf("\t%10d vcacheXAllocs\n",		a_ovP->vcacheXAllocs);

    printf("\t%10d bufAlloced\n",		a_ovP->bufAlloced);
    printf("\t%10d bufHits\n",			a_ovP->bufHits);
    printf("\t%10d bufMisses\n",		a_ovP->bufMisses);
    printf("\t%10d bufFlushDirty\n",		a_ovP->bufFlushDirty);

    printf("\t%10d LargeBlocksActive\n",	a_ovP->LargeBlocksActive);
    printf("\t%10d LargeBlocksAlloced\n",	a_ovP->LargeBlocksAlloced);
    printf("\t%10d SmallBlocksActive\n",	a_ovP->SmallBlocksActive);
    printf("\t%10d SmallBlocksAlloced\n",	a_ovP->SmallBlocksAlloced);
    printf("\t%10d OutStandingMemUsage\n",	a_ovP->OutStandingMemUsage);
    printf("\t%10d OutStandingAllocs\n",	a_ovP->OutStandingAllocs);
    printf("\t%10d CallBackAlloced\n",	a_ovP->CallBackAlloced);
    printf("\t%10d CallBackFlushes\n",	a_ovP->CallBackFlushes);

    printf("\t%10d srvRecords\n",		a_ovP->srvRecords);
    printf("\t%10d srvNumBuckets\n",		a_ovP->srvNumBuckets);
    printf("\t%10d srvMaxChainLength\n",	a_ovP->srvMaxChainLength);
    printf("\t%10d srvMaxChainLengthHWM\n",	a_ovP->srvMaxChainLengthHWM);
    printf("\t%10d srvRecordsHWM\n",		a_ovP->srvRecordsHWM);

    printf("\t%10d sysName_ID\n",	  	a_ovP->sysName_ID);

    printf("\tFile Server up/downtimes, same cell:\n");
    Print_cm_UpDownStats(&(a_ovP->fs_UpDown[0]));

    printf("\tFile Server up/downtimes, diff cell:\n");
    Print_cm_UpDownStats(&(a_ovP->fs_UpDown[1]));

    printf("\tVL Server up/downtimes, same cell:\n");
    Print_cm_UpDownStats(&(a_ovP->vl_UpDown[0]));

    printf("\tVL Server up/downtimes, diff cell:\n");
    Print_cm_UpDownStats(&(a_ovP->vl_UpDown[1]));

} /*Print_cm_OverallPerfInfo*/



/*------------------------------------------------------------------------
 * Print_cm_OpTiming
 *
 * Description:
 *	Print out the contents of an FS RPC op timing structure.
 *
 * Arguments:
 *	a_opIdx   : Index of the AFS operation we're printing number on.
 *	a_opNames : Ptr to table of operaton names.
 *	a_opTimeP : Ptr to the op timing structure to print.
 *
 *------------------------------------------------------------------------*/

void Print_cm_OpTiming(a_opIdx, a_opNames, a_opTimeP)
    int a_opIdx;
    char *a_opNames[];
    struct afs_stats_opTimingData *a_opTimeP;

{ /*Print_cm_OpTiming*/

    printf("%15s: %d ops (%d OK); sum=%d.%06d, min=%d.%06d, max=%d.%06d\n",
	   a_opNames[a_opIdx],
	   a_opTimeP->numOps, a_opTimeP->numSuccesses,
	   a_opTimeP->sumTime.tv_sec, a_opTimeP->sumTime.tv_usec,
	   a_opTimeP->minTime.tv_sec, a_opTimeP->minTime.tv_usec,
	   a_opTimeP->maxTime.tv_sec, a_opTimeP->maxTime.tv_usec);

} /*Print_cm_OpTiming*/


/*------------------------------------------------------------------------
 * Print_cm_XferTiming
 *
 * Description:
 *	Print out the contents of a data transfer structure.
 *
 * Arguments:
 *	a_opIdx : Index of the AFS operation we're printing number on.
 *	a_xferP : Ptr to the data transfer structure to print.
 *
 *------------------------------------------------------------------------*/

void Print_cm_XferTiming(a_opIdx, a_opNames, a_xferP)
    int a_opIdx;
    char *a_opNames[];
    struct afs_stats_xferData *a_xferP;

{ /*Print_cm_XferTiming*/

    printf("%s: %d xfers (%d OK), time sum=%d.%06d, min=%d.%06d, max=%d.%06d\n",
	   a_opNames[a_opIdx],
	   a_xferP->numXfers, a_xferP->numSuccesses,
	   a_xferP->sumTime.tv_sec, a_xferP->sumTime.tv_usec,
	   a_xferP->minTime.tv_sec, a_xferP->minTime.tv_usec,
	   a_xferP->maxTime.tv_sec, a_xferP->maxTime.tv_usec);
    printf("\t[bytes: sum=%d, min=%d, max=%d]\n",
	   a_xferP->sumBytes, a_xferP->minBytes, a_xferP->maxBytes);
    printf("\t[buckets: 0: %d, 1: %d, 2: %d, 3: %d, 4: %d, 5: %d, 6: %d, 7: %d, 8: %d]\n",
	   a_xferP->count[0],
	   a_xferP->count[1],
	   a_xferP->count[2],
	   a_xferP->count[3],
	   a_xferP->count[4],
	   a_xferP->count[5],
	   a_xferP->count[6],
	   a_xferP->count[7],
	   a_xferP->count[8]);

} /*Print_cm_XferTiming*/


/*------------------------------------------------------------------------
 * Print_cm_ErrInfo
 *
 * Description:
 *	Print out the contents of an FS RPC error info structure.
 *
 * Arguments:
 *	a_opIdx   : Index of the AFS operation we're printing.
 *	a_opNames : Ptr to table of operation names.
 *	a_opErrP  : Ptr to the op timing structure to print.
 *
 *------------------------------------------------------------------------*/

void Print_cm_ErrInfo(a_opIdx, a_opNames, a_opErrP)
    int a_opIdx;
    char *a_opNames[];
    struct afs_stats_RPCErrors *a_opErrP;

{ /*Print_cm_ErrInfo*/

    printf("%15s: %d server, %d network, %d prot, %d vol, %d busies, %d other\n",
	   a_opNames[a_opIdx],
	   a_opErrP->err_Server,
	   a_opErrP->err_Network,
	   a_opErrP->err_Protection,
	   a_opErrP->err_Volume,
	   a_opErrP->err_VolumeBusies,
	   a_opErrP->err_Other);

} /*Print_cm_ErrInfo*/


/*------------------------------------------------------------------------
 * Print_cm_RPCPerfInfo
 *
 * Description:
 *	Print out a set of RPC performance numbers.
 *
 * Arguments:
 *	a_rpcP : Ptr to RPC perf numbers to print.
 *
 *------------------------------------------------------------------------*/

void Print_cm_RPCPerfInfo(a_rpcP)
    struct afs_stats_RPCOpInfo *a_rpcP;

{ /*Print_cm_RPCPerfInfo*/

    int currIdx;		/*Loop variable*/

    /*
     * Print the contents of each of the opcode-related arrays.
     */
    printf("FS Operation Timings:\n---------------------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_RPC_OPS; currIdx++)
	Print_cm_OpTiming(currIdx, fsOpNames, &(a_rpcP->fsRPCTimes[currIdx]));

    printf("\nError Info:\n-----------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_RPC_OPS; currIdx++)
	Print_cm_ErrInfo(currIdx, fsOpNames, &(a_rpcP->fsRPCErrors[currIdx]));

    printf("\nTransfer timings:\n-----------------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_XFER_OPS; currIdx++)
	Print_cm_XferTiming(currIdx, xferOpNames, &(a_rpcP->fsXferTimes[currIdx]));

    printf("\nCM Operation Timings:\n---------------------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_CM_RPC_OPS; currIdx++)
	Print_cm_OpTiming(currIdx, cmOpNames, &(a_rpcP->cmRPCTimes[currIdx]));

} /*Print_cm_RPCPerfInfo*/


/*------------------------------------------------------------------------
 * Print_cm_FullPerfInfo
 *
 * Description:
 *	Print out a set of full performance numbers.
 *
 * Arguments:
 *	None.
 *
 *------------------------------------------------------------------------*/

void Print_cm_FullPerfInfo(a_fullP)
struct afs_stats_CMFullPerf *a_fullP;
{ /*Print_cm_FullPerfInfo*/

    static char rn[] = "Print_cm_FullPerfInfo"; /* routine name */
    struct afs_stats_AuthentInfo *authentP;	/*Ptr to authentication stats*/
    struct afs_stats_AccessInfo *accessinfP;	/*Ptr to access stats*/
    struct afs_stats_AuthorInfo *authorP;	/*Ptr to authorship stats*/
   static long fullPerfLongs =
	(sizeof (struct afs_stats_CMFullPerf) >> 2); /*Correct #longs*/
    long numLongs;				/*# longs actually received*/
    struct afs_stats_CMFullPerf *fullP;

    fullP = a_fullP;

    /*
     * Print the overall numbers first, followed by all of the RPC numbers,
     * then each of the other groupings.
     */
    printf("Overall Performance Info:\n-------------------------\n");
    Print_cm_OverallPerfInfo(&(fullP->perf));
    printf("\n");
    Print_cm_RPCPerfInfo(&(fullP->rpc));

    authentP = &(fullP->authent);
    printf("\nAuthentication info:\n--------------------\n");
    printf("\t%d PAGS, %d records (%d auth, %d unauth), %d max in PAG, chain max: %d\n",
	   authentP->curr_PAGs,
	   authentP->curr_Records,
	   authentP->curr_AuthRecords,
	   authentP->curr_UnauthRecords,
	   authentP->curr_MaxRecordsInPAG,
	   authentP->curr_LongestChain);
    printf("\t%d PAG creations, %d tkt updates\n",
	   authentP->PAGCreations,
	   authentP->TicketUpdates);
    printf("\t[HWMs: %d PAGS, %d records, %d max in PAG, chain max: %d]\n",
	   authentP->HWM_PAGs,
	   authentP->HWM_Records,
	   authentP->HWM_MaxRecordsInPAG,
	   authentP->HWM_LongestChain);

    accessinfP = &(fullP->accessinf);
    printf("\n[Un]replicated accesses:\n------------------------\n");
    printf("\t%d unrep, %d rep, %d reps accessed, %d max reps/ref, %d first OK\n\n",
	   accessinfP->unreplicatedRefs,
	   accessinfP->replicatedRefs,
	   accessinfP->numReplicasAccessed,
	   accessinfP->maxReplicasPerRef,
	   accessinfP->refFirstReplicaOK);

    /* There really isn't any authorship info
    authorP = &(fullP->author); */

} /*Print_cm_FullPerfInfo*/




main(argc,argv)
int argc;
char *argv[];
{
  static char rn[] = "main";
  FILE *inFD;
  char	*line, *charPtr;
  long	*longs, *longPtr;
  int 	block_size, exitcode, i, numLongs, counter;
  char  day[5],month[5],date[5],time[10],year[5],hostname[80],hosttype[5];
  struct afs_stats_CMFullPerf *cmPerfP;
  struct fs_stats_FullPerfStats *fsPerfP;
  char	tmpstr[20];


  if ((argc < 2) || (strcasecmp(argv[1],"-h")==0) || 
      (strcasecmp(argv[1],"-help")==0) || (strcasecmp(argv[1],"help")==0) ) {
    fprintf(stderr,"\nUsage: %s  <file>\n",argv[0]);
    fprintf(stderr,"\twhere <file> is the output generated by AFSMonitor\n\n");
    exit(1);
   }



  inFD = fopen(argv[1],"r");
  if (inFD == (FILE *)0) {
    fprintf(stderr,"\n[ %s ] Unable to open input file %s. \n",rn,argv[1]);
    exit(5);
  }

  block_size = XSTAT_CM_FULLPERF_RESULTS_LEN * sizeof(long);
  
  /* Malloc two blocks of data, one for reading each line from the data file
     and the other for coverting data to longs */

  if ( (line = malloc(block_size+256)) == (char *)NULL) {
    	fprintf(stderr,"[ %s ] malloc %d bytes failed\n",rn,block_size+256);
	exit(10);
      }

  if ( (longs = malloc(block_size)) == (long *) NULL) {
    fprintf(stderr,"[ %s ] malloc %d bytes failed\n",rn,block_size);
    exit(20);
  }

  /* Parse the data file */
  while (1) {
  if (fgets(line, block_size, inFD) == NULL) {
    exitcode = 0; 
    goto FINISH;
  }
  if (strlen(line) < 5)
    continue;

  /* Parse the date, hostname, and hosttype (FS or CM) */
  
  charPtr = line;
  sscanf(charPtr,"%s %s %s %s %s %s %s", day,month,date,time,
	 		year,hostname,hosttype);
  charPtr += strlen(day) + strlen(month) + strlen(date) +  strlen(time) + 
    	 strlen(year) +  strlen(hostname) +  strlen(hosttype) + 8;

  printf("\n\n%s %s %s %s %s %s %s \n\n",
	 day,month,date,time,year,hostname, hosttype);

  /* Check the first datum. If it is -1 the probe had failed */

  sscanf(charPtr,"%s",tmpstr);
  if ( atoi(tmpstr) == -1) {
    printf("Probe failed, no data to process.\n");
    continue;
  }

  /* Convert the data to longs */
  
  if (strcmp(hosttype,"FS") == 0) numLongs = XSTAT_FS_FULLPERF_RESULTS_LEN;
  else if (strcmp(hosttype,"CM") == 0) numLongs=XSTAT_CM_FULLPERF_RESULTS_LEN;
  else {
    fprintf(stderr, "Cannot determine hosttype %s\n",hosttype);
    fprintf(stderr,"Skipping this entry\n");
    continue;
  }

  longPtr = longs;
  counter = 0;
  for (i = 0; i < numLongs; i++) {
    sscanf(charPtr,"%ld",longPtr);
    sscanf(charPtr,"%s",tmpstr);
    longPtr++;
    charPtr += strlen(tmpstr) + 1;
    counter++;
   }

  /* Verify that we read the correct number of longs and print them */

  if (strcmp(hosttype,"CM") == 0) {
    if (counter == XSTAT_CM_FULLPERF_RESULTS_LEN)
      Print_cm_FullPerfInfo( (struct afs_stats_CMFullPerf *) longs);
    else 
      fprintf(stderr,"Data size mismatch error. Expected %d longs, found %d longs\n",numLongs,counter);
  }
  else {
    if (counter == XSTAT_FS_FULLPERF_RESULTS_LEN) {
      fsPerfP = (struct fs_stats_FullPerfStats *) longs;
      Print_fs_OverallPerfInfo (&(fsPerfP->overall));
      Print_fs_DetailedPerfInfo (&(fsPerfP->det));
    }
    else
      fprintf(stderr,"Data size mismatch error. Expected %d longs, found %d longs\n",numLongs,counter);
      
  }

  printf("\n-------------------------------------------------------------------------\n"); 
  } /* while */

  exitcode = 0;

FINISH:
  fclose(inFD);
  free(line);
  free(longs);
}
  

