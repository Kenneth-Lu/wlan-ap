//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_rtu.c
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
#include "bc_rtu.h"
#include "bc_net.h"
#include "bc_http.h"
#include "bc_proto.h"
#include "bc_string.h"
#include "bc_cache.h"
#include "bccsdk.h"
#include "md5.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <syslog.h>

#define MAX_LINE 2048
#define REPUTATION_CATEGORY 84
static struct bc_connection Connection;
static struct bc_rtu_request RtuRequest;

static void (*Subscriber)(const uint64_t key[2], const struct bc_url_data* data);
static int PollTime = 300;
static int Enabled = 0;

/* runtime */
static int Major = 0;
static int Minor = 0;
static int Rtu = 0;
static int Updating = 0;
static uint32_t LastUpdate = 0;
unsigned long RtuTimerId = 0;

int bc_rtu_init(const struct bc_init_params* params)
{
	Enabled = params->enable_rtu;
	PollTime = params->rtu_poll > 300 ? params->rtu_poll : 300;
	memset(&Connection, 0, sizeof(struct bc_connection));
	Connection.fd = -1;
	return 0;
}

void bc_rtu_shutdown(void)
{
	if (Connection.fd != -1)
		bc_net_close(&Connection);
}

int bc_rtu_query_active(void)
{
	return Connection.busy;
}

void bc_rtu_set_subscriber(
	void (*callback)(const uint64_t key[2], const struct bc_url_data* data))
{
	Subscriber = callback;
}

static void clear_connection(int clear_data)
{
	if (clear_data)
		Connection.data = 0;
	Connection.reader = 0;
	Connection.error_handler = 0;
	Connection.buf[0] = '\0';
	Connection.buf_len = 0;
	Connection.busy = 0;
	Connection.flags = 0;
	Connection.host[0] = '\0';
}

static void cleanup(struct bc_connection* conn)
{
	bc_net_close(conn);
	clear_connection(1);
	Updating = 0;
}

static void error_handler(struct bc_connection* conn, int state)
{
	/* If we're here the connection failed */
	cleanup(conn);
}

static void update_cache_reputation(char* buf)
{
	uint64_t key[2];
	struct bc_url_data data;
	char* md5 = buf;
	char* start = md5;
	uint16_t cc;
	uint8_t cat;
	uint8_t conf;
	int i = 0;
	memset(&data, 0, sizeof(struct bc_url_data));
	// look for end of md5 and terminate it
	while (*start && !isspace(*start))
		++start;
	*start++ = '\0';
	// convert key
	md5_hashtokey(md5, key);
	while (*start)
	{
		while (*start && isspace(*start))
			++start;
		if (*start)
		{
			cc = atoi(start);
			cat = cc >> 8;
			conf = cc & 0xFF;
			if (cat == 84)
				data.reputation = conf;
			else
			{
				data.cc[i].category = cat;
				data.cc[i].confidence = conf;
				++i;
			}
			while (*start && !isspace(*start))
				++start;
		}
	}
	bc_cache_update(key, &data);
	if (Subscriber)
		Subscriber(key, &data);

	//printf("%16llx%16llx CC: %d,%d R: %d\n", (unsigned long long)key[0],
	//	(unsigned long long)key[1], data.cc[0].category,
	//	data.cc[0].confidence, data.reputation);
}

static int stream_update(struct bc_connection* conn, size_t content_length)
{
	size_t remaining = content_length;
	char* end = 0;
	int n;
	while (remaining > 0)
	{
		n = bc_net_readline(conn, conn->buf, &end, BCA_NET_BUFFER_SIZE);
		if (n < 1)
			break;
		//printf("%s", conn->buf);
		if (conn->buf[0] != '[' && isxdigit(conn->buf[0]))
			update_cache_reputation(conn->buf);
		remaining -= n;
	}
	return 0;
}

void rtu_timer_event(void* data, int timer_id)
{
	bc_rtu_update();
}

/* NOTE: Download code bypasses network/protocol stack */
static int rtu_download_reader(struct bc_connection* conn)
{
	struct bc_http_header hdr;
	struct bc_rtu_request* req = (struct bc_rtu_request*) conn->data;
	if (bc_http_read_header(conn, &hdr) > 0 && hdr.result_code == 200 &&
		hdr.content_length > 0 && stream_update(conn, hdr.content_length) == 0)
	{
		Major = req->major;
		Minor = req->minor;
		Rtu = req->rtu;
		syslog(LOG_INFO, "Update to rtu version %d.%d.%d", Major, Minor, Rtu);
		//printf("current %d.%d.%d\n", Major, Minor, Rtu);
		if (bc_net_get_poller())
		{
			if (!RtuTimerId)
			{
				cleanup(conn);
				bc_net_add_timer(10, rtu_timer_event, 0);
			}
		}
		else
		{
			bc_net_close(conn);
			clear_connection(1);
		}
	}
	else
	{
		cleanup(conn);
		return -1;
	}
	return 0;
}

static int download_update(struct bc_connection* conn)
{
	const char* RequestFormat =
		"GET /%s HTTP/1.1\r\n"
		"Host: %s:443\r\n"
		"Connection: close\r\n\r\n";

	char host[256];
	char path[256];
	char* p = 0;
	struct bc_rtu_request* req = (struct bc_rtu_request*) conn->data;
	if (!req)
	{
		assert(req);
		return -1;
	}
	bc_cpystrn(host, req->contentLocation, 256);
	p = strchr(host, '/');
	if (!p)
	{
		// garbled response, skip it
		++Rtu;
		syslog(LOG_ERR, "Invalid path for RTU: %s", req->contentLocation);
		cleanup(conn);
		return -1;
	}
	*p++ = '\0';
	bc_cpystrn(path, p, 256);

	clear_connection(0);
	conn->busy = 1;
	snprintf(conn->buf, BCA_NET_BUFFER_SIZE, RequestFormat, path, host);
	//printf("Download request: %s", conn->buf);
	conn->buf_len = strlen(conn->buf);
	conn->error_handler = error_handler;
	conn->reader = rtu_download_reader;
	conn->flags = BCA_NET_SSL | BCA_NET_USEHOST;
	bc_cpystrn(conn->host, host, BCA_NET_HOST_SIZE);
	if (bc_net_write(conn) == -1)
	{
		cleanup(conn);
		return -1;
	}
	return 0;
}

static int get_rtu_read(struct bc_connection* conn)
{
	struct bc_rtu_request* req = (struct bc_rtu_request*) conn->data;
	if (bc_proto_get_rtu_read(conn) == -1)
	{
		cleanup(conn);
		return -1;
	}
	if (req->major != Major || req->minor != Minor || req->rtu != Rtu)
	{
		if (req->update_available)
		{
			download_update(conn);
		}
		else
		{
			//printf("No new updates available\n");
			cleanup(conn);
		}
	}
	else
	{
		cleanup(conn);
	}
	return 0;
}

int bc_rtu_update(void)
{
	struct bc_rtu_request* req = &RtuRequest;
	if (Connection.busy)
	{
		// already doing an update
		return -1;
	}
	Updating = 1;
	memset(req, 0, sizeof(struct bc_rtu_request));
	req->major = Major;
	req->minor = Minor;
	req->rtu = Rtu;
	Connection.busy = 1;
	Connection.data = req;
	Connection.error_handler = error_handler;
	Connection.reader = get_rtu_read;
	if (bc_net_use_ssl())
		Connection.flags = BCA_NET_SSL;
	if (bc_proto_get_rtu(&Connection, req) == -1)
	{
		cleanup(&Connection);
		return -1;
	}	
	return 0;
}

static void do_updates(void)
{
	bc_rtu_update();
	LastUpdate = bc_net_get_clock();
}

void bc_rtu_update_loop(void)
{
	Updating = 1;
	while (Updating)
		bc_rtu_update();
}

void bc_rtu_timer_event(void)
{
	if (Enabled && (LastUpdate + PollTime < bc_net_get_clock() || LastUpdate == 0))
		do_updates();
}

