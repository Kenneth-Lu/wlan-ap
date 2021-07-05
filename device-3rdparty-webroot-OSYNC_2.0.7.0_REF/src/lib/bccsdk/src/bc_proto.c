//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_proto.c
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
#include "bc_proto.h"
#include "bccsdk.h"
#include "bc_http.h"
#include "bc_net.h"
#include "bc_string.h"
#include "bc_cache.h"
#include "bc_queue.h"
#include "bc_stats.h"
#include "bc_rtu.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <assert.h>


struct bcap_status {
    int         code;
    const char* msg;
};

static struct bcap_status StatusCodes[] = {
    {200, "OK"},
    {401, "Unauthorized"},
    {402, "Unsupported protocol"},
    {403, "Malformed URL"},
    {404, "Uncategorized information"},
    {402, "Unsupported protocol"},
    {403, "Malformed URL"},
    {405, "Malformed XML"},
    {497, "License in use"},
    {499, "License exceeded"},
    {503, "Server unavailable"},
    {590, "Server busy"},
    {591, "Unsupported BCAP version"},
    {601, "Invalid OEM"},
    {602, "Invalid product"},
    {603, "License expired"},
    {606, "No license"},
    {701, "No update available"},
    {703, "Invalid request"},
    {704, "Invalid major or minor version"},
    {705, "Invalid RTU version"},
    {706, "No new RTU available"},
};

static const size_t StatusCodesSize = sizeof(StatusCodes) / sizeof(struct bcap_status);

struct xml_entity {
    const char* str;
    size_t      len;
    char        ch;
};

static struct xml_entity Entity[] = {
    { "&amp;",  5, '&' },
    { "&lt;",   4, '<' },
    { "&gt;",   4, '>' },
    { "&quot;", 6, '\"' },
    { "&apos;", 6, '\'' }
};

static const size_t EntitySize = sizeof(Entity) / sizeof(struct xml_entity);

static const char* BcapEnvelope =
    "<?BrightCloud version=bcap/1.1?>\
<bcap>\
<seqnum>%d</seqnum>\
<encrypt-type>%s</encrypt-type>\
%s\
</bcap>\
\r\n";

static const char* GetUriInfoEx =
    "<request>\
<method>geturiinfoex2</method>\
<uid>%s</uid>\
<productid>%s</productid>\
<oemid>%s</oemid>\
%s\
</request>";

static const char* GetMd5UpdateRep =
    "<request>\
<method>getmd5update1mrep</method>\
<uid>%s</uid>\
<productid>%s</productid>\
<oemid>%s</oemid>\
<md5currentmajor>%d</md5currentmajor>\
<md5currentminor>%d</md5currentminor>\
</request>";

static const char* GetRtu =
    "<request>\
<method>getrtu</method>\
<uid>%s</uid>\
<productid>%s</productid>\
<oemid>%s</oemid>\
<md5currentmajor>%d</md5currentmajor>\
<md5currentminor>%d</md5currentminor>\
<rtuversion>%d</rtuversion>\
</request>";

static const char* PostDeviceStats =
    "<request>\
<method>postdevicestats</method>\
<uid>%s</uid>\
<productid>%s</productid>\
<oemid>%s</oemid>\
<cache>%u</cache>\
<cloud>%u</cloud>\
<clu>%u</clu>\
<db>%u</db>\
<uncat>%u</uncat>\
<cs>%u</cs>\
<qs>%u</qs>\
<trust>%u</trust>\
<guest>%u</guest>\
<net>%u</net>\
<http>%u</http>\
<bcap>%u</bcap>\
<major>%u</major>\
<minor>%u</minor>\
<flags>%u</flags>\
<start>%u</start>\
</request>";

struct xml_tag {
    const char*  start;
    const char*  end;
    unsigned int length;
};

static const struct xml_tag Tags[] = {
    { "<status>", "</status>", 8 },
    { "<catid>", "</catid>", 7 },
    { "<conf>", "</conf>", 6 },
    { "<bcri>", "</bcri>", 6 },
    { "<a1cat>", "</a1cat>", 7 },
    { "<filename>", "</filename>", 10 },
    { "<checksum>", "</checksum>", 10 },
    { "<updateMajorVersion>", "</updateMajorVersion>", 20 },
    { "<updateMinorVersion>", "</updateMinorVersion>", 20 },
    { "<ttlBias>", "</ttlBias>", 9 },
    { "<ttl>", "</ttl>", 5 },
    { "<trustBias>", "</trustBias>", 11 },
    { "<guestBias>", "</guestBias>", 11 },
    { "<flags>", "</flags>", 7 },
    { "<uri>", "</uri>", 5 },
    { "<lcp>", "</lcp>", 5 },
    { "<item>", "</item>", 6 },
    { "<major>", "</major>", 7},
    { "<minor>", "</minor>", 7},
    { "<updatertuversion>", "</updatertuversion>", 18 },
    { "<contentlocation>", "</contentlocation>", 17 },
    { "<contentchecksum>", "</contentchecksum>", 17 },
    { "<replocation>", "</replocation>", 13 },
    { "<repchecksum>", "</repchecksum>", 13 }
};

#define STATUS_TAG       0
#define CATID_TAG        1
#define CONF_TAG         2
#define BCRI_TAG         3
#define A1CAT_TAG        4
#define FILENAME_TAG     5
#define CHECKSUM_TAG     6
#define UMAJOR_TAG       7
#define UMINOR_TAG       8
#define TTLBIAS_TAG      9
#define TTL_TAG         10
#define TRUST_TAG       11
#define GUEST_TAG       12
#define FLAGS_TAG       13
#define URI_TAG         14
#define LCP_TAG         15
#define ITEM_TAG        16
#define MAJOR_TAG       17
#define MINOR_TAG       18
#define RTUVER_TAG      19
#define CONTENTLOC_TAG  20
#define CONTENTSUM_TAG  21
#define REPLOC_TAG      22
#define REPSUM_TAG      23

static char OemId[64] = { 0 };
static char Device[64] = { 0 };
static char UID[64] = { 0 };
static int QueuePollTime = 10;

/* stats */
static unsigned int RequestCount = 0;
static unsigned int LookupCount = 0;
static unsigned int ErrorCount = 0;
static unsigned int UncatCount = 0;

/* run */
#define MAX_BATCH 16
static char URIBuf[MAX_BATCH * BCA_URL_LENGTH];
static char RequestBuf[BCA_NET_BUFFER_SIZE];
static char RequestEnvelope[BCA_NET_BUFFER_SIZE];
static unsigned long QueueTimerId = 0;


int bc_proto_init(const struct bc_init_params* params) {
    bc_cpystrn(OemId, params->oem, 64);
    bc_cpystrn(Device, params->device, 64);
    bc_cpystrn(UID, params->uid, 64);
    QueuePollTime = params->queue_poll;
    bc_queue_init(params);
    return 0;
}

void bc_proto_shutdown(void) {
    bc_queue_shutdown();
}

const char* bc_proto_get_uid(void) {
    return UID;
}

const char* bc_proto_get_deviceid(void) {
    return Device;
}

const char* bc_proto_get_oemid(void) {
    return OemId;
}

void bc_proto_get_stats(struct bc_stats* stats) {
    if (!stats) {
	assert(stats);
	return;
    }
    stats->cloud_hits = RequestCount;
    stats->cloud_lookups = LookupCount;
    stats->bcap_errors = ErrorCount;
    stats->uncat_responses = UncatCount;
    bc_http_get_stats(stats);
}

void bc_proto_reset_stats(void) {
    RequestCount = LookupCount = ErrorCount = UncatCount = 0;
    bc_http_reset_stats();
}

unsigned int bc_proto_get_lookups(void) {
    return LookupCount;
}

unsigned int bc_proto_get_errors(void) {
    return ErrorCount;
}

static const char* bcap_error(int status) {
    size_t i;
    for (i = 0; i < StatusCodesSize; ++i) {
	if (StatusCodes[i].code == status)
	    return StatusCodes[i].msg;
    }
    return "Unknown bcap error";
}

static int parse_int(const char* buf, int tag, const char** endp) {
    const char* st;
    const char* et;
    unsigned int len = Tags[tag].length;

    if ((st = strstr(buf, Tags[tag].start)) &&
	(et = strstr(st + len, Tags[tag].end))) {
	*endp = et + len + 1;
	return atoi(st + len);
    }
    return 0;
}

static const char* parse_string(const char* buf, int tag, const char** endp,
				char* dest, size_t size) {
    const char* st;
    const char* et;
    unsigned int len = Tags[tag].length;
    size_t count;

    if ((st = strstr(buf, Tags[tag].start)) &&
	(et = strstr(st + len, Tags[tag].end))) {
	*endp = et + len + 1;
	count = (et - (st + len));
	if (count < size) {
	    bc_strncpy(dest, st + len, count);
	    dest[count] = '\0';
	    return dest;
	}
    }
    return 0;
}

static int parse_status(const char* msg) {
    const char* e = 0;
    return parse_int(msg, STATUS_TAG, &e);
}

/* special handling for cat conf entries */
static int parse_catconf(const char* buf,
			 struct bc_url_data* data, const char** endp) {
    int conf;
    int cat;
    int cat_count = 0;
    const char* cat_start;
    const char* conf_start;
    const char* conf_end;
    const char* cat_end = buf;
    *endp = buf;

    while (cat_count < BCA_MAX_CATCONF) {
	/* find the catid tags */
	if ((cat_start = strstr(cat_end, "<catid>")) == 0)
	    break;
	if ((cat_end = strstr(cat_start + 7, "</catid>")) == 0)
	    break;

	cat = atoi(cat_start + 7);
	cat_end += 8;
	if ((conf_start = strstr(cat_end, "<conf>")) &&
	    (conf_end = strstr(conf_start + 6, "</conf>"))) {
	    conf = atoi(conf_start + 6);
	    cat_end = conf_end + 7;
	} else {
	    conf = 0;
	}
	data->cc[cat_count].category = cat;
	data->cc[cat_count].confidence = conf;
	++cat_count;
	*endp = cat_end;
    }
    return cat_count;
}

static struct bc_url_request* xml_unescape_match(const char* url,
						 struct bc_url_request* req) {
    char buf[1024];
    const char* s;
    unsigned int i;
    struct xml_entity* e;
    char* d = buf;
    struct bc_url_request* p;
    for (s = url; *s; ++s) {
	if (*s == '&') {
	    e = 0;
	    for (i = 0; i < EntitySize; ++i) {
		if (strncmp(Entity[i].str, s, Entity[i].len) == 0) {
		    e = &Entity[i];
		    break;
		}
	    }
	    if (e) {
		*d++ = e->ch;
		s += e->len - 1;
	    } else
		*d++ = *s;
	} else
	    *d++ = *s;
    }
    *d = '\0';
    //fprintf(stderr, "Unescaped: %s\n", buf);
    for (p = req; p; p = p->next) {
	if (p->mark != 0)
	    continue;
	if (strcmp(p->url, buf) == 0)
	    return p;
    }
    return 0;
}

static void parse_response(const char* buf, struct bc_url_request* req) {
    int a1cat = 0;
    const char* endp;
    char uri[BCA_URL_LENGTH];
    char lcp[BCA_URL_LENGTH];
    char item[4096];
    const char* it = buf;
    int count;
    struct bc_url_request* p;

    for (p = req; p; p = p->next) {
	p->mark = 0;
    }
    while (parse_string(it, ITEM_TAG, &it, item, 4096)) {
	a1cat = 0;
	endp = item;
	if (!parse_string(endp, URI_TAG, &endp, uri, BCA_URL_LENGTH))
	    break;
	lcp[0] = '\0';
	parse_string(endp, LCP_TAG, &endp, lcp, BCA_URL_LENGTH);

	for (p = req; p; p = p->next) {
	    if (p->mark != 0)
		continue;
	    if (strcmp(p->url, uri) == 0)
		break;
	}
	if (!p) {
	    p = xml_unescape_match(uri, req);
	    if (!p) {
		syslog(LOG_WARNING, "Cannot find request for %s", uri);
		continue;
	    }
	}
	count = parse_catconf(endp, &p->data, &endp);
	p->data.reputation = parse_int(endp, BCRI_TAG, &endp);
	if (count == 1 && p->data.cc[0].category == 0) {
	    /* uncategorized case */
	    ++UncatCount;
	    a1cat = 1;
	} else {
	    a1cat = parse_int(endp, A1CAT_TAG, &endp);
	}
	if (a1cat) {
	    p->data.flags |= BCA_FLAG_ONE_CAT;
	}
	if (lcp[0] != '\0')
	    bc_cpystrn(p->lcp, lcp, BCA_URL_LENGTH);
	bc_cache_add(p);
	p->mark = 1;
    }
}

static void process_queue(void* data, int timer_id)
{
    /* see if anything is in the queue */
    struct bc_url_request* list = 0;
    struct bc_url_request* req;
    int count = 0;
    if (!bc_queue_empty()) {
	if (bc_net_get_connection() != 0) {
	    /* we have a connection available */
	    while (count++ < MAX_BATCH && (req = bc_queue_pop())) {
		req->next = list;
		list = req;
	    }
	    bc_proto_getinfo(list, BCA_FROM_QUEUE);
	}
	if (bc_queue_empty()) {
	    bc_net_remove_timer(QueueTimerId);
	    QueueTimerId = 0;
	}
    } else {
	bc_net_remove_timer(QueueTimerId);
	QueueTimerId = 0;
    }
}

static void clean_connection_queue(struct bc_connection* conn, int start_next)
{
    conn->data = 0;
    conn->busy = 0;
    conn->reader = 0;
    conn->error_handler = 0;
    conn->buf_len = 0;
    conn->buf[0] = '\0';
    conn->retry = 0;
    conn->loop = 0;
    if (!bc_queue_empty()) {
	if (!QueueTimerId) {
	    QueueTimerId = bc_net_add_timer(QueuePollTime, process_queue, 1);
	}
    }
}

static void error_handler(struct bc_connection* conn, int state)
{
    /* If we're here the connection failed */
    struct bc_url_request* req = (struct bc_url_request*) conn->data;
    struct bc_url_request* next;
    while (req) {
        req->connect_error = conn->connect_error;
	next = req->next;
	if (req->callback)
	    req->callback(req, -1);
	req = next;
    }
    clean_connection_queue(conn, 1);
}

static int getinfo_error(struct bc_url_request* req)
{
    int ret = -1;
    struct bc_url_request* next;
    while (req) {
	next = req->next;
	if (req->callback) {
	    ret = 0;
	    req->callback(req, -1);
	}
	req = next;
    }
    return ret;
}

static int getinfo_read(struct bc_connection* conn)
{
    int r;
    int status;
    struct bc_http_header hdr;
    struct bc_url_request* next;
    struct bc_url_request* req = (struct bc_url_request*) conn->data;
    char* p;
    r = bc_http_read(conn, &hdr);
    if (r > 0) {
	p = conn->buf;
	//printf("BCAP Response:\n%s\n", p);
	status = parse_status(p);
	//printf("BCAP Status: %d\n", status);
	if (200 == status) {
	    if (req) {
		parse_response(p, req);
		while (req) {
		    next = req->next;
		    //bc_cache_add(req);
		    bc_stats_update_rep_score(req->data.reputation, req->trusted);
		    if (req->callback)
			req->callback(req, req->mark ? 0 : -1);
		    req = next;
		}
		r = 0;
	    }
	} else {
	    ++ErrorCount;
	    r = getinfo_error(req);
	    syslog(LOG_ERR, "BCAP server error (%d): %s", status, bcap_error(status));
            if (req) {
                req->error = r;
                req->http_status = status;
            }
	}
	conn->buf[0] = '\0';
        if (conn->loop == 0)
            clean_connection_queue(conn, 1);
        return 0;
    }
    else if (r == -2 && bc_net_get_poller() != 0) {
	// special case, retrying send
	return 0;
    } else if (r == -1) {
	/* should be counted as http or net error */
	r = getinfo_error(req);
        if (req) {
            req->error = r;
        }
    }
    /* NOTE: Once the callback has been called we cannot return -1 from here */
    clean_connection_queue(conn, 1);
    return r;
}

static char* xml_escape(char* dst, const char* src, size_t len)
{
    size_t i;
    char* d = dst;
    char* e = d + len - 1;
    const char* s = src;
    const struct xml_entity* entity;
    while (*s && d < e) {
	entity = 0;
	for (i = 0; i < EntitySize; ++i) {
	    if (*s == Entity[i].ch) {
		entity = &Entity[i];
		break;
	    }
	} if (entity) {
	    bc_strncpy(d, entity->str, e - d);
	    d += entity->len;
	    ++s;
	} else {
	    *d++ = *s++;
	}
    }
    *d = '\0';
    return d;
}

static void fill_uri_tags(struct bc_url_request* req)
{
    char* s = URIBuf;
    char* e = URIBuf + MAX_BATCH * BCA_URL_LENGTH;
    while (req) {
	s = bc_cpystrn(s, "<uri>", e - s);
	s = xml_escape(s, req->url, e - s);
	s = bc_cpystrn(s, "</uri>", e - s);
	req = req->next;
    }
}

int bc_proto_getinfo(struct bc_url_request* req, unsigned int flags)
{
    const char* p;
    struct bc_url_request* list = 0;
    struct bc_url_request* next = 0;
    struct bc_connection* conn = bc_net_get_connection();
    assert(req);
    if (!conn) {
	/* everyone is busy, queue the request */
	assert((flags & BCA_FROM_QUEUE) == 0);
	if (bc_queue_push(req) == -1)
	    return -1; /* queue limit reached */
	if (!QueueTimerId) {
	    QueueTimerId = bc_net_add_timer(QueuePollTime, process_queue, 1);
	}
	return 0;
    }
    if ((flags & BCA_FROM_QUEUE) == 0 && !bc_queue_empty()) {
	/* busy, queue it */
	if (bc_queue_push(req) == -1)
	    return -1; /* queue limit reached */
	if (!QueueTimerId) {
	    QueueTimerId = bc_net_add_timer(QueuePollTime, process_queue, 1);
	}
	return 0;
    }
    if ((flags & BCA_FROM_QUEUE) != 0) {
	while (req) {
	    next = req->next;
	    /* request was queued, see if it got put in the cache in the meantime */
	    if (bc_cache_lookup(req) > 0) {
		if (req->callback)
		    req->callback(req, 0);
	    } else {
		req->next = list;
		list = req;
	    }
	    req = next;
	}
	req = list;
	if (!req) {
	    if (!bc_queue_empty()) {
		/* schedule requests in the queue */
		if (!QueueTimerId) {
		    QueueTimerId = bc_net_add_timer(QueuePollTime,
						    process_queue, 1);
		}
	    }
	    return 0;
	}
    }
    /*
     * If we're here, either the request is from the queue or we're not busy
     * processing another request. Mark the connection busy in case we get more
     * requests while waiting for a response from this one.
     */
    for (list = req; list; list = list->next) {
	++RequestCount;
    }
    ++LookupCount;
    conn->busy = 1;
    conn->data = req;
    conn->reader = getinfo_read;
    conn->error_handler = error_handler;

    fill_uri_tags(req);
    snprintf(RequestBuf, BCA_NET_BUFFER_SIZE, GetUriInfoEx, UID, Device, OemId, URIBuf);
    p = RequestBuf;
    snprintf(RequestEnvelope, BCA_NET_BUFFER_SIZE, BcapEnvelope, req->serial, "none", p);
    //printf("BCAP Request:\n%s\n", RequestEnvelope);
    /* NOTE: return of -1 from here means delete the callback */
    if (bc_http_write(conn, RequestEnvelope, strlen(RequestEnvelope),
		      BCA_HTTP_KEEP_ALIVE) == -1) {
	clean_connection_queue(conn, 1);
	return -1;
    }
    return 0;
}

/*
 * DB Version Handlers
 */
static void db_error_handler(struct bc_connection* conn, int state) {
    syslog(LOG_ERR, "%s", "db default error handler called");
    /* If we're here the connection failed */
    struct bc_db_request* req = (struct bc_db_request*) conn->data;
    if (req->callback)
	req->callback(req, -1);
}

static void parse_db_version_response(const char* src, struct bc_db_request* req) {
    const char* s = src;
    parse_string(s, FILENAME_TAG, &s, req->filename, 2048);
    parse_string(s, CHECKSUM_TAG, &s, req->checksum, 64);
    req->update_major = parse_int(s, UMAJOR_TAG, &s);
    req->update_minor = parse_int(s, UMINOR_TAG, &s);
}

int bc_proto_get_db_version_read(struct bc_connection* conn) {
    int r;
    int status;
    struct bc_http_header hdr;
    struct bc_db_request* req = (struct bc_db_request*) conn->data;
    char* p;
    r = bc_http_read(conn, &hdr);
    if (r > 0) {
	p = conn->buf;
	//printf("BCAP Response:\n%s\n", p);
	status = parse_status(p);
	//printf("BCAP Status: %d\n", status);
	switch (status) {
	case 200:
	    parse_db_version_response(p, req);
	    r = 0;
	    break;
	case 701:
	    req->update_major = req->current_major;
	    req->update_minor = req->current_minor;
	    r = 0;
	    break;
	default:
	    ++ErrorCount;
	    r = -1;
	    syslog(LOG_ERR, "BCAP server error (%d): %s",
		   status, bcap_error(status));
	}
    } else {
	/* should be counted as http or net error */
	r = -1;
    }
    /* NOTE: Once the callback has been called we cannot return -1 from here */
    bc_net_close(conn);
    return r;
}

int bc_proto_get_db_version(struct bc_connection* conn, struct bc_db_request* req)
{
    static int serial = 0;
    char request_buf[4096];
    char envelope[8192];
    const char* p;
    assert(req);
    if (!conn) {
	assert(conn);
	return -1;
    }
    conn->busy = 1;
    conn->data = req;
    if (!conn->reader)
	conn->reader = bc_proto_get_db_version_read;
    if (!conn->error_handler)
	conn->error_handler = db_error_handler;

    snprintf(request_buf, 4096, GetMd5UpdateRep, UID, Device, OemId,
	     req->current_major, req->current_minor);
    p = request_buf;
    //printf("BCAP Request:\n%s\n", p);
    snprintf(envelope, 8192, BcapEnvelope, ++serial, "none", p);
    /* NOTE: return of -1 from here means delete the callback */
    if (bc_http_write(conn, envelope, strlen(envelope), 0) == -1) {
	return -1;
    }
    return 0;
}

/*
 *  RTU Update Handlers
 */
static void rtu_error_handler(struct bc_connection* conn, int state) {
    /* If we're here the connection failed */
    syslog(LOG_ERR, "%s", "RTU default error handler called");
}

static void parse_rtu_response(const char* src, struct bc_rtu_request* req) {
    const char* s = src;
    parse_string(s, CONTENTLOC_TAG, &s, req->contentLocation, 1024);
    parse_string(s, CONTENTSUM_TAG, &s, req->contentChecksum, 34);
    parse_string(s, REPLOC_TAG, &s, req->repLocation, 1024);
    parse_string(s, REPSUM_TAG, &s, req->repChecksum, 34);
    req->rtu = parse_int(s, RTUVER_TAG, &s);
    req->major = parse_int(s, UMAJOR_TAG, &s);
    req->minor = parse_int(s, UMINOR_TAG, &s);
    if (req->rtu != 0)
	req->update_available = 1;
}

int bc_proto_get_rtu_read(struct bc_connection* conn) {
    int r;
    int status;
    struct bc_http_header hdr;
    struct bc_rtu_request* req = (struct bc_rtu_request*) conn->data;
    char* p;
    r = bc_http_read(conn, &hdr);
    if (r > 0) {
	p = conn->buf;
	//printf("BCAP Response:\n%s\n", p);
	status = parse_status(p);
	//printf("BCAP Status: %d\n", status);
	switch (status) {
	case 200:
	    parse_rtu_response(p, req);
	    r = 0;
	    break;
	case 706:
	    r = 0;
	    req->update_available = 0;
	    break;
	default:
	    ++ErrorCount;
	    r = -1;
	    syslog(LOG_ERR, "BCAP server error (%d): %s",
		   status, bcap_error(status));
	}
    } else {
	/* should be counted as http or net error */
	r = -1;
    }
    /* NOTE: Once the callback has been called we cannot return -1 from here */
    bc_net_close(conn);
    return r;
}

int bc_proto_get_rtu(struct bc_connection* conn, struct bc_rtu_request* req) {
    static int serial = 0;
    char request_buf[4096];
    char envelope[8192];
    const char* p;
    assert(req);
    if (!conn) {
	assert(conn);
	return -1;
    }
    conn->busy = 1;
    conn->data = req;
    if (!conn->reader)
	conn->reader = bc_proto_get_rtu_read;
    if (!conn->error_handler)
	conn->error_handler = rtu_error_handler;

    snprintf(request_buf, 4096, GetRtu, UID, Device, OemId,
	     req->major, req->minor, req->rtu);
    p = request_buf;
    //printf("BCAP Request:\n%s\n", p);
    snprintf(envelope, 8192, BcapEnvelope, ++serial, "none", p);
    /* NOTE: return of -1 from here means delete the callback */
    if (bc_http_write(conn, envelope, strlen(envelope), 0) == -1) {
	return -1;
    }
    return 0;
}

/*
 * Lies, damn lies, and statistics
 */
static void parse_stats_response(const char* src,
				 struct bc_stats_request* req) {
    const char* s = src;
    char buf[2048];
    int i;
    int v;
    char* end = buf;
    parse_string(s, TTLBIAS_TAG, &s, buf, 2048);
    for (i = 0; i < 100; ++i) {
	v = strtol(end, &end, 10);
	req->ttl_bias[i] = v;
	if (*end == '\0')
	    break;
	else if (*end == ',')
	    ++end;
    }
    req->ttl = parse_int(s, TTL_TAG, &s);
    req->trust_bias = parse_int(s, TRUST_TAG, &s);
    req->guest_bias = parse_int(s, GUEST_TAG, &s);
    req->major = parse_int(s, MAJOR_TAG, &s);
    req->minor = parse_int(s, MINOR_TAG, &s);
    req->flags = parse_int(s, FLAGS_TAG, &s);
}

int bc_proto_stats_read(struct bc_connection* conn) {
    int r;
    int status;
    struct bc_http_header hdr;
    struct bc_stats_request* req = (struct bc_stats_request*) conn->data;
    char* p;
    r = bc_http_read(conn, &hdr);
    if (r > 0) {
	p = conn->buf;
	//printf("BCAP Response:\n%s\n", p);
	status = parse_status(p);
	//printf("BCAP Status: %d\n", status);
	switch (status) {
	case 200:
	    parse_stats_response(p, req);
	    r = 0;
	    break;
	default:
	    ++ErrorCount;
	    r = -1;
	    syslog(LOG_ERR, "BCAP server error (%d): %s",
		   status, bcap_error(status));
	}
    } else {
	/* should be counted as http or net error */
	r = -1;
    }
    /* NOTE: Once the callback has been called we cannot return -1 from here */
    bc_net_close(conn);
    return r;
}


int bc_proto_post_stats(struct bc_connection* conn,
			struct bc_stats_request* req) {
    static int serial = 0;
    char request_buf[4096];
    char envelope[8192];
    const char* p;
    assert(req);
    if (!conn) {
	assert(conn);
	return -1;
    }
    conn->busy = 1;
    conn->data = req;

    snprintf(request_buf, 3072, PostDeviceStats, UID, Device, OemId,
	     req->stats.cache_hits,
	     req->stats.cloud_hits,
	     req->stats.cloud_lookups,
	     req->stats.db_hits,
	     req->stats.uncat_responses,
	     req->stats.cache_size,
	     req->stats.queue_depth,
	     req->stats.trust_score,
	     req->stats.guest_score,
	     req->stats.net_errors,
	     req->stats.http_errors,
	     req->stats.bcap_errors,
	     req->stats.major,
	     req->stats.minor,
	     0,
	     req->stats.up_time);
    p = request_buf;
    //printf("BCAP Request:\n%s\n", p);
    snprintf(envelope, 8192, BcapEnvelope, ++serial, "none", p);
    /* NOTE: return of -1 from here means delete the callback */
    if (bc_http_write(conn, envelope, strlen(envelope), 0) == -1) {
	return -1;
    }
    return 0;
}
