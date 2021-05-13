/* -*- C -*- */
/*
 *
 *  Filename: icache.h
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
 *	license before altering or copying the source code.  If you
 *	are not willing to be bound by those terms, you may not view or
 *	use this source code.
 *
 *	   	  Export Restrictions
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
 *	Copyright(c) 2006 - 2014
 *	Webroot, Inc.
 *	385 Interlocken Crescent
 *	Broomfield, Colorado, USA 80021
 *
 */
#ifndef wra_icache_h
#define wra_icache_h
#include <stdint.h>


#ifdef __cplusplus
extern "C"
{
#endif
struct bc_url_data;
/**
 * Statistics structure used to get information about a cache.
 */
struct icache_stats
{
	unsigned int table_size;
	unsigned int record_count;
	unsigned int max_records;
	unsigned int record_ttl;
	unsigned int table_used;
	unsigned int buckets_allocated;
	unsigned int table_memory_used;
	unsigned int chain_lengths[10];
};

struct icache;

/**
 * Calculate record count based on data record size and the maximum memory to use for the cache.
 * @param max_mem The maximum amount of memory to use for the cache.
 * @param data_record_size The size of the data record that will be stored in the cache.
 * @return The number of records that can be stored in the cache.
 */
unsigned int icache_calc_record_count(unsigned int max_mem);

/**
 * Create a new cache.
 * Allocates and initializes a new cache object.
 * If the new cache cannot be created a null pointer is returned and errno is set to ENOMEM.
 * @param record_count The maximum number of records that can be stored in the cache.
 * @return A pointer to the newly created cache.
 */
struct icache* icache_create(unsigned int record_count);

/**
 * Destroy and deallocate a cache.
 * This will also call the data destructor function to destroy all of the contained objects.
 * @see icache_set_data_destructor()
 * @param cache The cache to destroy.
 */
void icache_destroy(struct icache* cache);

/**
 * Scan the cache and remove any entries that have expired.
 * @see icache_set_data_destructor()
 * @param cache The cache to purge.
 * @return The number of entries removed, -1 if cache is invalid.
 */
int icache_purge(struct icache* cache);

/**
 * Set the global TTL (Time To Live) for records in the cache.
 * By default, cache records expire in 1 day (86400) seconds.
 * @param cache The cache to update.
 * @param seconds The number of seconds a cache data record is valid.
 * @return Returns 0 on success, -1 if the cache is invalid.
 */
int icache_set_ttl(struct icache* cache, unsigned int seconds);

/**
 * Add a record to the cache using a 128 bit key.
 * Adds a record to the cache using a 128 bit key. If the cache is full, icache_purge is
 * called in attempt to find an empty slot. If an empty slot cannot be allocated, the
 * record is not added, errno is set to ENOMEM and -1 is returned.
 * @see icache_purge()
 * @see icache_set_data_destructor()
 * @param cache The cache to add the record to.
 * @param key The 128 bit md5 hash value.
 * @param data The data record to store.
 * @param ttl The time to live for this record, if the value is 0, the default is used.
 * @return Returns 0 on success, -1 if the cache is invalid or the record cannot be added.
 */
int icache_add(struct icache* cache, uint64_t key[2],
	const struct bc_url_data* data, unsigned int ttl);

/**
 * Add a record to the cache using an md5 hash string.
 * @see icache_add()
 * @param cache The cache to add the record to.
 * @param md5 The md5 string to be converted to the key.
 * @param data The data record to store.
 * @param ttl The time to live for this record.
 * @return Returns 0 on success, -1 if the cache is invalid or the record cannot be added.
 */
int icache_add_str(struct icache* cache, const char* md5,
	const struct bc_url_data* data, unsigned int ttl);

/**
 * Remove a record from the cache.
 * Removes the data record associated with the key and calls the data destruction function.
 * @see icache_set_data_destructor()
 * @param cache The cache to remove the record from.
 * @param key The key for the data record.
 * @returns 0 on success, -1 if the cache is invalid.
 */
int icache_remove(struct icache* cache, uint64_t key[2]);

/**
 * Remove a record from the cache.
 * Removes the data record associated with the key and calls the data destructor function.
 * @see icache_remove()
 * @param cache The cache to remove the record from.
 * @param md5 The md5 string to be converted to the record key.
 * @returns 0 on success, -1 if the cache is invalid.
 */
int icache_remove_str(struct icache* cache, const char* md5);

/**
 * Lookup a record in the cache.
 * If the record exists in the cache and has not expired, the data record associated
 * with the specified key is returned. If the record exists and has expired, the record
 * is removed and the data destructor function is called.
 * @see icache_set_data_destructor()
 * @param cache The cache to search.
 * @param key The 128 bit key for the data record.
 * @return The pointer to the data record in the cache, NULL if not found, expired, or error.
 */
struct bc_url_data* icache_lookup(struct icache* cache, uint64_t key[2]);

/**
 * Lookup a record in the cache.
 * String version of icache_lookup.
 * @see icache_lookup()
 * @param cache The cache to search.
 * @param md5 The md5 string to convert to the record key.
 * @return The pointer to the data record in the cache, NULL if not found, expired, or error.
 */
struct bc_url_data* icache_lookup_str(struct icache* cache, const char* md5);

/**
 * Analyze the cache.
 * Traverses the cache and fills in an icach_stats struct with information about the
 * cache object. This information is useful in tuning the cache.
 * @param cache The cache to analyze.
 * @param stats The stats record to fill in.
 * @return Returns 0 on success, -1 if the cache is invalid.
 */
int icache_analyze(const struct icache* cache, struct icache_stats* stats);

#ifdef __cplusplus
}
#endif

#endif /* wra_icache_h */


