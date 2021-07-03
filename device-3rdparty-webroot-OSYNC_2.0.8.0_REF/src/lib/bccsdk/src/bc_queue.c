//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_queue.c
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
#include "bc_queue.h"
#include "bc_alloc.h"
#include "bc_stats.h"
#include "bccsdk.h"
#include <assert.h>
#include <stdio.h>
#include <syslog.h>


/* stats */
static unsigned int Depth = 0;
static unsigned int MaxDepth = 0;

/* run */
static unsigned int DepthLimit = 2048;
static struct bc_url_request* ReqHead = 0;
static struct bc_url_request* ReqTail = 0;

int bc_queue_init(const struct bc_init_params* params)
{
	if (params->queue_max > 0)
		DepthLimit = params->queue_max;
	return 0;
}

void bc_queue_shutdown(void)
{
	struct bc_url_request* p;
	for (p = ReqHead; p; p = ReqHead)
	{
		ReqHead = p->next;
		bc_free(p);
	}
	ReqHead = ReqTail = 0;
}

int bc_queue_push(struct bc_url_request* req)
{
	//printf("bc_queue_push(%s)\n", req->url);
	if (Depth >= DepthLimit)
	{
		syslog(LOG_WARNING, "Queue limit reached (%d): failed request for %s", DepthLimit,
			req->url);
		return -1;
	}
	req->prev = req->next = 0;
	if (!ReqTail)
	{
		ReqHead = ReqTail = req;
	}
	else
	{
		ReqHead->prev = req;
		req->next = ReqHead;
		ReqHead = req;
	}
	if (++Depth > MaxDepth)
		MaxDepth = Depth;
	return 0;
}

struct bc_url_request* bc_queue_pop(void)
{
	struct bc_url_request* req = 0;
	if (ReqTail)
	{
		req = ReqTail;
		ReqTail = req->prev;
		if (!ReqTail)
			ReqHead = 0;
		else
			ReqTail->next = 0;
		req->prev = 0;
		req->next = 0;
		--Depth;
		//printf("bc_queue_pop(%s)\n", req->url);
	}
	return req;
}

int bc_queue_empty(void)
{
	return ReqTail == 0;
}

void bc_queue_get_stats(struct bc_stats* stats)
{
	stats->queue_depth = MaxDepth;
}

void bc_queue_reset_stats(void)
{
	MaxDepth = 0;
}

