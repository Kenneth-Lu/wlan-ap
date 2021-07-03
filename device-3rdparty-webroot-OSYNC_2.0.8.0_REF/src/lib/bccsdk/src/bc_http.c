//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_http.c
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
#include "bc_http.h"
#include "bc_string.h"
#include "bccsdk.h"
#include "bc_net.h"
#include "bc_stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

/* runtime */
static const char* HttpEnvelope =
    "POST / HTTP/1.1\r\n\
Content-Type: text/html\r\n\
Host: %s\r\n\
Content-Length: %d\r\n\
Connection: keep-alive\r\n\
\r\n\
%s";

static const char* HttpEnvelopeClose =
    "POST / HTTP/1.1\r\n\
Content-Type: text/html\r\n\
Host: %s\r\n\
Content-Length: %d\r\n\
Connection: close\r\n\
\r\n\
%s";


#define HEADER_BUF_SIZE 8192
static char HeaderBuf[HEADER_BUF_SIZE];

/* stats */
static unsigned int ErrorCount = 0;

void bc_http_get_stats(struct bc_stats* stats) {
    if (!stats) {
	assert(stats);
	return;
    }
    stats->http_errors = ErrorCount;
    bc_net_get_stats(stats);
}

void bc_http_reset_stats(void) {
    ErrorCount = 0;
    bc_net_reset_stats();
}

int bc_http_write(struct bc_connection* conn, const char* buf,
		  size_t len, unsigned int flags) {
    if (!conn || !buf) {
	assert(conn);
	assert(buf);
	return -1;
    }
    if (len == 0)
	return -1;
    const char* fmt = ((flags & BCA_HTTP_KEEP_ALIVE) != 0) ? HttpEnvelope : HttpEnvelopeClose;
    // const char* fmt = HttpEnvelopeClose;
    snprintf(conn->buf, BCA_NET_BUFFER_SIZE - 1, fmt, bc_net_get_host(), len, buf);
    conn->buf[BCA_NET_BUFFER_SIZE - 1] = '\0';
    conn->buf_len = strlen(conn->buf);
    //printf("HTTP Request:\n%s\n", conn->buf);
    return bc_net_write(conn);
}

static void parse_header(char* http, struct bc_http_header* hdr) {
    char* body;
    const char* p;
    body = strstr(http, "\r\n\r\n");
    if (body) {
	/*
	 * terminate the header so we don't scan the entire message if something is
	 * missing below
	 */
	*body = '\0';
    }
    if (strncmp(http, "HTTP", 4) == 0) {
	/* HTTP/1.1 200 OK */
	p = http + 4;
	while (*p && !isspace(*p))
	    ++p;
	while (*p && isspace(*p))
	    ++p;
	/* expect the result code here */
	if (isdigit(*p))
	    hdr->result_code = atoi(p);
    }
    /* NV pairs can be anywhere */
    p = strstr(http, "Content-Length: ");
    if (p)
	p += 16;
    hdr->content_length = p ? atoi(p) : 0;
    p = strstr(http, "Connection: keep-alive");
    hdr->keep_alive = p ? 1 : 0;
}

static int is_empty(const char* s, const char* e) {
    while (s < e && *s) {
	if (!isspace(*s))
	    return 0;
	++s;
    }
    return 1;
}

int bc_http_read_header(struct bc_connection* conn, struct bc_http_header* hdr) {
    char* s = HeaderBuf;
    char* line = s;
    int count = 0;
    int n;
    int size = HEADER_BUF_SIZE - 1;
    while ((n = bc_net_readline(conn, line, &s, HEADER_BUF_SIZE)) > 0) {
	++count;
	if (is_empty(line, s))
	    break;
	line = s;
        size -= n;
        if (size < 32)
        {
            // Header way too big
            return -1;
        }
    }
    if (count > 0) {
	//printf("HTTP Header: %s\n", HeaderBuf);
	parse_header(HeaderBuf, hdr);
    }
    return count;
}

int bc_http_read(struct bc_connection* conn, struct bc_http_header* hdr) {
    int r = 0;
    if (!conn || !hdr) {
	assert(conn);
	assert(hdr);
	return -1;
    }
    memset(hdr, 0, sizeof(struct bc_http_header));

    r = bc_http_read_header(conn, hdr);
    if (r > 0) {
	if (hdr->content_length < BCA_NET_BUFFER_SIZE &&
	    hdr->content_length > 0) {
	    r = bc_net_readn(conn, conn->buf, hdr->content_length);
	    if (r == hdr->content_length) {
		conn->buf[r] = '\0';
		conn->buf_len = r;
		//printf("HTTP Response:\n%s\n", conn->buf);
		if (!hdr->keep_alive)
		    bc_net_close(conn);
		if (hdr->result_code != 200) {
		    ++ErrorCount;
		    r = -1;
		    syslog(LOG_ERR, "Received HTTP %d response from server.",
			   hdr->result_code);
		}
	    } else {
		++ErrorCount;
		r = -1;
		syslog(LOG_ERR,
		       "Truncated HTTP response from server: "
		       "expected %d, received %d",
		       hdr->content_length, r);
	    }
	} else {
	    if (0 == conn->loop) {
		conn->loop = 1;
		return bc_net_write(conn);
	    }
	    ++ErrorCount;
	    r = -1;
	    syslog(LOG_ERR, "Invalid HTTP content length: %d",
		   hdr->content_length);
	}
    } else {
	++ErrorCount;
	if (r == 0 &&
	    (strncmp(conn->buf, "POST", 4) == 0 ||
	     strncmp(conn->buf, "GET", 3) == 0)) {
	    // request is still intact, retry the send
	    conn->loop = 1;
	    return bc_net_write(conn);
	}
	// if !EOF, fail the request
	r = -1;
	if (errno != 0)
	    syslog(LOG_ERR, "Cannot read HTTP header: %s", strerror(errno));
	else
	    syslog(LOG_ERR, "Cannot read HTTP header");
    }
    return r;
}
