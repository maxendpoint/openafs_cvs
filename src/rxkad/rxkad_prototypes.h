/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_RXKAD_PROTOTYPES_H
#define _RXKAD_PROTOTYPES_H

/* till the typedefs are moved elsewhere */
#include "fcrypt.h"
#include "rx/rx.h"

/* domestic/crypt_conn.c */
extern afs_int32 rxkad_DecryptPacket (const struct rx_connection *conn,
        const fc_KeySchedule *schedule, const fc_InitializationVector *ivec,
        const int len, struct rx_packet *packet);
extern afs_int32 rxkad_EncryptPacket (const struct rx_connection *conn,
        const fc_KeySchedule *schedule, const fc_InitializationVector *ivec, 
        const int len, struct rx_packet *packet);


/* domestic/fcrypt.c */
extern int fc_keysched (struct ktc_encryptionKey *key, 
        fc_KeySchedule schedule);
extern afs_int32 fc_ecb_encrypt(afs_uint32 *clear, afs_uint32 *cipher, 
        fc_KeySchedule schedule, int encrypt);
extern afs_int32 fc_cbc_encrypt (char *input, char *output, afs_int32 length, 
        fc_KeySchedule key, afs_uint32 *xor, int encrypt);

/* rxkad_client.c */
extern int rxkad_AllocCID(struct rx_securityClass *aobj, struct rx_connection *aconn);
extern struct rx_securityClass *rxkad_NewClientSecurityObject(
        rxkad_level level, struct ktc_encryptionKey *sessionkey,
        afs_int32 kvno, int ticketLen, char *ticket);
extern int rxkad_GetResponse(struct rx_securityClass *aobj, 
        struct rx_connection *aconn, struct rx_packet *apacket);
extern void rxkad_ResetState(void);

/* rxkad_common.c */
#if 0
/* can't prototype these due to types */
extern int rxkad_SetupEndpoint(struct rx_connection *aconnp, 
        struct rxkad_endpoint *aendpointp);
extern afs_uint32 rxkad_CksumChallengeResponse(struct rxkad_v2ChallengeResponse *v2r);
#endif
extern int rxkad_DeriveXORInfo(struct rx_connection *aconnp, 
        fc_KeySchedule *aschedule, char *aivec, char *aresult);
extern void rxkad_SetLevel(struct rx_connection *conn, rxkad_level level);
extern int rxkad_Close(struct rx_securityClass *aobj);
extern int rxkad_NewConnection(struct rx_securityClass *aobj,
    struct rx_connection *aconn);
extern int rxkad_DestroyConnection(struct rx_securityClass *aobj, 
        struct rx_connection *aconn);
extern int rxkad_CheckPacket(struct rx_securityClass *aobj,
    struct rx_call *acall, struct rx_packet *apacket);
extern int rxkad_PreparePacket(struct rx_securityClass *aobj, 
        struct rx_call *acall, struct rx_packet *apacket);
extern int rxkad_GetStats(struct rx_securityClass *aobj, 
        struct rx_connection *aconn, struct rx_securityObjectStats *astats);


/* rxkad_errs.c */

/* rxkad_server.c */
extern struct rx_securityClass *rxkad_NewServerSecurityObject (
        rxkad_level level, char *get_key_rock, 
	int (*get_key)(char *get_key_rock, int kvno, struct ktc_encryptionKey *serverKey), 
	int (*user_ok)(char *name, char *instance, char *cell, afs_int32 kvno));
extern int rxkad_CheckAuthentication (struct rx_securityClass *aobj, 
        struct rx_connection *aconn);
extern int rxkad_CreateChallenge(struct rx_securityClass *aobj, 
        struct rx_connection *aconn);
extern int rxkad_GetChallenge (struct rx_securityClass *aobj, 
        struct rx_connection *aconn, struct rx_packet *apacket);
extern int rxkad_CheckResponse (struct rx_securityClass *aobj, 
        struct rx_connection *aconn, struct rx_packet *apacket);
extern afs_int32 rxkad_GetServerInfo (struct rx_connection *aconn, 
        rxkad_level *level, afs_uint32 *expiration, char *name, char *instance, 
        char *cell, afs_int32 *kvno);



/* ticket.c */
extern int tkt_DecodeTicket (char *asecret, afs_int32 ticketLen, 
        struct ktc_encryptionKey *key, char *name, char *inst, char *cell, 
        char *sessionKey, afs_int32 *host, afs_int32 *start, afs_int32 *end);
extern int tkt_MakeTicket (char *ticket, int *ticketLen, 
        struct ktc_encryptionKey *key, char *name, char *inst, char *cell,
        afs_uint32 start, afs_uint32 end, struct ktc_encryptionKey *sessionKey, 
        afs_uint32 host, char *sname, char *sinst);
extern int tkt_CheckTimes (afs_uint32 start, afs_uint32 end, afs_uint32 now);
extern afs_int32 ktohl (char flags, afs_int32 l);
extern afs_uint32 life_to_time (afs_uint32 start, unsigned char life);
extern unsigned char time_to_life (afs_uint32 start, afs_uint32 end);

/* ticket5.c */
extern int tkt_DecodeTicket5(char *ticket, afs_int32 ticket_len, 
		      int (*get_key)(char *, int, struct ktc_encryptionKey *),
		      char *get_key_rock,
		      int serv_kvno,
		      char *name, 
		      char *inst, 
		      char *cell, 
		      char *session_key, 
		      afs_int32 *host, 
		      afs_int32 *start, 
		      afs_int32 *end);

#endif
