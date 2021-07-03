//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_rtu.h
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
#ifndef bc_rtu_h
#define bc_rtu_h
#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
struct bc_init_params;
struct bc_url_data;
struct bc_rtu_request
{
	int major;
	int minor;
	int rtu;
	int update_available;
	char contentLocation[1024];
	char contentChecksum[34];
	char repLocation[1024];
	char repChecksum[34];
};

int bc_rtu_init(const struct bc_init_params* params);
void bc_rtu_shutdown(void);
int bc_rtu_update(void);
int bc_rtu_query_active(void);
void bc_rtu_update_loop(void);
void bc_rtu_timer_event(void);
void bc_rtu_set_subscriber(
	void (*callback)(const uint64_t key[2], const struct bc_url_data* data));

#ifdef __cplusplus
}
#endif

#endif /* bc_rtu_h */


