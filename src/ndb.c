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

#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/tree.h>

#include <string.h>

#include <curl/curl.h>

#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/keyvalq_struct.h>
#include <event2/util.h>

#include <jansson.h>

#include <log.h>
#include <pki.h>

#include "agent.h"

#define NDB_VERSION 1

RB_HEAD(network_tree, network);

struct network_tree	 networks;
json_t			*g_jdb = NULL;
json_t			*jnetworks;
int			 version;
char			 ndb_path[256];

static struct network	*ndb_network_new();
void			 ndb_network_free(struct network *);
static int		 ndb_save();
static int		 ndb_fullpath(const char *, char *);
static int		 network_cmp(const struct network *, const struct network *);
RB_PROTOTYPE_STATIC(network_tree, network, entry, network_cmp);

int
network_cmp(const struct network *a, const struct network *b)
{
	return strcmp(a->name, b->name);
}

#if defined(__unix__) || defined(__APPLE__)
static int
mkfullpath(const char *fullpath)
{
	char	*p = NULL;
	char	 tmp[256];

	if (fullpath == NULL)
		return (-1);

	snprintf(tmp, sizeof(tmp),"%s",fullpath);
	if ((p = strchr(tmp, '/')) == NULL)
		return (-1);

	while ((p = strchr(p+1, '/')) != NULL) {
		*p = '\0';
		mkdir(tmp, S_IRWXU | S_IWUSR | S_IXUSR);
		*p = '/';
	}
	return (0);
}
#endif

int
ndb_fullpath(const char *file, char *fullname)
{
#ifdef _WIN32
	return snprintf(fullname, 256, "%s%s%s%s",
	    getenv("AppData"), "\\netvfy\\", "\\", file);
#else
	return snprintf(fullname, 256, "%s%s%s%s",
	    getenv("HOME"), "/.config/netvfy", "/", file);
#endif
}

int
ndb_init(void)
{
	json_t		*jnetwork;
	json_error_t	 error;
	struct network	 *n;
	int		 version;
	size_t		 array_size;
	size_t		 i;

#if defined(__unix__) || defined(__APPLE__)
	{
		/* Create ~/.config/netvfy/ if it doesn't exist. */
		struct	 stat st;
		char	 path[256];

		if (ndb_fullpath("", path) < 0) {
			fprintf(stderr, "%s: ndb_fullpath\n", __func__);
			exit(-1);
		}

		if (stat(path, &st) != 0) {
			mkfullpath(path);
			if (stat(path, &st) != 0) {
				fprintf(stderr, "%s: stat\n", __func__);
				exit(-1);
			}
		}
	}
#endif

	if (ndb_fullpath("nvagent.json", ndb_path) < 0) {
		fprintf(stderr, "%s: ndb_fullpath\n", __func__);
		exit(-1);
	}

	if ((g_jdb = json_load_file(ndb_path, 0, &error)) == NULL)
		return (0);

	if ((json_unpack(g_jdb, "{s:i}", "version", &version)) == -1) {
		fprintf(stderr, "%s: json_unpack\n", __func__);
		return (-1);
	}

	if ((jnetworks = json_object_get(g_jdb, "networks")) == NULL)
		return (0);

	array_size = json_array_size(jnetworks);
	for (i = 0; i < array_size; i++) {

		if ((jnetwork = json_array_get(jnetworks, i)) == NULL)
			continue;

		if ((n = ndb_network_new()) == NULL)
			return (-1);

		if (json_unpack(jnetwork, "{s:s, s:s, s:s, s:s, s:s}",
		    "name", &n->name, "api_srv", &n->api_srv,
		    "cert", &n->cert, "pvkey", &n->pvkey, "cacert", &n->cacert) < 0) {
			fprintf(stderr, "%s: json_unpack\n", __func__);
			return (-1);
		}
		n->idx = i;

		RB_INSERT(network_tree, &networks, n);
	}

	return (0);
}

void
ndb_fini()
{
	struct network	*n;

	json_decref(g_jdb);

	while ((n = RB_ROOT(&networks)) != NULL) {
		RB_REMOVE(network_tree, &networks, n);
		ndb_network_free(n);
	}
}

void
ndb_networks(void(*fn)(const char *))
{
	struct network	*n;

	if (fn == NULL)
		printf("Networks:\n");

	RB_FOREACH(n, network_tree, &networks) {
		if (fn != NULL) {
			fn(n->name);
		} else {
#ifdef _WIN32
			printf("\t[%u] \"%s\"\n", n->idx, n->name);
#else
			printf("\t[%zu] \"%s\"\n", n->idx, n->name);
#endif
		}
	}
}

void
ndb_network_free(struct network *n)
{
	if (n == NULL)
		return;

	free(n);
}

struct network *
ndb_network_new()
{
	struct network	*n = NULL;

	if ((n = malloc(sizeof(struct network))) == NULL) {
		fprintf(stderr, "%s: malloc\n", __func__);
		return (NULL);
	}
	n->idx = 0;
	n->name = NULL;
	n->api_srv = NULL;
	n->cert = NULL;
	n->pvkey = NULL;
	n->cacert = NULL;
	n->buf_total = 0;

	return (n);
}

int
ndb_network_add(struct network *netcf, const char *cert, const char *cacert)
{
	netcf->cert = strdup(cert);
	netcf->cacert = strdup(cacert);

	RB_INSERT(network_tree, &networks, netcf);
	ndb_save();

	return (0);
}

int
ndb_network_remove(const char *network_name)
{
	struct network	*n;

	if ((n = ndb_network(network_name)) == NULL) {
		fprintf(stderr, "%s: unable to find network name '%s'\n", __func__, network_name);
		goto err;
	}

	RB_REMOVE(network_tree, &networks, n);
	ndb_network_free(n);
	ndb_save();

	return (0);

err:
	return (-1);
}

struct network *
ndb_network(const char *network_name)
{
	struct network	needle, *n = NULL;
	needle.name = (char *)network_name;
	if ((n = RB_FIND(network_tree, &networks, &needle)) == NULL)
		return (NULL);

	return (n);
}

int
ndb_save()
{
	json_t		*jdb = NULL;
	json_t		*jnetworks = NULL;
	json_t		*jnetwork = NULL;
	struct network	*n;
	int		 ret = -1;

	if ((jdb = json_object()) == NULL) {
		fprintf(stderr, "%s: json_object\n", __func__);
		goto out;
	}

	if ((jnetworks = json_array()) == NULL) {
		fprintf(stderr, "%s: json_array\n", __func__);
		goto out;
	}

	if (json_object_set_new_nocheck(jdb, "version",
	    json_integer(NDB_VERSION)) < 0 ||
	    json_object_set_new_nocheck(jdb, "networks", jnetworks) < 0) {
		fprintf(stderr, "%s: json_object_set_new_nocheck\n", __func__);
		goto out;
	}

	RB_FOREACH(n, network_tree, &networks) {

		if ((jnetwork = json_object()) == NULL) {
			fprintf(stderr, "%s: json_object\n", __func__);
			goto out;
		}

		if ((json_array_append_new(jnetworks, jnetwork)) < 0) {
			fprintf(stderr, "%s: json_array_append\n", __func__);
			goto out;
		}

		if (json_object_set_new_nocheck(jnetwork, "name",
		    json_string(n->name)) < 0 ||
		    json_object_set_new_nocheck(jnetwork, "api_srv",
		    json_string(n->api_srv)) < 0 ||
		    json_object_set_new_nocheck(jnetwork, "pvkey",
		    json_string(n->pvkey)) < 0 ||
		    json_object_set_new_nocheck(jnetwork, "cert",
		    json_string(n->cert)) < 0 ||
		    json_object_set_new_nocheck(jnetwork, "cacert",
		    json_string(n->cacert)) < 0) {
			fprintf(stderr, "%s: json_object_set_new_nocheck\n",
			    __func__);
				goto out;
		}
	}

	if (json_dump_file(jdb, ndb_path, JSON_INDENT(2)) < 0) {
		fprintf(stderr, "%s: json_dump_file\n", __func__);
		goto out;
	}

	ret = 0;

out:
	json_decref(jdb);

	return (ret);
}

size_t
ndb_prov_cb(void *ptr, size_t size, size_t nmemb, void *arg)
{
	struct network		*netcfg = arg;

	/* Is there still some space in the buffer ? */
	if (netcfg->buf_total < (sizeof(netcfg->buf) - 1))
		/* If not enough space in buffer for the data we received, return error */
			if ((sizeof(netcfg->buf) - 1 - netcfg->buf_total) < nmemb)
				return (-1);

	memcpy(netcfg->buf + netcfg->buf_total, ptr, nmemb);
	netcfg->buf_total += nmemb;

	return (nmemb);
}

int
ndb_provisioning(const char *provlink, const char *network_name)
{
	EVP_PKEY			*keyring = NULL;
	X509_REQ			*certreq = NULL;
	json_t				*jresp;
	json_t				*jmsg = NULL;
	json_error_t			 error;
	digital_id_t			*nva_id = NULL;
	struct evkeyvalq		 headers = TAILQ_HEAD_INITIALIZER(headers);
	struct evhttp_uri		 *uri = NULL;
	struct network			*netcf;
	long				 size = 0;
	char				*resp;
	char				*certreq_pem = NULL;
	char				*pvkey_pem = NULL;
	const char			*provsrv_addr;
	const char			*version;
	const char			*cacert;
	const char			*cert;

	if (strcmp(provlink, "") == 0) {
		fprintf(stderr, "%s: provisioning key must be defined\n", __func__);
		return (-1);
	}

	if (strcmp(network_name, "") == 0) {
		fprintf(stderr, "%s: network name must be defined\n", __func__);
		return (-1);
	}

	//TODO(sneha): when should this be changed for pki
	nva_id = pki_digital_id("",  "", "", "", "contact@dynvpn.com", "www.dynvpn.com");

	/* generate RSA public and private keys */
	keyring = pki_generate_keyring();

	/* create a certificate signing request */
	certreq = pki_certificate_request(keyring, nva_id);
	pki_free_digital_id(nva_id); // XXX that shounld't even exist

	/* write the certreq in PEM format */
	pki_write_certreq_in_mem(certreq, &certreq_pem, &size);
	X509_REQ_free(certreq);

	/* write the private key in PEM format */
	pki_write_privatekey_in_mem(keyring, &pvkey_pem, &size);
	EVP_PKEY_free(keyring);

	if ((netcf = ndb_network_new()) == NULL)
		return (-1);

	netcf->name = strdup(network_name);
	netcf->pvkey = pvkey_pem; // Steal the pointer

	jresp = json_object();
	json_object_set_new(jresp, "csr", json_string(certreq_pem));

	json_object_set_new(jresp, "provlink", json_string(provlink));
	resp = json_dumps(jresp, 0);

	if ((uri = evhttp_uri_parse(provlink)) == NULL) {
		fprintf(stderr, "%s: invalid provisioning key\n", __func__);
		return (-1);
	}

	if ((evhttp_parse_query_str(evhttp_uri_get_query(uri), &headers)) < 0) {
		fprintf(stderr, "%s: invalid provisioning key\n", __func__);
		return (-1);
	}

	if (((version = evhttp_find_header(&headers, "v")) == NULL) ||
	    ((provsrv_addr = evhttp_find_header(&headers, "a")) == NULL) ) {
		fprintf(stderr, "%s: invalid provisioning key\n", __func__);
		return (-1);
	}

	netcf->api_srv = strdup(provsrv_addr);

	/* XXX cleanup needed */

	CURL			*curl = NULL;
	CURLcode	 	 res;
	struct curl_slist	*req_headers = NULL;
	char			 url[256];

	snprintf(url, sizeof(url), "https://%s/v1/provisioning", provsrv_addr);

	/* In windows, this will init the winsock stuff */
	curl_global_init(CURL_GLOBAL_ALL);

	/* get a curl handle */
	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ndb_prov_cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, netcf);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

#ifdef _WIN32
		curl_easy_setopt(curl, CURLOPT_CAINFO, "curl-ca-bundle.crt");
#endif

		req_headers = curl_slist_append(req_headers, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, req_headers);

		/* Now specify the POST data */
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, resp);

		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curl);

		/* Check for errors */
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
			    curl_easy_strerror(res));

		if (netcf->buf_total == 0) {
			fprintf(stdout, "%s: empty response from server, maybe the key is invalid or already been used.\n", __func__);
			goto out;
		}

		/* Parse the json output */
		if ((jmsg = json_loadb((const char *)netcf->buf, netcf->buf_total, 0, &error)) == NULL) {
			fprintf(stdout, "%s: json_loadb - %s\n", __func__, error.text);
			goto out;
		}

		if (json_unpack(jmsg, "{s:s, s:s}",
		    "cert", &cert, "cacert", &cacert) < 0) {
			fprintf(stdout, "%s: json_unpack\n", __func__);
			goto out;
		}

		ndb_network_add(netcf, cert, cacert);
	}

out:
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	json_decref(jmsg);

	if (uri != NULL)
		evhttp_uri_free(uri);
	evhttp_clear_headers(&headers);

	free(resp);
	json_decref(jresp);
	free(certreq_pem);
	return (0);
}

RB_GENERATE_STATIC(network_tree, network, entry, network_cmp);
