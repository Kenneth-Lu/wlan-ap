//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_connect.c
//
//  Description: Socket connection handlers
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
#include "bc_net.h"
#include "bc_string.h"
#include "bccsdk.h"
#include "bc_stats.h"
#include "bc_rtu.h"
#include "bc_alloc.h"
#include "base64.h"
#include "bc_poll.h"
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#ifdef HAS_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include "log.h"

/* params */
static char Server[256] = { 0 };
static char DbServer[256] = { 0 };
static char CertPath[2048] = { 0 };
static char ProxyServer[256] = { 0 };
static char ProxyUser[64] = { 0 };
static char ProxyPass[64] = { 0 };
static int ProxyPort = 0;
static int Timeout = 20;
static int KeepAlive = 60;
static int PoolSize = 2;
static int UseSSL = 0;
static int UseProxy = 0;
static struct bc_poller* Poller = 0;

/* runtime */
#define POOL_MAX 10
static struct bc_connection* Pool[POOL_MAX] = { 0 };
static int SecondTimer = 0;
static int MinuteTimer = 0;
#ifdef HAS_OPENSSL
static SSL_CTX* SSLContext = 0;
#endif
static unsigned int Clock = 0;
static time_t TimeDiff = 0;

/* stats */
static unsigned int ReadErrors = 0;
static unsigned int WriteErrors = 0;
static unsigned int ConnectErrors = 0;
static unsigned int DNSErrors = 0;

void* bc_net_get_poller(void) {
    return Poller;
}

#ifdef HAS_OPENSSL
static int init_ssl_context(void) {
    int e = 0;
    const SSL_METHOD* method;

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ERR_load_BIO_strings();

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    method =  TLSv1_2_client_method();
#else
    method =  TLS_client_method();
#endif
    SSLContext = SSL_CTX_new(method);
    if (!SSLContext) {
	e = ERR_get_error();
	syslog(LOG_ERR, "Error creating SSL context: %s", ERR_error_string(e, 0));
	return -1;
    }
    SSL_CTX_set_cipher_list(SSLContext, "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384");
    if (CertPath[0] != '\0') {
	if (SSL_CTX_load_verify_locations(SSLContext, CertPath, 0) == 0)
	    {
		e = ERR_get_error();
		SSL_CTX_free(SSLContext);
		SSLContext = 0;
		syslog(LOG_ERR, "Error loading cert file %s: %s",
		       CertPath, ERR_error_string(e, 0));
		return -1;
	    }
    }
    return 0;
}
#endif

static int init_connection_pool(void) {
    int i;
    unsigned int flags = BCA_NET_KEEPALIVE;
#ifdef HAS_OPENSSL
    if (UseSSL && SSLContext)
	flags |= BCA_NET_SSL;
#endif

    for (i = 0; i < PoolSize; ++i) {
	Pool[i] = (struct bc_connection*) bc_malloc(sizeof(struct bc_connection));
	if (!Pool[i])
	    return -1;
	memset(Pool[i], 0, sizeof(struct bc_connection));
	Pool[i]->fd = -1;
	Pool[i]->flags = flags;
    }
    return 0;
}

unsigned int bc_net_get_clock(void) {
    return Poller ? Clock : time(0) - (unsigned int)TimeDiff;
}

static void minute_event(void* p, int id) {
    int i;
    struct bc_connection* conn;
    unsigned int now = bc_net_get_clock();
    /*
     * Compensate for timer drift in the seconds counter, the minute counter is
     * more accurate over time.
     * This code tracks reasonably well with time(0) - StartTime.
     */
    unsigned int diff = now % 60;
    if (diff != 0) {
	Clock -= diff;
	Clock += 60;
    }
    now = Clock;
    for (i = 0; i < PoolSize; ++i) {
	conn = Pool[i];
	assert(conn);
	if (!conn->busy && conn->fd != -1 && conn->time + KeepAlive < now) {
	    //syslog(LOG_DEBUG, "closing fd %d", conn->fd);
	    bc_net_close(conn);
	}
    }
    bc_stats_timer_event();
    bc_rtu_timer_event();
}

static void second_event(void* p, int id) {
    ++Clock;
}

void bc_net_close(struct bc_connection* conn) {
#ifdef HAS_OPENSSL
    SSL* ssl;
#endif
    if (conn) {
#ifdef HAS_OPENSSL
	if (conn->ssl) {
	    ssl = (SSL*) conn->ssl;
	    SSL_free(ssl);
	    conn->ssl = 0;
	}
#endif
	if (conn->fd != -1) {
	    if (Poller != 0) {
		bc_poll_remove_fd(Poller, conn->fd, BC_POLL_WRITE);
		bc_poll_remove_fd(Poller, conn->fd, BC_POLL_READ);
	    }
	    close(conn->fd);
	    conn->fd = -1;
	}
	conn->retry = 0;
	conn->loop = 0;
        conn->connect_error = 0;
    }
}

void shutdown_connection_pool(void) {
    int i;
    for (i = 0; i < PoolSize; ++i) {
	bc_net_close(Pool[i]);
	bc_free(Pool[i]);
	Pool[i] = 0;
    }
}

void bc_net_shutdown(void) {
    shutdown_connection_pool();
    if (Poller != 0) {
	if (SecondTimer != 0) {
	    bc_poll_remove_timer(Poller, SecondTimer);
	    SecondTimer = 0;
	}
	if (MinuteTimer != 0) {
	    bc_poll_remove_timer(Poller, MinuteTimer);
	    MinuteTimer = 0;
	}
	Poller = 0;
    }
#ifdef HAS_OPENSSL
    if (SSLContext) {
	SSL_CTX_free(SSLContext);
	SSLContext = 0;
	ERR_free_strings();
	EVP_cleanup();
    }
#endif
}

void bc_net_set_poller(void* val) {
    assert(val != 0);
    Poller = (struct bc_poller*) val;
    if (Poller) {
	if (!SecondTimer) {
	    SecondTimer = bc_poll_add_timer(Poller, 1000, second_event, 0, 1);
	}
	if (!MinuteTimer) {
	    MinuteTimer = bc_poll_add_timer(Poller, 60000, minute_event, 0, 1);
	}
    }
}

int bc_net_init(const struct bc_init_params* params) {
    int r = 0;
    bc_cpystrn(Server, params->server, sizeof(Server));
    bc_cpystrn(DbServer, params->dbserver, sizeof(DbServer));
    bc_cpystrn(CertPath, params->cert, sizeof(CertPath));
    bc_cpystrn(ProxyServer, params->proxy_server, sizeof(ProxyServer));
    bc_cpystrn(ProxyUser, params->proxy_user, sizeof(ProxyUser));
    bc_cpystrn(ProxyPass, params->proxy_pass, sizeof(ProxyPass));
    ProxyPort = params->proxy_port;
    UseProxy = params->proxy;
    Timeout = params->timeout;
    KeepAlive = params->keep_alive;
#ifdef HAS_OPENSSL
    UseSSL = params->ssl;
#endif
    PoolSize = (params->pool_size < POOL_MAX) ? params->pool_size : POOL_MAX;
    if (PoolSize < 1)
	PoolSize = 1;

    TimeDiff = time(0);
#ifdef HAS_OPENSSL
    if (UseSSL)
	r = init_ssl_context();
#endif
    /* Note: must be called after ssl context is initialized */
    if (0 == r) {
	r = init_connection_pool();
    }
    if (0 == r && params->poller) {
	bc_net_set_poller(params->poller);
    }
    return r;
}

void bc_net_get_stats(struct bc_stats* stats) {
    if (!stats) {
	assert(stats);
	return;
    }
    stats->net_errors = DNSErrors + ConnectErrors + ReadErrors + WriteErrors;
}

void bc_net_reset_stats(void) {
    DNSErrors = ConnectErrors = ReadErrors = WriteErrors = 0;
}

const char* bc_net_get_dbhost(void) {
    return DbServer;
}

const char* bc_net_get_host(void) {
    return Server;
}

int bc_net_active(void) {
    int i;
    for (i = 0; i < PoolSize; ++i) {
	if (Pool[i]->busy)
	    return 1;
    }
    return 0;
}

int bc_net_use_ssl(void) {
    return UseSSL;
}

/* This will always block, need async dns client to fix */
static int resolve(struct bc_connection* conn) {
    struct in_addr in = { 0 };
    struct addrinfo hints = { 0 };
    struct addrinfo* result = 0;
    int ret = -1;
    int r = 0;
    const char* server = Server;
    if (!conn) {
	assert(conn);
	return -1;
    }
    if (UseProxy) {
	server = ProxyServer;
    }
    else if ((conn->flags & BCA_NET_USEHOST) != 0) {
	server = conn->host;
    }
    else if ((conn->flags & BCA_NET_DATABASE) != 0) {
	server = DbServer;
    }
    if (inet_pton(AF_INET, server, &in) > 0) {
	conn->addr = in.s_addr;
	ret = 0;
    }
    else {
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	if ((r = getaddrinfo(server, 0, &hints, &result)) == 0 && result) {
	    conn->addr = ((struct sockaddr_in*)result->ai_addr)->sin_addr.s_addr;
	    freeaddrinfo(result);
	    ret = 0;
	} else {
	    ++DNSErrors;
	    syslog(LOG_ERR, "Cannot resolve host %s: %s", server, gai_strerror(r));
	}
    }
    return ret;
}

static int set_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
	return -1;
    flags |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

static int set_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
	return -1;
    flags &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

struct bc_connection* bc_net_get_connection(void) {
    int i;
    unsigned int now = bc_net_get_clock();
    for (i = 0; i < PoolSize; ++i) {
	if (Pool[i]) {
	    if (Pool[i]->busy && now - Pool[i]->time > 30) {
		syslog(LOG_WARNING, "%s", "Connection pool deadlock detected: recovering");
		if (Pool[i]->data && Pool[i]->error_handler)
		    Pool[i]->error_handler(Pool[i], BCA_STATE_TIME_OUT);
		Pool[i]->data = 0;
		Pool[i]->reader = 0;
		Pool[i]->error_handler = 0;
		Pool[i]->busy = 0;
		Pool[i]->buf_len = 0;
		Pool[i]->retry = 0;
		Pool[i]->loop = 0;
	    }
	    if (!Pool[i]->busy)
		return Pool[i];
	}
    }
    return 0;
}

#ifdef HAS_OPENSSL
static int ssl_connect(struct bc_connection* conn) {
    SSL* ssl;
    int ret;
    int status;
    int e;
    if (!SSLContext || !conn) {
	assert(SSLContext);
	assert(conn);
	return -1;
    }
    ssl = SSL_new(SSLContext);
    if (!ssl) {
	e = ERR_get_error();
	syslog(LOG_ERR, "Unable to create new ssl connection: %s", ERR_error_string(e, 0));
	bc_net_close(conn);
	return -1;
    }
    if (!SSL_set_fd(ssl, conn->fd)) {
	e = ERR_get_error();
	syslog(LOG_ERR, "Unable to set the SSL file descriptor: %s", ERR_error_string(e, 0));
	SSL_free(ssl);
	bc_net_close(conn);
	return -1;
    }
    const char* tlsHost = Server;
    if ((conn->flags & BCA_NET_USEHOST) != 0)
	tlsHost = conn->host;
    else if ((conn->flags & BCA_NET_DATABASE) != 0)
	tlsHost = DbServer;
    SSL_set_tlsext_host_name(ssl, tlsHost);

    ret = SSL_connect(ssl);
    if (1 != ret) {
	status = SSL_get_error(ssl, ret);
	if (status != SSL_ERROR_NONE) {
	    e = ERR_get_error();
	    SSL_free(ssl);
	    syslog(LOG_ERR, "SSL connect failed: %s", ERR_error_string(e, 0));
	    bc_net_close(conn);
	    return -1;
	}
    }
    X509* cert = SSL_get_peer_certificate(ssl);
    if (cert) {
	X509_free(cert);
    } else {
	syslog(LOG_ERR, "%s", "No SSL peer certificate received during negotiation");
	SSL_free(ssl);
	bc_net_close(conn);
	return -1;
    }
    if (CertPath[0] != '\0') {
	long vres = SSL_get_verify_result(ssl);
	if (vres != X509_V_OK) {
	    syslog(LOG_ERR, "SSL certificate verification failed: %ld", vres);
	    SSL_free(ssl);
	    bc_net_close(conn);
	    return -1;
	}
    }
    conn->ssl = ssl;
    return 0;
}
#endif

static int read_line(int fd, char* line, size_t len) {
    char c;
    int n = 0;
    int r = 0;
    char* s = line;
    char* e = line + len - 1;

    while (s < e) {
	if ((r = recv(fd, &c, 1, 0)) != 1)
	    break;
	*s++ = c;
	++n;
	if (c == '\n')
	    break;
    }
    *s = '\0';
    while (line < s) {
	--s;
	if (*s == '\r' || *s == '\n')
	    *s = '\0';
	else
	    break;
    }
    return r > 0 ? n : r;
}

static int proxy_handshake(struct bc_connection* conn) {
    const char* ConnectFmt =
	"CONNECT %s:%d HTTP/1.1\r\n"
	"Host: %s:%d\r\n"
	"Proxy-Connection: keep-alive\r\n";

    int status = 0;
    int content_length = 0;
    int len;
    int r;
    char* p;
    char buf[2048];
    char auth_buf[1024];
    uint16_t port = 80;
    const char* server = Server;

    if ((conn->flags & BCA_NET_USEHOST) != 0) {
	server = conn->host;
    } else if ((conn->flags & BCA_NET_DATABASE) != 0) {
	server = DbServer;
    }
#ifdef HAS_OPENSSL
    if ((conn->flags & BCA_NET_SSL) != 0 && SSLContext) {
	port = 443;
    }
#endif
    snprintf(buf, sizeof(buf), ConnectFmt, server, port, server, port);
    if (send(conn->fd, buf, strlen(buf), MSG_NOSIGNAL) < 1) {
	syslog(LOG_ERR, "Proxy handshake write error: %s", strerror(errno));
	++ConnectErrors;
	bc_net_close(conn);
	return -1;
    }
    if (ProxyUser[0] != '\0' && ProxyPass[0] != '\0') {
	snprintf(buf, sizeof(buf), "%s:%s", ProxyUser, ProxyPass);
	modp_b64_encode(auth_buf, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Proxy-Authorization: basic %s\r\n\r\n", auth_buf);
	if (send(conn->fd, buf, strlen(buf), MSG_NOSIGNAL) < 1) {
	    syslog(LOG_ERR, "Proxy handshake write error: %s", strerror(errno));
	    ++ConnectErrors;
	    bc_net_close(conn);
	    return -1;
	}
    } else if (send(conn->fd, "\r\n", 2, MSG_NOSIGNAL) < 1) {
	syslog(LOG_ERR, "Proxy handshake write error: %s", strerror(errno));
	++ConnectErrors;
	bc_net_close(conn);
	return -1;
    }
    for ( ; ; ) {
	r = read_line(conn->fd, buf, sizeof(buf));
	if (r < 1) {
	    syslog(LOG_ERR, "Cannot read proxy response: %s", strerror(errno));
	    bc_net_close(conn);
	    return -1;
	}
	if (buf[0] == '\0')
	    break;
	else if (strncmp(buf, "HTTP/1.", 7) == 0) {
	    p = buf + 7;
	    while (!isspace(*p))
		++p;
	    while (isspace(*p))
		++p;
	    status = atoi(p);
	}
	else if (strncmp(buf, "Content-Length:", 15) == 0)
	    {
		p = buf + 15;
		while (!isspace(*p))
		    ++p;
		while (isspace(*p))
		    ++p;
		content_length = atoi(p);
	    }
    }
    while (content_length > 0) {
	len = (int) sizeof(buf) - 1;
	if (content_length < len)
	    len = content_length;
	r = read(conn->fd, buf, len);
	if (r == -1) {
	    syslog(LOG_ERR, "Error reading proxy content: %s", strerror(errno));
	    bc_net_close(conn);
	    return -1;
	} else if (r == 0) {
	    syslog(LOG_ERR, "%s", "Unexpected EOF reading proxy content");
	    bc_net_close(conn);
	    return -1;
	}
	content_length -= r;
	buf[r] = '\0';
	//syslog(LOG_DEBUG, "%s", buf);
    }
    if (status != 200) {
	syslog(LOG_ERR, "Proxy returned HTTP %d response to CONNECT", status);
	bc_net_close(conn);
	return -1;
    }
    return 0;
}

void connect_event(void* data, int fd) {
    int err = 0;
    socklen_t len = sizeof(err);
    struct bc_connection* conn = (struct bc_connection*) data;
    bc_poll_remove_fd(Poller, conn->fd, BC_POLL_WRITE);
    if (getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1 || err != 0) {
	if (err == 0) {
	    syslog(LOG_ERR, "getsockopt failed on connect to %s: %s",
		   Server, strerror(errno));
	} else {
	    syslog(LOG_ERR, "Cannot connect to %s: %s",
		   Server, strerror(err));
	}
	++ConnectErrors;
	bc_net_close(conn);
	if (conn->error_handler)
	    conn->error_handler(conn, BCA_STATE_CONNECT);
	return;
    } else {
	set_blocking(conn->fd);
	if (UseProxy) {
	    if (proxy_handshake(conn) == -1) {
		if (conn->error_handler)
		    conn->error_handler(conn, BCA_STATE_CONNECT);
		return;
	    }
	}
#ifdef HAS_OPENSSL
	if ((conn->flags & BCA_NET_SSL) != 0 && SSLContext) {
	    if (ssl_connect(conn) == -1) {
		if (conn->error_handler)
		    conn->error_handler(conn, BCA_STATE_CONNECT);
		return;
	    }
	}
#endif
	if (conn->buf_len) {
	    if (bc_net_write(conn) == -1) {
		if (conn->error_handler)
		    conn->error_handler(conn, BCA_STATE_WRITE);
		return;
	    }
	}
    }
}

int bc_net_connect(struct bc_connection* conn) {
    struct sockaddr_in server = { 0 };
    int opt;
    struct timeval tv = { Timeout, 0 };
    /* NOTE: All bcap server connections are either to port 80 or port 443 */
    uint16_t port = 80;

    if (!conn) {
	assert(conn);
	return -1;
    }
    if (conn->fd != -1) {
	/* already connected */
	return 0;
    }
    if (resolve(conn) == -1) {
	return -1;
    }
    if (UseProxy) {
	port = ProxyPort;
    }
#if HAS_OPENSSL
    if ((conn->flags & BCA_NET_SSL) != 0 && SSLContext) {
	port = 443;
    }
#endif
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = conn->addr;
    server.sin_port = htons(port);
    conn->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->fd == -1) {
	++ConnectErrors;
	syslog(LOG_ERR, "Cannot create socket: %s", strerror(errno));
	return -1;
    }
    opt = 1;
    /* disable nagle */
    setsockopt(conn->fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    setsockopt(conn->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* set receive timeout */
    setsockopt(conn->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(conn->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (Poller != 0) {
	set_non_blocking(conn->fd);
    } else {
        set_blocking(conn->fd);
    }

    if (connect(conn->fd, (struct sockaddr*) &server, sizeof(server)) == -1) {
        bool bail;

        bail = ((Poller == 0) || ((Poller != 0) && (errno != EINPROGRESS)));
        if (bail) {
	    ++ConnectErrors;
            conn->connect_error = 1;
	    if (conn->error_handler)
		conn->error_handler(conn, BCA_STATE_CONNECT);
	    bc_net_close(conn);
	    syslog(LOG_ERR, "Cannot connect to host %s: %s", Server, strerror(errno));
	    return -1;
	}
    }
    conn->time = bc_net_get_clock();
    if (Poller != 0) {
	bc_poll_add_fd(Poller, conn->fd, BC_POLL_WRITE, connect_event, conn);
	return 0;
    }
    if (UseProxy) {
	if (proxy_handshake(conn) == -1) {
	    if (conn->error_handler)
		conn->error_handler(conn, BCA_STATE_CONNECT);
	    return -1;
	}
    }
#ifdef HAS_OPENSSL
    if ((conn->flags & BCA_NET_SSL) != 0 && SSLContext) {
	return ssl_connect(conn);
    }
#endif
    return 0;
}

static int conn_read(struct bc_connection* conn, char* buf, size_t len) {
#ifdef HAS_OPENSSL
    SSL* ssl;
    int stat;
    int e;
#endif
    int r;
    if (conn->ssl) {
#ifdef HAS_OPENSSL
	ssl = (SSL*) conn->ssl;
	r = SSL_read(ssl, buf, len);
	if (r < 1 && (stat = SSL_get_error(ssl, r))) {
	    if (stat == SSL_ERROR_ZERO_RETURN) {
		// ssl connection closed cleanly
		bc_net_close(conn);
		return 0;
	    }
	    e = ERR_get_error();
	    syslog(LOG_ERR, "SSL Read error: %s", ERR_error_string(e, 0));
	    return -1;
	}
#endif
    } else {
	r = read(conn->fd, buf, len);
	if (r == -1 && errno == EINTR) {
	    r = read(conn->fd, buf, len);
	}
    }
    return r;
}

int bc_net_readn(struct bc_connection* conn, char* buf, size_t len) {
    size_t remaining = 0;
    ssize_t res = 0;
    char* p = 0;
    if (!conn || !buf) {
	assert(conn);
	assert(buf);
	return -1;
    }
    if (len == 0) {
	return 0;
    }
    if (conn->fd == -1) {
	syslog(LOG_ERR, "%s", "Cannot read from socket: Not connected.");
	return -1;
    }
    p = buf;
    remaining = len;
    while (remaining > 0) {
	if ((res = conn_read(conn, p, remaining)) < 0) {
	    ++ReadErrors;
	    syslog(LOG_ERR, "Error reading from socket: %s",
		   strerror(errno));
	    bc_net_close(conn);
	    buf[0] = '\0';
	    return res;
	} else if (res == 0) {
	    bc_net_close(conn);
	    break;
	}
	remaining -= res;
	p += res;
    }
    conn->time = bc_net_get_clock();
    return len - remaining;
}

/* TODO - Read can be made non-blocking (select on read) */
int bc_net_read(struct bc_connection* conn, char* buf, size_t len) {
    int r = 0;
    if (!conn || !buf) {
	assert(conn);
	assert(buf);
	return -1;
    }
    if (len == 0) {
	return -1;
    }
    if (conn->fd == -1) {
	syslog(LOG_ERR, "%s", "Cannot read from socket: Not connected.");
	return -1;
    }
    r = conn_read(conn, buf, len);
    if (r == 0) {
	bc_net_close(conn);
	//buf[0] = '\0';
    } else if (r == -1) {
	++ReadErrors;
	syslog(LOG_ERR, "Error reading from socket: %s", strerror(errno));
	bc_net_close(conn);
	conn->fd = -1;
	//buf[0] = '\0';
    }
    if (r > 0)
	conn->time = bc_net_get_clock();
    return r;
}

int bc_net_readline(struct bc_connection* conn, char* line,
		    char** last, size_t len) {
    char c;
    int n = 0;
    int r = 0;
    char* e = line + len;
    while (line < e) {
	if ((r = bc_net_read(conn, &c, 1)) < 1)
        {
            bc_net_close(conn);
            conn->fd = -1;
	    break;
        }
	*line++ = c;
	++n;
	if (c == '\n') {
	    *last = line;
	    break;
	}
    }
    *line = '\0';
    return r > 0 ? n : r;
}

static void read_event(void* p, int fd) {
    struct bc_connection* conn = (struct bc_connection*)p;
    bc_poll_remove_fd(Poller, conn->fd, BC_POLL_READ);
    conn->time = bc_net_get_clock();
    if (conn->reader) {
	conn->reader(conn);
    }
}

int bc_net_set_non_blocking(struct bc_connection* conn) {
    if (Poller != 0) {
	return set_non_blocking(conn->fd);
    }
    return 0;
}

void bc_net_wait_for_read(struct bc_connection* conn) {
    if (Poller != 0) {
	bc_poll_add_fd(Poller, conn->fd, BC_POLL_READ, read_event, conn);
    }
}

static int conn_write(struct bc_connection* conn,
		      const char* buf, size_t len) {
#ifdef HAS_OPENSSL
    SSL* ssl;
    int stat;
    int e;
#endif
    int r = 0;
    if (conn->ssl) {
#ifdef HAS_OPENSSL
	ssl = (SSL*) conn->ssl;
	r = SSL_write(ssl, buf, len);
	if (r < 1 && (stat = SSL_get_error(ssl, r))) {
	    e = ERR_get_error();
	    syslog(LOG_ERR, "SSL write error: %s", ERR_error_string(e, 0));
	    return -1;
	}
#endif
    } else {
	// Do not generate SIGPIPE
	r = send(conn->fd, buf, len, MSG_NOSIGNAL);
	// Retry once on interrupted system call
	if (r == -1 && errno == EINTR) {
	    r = send(conn->fd, buf, len, MSG_NOSIGNAL);
	}
    }
    return r;
}

static int writen(struct bc_connection* conn, const char* buf, size_t len) {
    ssize_t written = 0;
    const char* p = buf;
    size_t remaining = len;
    while (remaining > 0) {
	if ((written = conn_write(conn, p, remaining)) <= 0)
	    return written;
	remaining -= written;
	p += written;
    }
    return len;
}

int bc_net_write(struct bc_connection* conn) {
    int r;
    if (!conn) {
	assert(conn);
	return -1;
    }
    if (conn->buf_len == 0) {
	return -1;
    }
    if (conn->fd == -1) {
	/* closed, can't reconnect */
	if (bc_net_connect(conn) == -1)
	    return -1;
	else if (Poller != 0)
	    return 0;
    }
    r = writen(conn, conn->buf, conn->buf_len);
    if (r == -1) {
	int err = errno;
	/* error */
	++WriteErrors;
	bc_net_close(conn);
	syslog(LOG_ERR, "Error writing to socket: %s", strerror(errno));
	if (Poller != 0 && (err == EPIPE || err == EAGAIN || err == EBADF)) {
	    if (++conn->retry > 3)
		return -1;
	    bc_net_connect(conn);
	    return 0;
	}
    } else if (r > 0) {
	conn->retry = 0;
	conn->time = bc_net_get_clock();
	if (Poller != 0) {
	    bc_poll_add_fd(Poller, conn->fd, BC_POLL_READ, read_event, conn);
	    r = 0;
	}
	else if (conn->reader) {
	    /* this goes in the poller callback */
	    r = conn->reader(conn);
	}
    }
    return r;
}

unsigned long bc_net_add_timer(int ms, void (*callback)(void*, int),
			       int restart) {
    if (Poller) {
	return bc_poll_add_timer(Poller, ms, callback, &Poller, restart);
    }
    return 0;
}

void bc_net_remove_timer(unsigned long timer_id) {
    if (Poller) {
	bc_poll_remove_timer(Poller, timer_id);
    }
}

void bc_net_update(struct bc_init_params *params) {
    if (params->keep_alive != 0) {
        LOGI("%s: setting keep alive to %d", __func__,
              params->keep_alive);
        KeepAlive = params->keep_alive;
    }
}
