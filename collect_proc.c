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

#include <libproc.h>
#include <zone.h>

#include "metrics.h"
#include "log.h"

struct proc_modpriv {
	struct metric *count, *count_lwps;
};

struct metric_ops proc_metric_ops = {
	.mo_collect = NULL,
	.mo_free = NULL
};

static void
proc_register(struct registry *r, void **modpriv)
{
	struct proc_modpriv *priv;

	priv = calloc(1, sizeof (struct proc_modpriv));
	*modpriv = priv;

	priv->count = metric_new(r, "proc_count",
	    "Count of processes running in zone",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &proc_metric_ops,
	    metric_label_new("zonename", METRIC_VAL_STRING),
	    metric_label_new("execname", METRIC_VAL_STRING),
	    NULL);
	priv->count_lwps = metric_new(r, "proc_thread_count",
	    "Count of all threads running on zone",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &proc_metric_ops,
	    metric_label_new("zonename", METRIC_VAL_STRING),
	    NULL);
}

static int
proc_walker(psinfo_t *psinfo, lwpsinfo_t *lpwsinfo, void *arg)
{
	struct proc_modpriv *priv = (struct proc_modpriv *)arg;
	char zname[ZONENAME_MAX];

	bzero(zname, sizeof (zname));
	getzonenamebyid(psinfo->pr_zoneid, zname, sizeof (zname));

	if (strstr(psinfo->pr_fname, "smbd") != NULL) {
		metric_inc(priv->count, zname, psinfo->pr_fname);
	}

	metric_inc(priv->count, zname, NULL);
	metric_inc_by(priv->count_lwps, zname, psinfo->pr_nlwp);

	return (0);
}

static int
proc_collect(void *modpriv)
{
	struct proc_modpriv *priv = modpriv;
	int r;

	metric_clear(priv->count);
	metric_clear(priv->count_lwps);

	r = proc_walk(proc_walker, priv, PR_WALK_PROC);
	if (r < 0) {
		tslog("failed to walk procs: %s", strerror(errno));
		return (0);
	}

	return (0);
}

static void
proc_free(void *modpriv)
{
	struct proc_modpriv *priv = modpriv;
	free(priv);
}

struct metrics_module_ops collect_proc_ops = {
	.mm_register = proc_register,
	.mm_collect = proc_collect,
	.mm_free = proc_free
};
