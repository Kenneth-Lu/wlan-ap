//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_cache.h
//
//  Description: URL Cache
//
//     history:
//
//	   date		 who	  what
//	   ====		 ===	  ====
//     01/02/14  TH		Init
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
#ifndef bc_cache_h
#define bc_cache_h
#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

struct bc_url_request;
struct bc_url_data;
struct bc_init_params;
struct bc_stats;

/*! initialize the cache */
int bc_cache_init(const struct bc_init_params* ip);

/*! shutdown the cache (free allocated memory) */
void bc_cache_shutdown(void);

/*! Lookup a URL in the cache */
int bc_cache_lookup_url(struct bc_url_request* req);

/*! Add a URL to the cache */
/*!
	Returns 1 if record added to the cache, 0 if not.
	There are a number of reasons why something might not get added including:
	out of memory, the record already exists, cache full, etc. It is not necessarily
	an error is a record is not added to the cache and this call returns 0.
*/
int bc_cache_add(const struct bc_url_request* req);

/*! Add a database entry */
int bc_cache_add_db(uint64_t key[2], const struct bc_url_data* data);

/*! Update a cache entry */
int bc_cache_update(uint64_t key[2], const struct bc_url_data* data);

/*! Get total number of cache hits */
unsigned int bc_cache_get_hits(void);
void bc_cache_get_stats(struct bc_stats* stats);
void bc_cache_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* bc_cache_h */


