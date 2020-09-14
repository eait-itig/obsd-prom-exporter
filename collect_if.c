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

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <err.h>
#include <sys/types.h>

#include <sys/param.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>

#include "metrics.h"
#include "log.h"

struct if_modpriv {
	char *buf;
	size_t bsize;
	struct metric *ipackets, *ibytes, *ierrors, *iqdrops;
	struct metric *opackets, *obytes, *oerrors, *oqdrops;
};

struct metric_ops if_metric_ops = {
	.mo_collect = NULL,
	.mo_free = NULL
};

static void
if_register(struct registry *r, void **modpriv)
{
	struct if_modpriv *priv;

	priv = calloc(1, sizeof (struct if_modpriv));
	*modpriv = priv;

	priv->bsize = 64*1024;
	priv->buf = malloc(priv->bsize);
	if (priv->buf == NULL)
		err(EXIT_MEMORY, "malloc");

	priv->ipackets = metric_new(r, "net_packets_in_total",
	    "Number of input packets received",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &if_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    NULL);
	priv->ibytes = metric_new(r, "net_bytes_in_total",
	    "Number of input bytes received",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &if_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    NULL);
	priv->ierrors = metric_new(r, "net_errors_in_total",
	    "Number of input errors encountered",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &if_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    NULL);
	priv->iqdrops = metric_new(r, "net_qdrops_in_total",
	    "Number of input queue drops encountered",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &if_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    NULL);

	priv->opackets = metric_new(r, "net_packets_out_total",
	    "Number of output packets sent",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &if_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    NULL);
	priv->obytes = metric_new(r, "net_bytes_out_total",
	    "Number of output bytes sent",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &if_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    NULL);
	priv->oerrors = metric_new(r, "net_errors_out_total",
	    "Number of output errors encountered",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &if_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    NULL);
	priv->oqdrops = metric_new(r, "net_qdrops_out_total",
	    "Number of output queue drops encountered",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &if_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    NULL);
}

static void
rt_getaddrinfo(struct sockaddr *sa, int addrs, struct sockaddr **info)
{
	int i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			info[i] = sa;
			sa = (struct sockaddr *) ((char *)(sa) +
			    roundup(sa->sa_len, sizeof(long)));
		} else
			info[i] = NULL;
	}
}

static int
if_collect(void *modpriv)
{
	struct if_modpriv *priv = modpriv;
	size_t need;
	struct if_msghdr ifm;
	char *buf, *lim, *next;
	int mib[6] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST, 0 };
	struct sockaddr *info[RTAX_MAX];
	struct sockaddr_dl *sdl;

	buf = priv->buf;

	if (sysctl(mib, 6, NULL, &need, NULL, 0) == -1) {
		tslog("failed to get if stats: %s", strerror(errno));
		return (0);
	}
	if (need > priv->bsize) {
		char *newbuf = malloc(need);
		if (newbuf == NULL) {
			tslog("failed to expand if buffer: %s",
				strerror(errno));
			return (0);
		}
		priv->bsize = need;
		free(priv->buf);
		priv->buf = newbuf;
	}
	if (sysctl(mib, 6, buf, &need, NULL, 0) == -1) {
		tslog("failed to get if stats: %s", strerror(errno));
		return (0);
	}

	lim = buf + need;
	for (next = buf; next < lim; next += ifm.ifm_msglen) {
		bcopy(next, &ifm, sizeof ifm);
		if (ifm.ifm_version != RTM_VERSION ||
		    ifm.ifm_type != RTM_IFINFO ||
		    !(ifm.ifm_addrs & RTA_IFP))
			continue;

		bzero(&info, sizeof(info));
		rt_getaddrinfo(
		    (struct sockaddr *)((struct if_msghdr *)next + 1),
		    ifm.ifm_addrs, info);
		sdl = (struct sockaddr_dl *)info[RTAX_IFP];

		if (sdl && sdl->sdl_family == AF_LINK && sdl->sdl_nlen > 0) {
			char name[IFNAMSIZ];

			bcopy(sdl->sdl_data, name, sdl->sdl_nlen);
			name[sdl->sdl_nlen] = '\0';

			metric_update(priv->ipackets, name,
			    ifm.ifm_data.ifi_ipackets);
			metric_update(priv->ibytes, name,
			    ifm.ifm_data.ifi_ibytes);
			metric_update(priv->ierrors, name,
			    ifm.ifm_data.ifi_ierrors);
			metric_update(priv->iqdrops, name,
			    ifm.ifm_data.ifi_iqdrops);

			metric_update(priv->opackets, name,
			    ifm.ifm_data.ifi_opackets);
			metric_update(priv->obytes, name,
			    ifm.ifm_data.ifi_obytes);
			metric_update(priv->oerrors, name,
			    ifm.ifm_data.ifi_oerrors);
			metric_update(priv->oqdrops, name,
			    ifm.ifm_data.ifi_oqdrops);
		}
	}

	metric_clear_old_values(priv->ipackets);
	metric_clear_old_values(priv->ibytes);
	metric_clear_old_values(priv->ierrors);
	metric_clear_old_values(priv->iqdrops);
	metric_clear_old_values(priv->opackets);
	metric_clear_old_values(priv->obytes);
	metric_clear_old_values(priv->oerrors);
	metric_clear_old_values(priv->oqdrops);

	return (0);
}

static void
if_free(void *modpriv)
{
	struct if_modpriv *priv = modpriv;
	free(priv->buf);
	free(priv);
}

struct metrics_module_ops collect_if_ops = {
	.mm_register = if_register,
	.mm_collect = if_collect,
	.mm_free = if_free
};
