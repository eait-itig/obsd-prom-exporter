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
#include <limits.h>
#include <sys/types.h>

#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/pool.h>

#include "metrics.h"
#include "log.h"

struct uvm_modpriv {
	struct uvmexp stats;
	struct metric *free, *active, *inactive, *total;
};

struct metric_ops uvm_metric_ops = {
	.mo_collect = NULL,
	.mo_free = NULL
};

static void
uvm_register(struct registry *r, void **modpriv)
{
	struct uvm_modpriv *priv;

	priv = calloc(1, sizeof (struct uvm_modpriv));
	*modpriv = priv;

	priv->free = metric_new(r, "uvm_free_bytes",
	    "Bytes in pages marked 'free' in UVM",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &uvm_metric_ops, NULL);
	priv->active = metric_new(r, "uvm_active_bytes",
	    "Bytes in pages marked 'active' in UVM",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &uvm_metric_ops, NULL);
	priv->inactive = metric_new(r, "uvm_inactive_bytes",
	    "Bytes in pages marked 'inactive' in UVM",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &uvm_metric_ops, NULL);
	priv->total = metric_new(r, "uvm_total_bytes",
	    "Total bytes in pages managed by uvm",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &uvm_metric_ops, NULL);
}

static int
uvm_collect(void *modpriv)
{
	struct uvm_modpriv *priv = modpriv;
	size_t size = sizeof (priv->stats);
	int mib[3] = { CTL_VM, VM_UVMEXP };

	if (sysctl(mib, 2, &priv->stats, &size, NULL, 0) == -1) {
		tslog("failed to get uvm stats: %s", strerror(errno));
		return (0);
	}

	metric_update(priv->free,
	    (uint64_t)((uint64_t)priv->stats.free * priv->stats.pagesize));
	metric_update(priv->active,
	    (uint64_t)((uint64_t)priv->stats.active * priv->stats.pagesize));
	metric_update(priv->inactive,
	    (uint64_t)((uint64_t)priv->stats.inactive * priv->stats.pagesize));
	metric_update(priv->total,
	    (uint64_t)((uint64_t)priv->stats.npages * priv->stats.pagesize));

	return (0);
}

static void
uvm_free(void *modpriv)
{
	struct uvm_modpriv *priv = modpriv;
	free(priv);
}

struct metrics_module_ops collect_uvm_ops = {
	.mm_register = uvm_register,
	.mm_collect = uvm_collect,
	.mm_free = uvm_free
};
