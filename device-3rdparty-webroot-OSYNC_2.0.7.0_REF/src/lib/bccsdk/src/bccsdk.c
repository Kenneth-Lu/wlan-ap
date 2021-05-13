/* -*- C -*- */
/*
 *
 *  Filename: bccsdk.c
 *
 *  Description: bccsdk library implementation
 *
 *     history:
 *
 *	   date		 who	  what
 *	   ====		 ===	  ====
 *     11/14/13	 TH		  ported to ISIS
 *
 *       CONFIDENTIAL and PROPRIETARY MATERIALS
 *
 *	This source code is covered by the Webroot Software Development
 *	Kit End User License Agreement. Please read the terms of this
 *    license before altering or copying the source code.  If you
 *	are not willing to be bound by those terms, you may not view or
 *	use this source code.
 *
 *		  Export Restrictions
 *
 *	This source code is subject to the U.S. Export Administration
 *	Regulations and other U.S. laws, and may not be exported or
 *	re-exported to certain countries (currently Cuba, Iran, Libya,
 *	North Korea, Sudan and Syria) or to persons or entities
 *	prohibited from receiving U.S. exports (including those (a)
 *	on the Bureau of Industry and Security Denied Parties List or
 *	Entity List, (b) on the Office of Foreign Assets Control list
 *	of Specially Designated Nationals and Blocked Persons, and (c)
 *	involved with missile technology or nuclear, chemical or
 *	biological weapons).
 *
 *	   Copyright(c) 2006 - 2014
 *		 Webroot, Inc.
 *       385 Interlocken Crescent
 *      Broomfield, Colorado, USA 80021
 *
 */
#include "bccsdk.h"
#include "bc_lcp.h"
#include "bc_url.h"
#include "bc_cache.h"
#include "bc_db.h"
#include "bc_rtu.h"
#include "bc_net.h"
#include "bc_proto.h"
#include "bc_stats.h"
#include <string.h>
#include <assert.h>
#include <syslog.h>
#include <ctype.h>

void bc_shutdown(void) {
    bc_net_shutdown();
    bc_db_shutdown();
    bc_rtu_shutdown();
    bc_proto_shutdown();
    bc_lcp_shutdown();
    bc_cache_shutdown();
    bc_stats_shutdown();
    closelog();
}

int bc_initialize(const struct bc_init_params* params) {
    if (bc_lcp_init(params) == -1)
	return -1;
    if (bc_cache_init(params) == -1) {
	bc_shutdown();
	return -1;
    }
    if (bc_net_init(params) == -1) {
	bc_shutdown();
	return -1;
    }
    if (bc_proto_init(params) == -1) {
	bc_shutdown();
	return -1;
    }
    if (bc_stats_init(params) == -1) {
	bc_shutdown();
	return -1;
    }
    if (bc_db_init(params) == -1) {
	bc_shutdown();
	return -1;
    }
    if (bc_rtu_init(params) == -1) {
	bc_shutdown();
	return -1;
    }
    return 0;
}

int validHost(const char* u) {
    const char* s = strstr(u, "://");
    if (s)
	s += 3;
    else
	s = u;
    const char* path = strchr(s, '/');
    const char* port = strchr(s, ':');
    if (port) {
	if (path && port < path)
	    path = port;
    }
    if (!path)  {
	const char* query = strchr(s, '?');
	if (query)
	    path = query;
	else
	    path = s + strlen(s);
    }
    size_t host_len = path - s;
    if (host_len < 4 || host_len > 255)
	return 0;
    for (; s < path; ++s) {
	if (!(isalnum(*s) || *s == '-' || *s == '_' || *s == '.'))
	    return 0;
    }
    return 1;
}

int bc_init_request(struct bc_url_request* req, const char* url,
		    int trusted, int serial) {
    if (!req || !url) {
	assert(req);
	assert(url);
	return -1;
    }
    memset(req, 0, sizeof(struct bc_url_request));
    if (!validHost(url) || bc_extract_url(req->url, url, BCA_URL_LENGTH) == -1) {
	/* invalid URL */
	syslog(LOG_WARNING, "Invalid URL: %s", url);
	return -1;
    }
    req->serial = serial;
    req->trusted = trusted != 0;
    return 0;
}

int bc_cache_lookup(struct bc_url_request* req) {
    return bc_cache_lookup_url(req);
}

int bc_cloud_lookup(struct bc_url_request* req,
		    void (*callback)(struct bc_url_request* info_out, int rc)) {
    if (!req) {
	assert(req);
	return -1;
    }
    req->callback = callback;
    if (bc_proto_getinfo(req, 0) == -1) {
	if (req->callback)
	    req->callback(req, -1);
	else
	    return -1;
    }
    return 0;
}

void bc_rtu_subscribe(void (*callback)(const uint64_t key[2],
				       const struct bc_url_data* data)) {
    bc_rtu_set_subscriber(callback);
}

int bc_update_database(struct bc_db_request* req,
		       void (*callback)(struct bc_db_request* out,
					int rc)) {
    memset(req, 0, sizeof(struct bc_db_request));
    req->callback = callback;
    if (bc_db_update(req) == -1) {
	if (req->callback) {
	    req->callback(req, -1);
	}
	else
	    return -1;
    }
    return 0;
}

int bc_open_database(const char* path, struct bc_db_context* ctx) {
    return bc_db_open(path, ctx);
}

int bc_next_record(struct bc_db_context* ctx, struct bc_db_record* rec) {
    return bc_db_next(ctx, rec);
}

void bc_close_database(struct bc_db_context* ctx) {
    bc_db_close(ctx);
}

void bc_params_update(struct bc_init_params *params) {
    bc_net_update(params);
}
