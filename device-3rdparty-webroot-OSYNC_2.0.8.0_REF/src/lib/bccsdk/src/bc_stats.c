//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_stats.c
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
#include "bc_stats.h"
#include "bccsdk.h"
#include "bc_proto.h"
#include "bc_cache.h"
#include "bc_queue.h"
#include "bc_net.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#define HOURS_96 345600
#define HOURS_48 172800
#define HOURS_24 86400
#define HOURS_12 43200
#define HOURS_1  3600

static struct bc_connection Connection;
static struct bc_stats_request StatsRequest;

static int Enabled = 1;
static unsigned int LastUpdate = 0;
static unsigned int LastErrorTime = 0;

/* server data */
static int TTLBase = HOURS_96;
static int VersionMajor = 0;
static int VersionMinor = 0;

/* statistics */
double TrustCA = 0;
int    TrustCount = 0;
double GuestCA = 0;
int    GuestCount = 0;

int bc_stats_init(const struct bc_init_params* params)
{
	Enabled = params->enable_stats;
	Connection.fd = -1;
	return 0;
}

void bc_stats_shutdown(void)
{
}

void bc_stats_get(struct bc_stats* stats)
{
	if (!stats)
	{
		assert(stats);
		return;
	}
	memset(stats, 0, sizeof(struct bc_stats));
	stats->up_time = bc_net_get_clock();
	stats->trust_score = (int) TrustCA;
	stats->guest_score = (int) GuestCA;
	stats->major = VersionMajor;
	stats->minor = VersionMinor;
	bc_proto_get_stats(stats);
	bc_cache_get_stats(stats);
	bc_queue_get_stats(stats);
}

void bc_stats_reset(void)
{
	TrustCA = 0;
	TrustCount = 0;
	GuestCA = 0;
	GuestCount = 0;
	bc_proto_reset_stats();
	bc_cache_reset_stats();
	bc_queue_reset_stats();
}

static void error_handler(struct bc_connection* conn, int state)
{
	/* If we're here the connection failed */
	//struct bc_stats_request* req = (struct bc_stats_request*) conn->data;
	/* retry again in a minute or so */
	LastErrorTime = bc_net_get_clock();
}

static int stats_reader(struct bc_connection* conn)
{
	//struct bc_stats_request* req = (struct bc_stats_request*) conn->data;
	if (bc_proto_stats_read(conn) == -1)
	{
		/* last update not changed, reschedule? */
		bc_net_close(conn);
		Connection.busy = 0;
		return -1;
	}
	bc_net_close(conn);
	LastUpdate = bc_net_get_clock();
	conn->busy = 0;
	return 0;
}

int bc_stats_post_update(void)
{
	int r = 0;
	if (!Connection.busy)
	{
		memset(&StatsRequest, 0, sizeof(struct bc_stats_request));
		bc_stats_get(&StatsRequest.stats);
		bc_stats_reset();
		Connection.busy = 1;
		Connection.data = &StatsRequest;
		Connection.error_handler = error_handler;
		Connection.reader = stats_reader;
		if (bc_net_use_ssl())
			Connection.flags = BCA_NET_SSL;
		r = bc_proto_post_stats(&Connection, &StatsRequest);	
		if (r == -1)
		{
			// give up for now
			LastErrorTime = bc_net_get_clock();
			LastUpdate = bc_net_get_clock();
			Connection.busy = 0;
		}
	}
	return r;
}

void bc_stats_timer_event(void)
{
	if (Enabled && ((bc_net_get_clock() - LastUpdate >= HOURS_24) || LastUpdate == 0))
		bc_stats_post_update();
}

unsigned int bc_stats_get_ttl_data(const struct bc_url_data* data)
{
	return TTLBase;
}

unsigned int bc_stats_get_ttl(const struct bc_url_request* req)
{
	if (!req)
	{
		assert(req);
		return TTLBase;
	}
	return bc_stats_get_ttl_data(&req->data);
}


void bc_stats_update_rep_score(int rep, int trusted)
{
	if (trusted)
	{
		TrustCA += ((double)rep - TrustCA) / (TrustCount + 1);
		++TrustCount;
	}
	else
	{
		GuestCA += ((double)rep - GuestCA) / (GuestCount + 1);
		++GuestCount;
	}
}


