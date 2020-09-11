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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>

#include "http-parser/http_parser.h"
#include "log.h"
#include "metrics.h"

const int BACKLOG = 8;
const size_t BUFLEN = 2048;

enum response_type {
	RESP_NOT_FOUND = 0,
	RESP_METRICS
};

struct req {
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

int
main(int argc, char *argv[])
{
	uint16_t port = 27600;
	int lsock, sock;
	struct sockaddr_in laddr, raddr;
	size_t plen, blen;
	ssize_t recvd;
	socklen_t slen;
	http_parser_settings settings;
	char *buf;
	struct registry *registry;

	registry = registry_build();

	bzero(&settings, sizeof (settings));
	settings.on_headers_complete = on_headers_complete;
	settings.on_message_complete = on_message_complete;
	settings.on_url = on_url;
	settings.on_header_field = on_header_field;
	settings.on_header_value = on_header_value;

	lsock = socket(AF_INET, SOCK_STREAM, 0);
	if (lsock < 0)
		err(EXIT_SOCKERR, "socket()");

	if (setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR,
	    &(int){ 1 }, sizeof (int))) {
		err(EXIT_SOCKERR, "setsockopt(SO_REUSEADDR)");
	}

	bzero(&laddr, sizeof (laddr));
	laddr.sin_family = AF_INET;
	laddr.sin_port = htons(port);

	if (bind(lsock, (struct sockaddr *)&laddr, sizeof (laddr)))
		err(EXIT_SOCKERR, "bind(%d)", port);

	if (listen(lsock, BACKLOG))
		err(EXIT_SOCKERR, "listen(%d)", port);

	blen = BUFLEN;
	buf = malloc(blen);
	if (buf == NULL)
		err(EXIT_MEMORY, "malloc(%zd)", blen);

	tslog("listening on port %d", port);

	while (1) {
		http_parser *parser;
		struct req *req;

		sock = accept(lsock, (struct sockaddr *)&raddr, &slen);
		if (sock < 0)
			err(EXIT_SOCKERR, "accept()");

		tslog("accepted connection from %s", inet_ntoa(raddr.sin_addr));

		req = calloc(1, sizeof (struct req));
		if (req == NULL)
			err(EXIT_MEMORY, "calloc(%zd)", sizeof (struct req));
		parser = calloc(1, sizeof (http_parser));
		if (parser == NULL)
			err(EXIT_MEMORY, "calloc(%zd)", sizeof (http_parser));

		http_parser_init(parser, HTTP_REQUEST);
		parser->data = req;

		req->sock = sock;
		req->registry = registry;

		req->wf = fdopen(sock, "w");
		if (req->wf == NULL) {
			tslog("failed to fdopen socket: %s", strerror(errno));
			close(req->sock);
			break;
		}

		while (!req->done) {
			recvd = recv(sock, buf, blen, 0);
			if (recvd < 0) {
				tslog("error recv: %s", strerror(errno));
				close(sock);
				break;
			}

			plen = http_parser_execute(parser, &settings, buf,
			    recvd);
			if (parser->upgrade) {
				/* we don't use this, so just close */
				tslog("upgrade?");
				close(sock);
				break;
			} else if (plen != recvd) {
				tslog("http-parser gave error, close");
				close(sock);
				break;
			}
		}

		free(parser);
		free(req);
	}
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
	fclose(req->wf);
	req->done = 1;
}

static int
on_message_complete(http_parser *parser)
{
	struct req *req = parser->data;
	FILE *mf;
	char *buf;
	size_t blen = 128*1024;
	off_t off;
	int r;

	if (req->resp == RESP_NOT_FOUND) {
		send_err(parser, 404);
		return (0);
	}

	buf = malloc(blen);
	if (buf == NULL) {
		tslog("failed to allocate metrics buffer");
		send_err(parser, 500);
		return (0);
	}
	mf = fmemopen(buf, blen, "w");
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

	fprintf(req->wf, "%s", buf);
	fflush(req->wf);

	fclose(req->wf);
	req->done = 1;

	return (0);
}
