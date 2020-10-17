/*
 *
 * Copyright 2020 The University of Queensland
 * Author: Alex Wilson <alex@uq.edu.au>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>

#include "http-parser/http_parser.h"
#include "log.h"
#include "metrics.h"

const int BACKLOG = 8;
const size_t BUFLEN = 2048;
const size_t REQ_TIMEOUT = 30;

enum response_type {
	RESP_NOT_FOUND = 0,
	RESP_METRICS
};

struct req {
	struct req *next;
	struct req *prev;
	struct sockaddr_in raddr;
	size_t pfdnum;
	time_t last_active;
	struct http_parser *parser;
	enum response_type resp;
	int sock;
	int done;
	FILE *wf;
	struct registry *registry;
};

static int on_url(http_parser *, const char *, size_t);
static int on_header_field(http_parser *, const char *, size_t);
static int on_header_value(http_parser *, const char *, size_t);
static int on_headers_complete(http_parser *);
static int on_message_complete(http_parser *);

static char *stats_buf = NULL;
static size_t stats_buf_sz = 0;

static struct req *reqs = NULL;

static void
free_req(struct req *req)
{
	if (req->prev == NULL)
		reqs = req->next;
	if (req->prev != NULL)
		req->prev->next = req->next;
	if (req->next != NULL)
		req->next->prev = req->prev;
	fclose(req->wf);
	free(req->parser);
	free(req);
}

static void
usage(const char *arg0)
{
	fprintf(stderr, "usage: %s [-f] [-l logfile] [-p port]\n", arg0);
	fprintf(stderr, "listens for prometheus http requests\n");
}

extern FILE *logfile;

int
main(int argc, char *argv[])
{
	const char *optstring = "p:fl:P";
	uint16_t port = 27600;
	int daemon = 1;
	/* XXX: default on after new pledges are in base */
	int do_pledge = 0;

	int lsock, sock;
	struct sockaddr_in laddr, raddr;
	size_t plen, blen;
	ssize_t recvd;
	socklen_t slen;
	http_parser_settings settings;
	char *buf;
	struct registry *registry;
	int c, rc;
	unsigned long int parsed;
	char *p;
	pid_t kid;
	struct pollfd *pfds;
	size_t npfds, upfds;
	struct req *req, *nreq;
	http_parser *parser;

	logfile = stdout;

	tzset();

	while ((c = getopt(argc, argv, optstring)) != -1) {
		switch (c) {
		case 'P':
			do_pledge = 1;
			break;
		case 'p':
			errno = 0;
			parsed = strtoul(optarg, &p, 0);
			if (errno != 0 || *p != '\0') {
				errx(EXIT_USAGE, "invalid argument for "
				    "-p: '%s'", optarg);
			}
			if (parsed >= (1 << 16)) {
				errx(EXIT_USAGE, "invalid argument for "
				    "-p: '%s' (too high)", optarg);
			}
			port = parsed;
			break;
		case 'f':
			daemon = 0;
			break;
		case 'l':
			logfile = fopen(optarg, "a");
			if (logfile == NULL)
				err(EXIT_USAGE, "open('%s')", optarg);
			break;
		default:
			usage(argv[0]);
			return (EXIT_USAGE);
		}
	}

	if (daemon) {
		kid = fork();
		if (kid < 0) {
			err(EXIT_ERROR, "fork");
		} else if (kid > 0) {
			return (0);
		}
		umask(0);
		if (setsid() < 0)
			tserr(EXIT_ERROR, "setsid");
		chdir("/");

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}

	registry = registry_build();

	bzero(&settings, sizeof (settings));
	settings.on_headers_complete = on_headers_complete;
	settings.on_message_complete = on_message_complete;
	settings.on_url = on_url;
	settings.on_header_field = on_header_field;
	settings.on_header_value = on_header_value;

	npfds = 64;
	pfds = calloc(64, sizeof (struct pollfd));
	upfds = 0;

	signal(SIGPIPE, SIG_IGN);

	lsock = socket(AF_INET, SOCK_STREAM, 0);
	if (lsock < 0)
		tserr(EXIT_SOCKERR, "socket()");

	if (setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR,
	    &(int){ 1 }, sizeof (int))) {
		tserr(EXIT_SOCKERR, "setsockopt(SO_REUSEADDR)");
	}

	bzero(&laddr, sizeof (laddr));
	laddr.sin_family = AF_INET;
	laddr.sin_port = htons(port);

	if (bind(lsock, (struct sockaddr *)&laddr, sizeof (laddr)))
		tserr(EXIT_SOCKERR, "bind(%d)", port);

	if (listen(lsock, BACKLOG))
		tserr(EXIT_SOCKERR, "listen(%d)", port);

	blen = BUFLEN;
	buf = malloc(blen);
	if (buf == NULL)
		tserr(EXIT_MEMORY, "malloc(%zd)", blen);

	pfds[upfds].fd = lsock;
	pfds[upfds].events = POLLIN | POLLHUP;
	++upfds;

	tslog("listening on port %d", port);

	if (do_pledge) {
		if (pledge("stdio inet route vminfo pf", NULL) != 0) {
			tslog("pledge() failed: %s", strerror(errno));
			tserr(EXIT_ERROR, "pledge()");
		}
	}

	while (1) {
		time_t now;

		rc = poll(pfds, upfds, 1000);

		if (rc < 0) {
			if (errno == EINTR)
				continue;
			tserr(EXIT_ERROR, "poll");
		}

		now = time(NULL);

		if (rc == 0)
			goto check_timeouts;

		if (pfds[0].revents & POLLIN) {
			slen = sizeof (raddr);
			sock = accept(lsock, (struct sockaddr *)&raddr, &slen);
			if (sock < 0)
				tserr(EXIT_SOCKERR, "accept()");

			tslog("accepted connection from %s",
			    inet_ntoa(raddr.sin_addr));

			req = calloc(1, sizeof (struct req));
			if (req == NULL) {
				tserr(EXIT_MEMORY, "calloc(%zd)",
				    sizeof (struct req));
			}
			parser = calloc(1, sizeof (http_parser));
			if (parser == NULL) {
				tserr(EXIT_MEMORY, "calloc(%zd)",
				    sizeof (http_parser));
			}

			http_parser_init(parser, HTTP_REQUEST);
			parser->data = req;

			req->sock = sock;
			req->raddr = raddr;
			req->registry = registry;
			req->parser = parser;
			req->last_active = now;

			req->wf = fdopen(sock, "w");
			if (req->wf == NULL) {
				tslog("failed to fdopen socket: %s",
				    strerror(errno));
				close(req->sock);
				free(parser);
				free(req);
				continue;
			}

			req->next = reqs;
			if (reqs != NULL)
				reqs->prev = req;
			reqs = req;
		}

		for (req = reqs; req != NULL; req = nreq) {
			nreq = req->next;

			if (req->pfdnum == 0)
				continue;

			if (pfds[req->pfdnum].revents & (POLLERR|POLLNVAL)) {
				tslog("connection error, discarding");
				free_req(req);
				continue;
			}
			if (pfds[req->pfdnum].revents & POLLIN) {
				recvd = recv(sock, buf, blen, 0);
				if (recvd < 0) {
					tslog("error recv: %s",
					    strerror(errno));
					free_req(req);
					continue;
				}

				req->last_active = now;

				plen = http_parser_execute(req->parser,
				    &settings, buf, recvd);
				if (parser->upgrade) {
					/* we don't use this, so just close */
					tslog("upgrade?");
					free_req(req);
					continue;
				} else if (plen != recvd) {
					tslog("http-parser gave error, close");
					free_req(req);
					continue;
				}

				if (req->done) {
					free_req(req);
					continue;
				}
			}
			if (pfds[req->pfdnum].revents & POLLHUP) {
				tslog("connection closed!");
				free_req(req);
				continue;
			}
		}
check_timeouts:
		for (req = reqs; req != NULL; req = nreq) {
			size_t delta;
			nreq = req->next;
			delta = now - req->last_active;
			if (delta > REQ_TIMEOUT) {
				tslog("conn idle for %zd sec, closing", delta);
				free_req(req);
			}
		}

		upfds = 0;
		pfds[upfds].fd = lsock;
		pfds[upfds].events = POLLIN | POLLHUP;
		pfds[upfds].revents = 0;
		++upfds;

		for (req = reqs; req != NULL && upfds < npfds; req = nreq) {
			nreq = req->next;

			req->pfdnum = upfds;
			pfds[upfds].fd = req->sock;
			pfds[upfds].events = POLLIN | POLLHUP;
			pfds[upfds].revents = 0;
			++upfds;
		}
	}

	free(buf);
	free(pfds);

	free(stats_buf);
	stats_buf_sz = 0;

	registry_free(registry);
	return (0);
}

static int
on_url(http_parser *parser, const char *url, size_t ulen)
{
	struct req *req = parser->data;
	if (parser->method == HTTP_GET &&
	    ulen >= strlen("/metrics") &&
	    strncmp(url, "/metrics", strlen("/metrics")) == 0) {
		req->resp = RESP_METRICS;
	}
        if (parser->method == HTTP_GET &&
            ulen >= strlen("/stopme") &&
            strncmp(url, "/stopme", strlen("/stopme")) == 0) {
		exit(0);
        }
	return (0);
}

static int
on_header_field(http_parser *parser, const char *hdrname, size_t hlen)
{
	//struct req *req = parser->data;
	return (0);
}

static int
on_header_value(http_parser *parser, const char *hdrval, size_t vlen)
{
	//struct req *req = parser->data;
	return (0);
}

static int
on_headers_complete(http_parser *parser)
{
	//struct req *req = parser->data;
	return (0);
}

static void
send_err(http_parser *parser, enum http_status status)
{
	struct req *req = parser->data;
	tslog("sending http %d", status);
	fprintf(req->wf, "HTTP/%d.%d %d %s\r\n", parser->http_major,
	    parser->http_minor, status, http_status_str(status));
	fprintf(req->wf, "Server: obsd-prom-exporter\r\n");
	fprintf(req->wf, "Connection: close\r\n");
	fprintf(req->wf, "\r\n");
	fflush(req->wf);
	req->done = 1;
}

static int
on_message_complete(http_parser *parser)
{
	struct req *req = parser->data;
	FILE *mf;
	off_t off;
	int r;

	if (req->resp == RESP_NOT_FOUND) {
		send_err(parser, 404);
		return (0);
	}

	if (stats_buf == NULL) {
		stats_buf_sz = 256*1024;
		stats_buf = malloc(stats_buf_sz);
	}
	if (stats_buf == NULL) {
		tslog("failed to allocate metrics buffer");
		send_err(parser, 500);
		return (0);
	}
	mf = fmemopen(stats_buf, stats_buf_sz, "w");
	if (mf == NULL) {
		tslog("fmemopen failed: %s", strerror(errno));
		send_err(parser, 500);
		return (0);
	}

	tslog("generating metrics...");
	r = registry_collect(req->registry);
	if (r != 0) {
		tslog("metric collection failed: %s", strerror(r));
		send_err(parser, 500);
		return (0);
	}
	print_registry(mf, req->registry);
	fflush(mf);
	off = ftell(mf);
	if (off < 0) {
		send_err(parser, 500);
		return (0);
	}
	fclose(mf);
	tslog("done, sending %lld bytes", off);

	fprintf(req->wf, "HTTP/%d.%d %d %s\r\n", parser->http_major,
	    parser->http_minor, 200, http_status_str(200));
	fprintf(req->wf, "Server: obsd-prom-exporter\r\n");
	fprintf(req->wf, "Content-Type: "
	    "text/plain; version=0.0.4; charset=utf-8\r\n");
	fprintf(req->wf, "Content-Length: %lld\r\n", off);
	fprintf(req->wf, "Connection: close\r\n");
	fprintf(req->wf, "\r\n");
	fflush(req->wf);

	fprintf(req->wf, "%s", stats_buf);
	fflush(req->wf);

	req->done = 1;

	return (0);
}
