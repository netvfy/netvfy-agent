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

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../agent.h"

static void		 usage(void);

void
usage(void)
{
	extern char	*__progname;
	fprintf(stderr, "usage: %s\n"
	    "\t-k\tConfigure new network [provisioning key]\n"
	    "\t-n\tName network [should be used with the -k flag]\n"
	    "\t-l\tList networks\n"
	    "\t-c\tConnect [network name]\n"
	    "\t-d\tDelete [network name]\n"
	    "\t-h\thelp\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int		 ch;
	int		 list_networks = 0;
	int		 del_network = 0;
	char		*provcode = NULL;
	char		*network_name = NULL;
	char		 new_name[64];

	while ((ch = getopt(argc, argv, "hk:n:lc:d:")) != -1) {

		switch (ch) {
		case 'n':
			network_name = optarg;
			break;
		case 'k':
			provcode = optarg;
			break;
		case 'l':
			list_networks = 1;
			break;
		case 'c':
			network_name = optarg;
			break;
		case 'd':
			del_network = 1;
			network_name = optarg;
			break;
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

#ifdef _WIN32
        WORD wVersionRequested = MAKEWORD(1,1);
        WSADATA wsaData;
        WSAStartup(wVersionRequested, &wsaData);
#endif

	if (ndb_init() < 0) {
		fprintf(stderr, "%s: db_init\n", __func__);
		exit(-1);
	}

	if (list_networks) {
		ndb_networks(NULL);
		exit(0);
	}

	if (del_network) {
		ndb_network_remove(network_name);
		exit(0);
	}

	char *p;
	if (provcode != NULL) {	
		if (network_name == NULL) {
			printf("Give this network the name you want: ");
			if (fgets(new_name, sizeof(new_name)-1, stdin) == NULL)
				errx(0, "fgets");
				
			if ((p = strchr(new_name, '\n')) != NULL)
				*p = '\0';

			network_name = new_name;
		} 

		if (ndb_provisioning(provcode, network_name) < 0)
			usage();
		else
			goto out;

	} else if (network_name)
		agent_start(network_name);
	else
		usage();

	printf("agent shutdown...\n");
out:
	ndb_fini();
	control_fini();
	switch_fini();
	agent_fini();

	return (0);
}

