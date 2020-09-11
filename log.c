#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <errno.h>

#include <time.h>
#include <stdarg.h>
#include <err.h>

#include "log.h"

void
tslog(const char *fmt, ...)
{
	static char *buf;
	static size_t len = 0;

	va_list ap;
	char *p;
	size_t rem;
	int w;
	struct timespec ts;
	struct tm *info;

	if (len == 0) {
		len = strlen(fmt) * 2 + 64;
		buf = malloc(len);
		if (buf == NULL)
			err(EXIT_MEMORY, "malloc(%zd)", len);
	}

	buf[0] = '\0';
	rem = len - 1;
	p = buf;

	if (clock_gettime(CLOCK_REALTIME, &ts))
		err(EXIT_ERROR, "clock_gettime(CLOCK_REALTIME)");
	info = gmtime(&ts.tv_sec);
	if (info == NULL)
		err(EXIT_ERROR, "gmtime");

	w = snprintf(p, rem, "[%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ] ",
	    info->tm_year + 1900, info->tm_mon + 1, info->tm_mday,
	    info->tm_hour, info->tm_min, info->tm_sec, ts.tv_nsec / 1000000);
	if (w < 0)
		err(EXIT_ERROR, "snprintf");
	p += w;
	rem -= w;

	va_start(ap, fmt);
	w = vsnprintf(p, rem, fmt, ap);
	if (w < 0)
		err(EXIT_ERROR, "vsnprintf");
	rem -= w;
	p += w;
	va_end(ap);

	/* rem was len - 1, so there's always space for the nul */
	*p++ = '\0';

	fprintf(stdout, "%s\n", buf);
}
