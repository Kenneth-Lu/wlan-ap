//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_stats.h
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
#ifndef bc_stats_h
#define bc_stats_h
#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif
struct bc_url_request;
struct bc_url_data;
struct bc_init_params;

/*! Run statistics */
struct bc_stats
{
	unsigned int cache_hits;         /*!< number of cache hits, roll-up */
	unsigned int cloud_hits;         /*!< number of cloud hits, roll-up */
	unsigned int cloud_lookups;      /*!< number of cloud lookups, roll-up */
	unsigned int db_hits;            /*!< number of local database hits */
	unsigned int uncat_responses;    /*!< number of uncategorized responses */
	unsigned int cache_size;         /*!< number of cache entries */
	unsigned int queue_depth;        /*!< maximum queue depth reached */
	unsigned int trust_score;        /*!< trusted user rep average */
	unsigned int guest_score;        /*!< guest user rep average */
	unsigned int net_errors;         /*!< total number of lookup errors */
	unsigned int http_errors;        /*!< total number of lookup errors */
	unsigned int bcap_errors;        /*!< total number of lookup errors */
	unsigned int major;              /*!< current major version */
	unsigned int minor;              /*!< current minor version */
	unsigned int up_time;            /*!< up time in fuzzy seconds */
    	unsigned int cache_max_entries;  /*!< number of cache entries */
};

struct bc_stats_request
{
	struct bc_stats stats;
	int              ttl_bias[100];
	unsigned int     ttl;
	int              trust_bias;
	int              guest_bias;
	int              major;
	int              minor;
	unsigned int     flags;
};

int bc_stats_init(const struct bc_init_params* params);
void bc_stats_shutdown(void);
void bc_stats_reporting(int on_off);

unsigned int bc_stats_get_ttl(const struct bc_url_request* req);
unsigned int bc_stats_get_ttl_data(const struct bc_url_data* data);
int bc_stats_post_update(void);
void bc_stats_timer_event(void);
void bc_stats_get(struct bc_stats* stats);
void bc_stats_reset(void);
void bc_stats_update_rep_score(int rep, int trusted);

#ifdef __cplusplus
}
#endif

#endif /* bc_stats_h */


