#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>

#include <kstat.h>

#include "metrics.h"

struct kstat_modpriv {
	kstat_ctl_t *ctl;
	struct metric *rtime, *wtime;
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
}

static int
kstat_collect(void *modpriv)
{
	struct kstat_modpriv *priv = modpriv;
	kstat_t *sd, *sderr;
	kstat_named_t *dp;
	kstat_io_t io;
	char *prod, *serial, *ptr;

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
	}

	metric_clear_old_values(priv->rtime);
	metric_clear_old_values(priv->wtime);
	
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
