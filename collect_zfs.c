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

#include <libzfs.h>
#include <libnvpair.h>
#include <sys/fs/zfs.h>

#include "metrics.h"
#include "log.h"

struct zfs_modpriv {
	libzfs_handle_t *hdl;
	struct metric *vdev_alloc, *vdev_cap;
};

struct metric_ops zfs_metric_ops = {
	.mo_collect = NULL,
	.mo_free = NULL
};

static void
zfs_register(struct registry *r, void **modpriv)
{
	struct zfs_modpriv *priv;

	priv = calloc(1, sizeof (struct zfs_modpriv));
	*modpriv = priv;

	priv->hdl = libzfs_init();

	priv->vdev_alloc = metric_new(r, "zfs_alloc_bytes",
	    "Bytes currently allocated within a vdev/pool",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &zfs_metric_ops,
	    metric_label_new("pool", METRIC_VAL_STRING),
	    metric_label_new("vdev", METRIC_VAL_STRING),
	    metric_label_new("vdev_type", METRIC_VAL_STRING),
	    NULL);
	priv->vdev_cap = metric_new(r, "zfs_capacity_bytes",
	    "Total capacity of a vdev/pool in bytes",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &zfs_metric_ops,
	    metric_label_new("pool", METRIC_VAL_STRING),
	    metric_label_new("vdev", METRIC_VAL_STRING),
	    metric_label_new("vdev_type", METRIC_VAL_STRING),
	    NULL);
}

static int
pool_walker(zpool_handle_t *phdl, void *arg)
{
	struct zfs_modpriv *priv = (struct zfs_modpriv *)arg;
	int r;
	nvlist_t *config, *root, **vdevs;
	boolean_t missing = B_FALSE;
	uint_t c, kids, i;
	const char *name, *vname, *type;
	vdev_stat_t *vstat;

	r = zpool_refresh_stats(phdl, &missing);
	if (r != 0 || missing)
		return (0);

	name = zpool_get_name(phdl);
	config = zpool_get_config(phdl, NULL);

	root = fnvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE);

	nvlist_lookup_uint64_array(root, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vstat, &c);
	metric_update(priv->vdev_alloc, name, NULL, NULL, vstat->vs_alloc);
	metric_update(priv->vdev_cap, name, NULL, NULL, vstat->vs_space);

	if (nvlist_lookup_nvlist_array(root, ZPOOL_CONFIG_CHILDREN,
	    &vdevs, &kids) != 0) {
		return (0);
	}
	for (i = 0; i < kids; ++i) {
		if (nvlist_lookup_uint64_array(vdevs[i],
		    ZPOOL_CONFIG_VDEV_STATS, (uint64_t **)&vstat, &c) != 0) {
			continue;
		}
		nvlist_lookup_string(vdevs[i], ZPOOL_CONFIG_TYPE, (char **)&type);
		vname = zpool_vdev_name(priv->hdl, phdl, vdevs[i], 1);
		metric_update(priv->vdev_alloc, name, vname, type, vstat->vs_alloc);
		metric_update(priv->vdev_cap, name, vname, type, vstat->vs_space);
	}

	return (0);
}

static int
zfs_collect(void *modpriv)
{
	struct zfs_modpriv *priv = modpriv;
	int r;

	r = zpool_iter(priv->hdl, pool_walker, priv);
	if (r < 0) {
		tslog("failed to walk zpools: %s", strerror(errno));
		return (0);
	}

	metric_clear_old_values(priv->vdev_alloc);
	metric_clear_old_values(priv->vdev_cap);

	return (0);
}

static void
zfs_free(void *modpriv)
{
	struct zfs_modpriv *priv = modpriv;
	libzfs_fini(priv->hdl);
	free(priv);
}

struct metrics_module_ops collect_zfs_ops = {
	.mm_register = zfs_register,
	.mm_collect = zfs_collect,
	.mm_free = zfs_free
};
