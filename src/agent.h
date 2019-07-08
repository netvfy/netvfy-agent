/*
 * NetVirt - Network Virtualization Platform
 * Copyright (C) 2009-2017 Mind4Networks inc.
 * Nicolas J. Bouliane <nib@m4nt.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 3
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef NVAGENT_H
#define NVAGENT_H

#ifndef NVAGENT_VERSION
#error "Only versioned builds are supported.";
#endif

#include <sys/tree.h>

#include <tapcfg.h>

#include <pki.h>

#ifdef __cplusplus
extern "C" {
#endif

struct network {
	RB_ENTRY(network)	 entry;
	size_t			 idx;
	char			*name;
	char			*api_srv;
	char			*cert;
	char			*pvkey;
	char			*cacert;
	uint8_t			 buf[5000];
	unsigned long		 buf_total;
};

struct agent_event {
	void	(*connected)(const char *);
	void	(*disconnected)(void);
	void	(*disconnect)(void);
	void	(*log)(const char *);
};

void		 switch_fini(void);
int		 switch_init(tapcfg_t *, int, const char *, const char *, const char *, const char *);

int		 ndb_init(void);
void		 ndb_fini(void);
void		 ndb_networks(void(*fn)(const char *));
struct network	*ndb_network(const char *);
int		 ndb_network_remove(const char *);
int		 ndb_provisioning(const char *, const char *);

int		 control_init(const char *);
void		 control_fini(void);

void		 agent_init_cb(void);
void		 agent_init(void);
void		 agent_fini(void);
void		 agent_start(const char *);
#ifdef USE_THREAD
void		 agent_thread_fini(void);
void		 agent_thread_start(const char*);
#endif

extern struct event_base 	*ev_base;
extern struct agent_event	*agent_cb;

#ifdef __cplusplus
}
#endif

#endif
