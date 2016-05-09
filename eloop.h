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

#ifndef _ELOOP_H_
#define _ELOOP_H_
#include <sys/time.h>

typedef void(* ELOOP_CALLBACK)(void *);
typedef void * ELOOP_TIMER_ID;

#define ELOOP_EVENT_FLAG_READ	1<<0
#define ELOOP_EVENT_FLAG_WRITE	2<<0
#define ELOOP_EVENT_FLAG_OP		(ELOOP_EVENT_FLAG_READ|ELOOP_EVENT_FLAG_WRITE)

#define ELOOP_TIMER_ID_NULL	NULL

int
eloop_alloc_queue ();

ELOOP_TIMER_ID
eloop_timer_add (int queue,
		struct timeval *timeout,
		ELOOP_CALLBACK func, void *args);

ELOOP_TIMER_ID
eloop_timer_add_sec (int queue,
		int sec,
		ELOOP_CALLBACK func, void *args);

int
eloop_timer_del (ELOOP_TIMER_ID id);

int
eloop_timer_del_queue (int queue);

int
eloop_event_add (int fd,
		ELOOP_CALLBACK func, void *args,
		unsigned long flags);

int
eloop_event_del (int fd,
		unsigned long flags);

int
eloop_event_chg (int fd,
		ELOOP_CALLBACK func, void *args,
		unsigned long flags);

int
eloop_run (void);

void
eloop_stop ();

void
eloop_init (int max_event);

void
eloop_exit ();

#define eloop_call(func,args) eloop_timer_add_sec(-1,0,func,args)

#endif /*_ELOOP_H_ */
