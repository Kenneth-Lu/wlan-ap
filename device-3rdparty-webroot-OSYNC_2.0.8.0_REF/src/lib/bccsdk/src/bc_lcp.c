//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_lcp.c
//
//  Description: Optimized string functionality and linux/windows compatibility
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
#include "bc_lcp.h"
#include "bccsdk.h"
#include "bc_tld.h"
#include "bc_string.h"
#include "bc_alloc.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_TLD 256
static int LoopLimit = 10;

int bc_lcp_init(const struct bc_init_params* ip) {
    LoopLimit = ip->lcp_loop_limit;
    return 0;
}

void bc_lcp_shutdown(void) {}

int is_ip(const char* str) {
    if (!str || !*str)
	return 0;

    int i;
    unsigned int ip[4];
    if (sscanf(str, "%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]) != 4)
	return 0;
    for (i = 0; i < 4; ++i) {
	if (255 < ip[i])
	    return 0;
    }
    return 1;
}

static int over_limit(int subs, int paths) {
    int levels = subs + paths;
    return levels >= LoopLimit || levels >= LCP_POINTS_MAX - 1;
}

int bc_parse_lcp(struct bc_lcp_context* ctx, const char* url) {
    char tld[MAX_TLD + 1];
    const char* pathStart = 0;
    const char* slash = 0;
    const char* dot = 0;
    const char* urlEnd = 0;
    size_t len = 0;
    int subDomains = 0;
    int paths = 0;
    size_t urlLength;

    if (!url || !ctx) {
	assert(url);
	assert(ctx);
	return -1;
    }
    memset(ctx, 0, sizeof(struct bc_lcp_context));

    dot = strchr(url, '.');
    if (!dot) {
	return 0;
    }
    ctx->url = url;
    urlLength = strlen(url);
    if ((pathStart = strchr(url, '/'))) {
	ctx->domainEnd = pathStart;
    } else {
	ctx->domainEnd = url + urlLength;
    }
    if (ctx->domainEnd == url)
	return 0;
    if (is_ip(url)) {
	ctx->points[ctx->pointCount++] = url;
	++subDomains;
    } else {
	dot = ctx->domainEnd;
	len = dot - url;
	bc_strncpy(tld, url, len);
	tld[len] = '\0';
	const char* start = tld;
	const char* tld1 = bc_tld_plus_one(start);
	if (!tld1) {
	    if (bc_is_tld(start))
		return -1;
	    tld1 = start;
	}
	size_t index = tld1 - start;
	ctx->points[ctx->pointCount++] = url + index;
	++subDomains;
	if (index > 1) {
	    dot = url + index - 2;
	    while (dot != url) {
		if (*dot == '.') {
		    ctx->points[ctx->pointCount++] = dot + 1;
		    ++subDomains;
		    if (over_limit(subDomains, paths))
			break;
		}
		--dot; // walk left
	    }
	}
	ctx->points[ctx->pointCount++] = url;
	++subDomains;
    }

    if (over_limit(subDomains, paths) || !pathStart) {
	return subDomains + paths;
    }
    slash = pathStart + 1;
    urlEnd = url + urlLength;

    while (slash != urlEnd) {
	if (*slash == '/') {
	    ctx->points[ctx->pointCount++] = slash + 1;
	    ++paths;
	    if (over_limit(subDomains, paths))
		break;
	}
	++slash;
    }
    ctx->points[ctx->pointCount++] = url + urlLength;
    ++paths;
    return subDomains + paths;
}

int bc_next_lcp(struct bc_lcp_context* ctx, char* dest, size_t dest_size) {
    int idx;
    size_t len;
    if (!dest || !ctx || ctx->pointCount < 1) {
	assert(dest);
	assert(ctx->pointCount >= 1);
	return 0;
    }

    idx = ctx->pointCount - ctx->nextLevel - 1;
    if (idx < 0)
	return 0;

    if (ctx->points[idx] >= ctx->domainEnd) {
	len = ctx->points[idx] - ctx->url;
	if (len > dest_size - 1)
	    len = dest_size - 1;
	bc_strncpy(dest, ctx->url, len);
	if (dest[len - 1] == '/')
	    dest[len - 1] = '\0';
	else
	    dest[len] = '\0';
    } else {
	len = ctx->domainEnd - ctx->points[idx];
	if (len > dest_size - 1)
	    len = dest_size - 1;
	bc_strncpy(dest, ctx->points[idx], len);
	dest[len] = '\0';
    }
    ++ctx->nextLevel;
    return 1;
}
