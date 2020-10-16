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

#include <sys/time.h>
#include <time.h>
#include <stdarg.h>
#include <err.h>

#include "log.h"

FILE *logfile = NULL;

void
tslog(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vtslog(fmt, 0, ap);
	va_end(ap);
}

void
tserr(int status, const char *fmt, ...)
{
	va_list ap, errap;
	va_start(ap, fmt);
	va_copy(errap, ap);
	vtslog(fmt, errno, ap);
	verr(status, fmt, errap);
	va_end(ap);
	va_end(errap);
	exit(status);
}

void
tserrx(int status, const char *fmt, ...)
{
	va_list ap, errap;
	va_start(ap, fmt);
	va_copy(errap, ap);
	vtslog(fmt, 0, ap);
	verrx(status, fmt, errap);
	va_end(ap);
	va_end(errap);
	exit(status);
}

void
vtslog(const char *fmt, int eno, va_list ap)
{
	static char *buf;
	static size_t len = 0;

	char *p;
	size_t rem;
	int w;
	struct timeval tv;
	struct tm *info;

	if (len == 0) {
		len = strlen(fmt) * 2 + 64;
		buf = malloc(len);
		if (buf == NULL)
			err(EXIT_MEMORY, "malloc(%zd)", len);
	}

again:
	buf[0] = '\0';
	rem = len - 1;
	p = buf;

	bzero(&tv, sizeof (tv));
	if (gettimeofday(&tv, NULL))
		err(EXIT_ERROR, "gettimeofday()");
	info = gmtime(&tv.tv_sec);
	if (info == NULL)
		err(EXIT_ERROR, "gmtime");

	w = snprintf(p, rem, "[%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ] ",
	    info->tm_year + 1900, info->tm_mon + 1, info->tm_mday,
	    info->tm_hour, info->tm_min, info->tm_sec, tv.tv_usec / 1000);
	if (w < 0)
		err(EXIT_ERROR, "snprintf");
	if (w >= rem) {
		len *= 2;
		free(buf);
		buf = malloc(len);
		if (buf == NULL)
			err(EXIT_MEMORY, "malloc(%zd)", len);
		goto again;
	}
	p += w;
	rem -= w;

	w = vsnprintf(p, rem, fmt, ap);
	if (w < 0)
		err(EXIT_ERROR, "vsnprintf");
	if (w >= rem) {
		len *= 2;
		free(buf);
		buf = malloc(len);
		if (buf == NULL)
			err(EXIT_MEMORY, "malloc(%zd)", len);
		goto again;
	}
	rem -= w;
	p += w;

	if (eno != 0) {
		w = snprintf(p, rem, ": %d (%s)", eno, strerror(eno));
		if (w < 0)
			err(EXIT_ERROR, "snprintf");
		if (w >= rem) {
			len *= 2;
			free(buf);
			buf = malloc(len);
			if (buf == NULL)
				err(EXIT_MEMORY, "malloc(%zd)", len);
			goto again;
		}
		p += w;
		rem -= w;
	}

	/* rem was len - 1, so there's always space for the nul */
	*p++ = '\0';

	fprintf(logfile, "%s\n", buf);
	fflush(logfile);
}
