//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_db.h
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
#ifndef bc_db_h
#define bc_db_h
#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif
struct bc_init_params;
struct bc_db_request;
struct bc_db_context;
struct bc_db_record;

int bc_db_init(const struct bc_init_params* params);
void bc_db_shutdown(void);
int bc_db_update(struct bc_db_request* req);
int bc_db_query_active(void);

int bc_db_open(const char* path, struct bc_db_context* ctx);
int bc_db_next(struct bc_db_context* ctx, struct bc_db_record* rec);
void bc_db_close(struct bc_db_context* ctx);

#ifdef __cplusplus
}
#endif

#endif /* bc_db_h */


