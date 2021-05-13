//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_cache.c
//
//  Description: URL Cache implementation
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
//		  Export Restrictions
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
//		 Webroot, Inc.
//       385 Interlocken Crescent
//      Broomfield, Colorado, USA 80021
//
//////////////////////////////////////////////////////////////////////////
#include "bc_cache.h"
#include "bccsdk.h"
#include "icache.h"
#include "bc_lcp.h"
#include "bc_string.h"
#include "bc_alloc.h"
#include "bc_stats.h"
#include "bc_util.h"
#include "bc_url.h"
#include "md5.h"
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#define MB 1048576

static struct icache* Cache = 0;
static int CacheMb = 20; /* 20 MB */

/* stats */
static unsigned int CacheHits = 0;

int bc_cache_init(const struct bc_init_params* ip) {
    size_t size = 0;
    unsigned int count;
    unsigned int seed = 0;
    unsigned int i;
    CacheMb = ip->max_cache_mb;

    /* use the unit id (serial) for srand */
    count = strlen(ip->uid);
    for (i = 0; i < count; ++i)
	seed += ip->uid[i];
    srand(seed);

    if (CacheMb > 0) {
	size = CacheMb * MB;
	count = icache_calc_record_count(size);
	syslog(LOG_INFO, "Creating %d MB cache capacity: %d entries",
	       CacheMb, count);
	Cache = icache_create(count);
	if (!Cache) {
	    syslog(LOG_ERR, "Cannot create URL cache: %s", strerror(errno));
	} else {
	    /* 96 hours default */
	    icache_set_ttl(Cache, 345600);
	}
	return Cache ? 0 : -1;
    }
    return 0;
}

void bc_cache_shutdown(void) {
    if (Cache) {
	icache_destroy(Cache);
	Cache = 0;
    }
}

void bc_cache_get_stats(struct bc_stats* stats) {
    struct icache_stats i;
    if (!stats) {
	assert(stats);
	return;
    }
    stats->cache_hits = CacheHits;
    if (Cache) {
	icache_analyze(Cache, &i);
	stats->cache_size = i.record_count;
        stats->cache_max_entries = i.max_records;
    }
}

void bc_cache_reset_stats(void) {
    CacheHits = 0;
}

int bc_cache_lookup_url(struct bc_url_request* req) {
    struct bc_lcp_context ctx;
    struct bc_url_data* data;
    char lcp[BCA_URL_LENGTH];
    char buf[BCA_URL_LENGTH];
    char* p;
    uint64_t key[2];

    if (!Cache)
	return 0;
    if (bc_parse_lcp(&ctx, req->url) < 1)
	return -1;
    while (bc_next_lcp(&ctx, lcp, BCA_URL_LENGTH)) {
	//printf("Cache lcp: %s\n", lcp);
	md5_strtokey(lcp, strlen(lcp), key);
	data = icache_lookup(Cache, key);
	if (data) {
	    //printf("Cache found: %s\n", lcp);
	    /* found it */
	    if (strcmp(req->url, lcp) == 0) {
		//printf("Cache match: %s\n", lcp);
		memcpy(&req->data, data, sizeof(struct bc_url_data));
		//bc_cpystrn(req->url, lcp, BCA_URL_LENGTH);
		bc_cpystrn(req->lcp, lcp, BCA_URL_LENGTH);
		req->data.flags |= BCA_FLAG_CACHED;
		++CacheHits;
		bc_stats_update_rep_score(req->data.reputation, req->trusted);
		//bc_print_md5_match(lcp, key, data);
		return 1;
	    } else if ((data->flags & BCA_FLAG_ONE_CAT) != 0) {
		bc_cpystrn(buf, req->url, BCA_URL_LENGTH);
		p = strchr(buf, '/');
		if (p)
		    *p = '\0';
		if (strcmp(buf, lcp) == 0) {
		    //printf("Cache a1 match: %s\n", lcp);
		    memcpy(&req->data, data, sizeof(struct bc_url_data));
		    //bc_cpystrn(req->url, lcp, BCA_URL_LENGTH);
		    bc_cpystrn(req->lcp, lcp, BCA_URL_LENGTH);
		    req->data.flags |= BCA_FLAG_CACHED;
		    ++CacheHits;
		    bc_stats_update_rep_score(req->data.reputation, req->trusted);
		    //bc_print_md5_match(lcp, key, data);
		    return 1;
		}
	    } else {
		/* didn't match */
		//printf("Cache found: ");
		//bc_print_md5_match(lcp, key, data);
		break;
	    }
	}
    }
    return 0;
}

int bc_cache_update(uint64_t key[2], const struct bc_url_data* in) {
    struct bc_url_data* data;
    if (!Cache)
	return 0;
    data = icache_lookup(Cache, key);
    if (!data)
	return 0;
    // no a1cat flag in update
    struct bc_url_data tmp;
    memcpy(&tmp, in, sizeof(struct bc_url_data));
    tmp.flags = data->flags;
    icache_remove(Cache, key);
    return icache_add(Cache, key, &tmp, bc_stats_get_ttl_data(&tmp)) == 0;
}


int bc_cache_add(const struct bc_url_request* req) {
    uint64_t key[2];
    char buf[BCA_URL_LENGTH] = { 0 };
    const char* p;
    char* s;
    if (!Cache)
	return 0;

    p = req->url;
    if (strcmp(req->lcp, req->url) != 0) {
	// strings don't match, need to do more checks
	if ((req->data.flags & BCA_FLAG_ONE_CAT) != 0) {
	    bc_cpystrn(buf, req->url, BCA_URL_LENGTH);
	    s = strchr(buf, '/');
	    if (s)
		*s = '\0';
	    p = buf;
	}
    }
    //printf("Cache add: %s\n", p);
    md5_strtokey(p, strlen(p), key);
    return icache_add(Cache, key, &req->data, bc_stats_get_ttl(req)) == 0;
}

int bc_cache_add_db(uint64_t key[2], const struct bc_url_data* data_in) {
    if (!Cache)
	return 0;

    return icache_add(Cache, key, data_in, bc_stats_get_ttl_data(data_in)) == 0;
}

unsigned int bc_cache_get_hits(void) {
    return CacheHits;
}
