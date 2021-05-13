//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_db.c
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
#include "bc_db.h"
#include "bc_alloc.h"
#include "bc_net.h"
#include "bc_http.h"
#include "bc_proto.h"
#include "bc_string.h"
#include "bc_util.h"
#include "bccsdk.h"
#include "md5.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <syslog.h>
#include <byteswap.h>


#define MAX_LINE 2048
#define REPUTATION_CATEGORY 84
static struct bc_connection Connection;

static int Major = 0;
static int Minor = 0;
static char DbPath[2048] = { 0 };
static char DbFilename[256] = { 0 };

int bc_db_init(const struct bc_init_params* params) {
    bc_cpystrn(DbPath, params->dbpath, 2048);

    memset(&Connection, 0, sizeof(struct bc_connection));
    Connection.fd = -1;
    return 0;
}

void bc_db_shutdown(void) {
    if (Connection.fd != -1)
	bc_net_close(&Connection);
}

int bc_db_query_active(void) {
    return Connection.busy;
}

static void clear_connection(int clear_data) {
    if (clear_data)
	Connection.data = 0;
    Connection.reader = 0;
    Connection.error_handler = 0;
    Connection.buf[0] = '\0';
    Connection.buf_len = 0;
    Connection.busy = 0;
    Connection.flags = 0;
}

static void db_error_handler(struct bc_connection* conn, int state) {
    /* If we're here the connection failed */
    struct bc_db_request* req = (struct bc_db_request*) conn->data;
    if (req->callback)
	req->callback(req, -1);
}

static int stream_to_file(struct bc_connection* conn, size_t content_length) {
    const size_t CHUNK_SIZE = BCA_NET_BUFFER_SIZE;
    char path[PATH_MAX + 1];
    char* e;
    size_t read_size = CHUNK_SIZE;
    size_t remaining = content_length;
    ssize_t r;
    FILE* f;
    struct bc_db_request* req = (struct bc_db_request*)conn->data;
    if (!req) {
	assert(req);
	return -1;
    }
    /* create the path */
    e = bc_cpystrn(path, DbPath, PATH_MAX);
    if (e != path && *(e - 1) != '/')
	*e++ = '/';
    bc_cpystrn(e, req->filename, PATH_MAX - (e - path));
    f = fopen(path, "w");
    if (!f) {
	syslog(LOG_ERR, "Cannot open file %s for writing: %s", path, strerror(errno));
	return -1;
    }
    while (remaining > 0) {
	if (read_size > remaining)
	    read_size = remaining;
	r = bc_net_readn(conn, conn->buf, read_size);
	if (r <= 0)
	    break;
	fwrite(conn->buf, sizeof(char), r, f);
	remaining -= r;
    }
    fclose(f);
    bc_net_close(conn);
    if (remaining == 0) {
	bc_cpystrn(DbFilename, req->filename, 256);
	return md5_verify_checksum(path, req->checksum);
    }
    return -1;
}

/* NOTE: Download code bypasses network/protocol stack */
static int db_download_reader(struct bc_connection* conn) {
    struct bc_http_header hdr = { 0 };
    struct bc_db_request* req = (struct bc_db_request*) conn->data;
    if (bc_http_read_header(conn, &hdr) > 0 &&
	hdr.result_code == 200 &&
	hdr.content_length > 0 &&
	stream_to_file(conn, hdr.content_length) == 0) {
	bc_net_close(conn);
	clear_connection(1);
	Major = req->update_major;
	Minor = req->update_minor;
	req->update_available = 1;
	if (req->callback)
	    req->callback(req, 0);
    } else {
	bc_net_close(conn);
	clear_connection(1);
	if (req->callback) {
	    req->callback(req, -1);
	    return 0;
	}
	return -1;
    }
    return 0;
}

static int download_database_file(struct bc_connection* conn) {
    const char* RequestFormat =
	"GET /%s?uid=%s&deviceid=%s&oemid=%s HTTP/1.1\r\n"
	"Host: %s\r\nConnection: Close\r\n\r\n";
    struct bc_db_request* req = (struct bc_db_request*) conn->data;

    if (!req) {
	assert(req);
	return -1;
    }
    clear_connection(0);
    conn->busy = 1;
    snprintf(conn->buf, BCA_NET_BUFFER_SIZE, RequestFormat,
	     req->filename, bc_proto_get_uid(), bc_proto_get_deviceid(),
	     bc_proto_get_oemid(), bc_net_get_dbhost());
    conn->buf_len = strlen(conn->buf);
    conn->error_handler = db_error_handler;
    conn->reader = db_download_reader;
    conn->flags = BCA_NET_SSL | BCA_NET_DATABASE;
    if (bc_net_write(conn) == -1) {
	bc_net_close(conn);
	clear_connection(1);
	if (req->callback) {
	    req->callback(req, -1);
	    return 0;
	}
	return -1;
    }
    return 0;
}

static int get_db_version_read(struct bc_connection* conn)
{
    struct bc_db_request* req = (struct bc_db_request*) conn->data;
    if (bc_proto_get_db_version_read(conn) == -1) {
	bc_net_close(conn);
	clear_connection(1);
	if (req->callback) {
	    req->callback(req, -1);
	    return 0;
	}
	return -1;
    }
    if (req->update_major != Major || req->update_minor != Minor) {
	return download_database_file(conn);
    } else {
	bc_net_close(conn);
	clear_connection(1);
	req->update_available = 0;
	if (req->callback)
	    req->callback(req, 0);
    }
    return 0;
}

int bc_db_update(struct bc_db_request* req) {
    if (!req) {
	assert(req);
	return -1;
    }
    if (Connection.busy) {
	// already doing an update
	return -1;
    }

    req->current_major = Major;
    req->current_minor = Minor;
    Connection.busy = 1;
    Connection.data = req;
    Connection.error_handler = db_error_handler;
    Connection.reader = get_db_version_read;
    if (bc_net_use_ssl())
	Connection.flags = BCA_NET_SSL;
    if (bc_proto_get_db_version(&Connection, req) == -1) {
	clear_connection(1);
	bc_net_close(&Connection);
	if (req->callback) {
	    req->callback(req, -1);
	    return 0;
	}
	return -1;
    }
    return 0;
}

int bc_db_open(const char* path, struct bc_db_context* ctx) {
    char path_buf[PATH_MAX + 1];
    char buf[MAX_LINE];
    char lines[32];
    char* e;
    if (!ctx) {
	assert(ctx);
	return -1;
    }
    memset(ctx, 0, sizeof(struct bc_db_context));
    if (!path) {
	if (DbPath[0] == '\0' || DbFilename[0] == '\0')
	    return -1;
	e = bc_cpystrn(path_buf, DbPath, PATH_MAX);
	if (*(e - 1) != '/')
	    *e++ = '/';
	bc_cpystrn(e, DbFilename, PATH_MAX - (e - path_buf));
	path = path_buf;
    }
    ctx->file = fopen(path, "rb");
    if (!ctx->file) {
	syslog(LOG_ERR, "Unable to open file %s: %s", path, strerror(errno));
	return -1;
    }
    if (!fgets(buf, MAX_LINE, ctx->file)) {
	syslog(LOG_ERR, "Error reading file %s: %s", path, strerror(errno));
	fclose(ctx->file);
	ctx->file = 0;
	return -1;
    }
    if (sscanf(buf, "%s%s%s%s", ctx->type, ctx->version, ctx->date, lines) != 4) {
	syslog(LOG_ERR, "Database file version format not foundin file %s", path);
	fclose(ctx->file);
	ctx->file = 0;
	return -1;
    }
    ctx->line_count = atoi(lines);
    /* Skip the category list */
    while (fgets(buf, MAX_LINE, ctx->file) && strncmp(buf, "999", 3) != 0) {
	/* category description is category id followed by a name */
	/* sscanf(buf, "%d%s", &id, name); */
    }
    if (feof(ctx->file)) {
	/* truncated file? */
	syslog(LOG_ERR, "EOF read before data found in database file %s", path);
	fclose(ctx->file);
	ctx->file = 0;
	return -1;
    }
    /*
     * If we're here the file is open and the file pointer is at the beginning of
     * the database data.
     */
    return 0;
}


int bc_db_next(struct bc_db_context* ctx, struct bc_db_record* rec)
{
    unsigned short cc;
    int idx = 0;
    int count = 0;
    if (!ctx || !ctx->file || !rec) {
	assert(ctx);
	assert(rec);
	return 0;
    }
    memset(rec, 0, sizeof(struct bc_db_record));
    if (feof(ctx->file))
	return 0;

    if (!fread(rec->md5, 16, 1, ctx->file)) {
	fclose(ctx->file);
	ctx->file = 0;
	return 0;
    }
#ifdef BYTE_SWAP_DB
    rec->md5[0] = bswap_64(rec->md5[0]);
    rec->md5[1] = bswap_64(rec->md5[1]);
#endif
    do {
	if (!fread(&cc, sizeof(unsigned short), 1, ctx->file)) {
	    fclose(ctx->file);
	    ctx->file = 0;
	    return 0;
	}
#ifdef BYTE_SWAP_DB
	cc = bswap_16(cc);
#endif
	if (((cc & 0x7FFF) >> 8) == REPUTATION_CATEGORY) {
	    rec->data.reputation = cc & 0xFF;
	} else if (idx < BCA_MAX_CATCONF) {
	    // The first record is always a1cat with the rep 1m db
	    if (count == 0) {
		//printf("a1cat = %x ", cc);
		if ((cc & 0x7FFF) != 0)
		    rec->data.flags |= BCA_FLAG_ONE_CAT;
	    } else {
		rec->data.cc[idx].category = (cc >> 8) & 0x7F;
		rec->data.cc[idx].confidence = cc & 0xFF;
		++idx;
	    }
	}
	++count;
    } while ((cc & 0x8000) != 0);
    //bc_print_md5_data(rec->md5, &rec->data);
    return 1;
}

void bc_db_close(struct bc_db_context* ctx) {
    if (ctx && ctx->file) {
	fclose(ctx->file);
	ctx->file = 0;
    }
}
