/* -*- C -*- */
/*
 *
 *  Filename: icache.c
 *
 *  Description: md5 hashed cache implementation
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
#include "icache.h"
#include "bc_alloc.h"
#include "bc_net.h"
#include "bccsdk.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

struct icache_bucket {
    uint64_t              key[2];   /* md5 hash 2x64 bit values */
    struct icache_bucket* next;     /* for hash table and free list */
    uint32_t              expire;   /* when record expires */
    struct bc_url_data   data;     /* what we're looking for */
};

struct icache {
    struct icache_bucket** table;    /* the hash table */
    struct icache_bucket* free_list; /* spare buckets */
    struct icache_bucket* buckets;   /* all buckets */
    unsigned int table_size;         /* size of the hash table */
    unsigned int ttl;                /* how long records live */
    unsigned int record_count;       /* how many active buckets */
    unsigned int record_limit;       /* max number of records */
};

#define ICACHE_VALID(c) (c && c->table && c->table_size != 0)
#define ICACHE_KEY_EQUAL(x,y) (x[0] == y[0] && x[1] == y[1])
#define MD5_STRING_LEN 32
#define MD5_INDEX 1  /* somewhat fewer collisions using lower part of hash */
#define OVERLOAD_FACTOR 1
#define RANDOM_PURGE_COUNT 256
#ifdef _MSC_VER
#define strtoull _strtoui64
#endif

/*
 * Table of primes from 2^8 - 2^31
 * least positive integer k such that 2^n - k is prime
 */
static unsigned int prime_tab[] = {
    251,        /* n = 8, k = 5 */
    509,        /* n = 9, k = 3 */
    1021,       /* n = 10, k = 3 */
    2039,       /* n = 11, k = 9 */
    4093,       /* n = 12, k = 3 */
    8191,       /* n = 13, k = 1 */
    16381,      /* n = 14, k = 3 */
    32749,      /* n = 15, k = 19 */
    65521,      /* n = 16, k = 15 */
    131071,     /* n = 17, k = 1 */
    262139,     /* n = 18, k = 5 */
    524287,     /* n = 19, k = 1 */
    1048573,    /* n = 20, k = 3 */
    2097143,    /* n = 21, k = 9 */
    4194301,    /* n = 22, k = 3 */
    8388593,    /* n = 23, k = 15 */
    16777213,   /* n = 24, k = 3 */
    33554393,   /* n = 25, k = 39 */
    67108859,   /* n = 26, k = 5 */
    134217689,  /* n = 27, k = 39 */
    268435399,  /* n = 28, k = 57 */
    536870909,  /* n = 29, k = 3 */
    1073741789, /* n = 30, k = 35 */
    2147483647	/* n = 31, k = 1 */
};

static const unsigned int PRIME_TAB_SIZE = sizeof(prime_tab) / sizeof(unsigned int);

static unsigned int NextExpire = 0;

static unsigned int calc_table_size(unsigned int n) {
    unsigned i = 0;
    unsigned int count = n / OVERLOAD_FACTOR;
    if (n <= prime_tab[0])
	return prime_tab[0];

    for (i = 0; i < PRIME_TAB_SIZE - 1; ++i) {
	if (prime_tab[i] <= count && count < prime_tab[i + 1])
	    return prime_tab[i];
    }
    return prime_tab[PRIME_TAB_SIZE - 1];
}

struct icache* icache_create(unsigned int record_count) {
    unsigned int i;
    struct icache_bucket* bp;
    unsigned int table_size = calc_table_size(record_count);
    struct icache* p = (struct icache*) bc_malloc(sizeof(struct icache));
    if (!p) {
	errno = ENOMEM;
	return NULL;
    }
    memset(p, 0, sizeof(struct icache));
    p->table = (struct icache_bucket**)bc_malloc(sizeof(struct icache_bucket*) * table_size);
    if (!p->table) {
	bc_free(p);
	errno = ENOMEM;
	return NULL;
    }
    memset(p->table, 0, sizeof(struct icache_bucket*) * table_size);
    p->table_size = table_size;
    p->ttl = 86400; /* default to a day */
    p->buckets = (struct icache_bucket*)bc_malloc(sizeof(struct icache_bucket) * record_count);
    if (!p->buckets) {
	bc_free(p->table);
	bc_free(p);
	errno = ENOMEM;
	return NULL;
    }
    for (i = 0; i < record_count; ++i) {
	bp = &p->buckets[i];
	memset(bp, 0, sizeof(struct icache_bucket));
	bp->next = p->free_list;
	p->free_list = bp;
    }
    p->record_limit = record_count;
    return p;
}

void icache_destroy(struct icache* cache) {
    if (!ICACHE_VALID(cache)) {
	assert(ICACHE_VALID(cache));
	errno = EINVAL;
	return;
    }
    bc_free(cache->buckets);
    bc_free(cache->table);
    bc_free(cache);
}

int icache_set_ttl(struct icache* cache, unsigned int seconds) {
    if (!ICACHE_VALID(cache)) {
	assert(ICACHE_VALID(cache));
	errno = EINVAL;
	return -1;
    }
    if (seconds < 60)
	seconds = 60;
    cache->ttl = seconds;
    return 0;
}

static void convert_md5_to_key(const char* md5, uint64_t key[2]) {
    char buf[MD5_STRING_LEN + 1];
    strncpy(buf, md5, MD5_STRING_LEN);
    buf[MD5_STRING_LEN] = '\0';
    key[1] = strtoull(buf + 16, NULL, 16);
    buf[16] = '\0';
    key[0] = strtoull(buf, NULL, 16);
}

int icache_add(struct icache* cache, uint64_t key[2],
	       const struct bc_url_data* data, unsigned int ttl) {
    unsigned int idx = 0;
    struct icache_bucket* bucket = 0;
    if (!data || !ICACHE_VALID(cache)) {
	assert(ICACHE_VALID(cache));
	assert(data);
	errno = EINVAL;
	return -1;
    }
    idx = key[MD5_INDEX] % cache->table_size;
    /* scan for dups */
    for (bucket = cache->table[idx]; bucket; bucket = bucket->next) {
	if (ICACHE_KEY_EQUAL(key, bucket->key)) {
	    /* already in the table, punt on policy for now */
	    return 1;
	}
    }
    if (cache->record_count >= cache->record_limit) {
	/* LRU would be faster here */
	if (icache_purge(cache) < 1) {
	    return -1;
	}
    }
    /* if we're here, better get a bucket */
    if (cache->free_list) {
	bucket = cache->free_list;
	cache->free_list = bucket->next;
    } else {
	errno = ENOMEM;
	return -1;
    }
    memset(bucket, 0, sizeof(struct icache_bucket));
    bucket->key[0] = key[0];
    bucket->key[1] = key[1];
    if (ttl == 0)
	ttl = cache->ttl;
    bucket->expire = bc_net_get_clock() + ttl;
    memcpy(&bucket->data, data, sizeof(struct bc_url_data));
    bucket->next = cache->table[idx];
    cache->table[idx] = bucket;
    ++cache->record_count;
    return 0;
}

int icache_add_str(struct icache* cache, const char* md5,
		   const struct bc_url_data* data, unsigned int ttl) {
    uint64_t key[2];
    convert_md5_to_key(md5, key);
    return icache_add(cache, key, data, ttl);
}

int icache_remove(struct icache* cache, uint64_t key[2]) {
    struct icache_bucket** prev = 0;
    struct icache_bucket* bucket = 0;
    unsigned int idx = 0;

    errno = 0;
    if (!ICACHE_VALID(cache)) {
	assert(ICACHE_VALID(cache));
	errno = EINVAL;
	return -1;
    }
    idx = key[MD5_INDEX] % cache->table_size;
    for (prev = &cache->table[idx]; *prev; ) {
	if (ICACHE_KEY_EQUAL(key, (*prev)->key)) {
	    bucket = *prev;
	    *prev = bucket->next;
	    bucket->next = cache->free_list;
	    cache->free_list = bucket;
	    --cache->record_count;
	    return 0;
	}
	prev = &(*prev)->next;
    }
    /* not found */
    return 1;
}

int icache_remove_str(struct icache* cache, const char* md5) {
    uint64_t key[2];
    convert_md5_to_key(md5, key);
    return icache_remove(cache, key);
}

static int random_purge(struct icache* cache) {
    struct icache_bucket* bucket = 0;
    struct icache_bucket** prev = 0;
    int purge_count = 0;
    unsigned i;
    unsigned index;
    for (i = 0; i < RANDOM_PURGE_COUNT; ++i) {
	for (;;) {
	    index = rand() % cache->table_size;
	    prev = &cache->table[index];
	    if (*prev)
		break;
	}
	while ((*prev)->next)
	    prev = &(*prev)->next;
	bucket = *prev;
	*prev = 0;
	bucket->next = cache->free_list;
	cache->free_list = bucket;
	--cache->record_count;
	++purge_count;
    }
    return purge_count;
}

int icache_purge(struct icache* cache) {
    struct icache_bucket** prev = 0;
    struct icache_bucket* bucket = 0;
    int purge_count = 0;
    unsigned int i = 0;
    unsigned int now = 0;
    unsigned int expire = 0xffffffff;

    errno = 0;
    if (!ICACHE_VALID(cache)) {
	assert(ICACHE_VALID(cache));
	errno = EINVAL;
	return -1;
    }
    now = bc_net_get_clock();
    if (NextExpire > now) {
	return random_purge(cache);
    }
    for (i = 0; i < cache->table_size; ++i) {
	for (prev = &cache->table[i]; *prev; ) {
	    if ((*prev)->expire <= now) {
		bucket = *prev;
		*prev = bucket->next;
		bucket->next = cache->free_list;
		cache->free_list = bucket;
		--cache->record_count;
		++purge_count;
	    } else {
		if ((*prev)->expire < expire)
		    expire = (*prev)->expire;
		prev = &(*prev)->next;
	    }
	}
    }
    NextExpire = expire;
    if (purge_count == 0)
	purge_count = random_purge(cache);
    return purge_count;
}

struct bc_url_data* icache_lookup(struct icache* cache, uint64_t key[2]) {
    struct icache_bucket** prev = 0;
    struct icache_bucket* bucket = 0;
    unsigned int idx = 0;
    unsigned int now = 0;

    errno = 0;
    if (!ICACHE_VALID(cache)) {
	assert(ICACHE_VALID(cache));
	errno = EINVAL;
	return 0;
    }
    now = bc_net_get_clock();
    idx = key[MD5_INDEX] % cache->table_size;
    for (prev = &cache->table[idx]; *prev; ) {
	if (ICACHE_KEY_EQUAL(key, (*prev)->key)) {
	    if (now < (*prev)->expire) {
		return &(*prev)->data;
	    } else {
		bucket = *prev;
		*prev = bucket->next;
		bucket->next = cache->free_list;
		cache->free_list = bucket;
		--cache->record_count;
		return 0;
	    }
	}
	prev = &(*prev)->next;
    }
    /* not found */
    return 0;
}

struct bc_url_data* icache_lookup_str(struct icache* cache, const char* md5) {
    uint64_t key[2];
    convert_md5_to_key(md5, key);
    return icache_lookup(cache, key);
}

unsigned int icache_calc_record_count(unsigned int max_mem) {
    unsigned int mem = 0;
    unsigned int count = 0;
    unsigned int table_size = 0;
    mem = max_mem - sizeof(struct icache);
    count = mem / (sizeof(struct icache_bucket) + sizeof(struct icache_bucket*));
    table_size = calc_table_size(count);
    return (mem - table_size * sizeof(struct icache_bucket*)) /
	sizeof(struct icache_bucket);
}

int icache_analyze(const struct icache* cache, struct icache_stats* stats) {
    const struct icache_bucket* bucket = 0;
    unsigned int i = 0;
    unsigned int chain_length = 0;

    if (!ICACHE_VALID(cache) || !stats) {
	assert(ICACHE_VALID(cache));
	assert(stats);
	errno = EINVAL;
	return -1;
    }

    stats->table_size = cache->table_size;
    stats->record_count = cache->record_count;
    stats->max_records = cache->record_limit;
    stats->record_ttl = cache->ttl;
    stats->table_used = 0;
    stats->buckets_allocated = 0;
    stats->table_memory_used = 0;
    for (i = 0; i < 10; ++i)
	stats->chain_lengths[i] = 0;

    for (i = 0; i < cache->table_size; ++i) {
	chain_length = 0;
	if (cache->table[i])
	    ++stats->table_used;
	for (bucket = cache->table[i]; bucket; bucket = bucket->next)
	    ++chain_length;
	stats->buckets_allocated += chain_length;
	if (chain_length > 10)
	    chain_length = 10;
	if (chain_length > 0)
	    ++stats->chain_lengths[chain_length - 1];
    }
    for (bucket = cache->free_list; bucket; bucket = bucket->next)
	++stats->buckets_allocated;
    stats->table_memory_used += sizeof(struct icache) +
	sizeof(struct icache_bucket*) * cache->table_size;
    stats->table_memory_used += sizeof(struct icache_bucket) *
	stats->buckets_allocated;
    return 0;
}
