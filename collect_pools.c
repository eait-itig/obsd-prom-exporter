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
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/pool.h>

#include "metrics.h"
#include "log.h"

struct pools_modpriv {
	struct kinfo_pool stats;
	struct metric *size, *nitems, *nout;
	struct metric *nget, *nput, *nfail;
	struct metric *npagealloc, *npagefree, *hiwat, *nidle;
};

struct metric_ops pools_metric_ops = {
	.mo_collect = NULL,
	.mo_free = NULL
};

static void
pools_register(struct registry *r, void **modpriv)
{
	struct pools_modpriv *priv;

	priv = calloc(1, sizeof (struct pools_modpriv));
	*modpriv = priv;

	priv->size = metric_new(r, "pool_item_size_bytes",
	    "Size of an item in a particular pool",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &pools_metric_ops,
	    metric_label_new("pool", METRIC_VAL_STRING),
	    NULL);
	priv->nitems = metric_new(r, "pool_items",
	    "Number of items in a particular pool",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &pools_metric_ops,
	    metric_label_new("pool", METRIC_VAL_STRING),
	    NULL);
	priv->nout = metric_new(r, "pool_items_allocated",
	    "Number of items allocated from a particular pool",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &pools_metric_ops,
	    metric_label_new("pool", METRIC_VAL_STRING),
	    NULL);

	priv->nget = metric_new(r, "pool_gets_total",
	    "Number of times a pool has allocated an item successfully",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &pools_metric_ops,
	    metric_label_new("pool", METRIC_VAL_STRING),
	    NULL);
	priv->nput = metric_new(r, "pool_puts_total",
	    "Number of times a pool has released an item successfully",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &pools_metric_ops,
	    metric_label_new("pool", METRIC_VAL_STRING),
	    NULL);
	priv->nfail = metric_new(r, "pool_fails_total",
	    "Number of times a pool has failed to allocate an item",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &pools_metric_ops,
	    metric_label_new("pool", METRIC_VAL_STRING),
	    NULL);

	priv->npagealloc = metric_new(r, "pool_page_allocs_total",
	    "Number of times a pool has allocated a new page",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &pools_metric_ops,
	    metric_label_new("pool", METRIC_VAL_STRING),
	    NULL);
	priv->npagefree = metric_new(r, "pool_page_frees_total",
	    "Number of times a pool has released a page",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &pools_metric_ops,
	    metric_label_new("pool", METRIC_VAL_STRING),
	    NULL);

	priv->hiwat = metric_new(r, "pool_pages_max_allocated",
	    "Maximum number of pages a pool has allocated at once "
	    "(high water mark)",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &pools_metric_ops,
	    metric_label_new("pool", METRIC_VAL_STRING),
	    NULL);
	priv->nidle = metric_new(r, "pool_pages_idle",
	    "Number of idle pages currently in a pool",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &pools_metric_ops,
	    metric_label_new("pool", METRIC_VAL_STRING),
	    NULL);
}

static int
pools_collect(void *modpriv)
{
	struct pools_modpriv *priv = modpriv;
	int i, npools;
	size_t size;
	int nmib[] = { CTL_KERN, KERN_POOL, KERN_POOL_NPOOLS };
	int namemib[] = { CTL_KERN, KERN_POOL, KERN_POOL_NAME, 0 };
	int pmib[] = { CTL_KERN, KERN_POOL, KERN_POOL_POOL, 0 };
	char namebuf[32];

	size = sizeof (npools);
	if (sysctl(nmib, 3, &npools, &size, NULL, 0) == -1) {
		tslog("failed to get npools: %s", strerror(errno));
		return (0);
	}

	for (i = 1; i <= npools; ++i) {
		size = sizeof (namebuf);
		bzero(namebuf, sizeof (namebuf));
		namemib[3] = i;
		if (sysctl(namemib, 4, namebuf, &size, NULL, 0) == -1) {
			tslog("failed to get pool name %d: %s", i,
			    strerror(errno));
			return (0);
		}

		size = sizeof (priv->stats);
		pmib[3] = i;
		if (sysctl(pmib, 4, &priv->stats, &size, NULL, 0) == -1) {
			tslog("failed to get pool stats %d: %s", i,
			    strerror(errno));
			return (0);
		}

		metric_update(priv->size, namebuf, priv->stats.pr_size);
		metric_update(priv->nitems, namebuf, priv->stats.pr_nitems);
		metric_update(priv->nout, namebuf, priv->stats.pr_nout);

		metric_update(priv->nget, namebuf, priv->stats.pr_nget);
		metric_update(priv->nput, namebuf, priv->stats.pr_nput);
		metric_update(priv->nfail, namebuf, priv->stats.pr_nfail);
		metric_update(priv->npagealloc, namebuf,
		    priv->stats.pr_npagealloc);
		metric_update(priv->npagefree, namebuf,
		    priv->stats.pr_npagefree);

		metric_update(priv->hiwat, namebuf, priv->stats.pr_hiwat);
		metric_update(priv->nidle, namebuf, priv->stats.pr_nidle);
	}

	metric_clear_old_values(priv->size);
	metric_clear_old_values(priv->nitems);
	metric_clear_old_values(priv->nout);
	metric_clear_old_values(priv->nget);
	metric_clear_old_values(priv->nput);
	metric_clear_old_values(priv->nfail);
	metric_clear_old_values(priv->npagealloc);
	metric_clear_old_values(priv->npagefree);
	metric_clear_old_values(priv->hiwat);
	metric_clear_old_values(priv->nidle);

	return (0);
}

static void
pools_free(void *modpriv)
{
	struct pools_modpriv *priv = modpriv;
	free(priv);
}

struct metrics_module_ops collect_pools_ops = {
	.mm_register = pools_register,
	.mm_collect = pools_collect,
	.mm_free = pools_free
};
