/*
 * ippd - Internet Printing Protocol daemon
 * Copyright (c) 2011 Bo Yang <ybable@gmail.com>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the
 * following conditions are met:
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 * 2. Redistributions in binary form must reproduce the
 *    above copyright notice, this list of conditions and
 *    the following disclaimer in the documentation and/or
 *    other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include "comm.h"
#include "eloop.h"

#ifdef __MINGW32__
#include <windows.h>
#include <winsock2.h>
#endif

/* epoll only support in Linux-2.6
	other system use poll */
/************************only use one of the follows************************/
//#define ELOOP_USE_EPOLL
//#define ELOOP_USE_POLL
#define ELOOP_USE_SELECT
/***************************************************************************/
//#define ELOOP_MUTLITH




#ifdef ELOOP_USE_EPOLL
#include <sys/epoll.h>
#endif

#ifdef ELOOP_USE_POLL
#include <poll.h>
#endif

#ifdef ELOOP_USE_SELECT

#endif

#ifdef ELOOP_MUTLITH
#ifdef __MINGW32__
#define sem_t   HANDLE
#define sem_init(sm, pshared, value)    *sm = CreateSemaphore(NULL, 0, value, NULL)
#define sem_destroy(sm)                 CloseHandle(*sm); 
#define sem_wait(sm)                    WaitForSingleObject(*sm, INFINITE)
#define sem_post(sm)                    ReleaseSemaphore(*sm, 1, NULL);
#else
#include <semaphore.h>
#endif
#endif

#define ELOOP_DEBUG

#ifdef ELOOP_DEBUG
//#define eloop_debug(format, ...) printf("%s,%d, "format"\n", __func__, __LINE__, ##__VA_ARGS__)
#define eloop_debug(format, ...)  blog(LOG_DEBUG, format, ##__VA_ARGS__)
#else
#define eloop_debug(format, ...)
#endif


#ifdef ELOOP_MUTLITH
#define RUN_EVENT sem_post(&eloop_op_sem); \
                e->func(e->args); \
				sem_wait(&eloop_op_sem);
#else
#define RUN_EVENT eloop_debug("fd:%d, event:%ld\r\n", e->fd, e->flags); \
                    e->func(e->args);
#endif


#ifdef __MINGW32__
#define CLOCK_MONOTONIC 0
#define MICROSECONDS (1000 * 1000) 
#define timeradd(t1, t2, t3) do { \
            (t3)->tv_sec = (t1)->tv_sec + (t2)->tv_sec; \
            (t3)->tv_usec = (t1)->tv_usec + (t2)->tv_usec % MICROSECONDS; \
            if ((t1)->tv_usec + (t2)->tv_usec > MICROSECONDS) (t3)->tv_sec ++; \
        } while (0)

#define timersub(t1, t2, t3) do { \
            (t3)->tv_sec = (t1)->tv_sec - (t2)->tv_sec; \
            (t3)->tv_usec = (t1)->tv_usec - (t2)->tv_usec; \
            if ((t1)->tv_usec - (t2)->tv_usec < 0) (t3)->tv_usec --, (t3)->tv_usec += MICROSECONDS; \
} while (0) 


LARGE_INTEGER
getFILETIMEoffset()
{
    SYSTEMTIME s;
    FILETIME f;
    LARGE_INTEGER t;

    s.wYear = 1970;
    s.wMonth = 1;
    s.wDay = 1;
    s.wHour = 0;
    s.wMinute = 0;
    s.wSecond = 0;
    s.wMilliseconds = 0;
    SystemTimeToFileTime(&s, &f);
    t.QuadPart = f.dwHighDateTime;
    t.QuadPart <<= 32;
    t.QuadPart |= f.dwLowDateTime;
    return (t);
}

int
clock_gettime(int X, struct timeval *tv)
{
    LARGE_INTEGER           t;
    FILETIME            f;
    double                  microseconds;
    static LARGE_INTEGER    offset;
    static double           frequencyToMicroseconds;
    static int              initialized = 0;
    static BOOL             usePerformanceCounter = 0;

    if (!initialized) {
        LARGE_INTEGER performanceFrequency;
        initialized = 1;
        usePerformanceCounter = QueryPerformanceFrequency(&performanceFrequency);
        if (usePerformanceCounter) {
            QueryPerformanceCounter(&offset);
            frequencyToMicroseconds = (double)performanceFrequency.QuadPart / 1000000.;
        } else {
            offset = getFILETIMEoffset();
            frequencyToMicroseconds = 10.;
        }
    }
    if (usePerformanceCounter) QueryPerformanceCounter(&t);
    else {
        GetSystemTimeAsFileTime(&f);
        t.QuadPart = f.dwHighDateTime;
        t.QuadPart <<= 32;
        t.QuadPart |= f.dwLowDateTime;
    }

    t.QuadPart -= offset.QuadPart;
    microseconds = (double)t.QuadPart / frequencyToMicroseconds;
    t.QuadPart = microseconds;
    tv->tv_sec = t.QuadPart / 1000000;
    tv->tv_usec = t.QuadPart % 1000000;
    return (0);
}
#endif


#define ELOOP_TIMER_FREE_MAX	64
#define ELOOP_EVENT_FREE_MAX	16
//#define ELOOP_TIMER_FREE_MAX	0
//#define ELOOP_EVENT_FREE_MAX	0

struct eloop_timer {
    struct eloop_timer *next;
    struct timeval expect;
    unsigned long flags;
    ELOOP_CALLBACK func;
    void *args;
    int queue;
} eloop_timer_t;

struct eloop_event {
    struct eloop_event *next;
    int fd;
    unsigned long flags;
    ELOOP_CALLBACK func;
    void *args;
} eloop_event_t;

static int eloop_terminated = 0;

static struct eloop_timer *eloop_timers = NULL;
static struct eloop_event *eloop_events = NULL;

static struct eloop_timer *eloop_timers_free = NULL;
static struct eloop_event *eloop_events_free = NULL;
static int eloop_timers_free_n = 0;
static int eloop_events_free_n = 0;

static int eloop_fd = -1;
static int eloop_users = 0;
static int eloop_max_event = 0;
static int eloop_queue = 0;
#ifdef ELOOP_USE_SELECT
static int eloop_max_fd = -1;
static fd_set master_read_fd_set;
static fd_set master_write_fd_set;
static fd_set work_read_fd_set;
static fd_set work_write_fd_set;
#endif

#ifdef ELOOP_MUTLITH
static sem_t eloop_op_sem;
#endif

//#ifdef ELOOP_DEBUG
#if 0
static void
eloop_dump()
{
	struct eloop_event *e;
    if(eloop_events){
        for (e = eloop_events; e; e = e->next) {
            eloop_debug("e:%p, fd:%d, flags:%ld, param:%p", e, e->fd, e->flags, e->args);
        }
    } else {
        eloop_debug("eloop events is null");
    }
}
#else
#define eloop_dump()
#endif
int
eloop_alloc_queue ()
{
	return ++eloop_queue;
}

static void
eloop_del_timer (struct eloop_timer *del)
{
	if (eloop_timers_free_n < ELOOP_TIMER_FREE_MAX) {
		del->next = eloop_timers_free;
		eloop_timers_free = del;
		eloop_timers_free_n++;
	} else
		free(del);
}

static void
eloop_del_event (struct eloop_event *del)
{
	if (eloop_events_free_n < ELOOP_EVENT_FREE_MAX) {
		del->next = eloop_events_free;
		eloop_events_free = del;
		eloop_events_free_n++;
	} else
		free(del);
}

ELOOP_TIMER_ID
eloop_timer_add (int queue,
		struct timeval *timeout,
		ELOOP_CALLBACK func, void *args)
{
	struct eloop_timer *newt;
	struct eloop_timer *timer;
	struct timeval now, tv;
#ifdef __MINGW32__
    struct timeval ts;
#else
	struct timespec ts;
#endif

	eloop_debug("queue:%d, parap:%p\r\n", queue,
                args
           );

	if (!func)
		return NULL;
#ifdef ELOOP_MUTLITH
	sem_wait(&eloop_op_sem);
#endif
	if (eloop_timers_free) {
		newt = eloop_timers_free;
		eloop_timers_free = eloop_timers_free->next;
		eloop_timers_free_n--;
	} else
		newt = (struct eloop_timer *)
				malloc(sizeof(struct eloop_timer));

	if (newt == NULL) {
#ifdef ELOOP_MUTLITH
		sem_post(&eloop_op_sem);
#endif
		return NULL;
	}

	if (timeout->tv_sec == 0 && timeout->tv_usec == 0) {
		/* Save call of gettimeofday */
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	} else {
		clock_gettime(CLOCK_MONOTONIC, &ts);
		now.tv_sec = ts.tv_sec;
#ifdef __MINGW32__
		now.tv_usec = ts.tv_usec;
#else
		now.tv_usec = ts.tv_nsec/1000;
#endif
		timeradd(&now, timeout, &tv);
	}

	newt->queue = queue;
	newt->expect = tv;
	newt->func = func;
	newt->args = args;
	newt->flags = 0;

	if (!eloop_timers ||
			timercmp(&newt->expect, &eloop_timers->expect, <)) {
		newt->next = eloop_timers;
		eloop_timers = newt;
#ifdef ELOOP_MUTLITH
		sem_post(&eloop_op_sem);
#endif
		return newt;
	}
	for (timer = eloop_timers; timer->next; timer = timer->next) {
		/* If expect time is same, the newer the later. */
		if (timercmp(&newt->expect, &timer->next->expect, <)) {
			newt->next = timer->next;
			timer->next = newt;
#ifdef ELOOP_MUTLITH
			sem_post(&eloop_op_sem);
#endif
			return newt;
		}
	}
	newt->next = NULL;
	timer->next = newt;
#ifdef ELOOP_MUTLITH
	sem_post(&eloop_op_sem);
#endif
	return newt;
}

ELOOP_TIMER_ID
eloop_timer_add_sec (int queue,
		int sec,
		ELOOP_CALLBACK func, void *args)
{
	struct timeval tv;
	tv.tv_sec = sec;
	tv.tv_usec = 0;
	return eloop_timer_add(queue, &tv, func, args);
}

int
eloop_timer_del (ELOOP_TIMER_ID id)
{
	struct eloop_timer *del = (struct eloop_timer *)id;
	struct eloop_timer *timer;
	int found = 0;

	eloop_debug("timer id:%p\r\n", id);

	if (!eloop_timers || !id)
		return -1;

#ifdef ELOOP_MUTLITH
	sem_wait(&eloop_op_sem);
#endif
	if (eloop_timers == del) {
		eloop_timers = del->next;
	} else {
		for (timer = eloop_timers; timer->next; timer = timer->next) {
			if (timer->next == del) {
				timer->next = del->next;
				found = 1;
				break;
			}
		}
		if (!found) {
#ifdef ELOOP_MUTLITH
			sem_post(&eloop_op_sem);
#endif
			return -1;
		}
	}

	eloop_debug("params:%p\r\n", del->args);
	eloop_del_timer(del);

#ifdef ELOOP_MUTLITH
	sem_post(&eloop_op_sem);
#endif
	return 0;
}

int
eloop_timer_del_queue (int queue)
{
	struct eloop_timer *timer, *del, *prev = NULL;

	eloop_debug("queue:%d\r\n", queue);

	if (!eloop_timers)
		return 0;

#ifdef ELOOP_MUTLITH
	sem_wait(&eloop_op_sem);
#endif
	for (timer = eloop_timers; timer; ) {
		if (timer->queue == queue) {
			if (!prev) {
				del = timer;
				timer = timer->next;
				eloop_timers = timer;
				eloop_del_timer(del);
			} else {
				del = timer;
				timer = timer->next;
				prev->next = timer;
				eloop_del_timer(del);
			}
		} else {
			prev = timer;
			timer = timer->next;
		}
	}
#ifdef ELOOP_MUTLITH
	sem_post(&eloop_op_sem);
#endif

	return 0;
}

int
eloop_event_add (int fd,
		ELOOP_CALLBACK func, void *args,
		unsigned long flags)
{
	unsigned long eflags = 0;
	struct eloop_event *newe;
	struct eloop_event *e;
#ifdef ELOOP_USE_EPOLL
	int ret;
	struct eloop_event *event = NULL;
	struct epoll_event ev;
#endif

	eloop_debug("fd:%d, event:%ld\r\n", fd, flags);

	if (!(flags & ELOOP_EVENT_FLAG_OP))
		return -1;

#ifdef ELOOP_MUTLITH
	sem_wait(&eloop_op_sem);
#endif
	eflags = flags;
	for (e = eloop_events; e; e = e->next) {
		if (e->fd == fd) {
			if (e->flags & ELOOP_EVENT_FLAG_OP & eflags) {
#ifdef ELOOP_MUTLITH
				sem_post(&eloop_op_sem);
#endif
	            eloop_debug("event for fd:%d already exists\r\n", fd);
				return 1;/* duplicate */
			}
#ifdef ELOOP_USE_EPOLL
			eflags |= e->flags;
			event = e;
#endif
		}
	}

	if (eloop_events_free) {
		newe = eloop_events_free;
		eloop_events_free = eloop_events_free->next;
		eloop_events_free_n--;
	}
	else
		newe = malloc(sizeof(struct eloop_event));

	if (newe == NULL) {
#ifdef ELOOP_MUTLITH
		sem_post(&eloop_op_sem);
#endif
        eloop_debug("get new event room fail, fd:%d\r\n", fd);
		return -1;
	}

#ifdef ELOOP_USE_EPOLL
	ev.events = 0;
	if (eflags & ELOOP_EVENT_FLAG_READ)
		ev.events |= EPOLLIN | EPOLLET;
	if (eflags & ELOOP_EVENT_FLAG_WRITE)
		ev.events |= EPOLLOUT | EPOLLET;
	ev.data.fd = fd;
	if (event)
		ret = epoll_ctl(eloop_fd, EPOLL_CTL_MOD, fd, &ev);
	else
		ret = epoll_ctl(eloop_fd, EPOLL_CTL_ADD, fd, &ev);
	if (ret) {
		ret = errno;
		eloop_debug("epoll_ctl error: %d\r\n", errno);
		eloop_del_event(newe);
#ifdef ELOOP_MUTLITH
		sem_post(&eloop_op_sem);
#endif
		return -ret;
	}
#endif

#ifdef ELOOP_USE_SELECT
    eflags = flags & ELOOP_EVENT_FLAG_OP;
	if (eflags & ELOOP_EVENT_FLAG_READ){
        FD_SET(fd, &master_read_fd_set);
		eloop_debug("fd:%d read event set!\r\n", fd);
    }
	if (eflags & ELOOP_EVENT_FLAG_WRITE){
        FD_SET(fd, &master_write_fd_set);
		eloop_debug("fd:%d write event set!\r\n", fd);
    }
    if (eloop_max_fd != -1 && eloop_max_fd < fd)
        eloop_max_fd = fd;
#endif

	newe->fd = fd;
	newe->func = func;
	newe->args = args;
	newe->flags = flags;

	newe->next = eloop_events;
	eloop_events = newe;

#ifdef ELOOP_MUTLITH
	sem_post(&eloop_op_sem);
#endif
	return 0;
}

int
eloop_event_del (int fd,
		unsigned long flags)
{
	struct eloop_event *del = NULL;
	struct eloop_event *e;
	struct eloop_event *t;
    unsigned long eflags = 0;
#ifdef ELOOP_USE_EPOLL
	struct epoll_event ev;
#endif

	eloop_debug("fd:%d, event:%ld\r\n", fd, flags);

	if (!eloop_events || fd < 0)
		return -1;

#ifdef ELOOP_MUTLITH
	sem_wait(&eloop_op_sem);
#endif
	if (eloop_events->fd == fd && (eloop_events->flags & flags)) {
		del = eloop_events;
		eloop_events = eloop_events->next;
	} else {
        t = eloop_events;
		for (e = eloop_events; e; e = e->next) {
			if (e->fd == fd && (e->flags & flags)) {
                t->next = e->next;
                e->next = NULL;
				del = e;
				break;
			}
            t = e;
		}
	}

	if (!del) {
#ifdef ELOOP_MUTLITH
		sem_post(&eloop_op_sem);
#endif
		return -1;
	}


    eloop_debug("del e:%p, fd:%d, flags:%ld", del, del->fd, del->flags);

    flags &= ELOOP_EVENT_FLAG_OP;
    flags = ~flags;
    del->flags &= flags;
    eflags = del->flags;

    if (!(del->flags & ELOOP_EVENT_FLAG_OP)){
        //no read/write bind to this node, ok to del
        eloop_del_event(del);
    } else {
        //some event left, put node back
        del->next = eloop_events->next;
        eloop_events = del;
    }   


#ifdef ELOOP_USE_EPOLL
	if (eflags) {
		ev.events = 0;
		if (eflags & ELOOP_EVENT_FLAG_READ)
			ev.events |= EPOLLIN | EPOLLET;
		if (eflags & ELOOP_EVENT_FLAG_WRITE)
			ev.events |= EPOLLOUT | EPOLLET;
		ev.data.fd = fd;
		if (epoll_ctl(eloop_fd, EPOLL_CTL_MOD, fd, &ev)) {
#ifdef ELOOP_MUTLITH
			sem_post(&eloop_op_sem);
#endif
			return -1;
		}
	} else {/* kernel before 2.6.9 must set last arg */ 
		if (epoll_ctl(eloop_fd, EPOLL_CTL_DEL, fd, &ev)) {
#ifdef ELOOP_MUTLITH
			sem_post(&eloop_op_sem);
#endif
			return -1;
		}
	}
#endif

#ifdef ELOOP_USE_SELECT
    eflags = ~flags;
	if (eflags & ELOOP_EVENT_FLAG_READ){
        FD_CLR(fd, &master_read_fd_set);
		eloop_debug("fd:%d read event cleared!\r\n", fd);
    }
	if (eflags & ELOOP_EVENT_FLAG_WRITE){
        FD_CLR(fd, &master_write_fd_set);
		eloop_debug("fd:%d write event cleared!\r\n", fd);
    }
    if (eloop_max_fd == fd)
        eloop_max_fd = -1;
#endif

#ifdef ELOOP_MUTLITH
	sem_post(&eloop_op_sem);
#endif
	return 0;
}

int
eloop_event_chg (int fd,
		ELOOP_CALLBACK func, void *args,
		unsigned long flags)
{
	struct eloop_event *e;

	eloop_debug("fd:%d\r\n", fd);

	if (!eloop_events || fd < 0 || !func)
		return -1;

#ifdef ELOOP_MUTLITH
	sem_wait(&eloop_op_sem);
#endif
	for (e = eloop_events; e; e = e->next) {
		if (e->fd == fd && e->flags & flags) {
			e->func = func;
			e->args = args;
#ifdef ELOOP_MUTLITH
			sem_post(&eloop_op_sem);
#endif
			return 0;
		}
	}
#ifdef ELOOP_MUTLITH
	sem_post(&eloop_op_sem);
#endif
	return -1;
}

int
eloop_run (void)
{
#ifdef ELOOP_USE_EPOLL
#define ELOOP_EPOLL_EVENT_MAX 3
	struct epoll_event evs[ELOOP_EPOLL_EVENT_MAX];
#endif
#ifdef ELOOP_USE_POLL
	nfds_t nfds, nfdi;
	int fds_len = 0;
	struct pollfd *fds;
	int n;
#endif
#ifdef ELOOP_USE_SELECT
	struct timeval *tp;
	int tmp_max_fd;
#endif
	struct eloop_timer *tr, *tr_str;
	struct eloop_event *e;
	struct timeval now, tv;
#ifdef __MINGW32__
    struct timeval ts;
#else
	struct timespec ts;
#endif
	int msec;
	int ret;
	int i;

	eloop_debug("terminate:%d\r\n", eloop_terminated);
	/* Only start one eloop in process. */
	if (!eloop_terminated)
		return 1;

	eloop_terminated = 0;

	while (!eloop_terminated) {
		/* Calc timeout */
#ifdef ELOOP_MUTLITH
		sem_wait(&eloop_op_sem);
#endif
restart:
		clock_gettime(CLOCK_MONOTONIC, &ts);
		now.tv_sec = ts.tv_sec;
#ifdef __MINGW32__
		now.tv_usec = ts.tv_usec;
#else
		now.tv_usec = ts.tv_nsec/1000;
#endif
		for (tr_str = tr = eloop_timers; tr; tr = eloop_timers) {
			/* For the earliest at the front of list */
			if (timercmp(&now, &tr->expect, <)) {
			/* Only continue when eloop_timers change,
			break in other case. */
				if (tr == tr_str)
					break;
				else
					goto restart;
			}

			eloop_timers = tr->next;
#ifdef ELOOP_MUTLITH
			sem_post(&eloop_op_sem);
#endif
			/* We may delete queue in func! */
			tr->func(tr->args);
#ifdef ELOOP_MUTLITH
			sem_wait(&eloop_op_sem);
#endif
			eloop_del_timer(tr);
		}

		if (eloop_timers) {
			timersub(&eloop_timers->expect, &now, &tv);
			msec = tv.tv_sec*1000 + tv.tv_usec/1000 + 2;
			/* epoll timeout time is small than msec */
			if(msec <= 0)
				msec = 1;
		}
		else
			msec = -1;
#ifdef ELOOP_MUTLITH
		sem_post(&eloop_op_sem);
#endif

        //sleep(1);
		eloop_debug("wait time %12d msec\r\n", msec);

#ifdef ELOOP_USE_EPOLL
		ret = epoll_wait(eloop_fd, evs, ELOOP_EPOLL_EVENT_MAX, msec);
#endif
#ifdef ELOOP_USE_POLL
		/* Allocate memory for our pollfds as and when needed.
		 * We don't bother shrinking it. */
		nfds = 0;
		for (e = eloop_events; e; e = e->next)
			nfds++;
		if (nfds > fds_len) {
			if(fds_len)
				free(fds);
			/* Allocate 5 more than we need for future use */
			fds_len = nfds + 5;
			fds = malloc(sizeof(*fds) * fds_len);
			if(!fds)
				return -1;
		}
		nfds = 0;
		for (e = eloop_events; e; e = e->next) {
			fds[nfds].fd = e->fd;
			fds[nfds].events = 0;
			if(e->flags & ELOOP_EVENT_FLAG_READ)
				fds[nfds].events |= POLLIN;
			if(e->flags & ELOOP_EVENT_FLAG_WRITE)
				fds[nfds].events |= POLLOUT;
			fds[nfds].revents = 0;
			nfds++;
		}
		ret = poll(fds, nfds, msec);
#endif
#ifdef ELOOP_USE_SELECT
        if (eloop_max_fd == -1){
            for (e = eloop_events; e; e = e->next) {
                if(e->fd > eloop_max_fd)
                    eloop_max_fd = e->fd;
            }
        }

        work_read_fd_set = master_read_fd_set;
        work_write_fd_set = master_write_fd_set;
        tp = &tv;
        if(msec == -1)
            tp = NULL;
        tmp_max_fd = eloop_max_fd;
        ret = select(eloop_max_fd + 1, &work_read_fd_set, &work_write_fd_set, NULL, tp);
#endif
		if (!ret) {	/* No file, means timeout */
			/* It doesn't wait enough time */
			if(msec<2)
				usleep(100);
			continue;
		}

#ifdef ELOOP_MUTLITH
		sem_wait(&eloop_op_sem);
#endif
		/* check event */
#ifdef ELOOP_USE_EPOLL
		for (i=0; i<ret; i++) {
			if(evs[i].events & EPOLLIN){
                for (e = eloop_events; e; e = e->next)
                {
                    if(!((e->fd == evs[i].data.fd)  &&
                                (e->flags & ELOOP_EVENT_FLAG_READ)
                        ))
                        continue;
                    RUN_EVENT;
                    evs[i].events &= ~EPOLLIN;
                    break;
                }
            }
			if(evs[i].events & EPOLLOUT){
                for (e = eloop_events; e; e = e->next)
                {
                    if(!((e->fd == evs[i].data.fd)  &&
                                (e->flags & ELOOP_EVENT_FLAG_WRITE) 
                        ))
                        continue;
                    RUN_EVENT;
                    evs[i].events &= ~EPOLLOUT;
                    break;
                }
            }
		}/* check event end */
#endif
		/* check event */
#ifdef ELOOP_USE_POLL
		for (nfdi = 0; nfdi < nfds; i++) {
            if(fds[nfdi].revents & POLLIN){
                for(e = eloop_events; e; e = e->next) {
                    if(!((e->fd == fds[nfdi].fd) &&
                                (e->flags & ELOOP_EVENT_FLAG_READ)
                        ))
                        continue;
                    RUN_EVENT;
                    break;
                }
            }
            if(fds[nfdi].revents & POLLOUT){
                for(e = eloop_events; e; e = e->next) {
                    if(!((e->fd == fds[nfdi].fd) &&
                                (e->flags & ELOOP_EVENT_FLAG_WRITE)
                        ))
                        continue;
                    RUN_EVENT;
                    break;
                }
            }
		}/* check event end */
#endif
		/* check event */
#ifdef ELOOP_USE_SELECT
        for(i = 0; i <= tmp_max_fd; i ++){
                if(FD_ISSET(i, &work_read_fd_set))
                    eloop_debug("%s: fd:%d, read_event fired\r\n", "ELOOP_CHECK", i);
                if(FD_ISSET(i, &work_write_fd_set))
                    eloop_debug("%s: fd:%d, write_event fired\r\n", "ELOOP_CHECK", i);
        }
        eloop_dump();
        for (i = 0; i <= eloop_max_fd; i ++){
            if(FD_ISSET(i, &work_read_fd_set)){
                for (e = eloop_events; e; e = e->next){
                    if(!((e->fd == i) && (e->flags & ELOOP_EVENT_FLAG_READ))) 
                        continue;
                    RUN_EVENT;
                    break;
                }
            }
            if(FD_ISSET(i, &work_write_fd_set)){
                for (e = eloop_events; e; e = e->next){
                    if(!((e->fd == i) && (e->flags & ELOOP_EVENT_FLAG_WRITE))) 
                        continue;
                    RUN_EVENT;
                    break;
                }
            }
        }/* check event end */
#endif


#ifdef ELOOP_MUTLITH
		sem_post(&eloop_op_sem);
#endif
	}

#ifdef ELOOP_USE_POLL
	free(fds);
#endif
	return 0;
}

void
eloop_stop ()
{
	eloop_terminated = 1;
	return;
}

void
eloop_init (int max_event)
{
	if (eloop_fd != -1) {
		eloop_users++;
		return;
	}
	eloop_max_event = max_event;
#ifdef ELOOP_USE_EPOLL
	eloop_fd = epoll_create(max_event+1);
	if (eloop_fd < 0)
		fprintf(stderr, "%s:epoll_create failed - %d\r\n", __func__, errno);
#endif
#ifdef ELOOP_USE_POLL
	eloop_fd = 0;
#endif
#ifdef ELOOP_USE_SELECT
    FD_ZERO(&master_read_fd_set);
    FD_ZERO(&master_write_fd_set);
#endif
#ifdef ELOOP_MUTLITH
	sem_init(&eloop_op_sem, 0, 1);
#endif

	eloop_terminated = 1;
	eloop_users++;
	return;
}

void
eloop_exit ()
{
	struct eloop_timer *tr, *tr_del;
	struct eloop_event *e, *e_del;
	if (eloop_fd == -1 )
		return;
	if (!(--eloop_users))
		return;
	eloop_terminated = 0;
#ifdef ELOOP_USE_EPOLL
	close(eloop_fd);
#endif
	for (tr = eloop_timers; tr; eloop_timers = tr) {
		tr_del = tr;
		tr = tr->next;
		eloop_del_timer(tr_del);
	}
	for (e = eloop_events; e; eloop_events = e) {
		e_del = e;
		e = e->next;
		eloop_del_event(e_del);
	}
#ifdef ELOOP_MUTLITH
	sem_destroy(&eloop_op_sem);
#endif
	eloop_queue = 0;
	eloop_fd = -1;
	return;
}
