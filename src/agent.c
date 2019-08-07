#ifdef USE_THREAD
	#include <pthread.h>
#endif

#include <curl/curl.h>

#include <signal.h>
#include <event2/event.h>

#include "log.h"
#include "agent.h"

static void		sighandler(int, short, void *);
#ifdef USE_THREAD
static void		_agent_start(void *arg);
#endif

static struct event	*ev_sigint = NULL;
static struct event	*ev_sigterm = NULL;
#ifdef USE_THREAD
static pthread_t	 thread_start;
static const char	 *netname;
#endif

struct agent_event	*agent_cb = NULL;
struct event_base	*ev_base = NULL;

void
sighandler(int signal, short events, void *arg)
{
	(void)signal;
	(void)events;

	event_base_loopbreak(arg);
}

void
agent_log_cb(const char *msg)
{
	if (agent_cb && agent_cb->log)
		agent_cb->log(msg);
}

void
agent_init_cb(void)
{
	agent_cb = malloc(sizeof(struct agent_event));
	agent_cb->connected = NULL;
	agent_cb->disconnected = NULL;
	agent_cb->disconnect = NULL;
	agent_cb->log = NULL;

	log_setcb(agent_log_cb);
}

void
agent_init(void)
{
#ifdef _WIN32
        WORD wVersionRequested = MAKEWORD(1,1);
        WSADATA wsaData;
        WSAStartup(wVersionRequested, &wsaData);
#endif

#ifndef _WIN32
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		fprintf(stderr, "%s: signal", __func__);
		exit(-1);
	}
#endif

	if ((ev_base = event_base_new()) == NULL) {
		fprintf(stderr, "%s: event_init\n", __func__);
		exit(-1);
	}

	if ((ev_sigint = evsignal_new(ev_base, SIGINT, sighandler, ev_base))
	    == NULL) {
		fprintf(stderr, "%s: evsignal_new\n", __func__);
		exit(-1);
	}
	event_add(ev_sigint, NULL);

	if ((ev_sigterm = evsignal_new(ev_base, SIGTERM, sighandler, ev_base))
	    == NULL) {
		fprintf(stderr, "%s: evsignal_new\n", __func__);
		exit(-1);
	}
	event_add(ev_sigterm, NULL);

	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();

	/* In windows, this will init the winsock stuff */
	curl_global_init(CURL_GLOBAL_ALL);
}

void
agent_fini(void)
{
	curl_global_cleanup();

	if (ev_sigint != NULL)
		event_free(ev_sigint);
	if (ev_sigterm != NULL)
		event_free(ev_sigterm);
	if (ev_base != NULL)
		event_base_free(ev_base);
}

void
agent_start(const char *name)
{
	agent_init();
	control_init(name);
	event_base_dispatch(ev_base);
}

#ifdef USE_THREAD
void _agent_start(void *arg)
{
	const char	*name = arg;
	agent_start(name);
}

void
agent_thread_fini(void)
{
	event_base_loopbreak(ev_base);
	pthread_join(thread_start, NULL);
	agent_fini();
	free(netname);
}

void
agent_thread_start(const char *name)
{
	struct sched_param	param;
	/* When launching the connection thread via the Qt GUI,
	 * the priority is not high enough and the thread can
	 * starve. Making the agent disconnect simultanously from
	 * the controller and the switch because of a timeout with
	 * the keepalive. So the policy is set to SCHED_RR to
	 * avoid this situation.
	 */
	int			policy = SCHED_RR;

	netname = strdup(name);

	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	memset(&param, 0, sizeof(param));
	param.sched_priority = sched_get_priority_min(policy);

	pthread_create(&thread_start, NULL, _agent_start, (void*)netname);
	pthread_setschedparam(thread_start, policy, &param);

	pthread_attr_destroy(&attr);
}
#endif
