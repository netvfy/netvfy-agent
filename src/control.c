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

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <syslog-compat.h>
#else
	#include <sys/types.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <arpa/inet.h>
	#include <sys/socket.h>
	#include <netdb.h>
	#include <unistd.h>
	#include <syslog.h>
#endif

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <jansson.h>

#include <pki.h>
#include <log.h>
#include <tapcfg.h>
#include <sysname.h>

#include "agent.h"

struct vlink {
	passport_t		*passport;
	tapcfg_t		*tapcfg;
	struct tls_peer		*peer;
	struct event		*ev_reconnect;
	struct event		*ev_keepalive;
	struct event		*ev_readagain;
	int			 tapfd;
	char			*addr;
	char			*port;
	char			*api_srv;
	char			*netname;
};

struct tls_peer {
	SSL			*ssl;
	SSL_CTX			*ctx;
	struct bufferevent	*bev;
	struct tapif		*tapif;
	struct vlink		*vlink;
	int			 sock;
	int			 status;
};

static char			*local_ipaddr();
static int			 xmit_nodeinfo(struct tls_peer *);

static void			 tls_peer_free(struct tls_peer *);
static struct tls_peer		*tls_peer_new();

static int			 cert_verify_cb(int, X509_STORE_CTX *);
static void			 info_cb(const SSL *, int, int);
static SSL_CTX			*ctx_init();

static size_t			 fetch_netinfos_cb(void *, size_t, size_t, void *);
static int			 fetch_netinfos(struct vlink *);

static void			 vlink_free(struct vlink *);
static int			 vlink_connect(struct tls_peer *, struct vlink *);
static void			 vlink_keepalive(evutil_socket_t, short, void *);
static void			 vlink_readagain(evutil_socket_t, short, void *);
static void			 vlink_reconnect(evutil_socket_t, short, void *);
static void			 vlink_reset(struct vlink *vlink);

static void			 peer_event_cb(struct bufferevent *, short, void *);
static void			 peer_read_cb(struct bufferevent *, void *);

static struct vlink	*vlink;

char *
local_ipaddr()
{
#ifdef _WIN32
	WORD	wVersionRequested = MAKEWORD(1,1);
	WSADATA wsaData;
#endif
	struct addrinfo		*serv_info;
	struct sockaddr_in	 name;
	int			 sock;
	const char		*addr_ptr;
	char			*listen_addr = "dynvpn.com";
	char			*port = "9092";
	char			 local_ip[16];

#ifdef _WIN32
	/* init Winsocket */
	WSAStartup(wVersionRequested, &wsaData);
#endif
	// XXX handler errors
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	getaddrinfo(listen_addr, port, NULL, &serv_info);
	connect(sock, serv_info->ai_addr, serv_info->ai_addrlen);
	freeaddrinfo(serv_info);

	socklen_t namelen = sizeof(name);
	getsockname(sock, (struct sockaddr *)&name, &namelen);

#ifdef _WIN32
	closesocket(sock);
	WSACleanup();
#else
	close(sock);
#endif

	if ((addr_ptr = evutil_inet_ntop(AF_INET, &name.sin_addr, local_ip,
	 INET_ADDRSTRLEN)) == NULL)
		return (NULL);

	return (strdup(local_ip));
}

int
xmit_nodeinfo(struct tls_peer *p)
{
	json_t		*jmsg = NULL;
	int		 ret;
	int		 lladdr_len;
	const char	*lladdr;
	char		 lladdr_str[18];
	char		*lipaddr = NULL;
	char		*sysname = NULL;
	char		*msg = NULL;

	ret = -1;

	if ((lladdr = tapcfg_iface_get_hwaddr(p->vlink->tapcfg, &lladdr_len))
	    == NULL) {
		log_warnx("%s: tapcfg_iface_get_hwaddr", __func__);
		goto error;
	}

	if ((sysname = get_sysname()) == NULL) {
		log_warnx("%s: get_sysname", __func__);
		goto error;
	}

	if ((lipaddr = local_ipaddr()) == NULL) {
		log_warnx("%s: local_ipaddr", __func__);
		goto error;
	}

	snprintf(lladdr_str, sizeof(lladdr_str),
	    "%02x:%02x:%02x:%02x:%02x:%02x",
            ((uint8_t *)lladdr)[0],
            ((uint8_t *)lladdr)[1],
            ((uint8_t *)lladdr)[2],
            ((uint8_t *)lladdr)[3],
            ((uint8_t *)lladdr)[4],
            ((uint8_t *)lladdr)[5]);

	if ((jmsg = json_pack("{s:s,s:s,s:s,s:s,s:s}",
	    "action", "nodeinfo",
	    "local_ipaddr", lipaddr,
	    "sysname", sysname,
	    "lladdr", lladdr_str,
	    "agent_version", NVAGENT_VERSION)) == NULL) {
		log_warnx("%s: json_pack", __func__);
		goto error;
	}

	if ((msg = json_dumps(jmsg, 0)) == NULL) {
		log_warnx("%s: json_dumps", __func__);
		goto error;
	}

	// XXX use buffer and then one write
	if (bufferevent_write(p->bev, msg, strlen(msg)) != 0) {
		log_warnx("%s: bufferevent_write", __func__);
		goto error;
	}

	if (bufferevent_write(p->bev, "\n", strlen("\n")) != 0) {
		log_warnx("%s: bufferevent_write", __func__);
		goto error;
	}

	ret = 0;

error:
	json_decref(jmsg);
	free(msg);
	free(lipaddr);
	free(sysname);

	return (ret);
}

void
tls_peer_free(struct tls_peer *p)
{
	if (p == NULL)
		return;

	if (p->vlink != NULL)
		p->vlink->peer = NULL;

	if (p->bev != NULL)
		bufferevent_free(p->bev);
	else
		SSL_free(p->ssl);

	if (p->ctx != NULL) {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || \
    (defined (LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2070000fL)
		// Remove the reference to the store, otherwise OpenSSL will try to free it.
		// OpenSSL 1.0.1 doesn't have the function X509_STORE_up_ref().
		p->ctx->cert_store = NULL;
#endif
		SSL_CTX_free(p->ctx);
	}

	free(p);

	return;
}

struct tls_peer *
tls_peer_new()
{
	struct tls_peer		*p;

	if ((p = malloc(sizeof(struct tls_peer))) == NULL) {
		log_warn("%s: malloc", __func__);
		goto error;
	}
	p->ssl = NULL;
	p->ctx = NULL;
	p->bev = NULL;
	p->vlink = NULL;
	p->status = 0;

	if ((p->ctx = ctx_init()) == NULL) {
		log_warnx("%s: ctx_init", __func__);
		goto error;
	}

	if ((p->ssl = SSL_new(p->ctx)) == NULL ||
	    SSL_set_app_data(p->ssl, p) != 1) {
		log_warnx("%s: SSL_new", __func__);
		goto error;
	}

	return (p);

error:
	tls_peer_free(p);
	return (NULL);
}

int
cert_verify_cb(int ok, X509_STORE_CTX *store)
{
	X509		*cert;
	X509_NAME	*name;
	char		 buf[256];

	cert = X509_STORE_CTX_get_current_cert(store);
	name = X509_get_subject_name(cert);
	X509_NAME_get_text_by_NID(name, NID_commonName, buf, 256);

	return (ok);
}

void
info_cb(const SSL *ssl, int where, int ret)
{
	(void)ssl;
	(void)ret;

	if ((where & SSL_CB_HANDSHAKE_DONE) == 0)
		return;
}

SSL_CTX *
ctx_init()
{
	EC_KEY		*ecdh = NULL;
	SSL_CTX		*ctx = NULL;
	int		 err = -1;

	if ((ctx = SSL_CTX_new(TLSv1_2_client_method())) == NULL) {
		log_warnx("%s: SSL_CTX_new", __func__);
		goto error;
	}

	if (SSL_CTX_set_cipher_list(ctx,
	    "ECDHE-ECDSA-CHACHA20-POLY1305,"
	    "ECDHE-ECDSA-AES256-GCM-SHA384") != 1) {
		log_warnx("%s: SSL_CTX_set_cipher", __func__);
		goto error;
	}

	if ((ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1)) == NULL) {
		log_warnx("%s: EC_KEY_new_by_curve", __func__);
		goto error;
	}

	if (SSL_CTX_set_tmp_ecdh(ctx, ecdh) != 1) {
		log_warnx("%s: SSL_CTX_set_tmp_ecdh", __func__);
		goto error;
	}

	SSL_CTX_set_verify(ctx,
	    SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, cert_verify_cb);

	SSL_CTX_set_info_callback(ctx, info_cb);

	err = 0;

error:
	if (err < 0) {
		SSL_CTX_free(ctx);
		ctx = NULL;
	}
	EC_KEY_free(ecdh);
	return (ctx);
}

size_t
fetch_netinfos_cb(void *ptr, size_t size, size_t nmemb, void *arg)
{
	json_t		*jmsg = NULL;
	json_t		*jnetinfos;
	json_t		*jnetinfo;
	json_error_t	 error;
	struct vlink	*v = arg;
	size_t		 i, array_size;
	size_t		 ret = -1;
	const char	*family;
	const char	*addr;
	const char	*port;

	if ((jmsg = json_loadb(ptr, size*nmemb, 0, &error)) == NULL) {
		log_warnx("%s: json_loadb: %s\n", __func__, error.text);
		goto err;
	}

	if ((jnetinfos = json_object_get(jmsg, "netinfos")) == NULL) {
		log_warnx("%s: json_object_get", __func__);
		goto err;
	}

	if ((array_size = json_array_size(jnetinfos)) == 0) {
		log_warnx("%s: json_array_size", __func__);
		goto err;
	}

	for (i = 0; i < array_size; i++) {

		if ((jnetinfo = json_array_get(jnetinfos, i)) == NULL) {
			log_warnx("%s: json_array_get", __func__);
			goto err;
		}

		if (json_unpack(jnetinfo, "{s:s, s:s, s:s}",
		    "family", &family, "addr", &addr, "port", &port) < 0) {
			log_warnx("%s: json_unpack", __func__);
			goto err;
		}

		if (strcmp(family, "inet") == 0) {
			if ((v->addr = strdup(addr)) == NULL) {
				log_warn("%s: strdup", __func__);
				goto err;
			}

			if ((vlink->port = strdup(port)) == NULL) {
				log_warn("%s: strdup", __func__);
				goto err;
			}
		}
	}

	ret = size*nmemb;
err:
	json_decref(jmsg);

	return (ret);
}

int
fetch_netinfos(struct vlink *v)
{
	json_t			*jquery = NULL;
	CURL			*curl;
	CURLcode		 res;
	struct curl_slist	*req_headers = NULL;
	int			 ret = -1;
	char			 url[256];
	char			*payload = NULL;

	jquery = json_object();
	json_object_set_new(jquery, "network",
	    json_string(v->passport->certinfo->network_uid));
	json_object_set_new(jquery, "node",
	    json_string(v->passport->certinfo->node_uid));
	payload = json_dumps(jquery, 0);

	snprintf(url, sizeof(url), "https://%s/v1/netinfos", v->api_srv);

	if ((curl = curl_easy_init()) == NULL) {
		log_warnx("%s: curl_easy_init", __func__);
		goto err;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fetch_netinfos_cb);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, v);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

#ifdef _WIN32
	curl_easy_setopt(curl, CURLOPT_CAINFO, "curl-ca-bundle.crt");
#endif

	req_headers = curl_slist_append(req_headers, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, req_headers);

	/* Now specify the POST data */
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);

	if ((res = curl_easy_perform(curl)) != CURLE_OK) {
		log_warnx("%s: curl_easy_perform: %s\n",
		    __func__, curl_easy_strerror(res));
		goto err;
	}

	curl_easy_cleanup(curl);

	ret = 0;
err:
	curl_slist_free_all(req_headers);

	free(payload);
	json_decref(jquery);

	return (ret);
}

void
vlink_free(struct vlink *v)
{
	if (v == NULL)
		return;

	pki_passport_destroy(v->passport);
	tls_peer_free(v->peer);
#ifndef __APPLE__
	tapcfg_destroy(v->tapcfg);
#endif

	if (v->ev_reconnect != NULL)
		event_free(v->ev_reconnect);
	if (v->ev_keepalive != NULL)
		event_free(v->ev_keepalive);
	if (v->ev_readagain != NULL)
	event_free(v->ev_readagain);

	free(v->addr);
	free(v->port);
	free(v->netname);

	free(v);
}

int
vlink_connect(struct tls_peer *p, struct vlink *v)
{
	EC_KEY			*ecdh = NULL;
	struct addrinfo		 hints;
	struct addrinfo		*res = NULL;
	struct timeval		 tv;
	int			 ret;
	int			 err = -1;
	int			 flag;

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	if (event_add(v->ev_reconnect, &tv) < 0) {
		log_warn("%s: event_add", __func__);
		goto out;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ((ret = fetch_netinfos(v)) < 0) {
		log_warnx("%s: fetch_netinfos", __func__);
		goto out;
	}

	if (v->addr == NULL || v->port == NULL) {
		log_warnx("%s: invalid addr and port", __func__);
		goto out;
	}

	if ((ret = getaddrinfo(v->addr, v->port, &hints, &res)) < 0) {
		log_warnx("%s: getaddrinfo %s", __func__, gai_strerror(ret));
		goto out;
	}

	if ((p->sock = socket(res->ai_family, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		log_warnx("%s: socket", __func__);
		goto out;
	}

#ifndef WIN32
	flag = 1;
	if (setsockopt(p->sock, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) < 0 ||
	    setsockopt(p->sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
		log_warn("%s: setsockopt", __func__);
		goto out;
	}
#endif

	if (evutil_make_socket_nonblocking(p->sock) < 0) {
		log_warnx("%s: evutil_make_socket_nonblocking", __func__);
		goto out;
	}

	SSL_CTX_set_cert_store(p->ctx, v->passport->cacert_store);
	X509_STORE_up_ref(v->passport->cacert_store);

	SSL_set_SSL_CTX(p->ssl, p->ctx);

	if ((SSL_use_certificate(p->ssl, v->passport->certificate)) != 1) {
		log_warnx("%s: SSL_CTX_use_certificate", __func__);
		goto out;
	}

	if ((SSL_use_PrivateKey(p->ssl, v->passport->keyring)) != 1) {
		log_warnx("%s: SSL_CTX_use_PrivateKey", __func__);
		goto out;
	}

	SSL_set_tlsext_host_name(p->ssl, vlink->passport->certinfo->network_uid);

	if ((p->bev = bufferevent_openssl_socket_new(ev_base, p->sock, p->ssl,
	    BUFFEREVENT_SSL_CONNECTING, BEV_OPT_CLOSE_ON_FREE)) == NULL) {
		log_warnx("%s: bufferevent_openssl_socket_new", __func__);
		goto out;
	}

	bufferevent_setcb(p->bev, peer_read_cb, NULL, peer_event_cb, p);
	bufferevent_enable(p->bev, EV_READ | EV_WRITE);

	if (bufferevent_socket_connect(p->bev, res->ai_addr, res->ai_addrlen) < 0) {
		log_warnx("%s: bufferevent_socket_connected", __func__);
		goto out;
	}

	err = 0;

out:
	EC_KEY_free(ecdh);
	freeaddrinfo(res);

	return (err);

}

void
vlink_keepalive(evutil_socket_t fd, short event, void *arg)
{
	struct vlink	*v = arg;
	const char	*k = "{\"action\": \"keepalive\"}\n";
	(void)fd;
	(void)event;

	bufferevent_write(v->peer->bev, k, strlen(k));
}

void
vlink_readagain(evutil_socket_t fd, short event, void *arg)
{
	(void)fd;
	(void)event;
	(void)arg;
}

void
vlink_reconnect(evutil_socket_t fd, short event, void *arg)
{
	struct vlink	*vlink = arg;
	(void)fd;
	(void)event;

	if (vlink->peer != NULL)
		tls_peer_free(vlink->peer);

	if ((vlink->peer = tls_peer_new()) == NULL) {
		log_warnx("%s: tls_peer_new", __func__);
		goto error;
	}
	vlink->peer->vlink = vlink;

	if (vlink_connect(vlink->peer, vlink) < 0) {
		log_warnx("%s: vlink_connect", __func__);
		goto error;
	}

	return;

error:
	vlink_reset(vlink);
	return;
}

void
vlink_reset(struct vlink *vlink)
{
	struct timeval	wait_sec = {5, 0};

	log_info("connecting to the controller...");
	event_del(vlink->ev_keepalive);
	event_del(vlink->ev_readagain);
	event_del(vlink->ev_reconnect);

	if (vlink->peer) {
		vlink->peer->status = 0;

		if (vlink->peer->bev) {
			bufferevent_set_timeouts(vlink->peer->bev, NULL, NULL);
			bufferevent_disable(vlink->peer->bev, EV_READ | EV_WRITE);
		}
	}

	if (event_base_once(ev_base, -1, EV_TIMEOUT,
	    vlink_reconnect, vlink, &wait_sec) < 0)
		log_warnx("%s: event_base_once", __func__);
}

void
peer_read_cb(struct bufferevent *bev, void *arg)
{
	json_error_t		 error;
	json_t			*jmsg = NULL;
	struct tls_peer		*p = arg;
	size_t			 n_read_out;
	const char		*action;
	const char		*ipaddr;
	const char		*vswitch_addr;
	const char		*vswitch_port;
	char			*msg = NULL;

	while (evbuffer_get_length(bufferevent_get_input(bev)) > 0) {

		if ((msg = evbuffer_readln(bufferevent_get_input(bev),
		    &n_read_out, EVBUFFER_EOL_LF)) == NULL) {
			return;
		}

		if ((jmsg = json_loadb(msg, n_read_out, 0, &error)) == NULL) {
			log_warnx("%s: json_loadb", __func__);
			goto error;
		}

		if (json_unpack(jmsg, "{s:s}", "action", &action) < 0) {
			log_warnx("%s: json_unpack action", __func__);
			goto error;
		}

		if (strcmp(action, "netinfos") == 0) {
			if (json_unpack(jmsg, "{s:s, s:s, s:s}",
			    "addr", &vswitch_addr,
			    "port", &vswitch_port,
			    "ipaddr", &ipaddr) < 0) {
				log_warnx("%s: json_unpack ipaddr", __func__);
				goto error;
			}

			switch_fini();
			switch_init(p->vlink->tapcfg, p->vlink->tapfd, vswitch_addr, vswitch_port, ipaddr, p->vlink->netname);
		}

		json_decref(jmsg);
		free(msg);
	}

	return;

error:
	json_decref(jmsg);
	free(msg);

	vlink_reset(p->vlink);
	return;
}

void
peer_event_cb(struct bufferevent *bev, short events, void *arg)
{
	struct tls_peer	*p = arg;
	struct timeval	 tv;
	unsigned long	 e;

	if (events & BEV_EVENT_CONNECTED) {

		log_info("connected!");

		event_del(p->vlink->ev_reconnect);

		tv.tv_sec = 5;
		tv.tv_usec = 0;
		bufferevent_set_timeouts(p->bev, &tv, NULL);

		tv.tv_sec = 1;
		tv.tv_usec = 0;
		if (event_add(p->vlink->ev_keepalive, &tv) < 0) {
			log_warn("%s: event_add", __func__);
		}

		if (xmit_nodeinfo(p) < 0)
			goto error;

	} else if (events & (BEV_EVENT_TIMEOUT | BEV_EVENT_EOF | BEV_EVENT_ERROR)) {

		log_warnx("%s: disconnected: timeout(%d) EOF(%d) ERR(%d:%s)", __func__,
		    events & BEV_EVENT_TIMEOUT,
		    events & BEV_EVENT_EOF,
		    events & BEV_EVENT_ERROR, evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
		 
		while ((e = bufferevent_get_openssl_error(bev)) > 0) {
			log_warnx("%s: TLS error: %s", __func__,
			    ERR_reason_error_string(e));
		}

		goto error;
	}

	return;

error:
	vlink_reset(p->vlink);
	return;
}

void tapcfg_log(int level, char *msg)
{
	log_warnx("%s: %s", __func__, msg);
}

const char* strcat_copy(const char *str1, const char *str2);

int
control_init(const char *network_name)
{
	struct network		*netcf = NULL;

	if ((netcf = ndb_network(network_name)) == NULL) {
		log_warnx("%s: the network doesn't exist: %s",
		    __func__, network_name);
		goto error;
	}

	if ((vlink = malloc(sizeof(struct vlink))) == NULL) {
		log_warnx("%s: malloc", __func__);
		goto error;
	}
	vlink->passport = NULL;
	vlink->tapcfg = NULL;
	vlink->peer = NULL;
	vlink->ev_reconnect = NULL;
	vlink->ev_keepalive = NULL;
	vlink->ev_readagain = NULL;
	vlink->addr = NULL;
	vlink->port = NULL;
	vlink->api_srv = NULL;
	vlink->netname = NULL;

	if ((vlink->tapcfg = tapcfg_init()) == NULL) {
		log_warnx("%s: tapcfg_init", __func__);
		goto error;
	}

	tapcfg_set_log_callback(vlink->tapcfg, tapcfg_log);

	if ((vlink->netname = strdup(network_name)) == NULL) {
		log_warn("%s: strdup", __func__);
		goto error;
	}


	const char* ifname = strcat_copy(vlink->netname, "0");
	if ((vlink->tapfd = tapcfg_start(vlink->tapcfg, ifname, 1)) < 0) {
		log_warnx("%s: tapcfg_start", __func__);
		goto error;
	}

	vlink->api_srv = netcf->api_srv;

	if ((vlink->passport =
	    pki_passport_load_from_memory(netcf->cert, netcf->pvkey, netcf->cacert)) == NULL) {
		log_warnx("%s: pki_passport_load_from_memory", __func__);
		goto error;
	}

	if ((vlink->ev_reconnect = event_new(ev_base, 0,
	    EV_TIMEOUT, vlink_reconnect, vlink)) == NULL)
		log_warnx("%s: event_new", __func__);

	if ((vlink->ev_keepalive = event_new(ev_base, 0,
	    EV_TIMEOUT | EV_PERSIST, vlink_keepalive, vlink)) == NULL)
		log_warnx("%s: event_new", __func__);

	if ((vlink->ev_readagain = event_new(ev_base, 0,
	    EV_TIMEOUT, vlink_readagain, vlink)) == NULL)
		log_warnx("%s: event_new", __func__);

	vlink_reset(vlink);

	return (0);

error:
	return (-1);
}

void
control_fini(void)
{
	vlink_free(vlink);
}

const char* strcat_copy(const char *str1, const char *str2) {
	int str1_len, str2_len;
	char *new_str;

	/* null check */

	str1_len = strlen(str1);
	str2_len = strlen(str2);

	new_str = malloc(str1_len + str2_len + 1);

	/* null check */

	memcpy(new_str, str1, str1_len);
	memcpy(new_str + str1_len, str2, str2_len + 1);

	return new_str;
}
