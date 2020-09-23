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

#include "metrics.h"
#include "log.h"

struct procs_modpriv {
	struct metric *nfiles, *nprocs, *nthreads;
	struct metric *maxfiles, *maxproc, *maxthread;
};

struct metric_ops procs_metric_ops = {
	.mo_collect = NULL,
	.mo_free = NULL
};

static void
procs_register(struct registry *r, void **modpriv)
{
	struct procs_modpriv *priv;

	priv = calloc(1, sizeof (struct procs_modpriv));
	*modpriv = priv;

	priv->nfiles = metric_new(r, "system_files_open",
	    "Total number of files open on the system",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
	    NULL);
	priv->maxfiles = metric_new(r, "system_max_files_open",
	    "Maximum number of files which can be open on the system",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
	    NULL);
	priv->nprocs = metric_new(r, "system_processes_running",
	    "Total number of processes running on the system",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
	    NULL);
	priv->maxproc = metric_new(r, "system_max_processes_running",
	    "Maximum number of processes which can be running on the system",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
	    NULL);
	priv->nthreads = metric_new(r, "system_threads_running",
	    "Total number of threads running on the system",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
	    NULL);
	priv->maxthread = metric_new(r, "system_max_threads_running",
	    "Maximum number of threads which can be running on the system",
	    METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
	    NULL);
}

static int
procs_collect(void *modpriv)
{
	struct procs_modpriv *priv = modpriv;
	size_t size;
	int v;
	int mib[3] = { CTL_KERN, 0 };

	size = sizeof (int);
	mib[1] = KERN_NFILES;
	if (sysctl(mib, 2, &v, &size, NULL, 0) == -1) {
		tslog("failed to get stats: %s", strerror(errno));
		return (0);
	}
	metric_update(priv->nfiles, (uint64_t)v);

	size = sizeof (int);
	mib[1] = KERN_NPROCS;
	if (sysctl(mib, 2, &v, &size, NULL, 0) == -1) {
		tslog("failed to get stats: %s", strerror(errno));
		return (0);
	}
	metric_update(priv->nprocs, (uint64_t)v);

	size = sizeof (int);
	mib[1] = KERN_NTHREADS;
	if (sysctl(mib, 2, &v, &size, NULL, 0) == -1) {
		tslog("failed to get stats: %s", strerror(errno));
		return (0);
	}
	metric_update(priv->nthreads, (uint64_t)v);

	size = sizeof (int);
	mib[1] = KERN_MAXFILES;
	if (sysctl(mib, 2, &v, &size, NULL, 0) == -1) {
		tslog("failed to get stats: %s", strerror(errno));
		return (0);
	}
	metric_update(priv->maxfiles, (uint64_t)v);

	size = sizeof (int);
	mib[1] = KERN_MAXPROC;
	if (sysctl(mib, 2, &v, &size, NULL, 0) == -1) {
		tslog("failed to get stats: %s", strerror(errno));
		return (0);
	}
	metric_update(priv->maxproc, (uint64_t)v);

	size = sizeof (int);
	mib[1] = KERN_MAXTHREAD;
	if (sysctl(mib, 2, &v, &size, NULL, 0) == -1) {
		tslog("failed to get stats: %s", strerror(errno));
		return (0);
	}
	metric_update(priv->maxthread, (uint64_t)v);

	return (0);
}

static void
procs_free(void *modpriv)
{
	struct procs_modpriv *priv = modpriv;
	free(priv);
}

struct metrics_module_ops collect_procs_ops = {
	.mm_register = procs_register,
	.mm_collect = procs_collect,
	.mm_free = procs_free
};
