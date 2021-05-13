/* -*- C -*- */
/*
 *
 *  Filename: bccsdk.h
 *
 *  Description: main header for bccsdk library
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
 *	Copyright(c) 2006 - 2014
 *	Webroot, Inc.
 *	385 Interlocken Crescent
 *	Broomfield, Colorado, USA 80021
 *
 */
#ifndef bccsdk_h
#define bccsdk_h
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! Initialization parameters */
    struct bc_init_params {
	int   max_cache_mb;        /*!< maximum cache size in Mb (10) */
	int   lcp_loop_limit;      /*!< loop limit for LCP (15) */
	int   timeout;             /*!< Read timeout (20) */
	int   keep_alive;          /*!< Connection keep alive time in seconds */
	int   ssl;                 /*!< Use SSL for bcap queries */
	int   log_stderr;          /*!< Send syslog output to stderr too */
	int   enable_stats;        /*!< Enable stats reporting */
	int   enable_rtu;          /*!< Enable rtu updates */
	int   rtu_poll;            /*!< poll time in seconds for rtu updates (300) */
	int   pool_size;           /*!< Size of connection pool, hard limit is 10 */
	int   queue_poll;          /*!< queue polling interval in ms */
	int   queue_max;           /*!< maximum queue depth,
				     fail incoming requests if exceeded */
	int   proxy;               /*!< Use proxy for connections */
	int   proxy_port;          /*!< Proxy server port */
	void* poller;              /*!< external poll event handler */
	char  device[64];          /*!< Device identifier */
	char  oem[64];             /*!< OEM Id */
	char  uid[64];             /*!< UID */
	char  server[256];         /*!< Server name (service.brightcloud.com) */
	char  dbserver[256];       /*!< Database server name
				     (database.brightcloud.com) */
	char  proxy_server[256];   /*!< Proxy server name or IP */
	char  proxy_user[64];      /*!< Proxy user name for auth */
	char  proxy_pass[64];      /*!< Proxy password for auth */
	char  dbpath[2048];        /*!< Path to database dowload directory */
	char  cert[2048];          /*!< Path to CA certificate file for SSL */
    };

/*! Sizes for arrays */
#define BCA_MAX_CATCONF 5
#define BCA_URL_LENGTH 1024

/*! Flag values */
#define BCA_FLAG_ONE_CAT 0x01
#define BCA_FLAG_CACHED  0x02

/*! Category and confidence score structure */
    struct bc_cc {
	uint8_t category;     /*! Category for URL */
	uint8_t confidence;   /*! Confidence level category is correct 0-100 */
    };

/*! Data structure for URL information */
    struct bc_url_data {
	struct bc_cc cc[BCA_MAX_CATCONF];     /*!< Category and confidence scores */
	uint8_t      reputation;              /*!< Reputation score for URL */
	uint8_t      flags;                   /*!< Record flags */
    };

/*! URL information structure */
    struct bc_url_request {
	uint8_t dev_id[6]; /* plume: dev mac address */
	uint16_t req_id;   /* plume: dns request id */
	void *context;     /* plume: request context */
	char url[BCA_URL_LENGTH];    /*!< The URL [in/out]. */
	char lcp[BCA_URL_LENGTH];    /*!< LCP [out]. */
	struct bc_url_data data;     /*!< Return data. */
	int  trusted;                /*!< Request from trusted user */
	unsigned int  serial;        /*!< Request serial for cloud lookups. */
	int mark;                    /*!< Internal use */
	struct bc_url_request* prev; /*!< Multiple request list pointer */
	struct bc_url_request* next; /*!< Multiple request list pointer */
        int error;            /* plume: lookup error */
        int http_status;      /* plume: http_status */
        int connect_error;    /* plume: connect error */
	void (*callback)(struct bc_url_request*, int);
    };

    struct bc_db_request {
	char filename[2048];            /*!< filename for new database version */
	char checksum[64];              /*!< checksum for update */
	int  current_major;             /*!< major version for current db */
	int  current_minor;             /*!< minor version for current db */
	int  update_major;              /*!< major version for update */
	int  update_minor;              /*!< minor version for update */
	int  update_available;          /*!< an update is available */
	void (*callback)(struct bc_db_request*, int); /*!< notification handler */
    };

/*! Database record data */
    struct bc_db_record
    {
	uint64_t           md5[2];  /*!< Md5 hash of URL */
	struct bc_url_data data;    /*!< Rep and category info */
    };

/*! Database context */
    struct bc_db_context
    {
	char  type[32];       /*!< database type info */
	char  version[32];    /*!< database version */
	char  date[32];       /*!< database date */
	int   line_count;     /*!< line count */
	FILE* file;           /*!< file pointer */
    };


/*! Initialize the bca subsystem */
    int bc_initialize(const struct bc_init_params* params);

/*! Shutdown and cleanup the bca subsystem */
    void bc_shutdown(void);

/*! Initialize request data structure */
/*!
  This function initializes the request data structure and copies a sanitized
  version of the url into the URL field. This must be called first for any url
  request.
*/
    int bc_init_request(struct bc_url_request* req, const char* url,
			int trusted, int serial);

/*! Cache lookup for a URL. */
/*!
  Looks up a URL in the cache, returns 1 on success, 0 if not found, < 0 on error.
  If the cache lookup fails with an error, the URL cannot be looked up using the
  cloud lookup function. The cache lookup will generally fail with an error for
  URLs with unrecognized TLD's.
  \param request_in_out The URL and return data.
*/
    int bc_cache_lookup(struct bc_url_request* in_out);

/*! Cloud lookup for a URL. */
/*!
  Looks up a URL in the cloud. Returns 0 on success, -1 on error.
  If a callback is provided,
  the callback will be called exactly once in all cases.
  \param request_in The URL to look up and return data,
  pointer is returned in callback. Memory is owned by the app.
  \param callback Callback function with return data and request status code.
*/
    int bc_cloud_lookup(struct bc_url_request* request_in,
			void (*callback)(struct bc_url_request* request_out,
					 int rc));

/*! Subscribe to rtu updates */
/*!
  If a callback is specified, the callback will be called
  whenever there is an rtu update.
  If the callback is NULL, the rtu subscription is cleared.
  Only one subscription is available, data cannot be modified by the callback.
*/
    void bc_rtu_subscribe(void (*callback)(const uint64_t key[2],
					   const struct bc_url_data* data));

/*! Download database */
/*!
  Download the database.
*/
    int bc_update_database(struct bc_db_request* req,
			   void (*callback)(struct bc_db_request* out,
					    int rc));

/*! Open a database file */
/*!
  If path is NULL, the last database downloaded will be opened.
  Returns 0 on success, -1 if a database could not be opened.
*/
    int bc_open_database(const char* path, struct bc_db_context* context);
/*! Retrieve a record from the database */
/*!
  Returns 1 if a record is read, 0 if there is an error or no more records to read.
*/
    int bc_next_record(struct bc_db_context* ctx, struct bc_db_record* row);
/*! Close a database file */
    void bc_close_database(struct bc_db_context* context);

    void bc_params_update(struct bc_init_params *params);
#ifdef __cplusplus
}
#endif

#endif // bccsdk_h
