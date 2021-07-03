//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_connect.h
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
#ifndef bc_net_h
#define bc_net_h
#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

struct bc_init_params;
struct bc_stats;

/*! connection states for error handler callback (async) */
#define BCA_STATE_CONNECT   1
#define BCA_STATE_WRITE     2
#define BCA_STATE_TIME_OUT  3

/*! connection flags */
#define BCA_NET_SSL       0x01
#define BCA_NET_DATABASE  0x02
#define BCA_NET_KEEPALIVE 0x04
#define BCA_NET_NONBLOCK  0x08
#define BCA_NET_USEHOST   0x10

#define BCA_NET_BUFFER_SIZE 32768
#define BCA_NET_HOST_SIZE 256

struct bc_connection
{
	void*        data;                             /*!< data context (request) */
	char         buf[BCA_NET_BUFFER_SIZE];         /*!< connection i/o buffer */
	char         host[BCA_NET_HOST_SIZE];          /*!< server host name */
	void*        ssl;                              /*!< SSL context if ssl */
	int (*reader)(struct bc_connection*);         /*!< reader function */
	void (*error_handler)(struct bc_connection*, int);  /*!< error handler */
	size_t       buf_len;                          /*!< length of data in buffer */
	unsigned int time;                             /*!< last time connection was active */
	int          fd;                               /*!< file descriptor */
	uint32_t     addr;                             /*!< resolved address */
	int          busy;                             /*!< Connection in use */
	unsigned short retry;                          /*!< Retry count */
	unsigned short loop;                           /*!< Request loop */
	unsigned int flags;                            /*!< Connection flags */
        int          connect_error;                    /*!< Connection flags */
};

int bc_net_init(const struct bc_init_params* params);
void bc_net_shutdown(void);
struct bc_connection* bc_net_get_connection(void);
void bc_net_set_poller(void* p);
int bc_net_connect(struct bc_connection* conn);
int bc_net_write(struct bc_connection* conn);
int bc_net_read(struct bc_connection* conn, char* buf, size_t len);
int bc_net_readn(struct bc_connection* conn, char* buf, size_t len);
int bc_net_readline(struct bc_connection* conn, char* buf, char** last, size_t len);
void bc_net_close(struct bc_connection* conn);
unsigned long bc_net_add_timer(int ms, void (*callback)(void*, int), int restart);
void bc_net_remove_timer(unsigned long timer_id);
void bc_net_get_stats(struct bc_stats* stats);
void bc_net_reset_stats(void);
const char* bc_net_get_dbhost(void);
const char* bc_net_get_host(void);
int bc_net_use_ssl(void);
int bc_net_active(void);
unsigned int bc_net_get_clock();
void* bc_net_get_poller(void);
void bc_net_update(struct bc_init_params *params);
#ifdef __cplusplus
}
#endif

#endif /* bc_net_h */


