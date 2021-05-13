//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_poll.h
//
//  Description: Asynchronous Event Loop
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
#ifndef bc_poll_h
#define bc_poll_h
#include <sys/types.h>

#define BC_POLL_READ 1
#define BC_POLL_WRITE 2

struct bc_poller;

struct bc_poller* bc_poll_create(size_t max_events, size_t max_timers);
void bc_poll_destroy(struct bc_poller* poller);

int bc_poll_add_fd(struct bc_poller* poller, int fd, int mode,
	void (*cb)(void*, int), void* arg);
void bc_poll_remove_fd(struct bc_poller* poller, int fd, int mode);

int bc_poll_add_timer(struct bc_poller* poller, int ms,
	void (*cb)(void*, int), void* arg, int restart);
void bc_poll_remove_timer(struct bc_poller* poller, int timer);

int bc_poll_event_loop(struct bc_poller* poller);
void bc_poll_terminate(struct bc_poller* poller, int return_value);

#endif // bc_poll_h

