#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>

#include <kstat.h>

#include "metrics.h"

struct kstat_modpriv {
	kstat_ctl_t *ctl;
	struct metric *rtime, *wtime, *nread, *nwritten, *reads, *writes;
	struct metric *vfs_rtime, *vfs_rlentime, *vfs_wtime, *vfs_wlentime;
	struct metric *net_obytes64, *net_rbytes64, *net_opackets64,
	    *net_ipackets64, *net_ierrors, *net_oerrors, *net_norcvbuf;
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
	*modpriv = priv;

	priv->ctl = kstat_open();

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
}

static int
kstat_collect(void *modpriv)
{
	struct kstat_modpriv *priv = modpriv;
	kstat_t *sd, *sderr;
	kstat_named_t *dp;
	kstat_io_t io;
	char *prod, *serial, *ptr, *zname;

	kstat_chain_update(priv->ctl);

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

		kstat_read(priv->ctl, sderr, NULL);

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
		kstat_read(priv->ctl, sd, &io);

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
		kstat_read(priv->ctl, sd, &io);

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

		kstat_read(priv->ctl, sd, NULL);

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

	sd = kstat_lookup(priv->ctl, "link", -1, NULL);
	for (; sd != NULL; sd = sd->ks_next) {
		if (strcmp(sd->ks_module, "link") != 0)
			continue;
		if (sd->ks_type != KSTAT_TYPE_NAMED)
			continue;

		kstat_read(priv->ctl, sd, NULL);

		dp = kstat_data_lookup(sd, "zonename");
		if (dp == NULL)
			continue;
		if (dp->data_type == KSTAT_DATA_STRING)
			zname = strdup(dp->value.str.addr.ptr);
		else
			continue;

		dp = kstat_data_lookup(sd, "ipackets64");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->net_ipackets64, sd->ks_name, zname,
		    dp->value.ui64);

		dp = kstat_data_lookup(sd, "opackets64");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->net_opackets64, sd->ks_name, zname,
		    dp->value.ui64);

		dp = kstat_data_lookup(sd, "rbytes64");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->net_rbytes64, sd->ks_name, zname,
		    dp->value.ui64);

		dp = kstat_data_lookup(sd, "obytes64");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->net_obytes64, sd->ks_name, zname,
		    dp->value.ui64);

		dp = kstat_data_lookup(sd, "ierrors");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->net_ierrors, sd->ks_name, zname,
		    (uint64_t)dp->value.ui32);

		dp = kstat_data_lookup(sd, "oerrors");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->net_oerrors, sd->ks_name, zname,
		    (uint64_t)dp->value.ui32);

		dp = kstat_data_lookup(sd, "norcvbuf");
		if (dp == NULL) {
			free(zname);
			continue;
		}
		metric_update(priv->net_norcvbuf, sd->ks_name, zname,
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
