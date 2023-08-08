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

#include <kstat.h>
#include <sys/sysinfo.h>

#include "log.h"
#include "metrics.h"

struct kstat_modpriv {
	kstat_ctl_t *ctl;
	struct metric *rtime, *wtime, *nread, *nwritten, *reads, *writes;
	struct metric *vfs_rtime, *vfs_rlentime, *vfs_wtime, *vfs_wlentime;
	struct metric *net_obytes64, *net_rbytes64, *net_opackets64,
	    *net_ipackets64, *net_ierrors, *net_oerrors, *net_norcvbuf;
	struct metric *cpu_time, *ncpus;
	struct metric *nfs_calls;
	struct metric *arc_hits, *arc_misses, *arc_size, *arc_l2_hits,
	    *arc_l2_misses, *arc_l2_size;
	struct metric *swap_resv, *swap_alloc, *swap_avail, *swap_free,
	    *freemem;
};

struct metric_ops kstat_metric_ops = {
	.mo_collect = NULL,
	.mo_free = NULL
};


static void
kstat_register(struct registry *r, void **modpriv)
{
	struct kstat_modpriv *priv;

	priv = calloc(1, sizeof (struct kstat_modpriv));
	if (priv == NULL)
		err(EXIT_ERROR, "calloc");
	*modpriv = priv;

	priv->ctl = kstat_open();
	if (priv->ctl == NULL) {
		tslog("failed to open kstats");
		return;
	}

	priv->rtime = metric_new(r, "io_device_busy_nsec_total",
	    "IO device busy (service) total time in nanoseconds",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("device", METRIC_VAL_STRING),
	    metric_label_new("product", METRIC_VAL_STRING),
	    metric_label_new("serial", METRIC_VAL_STRING),
	    NULL);
	priv->wtime = metric_new(r, "io_device_wait_nsec_total",
	    "IO device wait (pre-service) total time in nanoseconds",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("device", METRIC_VAL_STRING),
	    metric_label_new("product", METRIC_VAL_STRING),
	    metric_label_new("serial", METRIC_VAL_STRING),
	    NULL);

	priv->nread = metric_new(r, "io_device_read_bytes_total",
	    "Count of bytes read from an I/O device",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("device", METRIC_VAL_STRING),
	    metric_label_new("product", METRIC_VAL_STRING),
	    metric_label_new("serial", METRIC_VAL_STRING),
	    NULL);
	priv->nwritten = metric_new(r, "io_device_written_bytes_total",
	    "Count of bytes written to an I/O device",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("device", METRIC_VAL_STRING),
	    metric_label_new("product", METRIC_VAL_STRING),
	    metric_label_new("serial", METRIC_VAL_STRING),
	    NULL);

	priv->reads = metric_new(r, "io_device_read_ops_total",
	    "Count of read operations completed by an I/O device",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("device", METRIC_VAL_STRING),
	    metric_label_new("product", METRIC_VAL_STRING),
	    metric_label_new("serial", METRIC_VAL_STRING),
	    NULL);
	priv->writes = metric_new(r, "io_device_write_ops_total",
	    "Count of write operations completed by an I/O device",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("device", METRIC_VAL_STRING),
	    metric_label_new("product", METRIC_VAL_STRING),
	    metric_label_new("serial", METRIC_VAL_STRING),
	    NULL);

	priv->vfs_rtime = metric_new(r, "vfs_read_busy_nsec_total",
	    "VFS busy (service) total time in nanoseconds spent on read ops",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("zonename", METRIC_VAL_STRING),
	    NULL);
	priv->vfs_rlentime = metric_new(r, "vfs_read_busy_qlen_nsec_total",
	    "Cumulative VFS I/O queue length per nanosecond in read ops",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("zonename", METRIC_VAL_STRING),
	    NULL);
	priv->vfs_wtime = metric_new(r, "vfs_write_busy_nsec_total",
	    "VFS busy (service) total time in nanoseconds spent on write ops",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("zonename", METRIC_VAL_STRING),
	    NULL);
	priv->vfs_wlentime = metric_new(r, "vfs_write_busy_qlen_nsec_total",
	    "Cumulative VFS I/O queue length per nanosecond in write ops",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("zonename", METRIC_VAL_STRING),
	    NULL);

	priv->net_ipackets64 = metric_new(r, "net_packets_in_total",
	    "Number of input packets received",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    metric_label_new("zonename", METRIC_VAL_STRING),
	    NULL);
	priv->net_rbytes64 = metric_new(r, "net_bytes_in_total",
	    "Number of input bytes received",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    metric_label_new("zonename", METRIC_VAL_STRING),
	    NULL);
	priv->net_ierrors = metric_new(r, "net_errors_in_total",
	    "Number of input errors encountered",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    metric_label_new("zonename", METRIC_VAL_STRING),
	    NULL);
	priv->net_norcvbuf = metric_new(r, "net_qdrops_in_total",
	    "Number of input queue drops encountered",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    metric_label_new("zonename", METRIC_VAL_STRING),
	    NULL);

	priv->net_opackets64 = metric_new(r, "net_packets_out_total",
	    "Number of output packets sent",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    metric_label_new("zonename", METRIC_VAL_STRING),
	    NULL);
	priv->net_obytes64 = metric_new(r, "net_bytes_out_total",
	    "Number of output bytes sent",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    metric_label_new("zonename", METRIC_VAL_STRING),
	    NULL);
	priv->net_oerrors = metric_new(r, "net_errors_out_total",
	    "Number of output errors encountered",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("interface", METRIC_VAL_STRING),
	    metric_label_new("zonename", METRIC_VAL_STRING),
	    NULL);

	priv->cpu_time = metric_new(r, "cpu_time_spent_nsec_total",
	    "Total time spent in different CPU states",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("cpu", METRIC_VAL_UINT64),
	    metric_label_new("state", METRIC_VAL_STRING),
	    NULL);
	priv->ncpus = metric_new(r, "cpu_count",
	    "Number of logical CPUs on the system",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    NULL);

	priv->nfs_calls = metric_new(r, "nfs_server_calls_total",
	    "Number of NFS calls handled by the NFS server",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    metric_label_new("version", METRIC_VAL_UINT64),
	    NULL);

	priv->arc_hits = metric_new(r, "arcstats_hits_total",
	    "ARC hits", METRIC_COUNTER, METRIC_VAL_UINT64, NULL,
	    &kstat_metric_ops, NULL);
	priv->arc_misses = metric_new(r, "arcstats_misses_total",
	    "ARC misses", METRIC_COUNTER, METRIC_VAL_UINT64, NULL,
	    &kstat_metric_ops, NULL);
	priv->arc_size = metric_new(r, "arcstats_size_bytes",
	    "ARC total size in bytes", METRIC_GAUGE, METRIC_VAL_UINT64,
	    NULL, &kstat_metric_ops, NULL);
	priv->arc_l2_hits = metric_new(r, "arcstats_l2_hits_total",
	    "L2 ARC hits", METRIC_COUNTER, METRIC_VAL_UINT64, NULL,
	    &kstat_metric_ops, NULL);
	priv->arc_l2_misses = metric_new(r, "arcstats_l2_misses_total",
	    "L2 ARC misses", METRIC_COUNTER, METRIC_VAL_UINT64, NULL,
	    &kstat_metric_ops, NULL);
	priv->arc_l2_size = metric_new(r, "arcstats_l2_size_bytes",
	    "L2 ARC total size in bytes", METRIC_GAUGE, METRIC_VAL_UINT64,
	    NULL, &kstat_metric_ops, NULL);

	priv->swap_resv = metric_new(r, "vminfo_swap_resv_sample_bytes_total",
	    "Sum of 1-second samples of reserved swap memory",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    NULL);
	priv->swap_alloc = metric_new(r, "vminfo_swap_alloc_sample_bytes_total",
	    "Sum of 1-second samples of allocated swap memory",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    NULL);
	priv->swap_avail = metric_new(r, "vminfo_swap_avail_sample_bytes_total",
	    "Sum of 1-second samples of available (unreserved) swap memory",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    NULL);
	priv->swap_free = metric_new(r, "vminfo_swap_free_sample_bytes_total",
	    "Sum of 1-second samples of free swap memory",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &kstat_metric_ops,
	    NULL);
}

static int
kstat_collect(void *modpriv)
{
	struct kstat_modpriv *priv = modpriv;
	kstat_t *sd, *sderr;
	kstat_named_t *dp;
	kstat_io_t io;
	char *prod, *serial, *ptr, *zname;
	const char *intf;

	if (priv->ctl == NULL)
		return (0);

	if (kstat_chain_update(priv->ctl) < 0) {
		tslog("failed to update kstats: %s", strerror(errno));
		return (-1);
	}

	sd = kstat_lookup(priv->ctl, "sd", -1, NULL);
	for (; sd != NULL; sd = sd->ks_next) {
		if (strcmp(sd->ks_module, "sd") != 0)
			continue;
		if (strcmp(sd->ks_class, "disk") != 0)
			continue;
		if (sd->ks_type != KSTAT_TYPE_IO)
			continue;

		sderr = kstat_lookup(priv->ctl, "sderr", sd->ks_instance, NULL);
		if (sderr == NULL)
			continue;

		if (kstat_read(priv->ctl, sderr, NULL) < 0) {
			continue;
		}

		dp = kstat_data_lookup(sderr, "Product");
		if (dp == NULL)
			continue;
		if (dp->data_type == KSTAT_DATA_CHAR)
			prod = strdup(dp->value.c);
		else if (dp->data_type == KSTAT_DATA_STRING)
			prod = strdup(dp->value.str.addr.ptr);
		else
			continue;
		ptr = strstr(prod, " ");
		if (ptr != NULL)
			*ptr = '\0';

		dp = kstat_data_lookup(sderr, "Serial No");
		if (dp == NULL) {
			free(prod);
			continue;
		}
		if (dp->data_type == KSTAT_DATA_CHAR)
			serial = strdup(dp->value.c);
		else if (dp->data_type == KSTAT_DATA_STRING)
			serial = strdup(dp->value.str.addr.ptr);
		else
			continue;
		ptr = strstr(serial, " ");
		if (ptr != NULL)
			*ptr = '\0';

		bzero(&io, sizeof (io));
		if (kstat_read(priv->ctl, sd, &io) < 0) {
			free(prod);
			free(serial);
			continue;
		}

		metric_update(priv->rtime, sd->ks_name, prod, serial, io.rtime);
		metric_update(priv->wtime, sd->ks_name, prod, serial, io.wtime);
		metric_update(priv->nread, sd->ks_name, prod, serial, io.nread);
		metric_update(priv->nwritten, sd->ks_name, prod, serial,
		    io.nwritten);
		metric_update(priv->reads, sd->ks_name, prod, serial, io.reads);
		metric_update(priv->writes, sd->ks_name, prod, serial,
		    io.writes);

		free(prod);
		free(serial);
	}

	for (sd = priv->ctl->kc_chain; sd != NULL; sd = sd->ks_next) {
		if (strcmp(sd->ks_module, "sd") == 0)
			continue;
		if (sd->ks_type != KSTAT_TYPE_IO)
			continue;
		if (strcmp(sd->ks_class, "disk") != 0)
			continue;
		bzero(&io, sizeof (io));
		if (kstat_read(priv->ctl, sd, &io) < 0)
			continue;

		metric_update(priv->rtime, sd->ks_name, NULL, NULL, io.rtime);
		metric_update(priv->wtime, sd->ks_name, NULL, NULL, io.wtime);
		metric_update(priv->nread, sd->ks_name, NULL, NULL, io.nread);
		metric_update(priv->nwritten, sd->ks_name, NULL, NULL,
		    io.nwritten);
		metric_update(priv->reads, sd->ks_name, NULL, NULL, io.reads);
		metric_update(priv->writes, sd->ks_name, NULL, NULL, io.writes);
	}

	metric_clear_old_values(priv->rtime);
	metric_clear_old_values(priv->wtime);
	metric_clear_old_values(priv->nread);
	metric_clear_old_values(priv->nwritten);
	metric_clear_old_values(priv->reads);
	metric_clear_old_values(priv->writes);

	sd = kstat_lookup(priv->ctl, "zone_vfs", -1, NULL);
	for (; sd != NULL; sd = sd->ks_next) {
		if (strcmp(sd->ks_module, "zone_vfs") != 0)
			continue;
		if (sd->ks_type != KSTAT_TYPE_NAMED)
			continue;

		if (kstat_read(priv->ctl, sd, NULL) < 0)
			continue;

		dp = kstat_data_lookup(sd, "zonename");
		if (dp == NULL)
			continue;
		if (dp->data_type == KSTAT_DATA_STRING)
			zname = strdup(dp->value.str.addr.ptr);
		else
			continue;

		dp = kstat_data_lookup(sd, "rtime");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->vfs_rtime, zname, dp->value.ui64);

		dp = kstat_data_lookup(sd, "rlentime");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->vfs_rlentime, zname, dp->value.ui64);

		dp = kstat_data_lookup(sd, "wtime");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->vfs_wtime, zname, dp->value.ui64);

		dp = kstat_data_lookup(sd, "wlentime");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->vfs_wlentime, zname, dp->value.ui64);

		free(zname);
	}

	metric_clear_old_values(priv->vfs_rtime);
	metric_clear_old_values(priv->vfs_rlentime);
	metric_clear_old_values(priv->vfs_wtime);
	metric_clear_old_values(priv->vfs_wlentime);

	for (sd = priv->ctl->kc_chain; sd != NULL; sd = sd->ks_next) {
		if (strcmp(sd->ks_name, "mac") == 0 &&
		    strcmp(sd->ks_class, "net") == 0) {
			intf = sd->ks_module;
		} else if (strcmp(sd->ks_module, "link") == 0 &&
		    strcmp(sd->ks_class, "net") == 0) {
			intf = sd->ks_name;
		} else {
			continue;
		}
		if (sd->ks_type != KSTAT_TYPE_NAMED)
			continue;

		if (kstat_read(priv->ctl, sd, NULL) < 0)
			continue;

		dp = kstat_data_lookup(sd, "zonename");
		if (dp != NULL &&
		    dp->data_type == KSTAT_DATA_STRING) {
			zname = strdup(dp->value.str.addr.ptr);
		} else {
			zname = NULL;
		}

		dp = kstat_data_lookup(sd, "ipackets64");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->net_ipackets64, intf, zname,
		    dp->value.ui64);

		dp = kstat_data_lookup(sd, "opackets64");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->net_opackets64, intf, zname,
		    dp->value.ui64);

		dp = kstat_data_lookup(sd, "rbytes64");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->net_rbytes64, intf, zname,
		    dp->value.ui64);

		dp = kstat_data_lookup(sd, "obytes64");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->net_obytes64, intf, zname,
		    dp->value.ui64);

		dp = kstat_data_lookup(sd, "ierrors");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->net_ierrors, intf, zname,
		    (uint64_t)dp->value.ui32);

		dp = kstat_data_lookup(sd, "oerrors");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->net_oerrors, intf, zname,
		    (uint64_t)dp->value.ui32);

		dp = kstat_data_lookup(sd, "norcvbuf");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->net_norcvbuf, intf, zname,
		    (uint64_t)dp->value.ui32);

		free(zname);
	}

	metric_clear_old_values(priv->net_ipackets64);
	metric_clear_old_values(priv->net_opackets64);
	metric_clear_old_values(priv->net_rbytes64);
	metric_clear_old_values(priv->net_obytes64);
	metric_clear_old_values(priv->net_ierrors);
	metric_clear_old_values(priv->net_oerrors);
	metric_clear_old_values(priv->net_norcvbuf);

	metric_clear(priv->ncpus);
	metric_clear(priv->cpu_time);

	sd = kstat_lookup(priv->ctl, "cpu", -1, NULL);
	for (; sd != NULL; sd = sd->ks_next) {
		if (strcmp(sd->ks_module, "cpu") != 0)
			continue;
		if (strcmp(sd->ks_name, "sys") != 0)
			continue;
		if (sd->ks_type != KSTAT_TYPE_NAMED)
			continue;

		if (kstat_read(priv->ctl, sd, NULL) < 0)
			continue;

		metric_inc(priv->ncpus);

		dp = kstat_data_lookup(sd, "cpu_nsec_dtrace");
		if (dp == NULL)
			continue;
		metric_update(priv->cpu_time, (uint64_t)sd->ks_instance,
		    "dtrace", dp->value.ui64);

		dp = kstat_data_lookup(sd, "cpu_nsec_intr");
		if (dp == NULL)
			continue;
		metric_update(priv->cpu_time, (uint64_t)sd->ks_instance,
		    "intr", dp->value.ui64);

		dp = kstat_data_lookup(sd, "cpu_nsec_idle");
		if (dp == NULL)
			continue;
		metric_update(priv->cpu_time, (uint64_t)sd->ks_instance,
		    "idle", dp->value.ui64);

		dp = kstat_data_lookup(sd, "cpu_nsec_kernel");
		if (dp == NULL)
			continue;
		metric_update(priv->cpu_time, (uint64_t)sd->ks_instance,
		    "kernel", dp->value.ui64);

		dp = kstat_data_lookup(sd, "cpu_nsec_user");
		if (dp == NULL)
			continue;
		metric_update(priv->cpu_time, (uint64_t)sd->ks_instance,
		    "user", dp->value.ui64);
	}

	sd = kstat_lookup(priv->ctl, "nfs", -1, NULL);
	for (; sd != NULL; sd = sd->ks_next) {
		if (strcmp(sd->ks_module, "nfs") != 0)
			continue;
		if (strcmp(sd->ks_name, "nfs_server") != 0)
			continue;
		if (sd->ks_type != KSTAT_TYPE_NAMED)
			continue;

		if (kstat_read(priv->ctl, sd, NULL) < 0)
			continue;

		dp = kstat_data_lookup(sd, "calls");
		if (dp == NULL)
			continue;
		metric_update(priv->nfs_calls, (uint64_t)sd->ks_instance,
		    dp->value.ui64);
	}

	sd = kstat_lookup(priv->ctl, "zfs", 0, "arcstats");
	if (sd != NULL && sd->ks_type == KSTAT_TYPE_NAMED &&
	    kstat_read(priv->ctl, sd, NULL) >= 0) {
		dp = kstat_data_lookup(sd, "hits");
		if (dp != NULL)
			metric_update(priv->arc_hits, dp->value.ui64);
		dp = kstat_data_lookup(sd, "misses");
		if (dp != NULL)
			metric_update(priv->arc_misses, dp->value.ui64);
		dp = kstat_data_lookup(sd, "size");
		if (dp != NULL)
			metric_update(priv->arc_size, dp->value.ui64);
		dp = kstat_data_lookup(sd, "l2_hits");
		if (dp != NULL)
			metric_update(priv->arc_l2_hits, dp->value.ui64);
		dp = kstat_data_lookup(sd, "l2_misses");
		if (dp != NULL)
			metric_update(priv->arc_l2_misses, dp->value.ui64);
		dp = kstat_data_lookup(sd, "l2_size");
		if (dp != NULL)
			metric_update(priv->arc_l2_size, dp->value.ui64);
	}

	sd = kstat_lookup(priv->ctl, "unix", 0, "vminfo");
	if (sd != NULL && sd->ks_type == KSTAT_TYPE_RAW &&
	    kstat_read(priv->ctl, sd, NULL) >= 0) {
		if (sd->ks_data_size >= sizeof (vminfo_t)) {
			const vminfo_t *vmi = (const vminfo_t *)sd->ks_data;

			metric_update(priv->swap_resv, vmi->swap_resv);
			metric_update(priv->swap_alloc, vmi->swap_alloc);
			metric_update(priv->swap_avail, vmi->swap_avail);
			metric_update(priv->swap_free, vmi->swap_free);
		}
	}

	return (0);
}

static void
kstat_free(void *modpriv)
{
	struct kstat_modpriv *priv = modpriv;
	kstat_close(priv->ctl);
	free(priv);
}

struct metrics_module_ops collect_kstat_ops = {
	.mm_register = kstat_register,
	.mm_collect = kstat_collect,
	.mm_free = kstat_free
};
