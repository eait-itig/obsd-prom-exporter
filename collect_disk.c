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
#include <sys/types.h>

#include <sys/sysctl.h>
#include <sys/disk.h>

#include "metrics.h"
#include "log.h"

struct disk_modpriv {
	struct diskstats *stats;
	size_t zstats;
	struct metric *rops, *wops, *rbytes, *wbytes;
	struct metric *rtime;
};

struct metric_ops disk_metric_ops = {
	.mo_collect = NULL,
	.mo_free = NULL
};

static void
disk_register(struct registry *r, void **modpriv)
{
	struct disk_modpriv *priv;

	priv = calloc(1, sizeof (struct disk_modpriv));
	*modpriv = priv;

	priv->rops = metric_new(r, "io_device_read_ops_total",
	    "Count of read operations completed by an I/O device",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &disk_metric_ops,
	    metric_label_new("device", METRIC_VAL_STRING),
	    NULL);
	priv->wops = metric_new(r, "io_device_write_ops_total",
	    "Count of write operations completed by an I/O device",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &disk_metric_ops,
	    metric_label_new("device", METRIC_VAL_STRING),
	    NULL);

	priv->rbytes = metric_new(r, "io_device_read_bytes_total",
	    "Count of bytes read from an I/O device",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &disk_metric_ops,
	    metric_label_new("device", METRIC_VAL_STRING),
	    NULL);
	priv->wbytes = metric_new(r, "io_device_written_bytes_total",
	    "Count of bytes written to an I/O device",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &disk_metric_ops,
	    metric_label_new("device", METRIC_VAL_STRING),
	    NULL);

	priv->rtime = metric_new(r, "io_device_busy_nsec_total",
	    "IO device busy (service) total time in nanoseconds",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &disk_metric_ops,
	    metric_label_new("device", METRIC_VAL_STRING),
	    NULL);

	priv->zstats = 16 * sizeof (struct diskstats);
	priv->stats = malloc(priv->zstats);
	if (priv->stats == NULL) {
		priv->zstats = 0;
		tslog("failed to allocate memory for disk stats");
	} else {
		bzero(priv->stats, priv->zstats);
	}
}

static int
disk_collect(void *modpriv)
{
	struct disk_modpriv *priv = modpriv;
	size_t size;
	int i, n;
	int mib[] = { CTL_HW, HW_DISKCOUNT };

	size = sizeof (int);
	if (sysctl(mib, 2, &n, &size, NULL, 0) == -1) {
		tslog("failed to get stats: %s", strerror(errno));
		return (0);
	}

	size = n * sizeof (struct diskstats);
	if (size > priv->zstats) {
		free(priv->stats);
		priv->zstats = size;
		priv->stats = malloc(size);
		if (priv->stats == NULL) {
			priv->zstats = 0;
			tslog("failed to allocate memory for disk stats");
			return (0);
		}
	}
	bzero(priv->stats, size);

	mib[1] = HW_DISKSTATS;
	if (sysctl(mib, 2, priv->stats, &size, NULL, 0) == -1) {
		tslog("failed to get stats: %s", strerror(errno));
		return (0);
	}

	for (i = 0; i < n; ++i) {
		uint64_t t;

		metric_update(priv->rops, priv->stats[i].ds_name,
		    priv->stats[i].ds_rxfer);
		metric_update(priv->wops, priv->stats[i].ds_name,
		    priv->stats[i].ds_wxfer);
		metric_update(priv->rbytes, priv->stats[i].ds_name,
		    priv->stats[i].ds_rbytes);
		metric_update(priv->wbytes, priv->stats[i].ds_name,
		    priv->stats[i].ds_wbytes);

		t = priv->stats[i].ds_time.tv_usec * 1000ULL;
		t += priv->stats[i].ds_time.tv_sec * 1000000000ULL;
		metric_update(priv->rtime, priv->stats[i].ds_name, t);
	}

	metric_clear_old_values(priv->rops);
	metric_clear_old_values(priv->wops);
	metric_clear_old_values(priv->rbytes);
	metric_clear_old_values(priv->wbytes);
	metric_clear_old_values(priv->rtime);

	return (0);
}

static void
disk_free(void *modpriv)
{
	struct disk_modpriv *priv = modpriv;
	free(priv->stats);
	free(priv);
}

struct metrics_module_ops collect_disk_ops = {
	.mm_register = disk_register,
	.mm_collect = disk_collect,
	.mm_free = disk_free
};
