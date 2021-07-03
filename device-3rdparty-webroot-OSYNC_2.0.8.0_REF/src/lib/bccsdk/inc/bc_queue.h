//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_queue.h
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
#ifndef bc_queue_h
#define bc_queue_h
#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif
struct bc_url_request;
struct bc_init_params;
struct bc_stats;

int bc_queue_init(const struct bc_init_params* params);
void bc_queue_shutdown(void);
void bc_queue_get_stats(struct bc_stats* stats);
void bc_queue_reset_stats(void);
int bc_queue_push(struct bc_url_request* req);
struct bc_url_request* bc_queue_pop(void);
int bc_queue_empty(void);

#ifdef __cplusplus
}
#endif

#endif /* bc_queue_h */


