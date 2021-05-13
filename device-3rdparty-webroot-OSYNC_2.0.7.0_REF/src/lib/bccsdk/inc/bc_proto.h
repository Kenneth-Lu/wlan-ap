//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_proto.h
//
//  Description: Protocol stuff
//
//     history:
//
//	   date		 who	  what
//	   ====		 ===	  ====
//     11/15/13  TH		  ported to ISIS
//
//       CONFIDENTIAL and PROPRIETARY MATERIALS
//
//	This source code is covered by the Webroot Software Development
//	Kit End User License Agreement. Please read the terms of this
//    license before altering or copying the source code.  If you
//	are not willing to be bound by those terms, you may not view or
//	use this source code.
//
//	   	  Export Restrictions
//
//	This source code is subject to the U.S. Export Administration
//	Regulations and other U.S. laws, and may not be exported or
//	re-exported to certain countries (currently Cuba, Iran, Libya,
//	North Korea, Sudan and Syria) or to persons or entities
//	prohibited from receiving U.S. exports (including those (a)
//	on the Bureau of Industry and Security Denied Parties List or
//	Entity List, (b) on the Office of Foreign Assets Control list
//	of Specially Designated Nationals and Blocked Persons, and (c)
//	involved with missile technology or nuclear, chemical or
//	biological weapons).
//
//	   Copyright(c) 2006 - 2014
//	         Webroot, Inc.
//       385 Interlocken Crescent
//      Broomfield, Colorado, USA 80021
//
//////////////////////////////////////////////////////////////////////////
#ifndef bc_proto_h
#define bc_proto_h
#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif
struct bc_url_request;
struct bc_db_request;
struct bc_rtu_request;
struct bc_stats_request;
struct bc_connection;
struct bc_init_params;
struct bc_stats;

/* Flags */
#define BCA_FROM_QUEUE 0x01

int bc_proto_init(const struct bc_init_params* params);
void bc_proto_shutdown(void);
int bc_proto_getinfo(struct bc_url_request* req, unsigned int flags);
int bc_proto_get_db_version(struct bc_connection* conn, struct bc_db_request* req);
int bc_proto_get_db_version_read(struct bc_connection* conn);
int bc_proto_get_rtu(struct bc_connection* conn, struct bc_rtu_request* req);
int bc_proto_get_rtu_read(struct bc_connection* conn);
int bc_proto_post_stats(struct bc_connection* conn, struct bc_stats_request* req);
int bc_proto_stats_read(struct bc_connection* conn);
const char* bc_proto_get_uid(void);
const char* bc_proto_get_deviceid(void);
const char* bc_proto_get_oemid(void);

/* statistics */
void bc_proto_get_stats(struct bc_stats* stats);
void bc_proto_reset_stats(void);
unsigned int bc_proto_get_lookups(void);
unsigned int bc_proto_get_errors(void);

#ifdef __cplusplus
}
#endif

#endif /* bc_proto_h */


