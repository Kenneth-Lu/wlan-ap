//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_poll.c
//
//  Description: Asynchronous Event Loop
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
#include "bc_poll.h"
#include "bc_alloc.h"
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <sys/timerfd.h>
#include <sys/select.h>

struct bc_timer {
    struct bc_timer* next;
    void (*callback)(void*, int);
    void* arg;
    int fd;
    int restart;
};

struct bc_event {
    struct bc_event* next;
    void (*callback)(void*, int);
    void* arg;
    int fd;
};

struct bc_poller {
    struct bc_timer* timer_free;
    struct bc_event* event_free;
    struct bc_timer* timer_alloc;
    struct bc_event* event_alloc;
    struct bc_timer* timers;
    struct bc_event* readers;
    struct bc_event* writers;
    int rvalue;
    int running;
};

struct bc_poller* bc_poll_create(size_t max_events, size_t max_timers) {
    size_t i;
    struct bc_poller* poller = (struct bc_poller*)bc_malloc(sizeof(struct bc_poller));
    if (!poller)
	return 0;
    memset(poller, 0, sizeof(struct bc_poller));
    poller->timer_alloc = (struct bc_timer*) bc_malloc(max_timers * sizeof(struct bc_timer));
    if (!poller->timer_alloc) {
	bc_free(poller);
	return 0;
    }
    poller->event_alloc = (struct bc_event*) bc_malloc(max_events * sizeof(struct bc_event));
    if (!poller->event_alloc) {
	bc_free(poller->timer_alloc);
	bc_free(poller);
	return 0;
    }
    memset(poller->timer_alloc, 0, max_timers * sizeof(struct bc_timer));
    memset(poller->event_alloc, 0, max_events * sizeof(struct bc_event));
    for (i = 0; i < max_timers; ++i) {
	poller->timer_alloc[i].next = poller->timer_free;
	poller->timer_free = &poller->timer_alloc[i];
    }
    for (i = 0; i < max_events; ++i) {
	poller->event_alloc[i].next = poller->event_free;
	poller->event_free = &poller->event_alloc[i];
    }
    return poller;
}

void bc_poll_destroy(struct bc_poller* poller) {
    if (poller) {
	if (poller->timer_alloc)
	    bc_free(poller->timer_alloc);
	if (poller->event_alloc)
	    bc_free(poller->event_alloc);
	bc_free(poller);
    }
}

int bc_poll_add_fd(struct bc_poller* poller, int fd, int mode,
		   void (*cb)(void*, int), void* arg) {
    if (!poller)
	return -1;
    struct bc_event* event = poller->event_free;
    if (!event)
	return -1;
    poller->event_free = event->next;
    event->fd = fd;
    event->callback = cb;
    event->arg = arg;
    if (mode == BC_POLL_WRITE) {
	event->next = poller->writers;
	poller->writers = event;
    } else {
	event->next = poller->readers;
	poller->readers = event;
    }
    return 0;
}

void bc_poll_remove_fd(struct bc_poller* poller, int fd, int mode) {
    struct bc_event** prev;
    struct bc_event* event;
    if (mode == BC_POLL_WRITE)
	prev = &poller->writers;
    else
	prev = &poller->readers;
    while (*prev) {
	if ((*prev)->fd == fd) {
	    event = *prev;
	    *prev = event->next;
	    event->next = poller->event_free;
	    poller->event_free = event;
	    break;
	}
	prev = &(*prev)->next;
    }
}

int bc_poll_add_timer(struct bc_poller* poller, int ms,
		      void (*cb)(void*, int), void* arg, int restart) {
    struct itimerspec nv;
    struct itimerspec ov;
    if (!poller)
	return -1;
    struct bc_timer* timer = poller->timer_free;
    if (!timer)
	return -1;
    timer->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timer->fd == -1)
	return -1;
    poller->timer_free = timer->next;
    int sec = ms / 1000;
    long nsec = (ms % 1000) * 1000000; // nanoseconds
    if (restart) {
	nv.it_interval.tv_sec = sec;
	nv.it_interval.tv_nsec = nsec;
    } else {
	nv.it_interval.tv_sec = 0;
	nv.it_interval.tv_nsec = 0;
    }
    nv.it_value.tv_sec = sec;
    nv.it_value.tv_nsec = nsec;
    timerfd_settime(timer->fd, 0, &nv, &ov);

    timer->callback = cb;
    timer->arg = arg;
    timer->restart = restart;
    timer->next = poller->timers;
    poller->timers = timer;
    return timer->fd;
}

void bc_poll_remove_timer(struct bc_poller* poller, int timer) {
    struct bc_timer** prev;
    struct bc_timer* tmp;
    if (!poller)
	return;
    prev = &poller->timers;
    while (*prev) {
	if ((*prev)->fd == timer) {
	    close(timer);
	    tmp = *prev;
	    *prev = tmp->next;
	    tmp->next = poller->timer_free;
	    poller->timer_free = tmp;
	    break;
	}
	prev = &(*prev)->next;
    }
}

int bc_poll_event_loop(struct bc_poller* poller) {
    fd_set rfds;
    fd_set wfds;
    int nfds;
    struct timeval tv;
    int readers;
    int writers;
    struct bc_timer* timer;
    struct bc_timer** prev;
    struct bc_event* event;
    int r;
    if (!poller)
	return -1;

    poller->running = 1;
    while (poller->running) {
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	readers = 0;
	writers = 0;
	nfds = 0;
	for (timer = poller->timers; timer; timer = timer->next) {
	    FD_SET(timer->fd, &rfds);
	    if (nfds < timer->fd)
		nfds = timer->fd;
	    ++readers;
	}
	for (event = poller->readers; event; event = event->next) {
	    FD_SET(event->fd, &rfds);
	    if (nfds < event->fd)
		nfds = event->fd;
	    ++readers;
	}
	for (event = poller->writers; event; event = event->next) {
	    FD_SET(event->fd, &wfds);
	    if (nfds < event->fd)
		nfds = event->fd;
	    ++writers;
	}
	if (nfds > 0)
	    ++nfds;
	r = select(nfds, readers ? &rfds : 0, writers ? &wfds : 0, 0, &tv);
	if (r == -1) {
	    if (errno == EINTR)
		continue;
	    syslog(LOG_ERR, "Error in select: %s", strerror(errno));
	    return -1;
	}
	for (event = poller->writers; event; ) {
	    if (FD_ISSET(event->fd, &wfds))
		{
		    struct bc_event* tmp = event;
		    event = event->next;
		    tmp->callback(tmp->arg, tmp->fd);
		} else
		event = event->next;
	}
	for (prev = &poller->timers; *prev; ) {
	    if (FD_ISSET((*prev)->fd, &rfds)) {
		struct bc_timer* tmp = *prev;
		uint64_t expires = 0;
		read(tmp->fd, &expires, sizeof(expires));
		if (!tmp->restart) {
		    close(tmp->fd);
		    *prev = tmp->next;
		    tmp->next = poller->timer_free;
		    poller->timer_free = tmp;
		} else
		    prev = &(*prev)->next;
		tmp->callback(tmp->arg, tmp->restart ? tmp->fd : 0);
	    } else
		prev = &(*prev)->next;
	}
	for (event = poller->readers; event; ) {
	    if (FD_ISSET(event->fd, &rfds)) {
		struct bc_event* tmp = event;
		event = event->next;
		tmp->callback(tmp->arg, tmp->fd);
	    } else
		event = event->next;
	}
    }
    return poller->rvalue;
}

void bc_poll_terminate(struct bc_poller* poller, int return_value) {
    if (poller) {
	poller->rvalue = return_value;
	poller->running = 0;
    }
}
