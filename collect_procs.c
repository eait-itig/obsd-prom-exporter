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
#include <sys/timeout.h>

struct procs_modpriv {
	struct timeoutstat tstats;
	struct metric *added, *cancelled, *deleted, *late;
	struct metric *pending, *readded, *rescheduled, *run_softclock;
	struct metric *run_thread, *scheduled, *softclocks, *thread_wakeups;
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

	priv->added = metric_new(r, "timeouts_added",
	    "timeout_add*(9) calls",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
	    NULL);
	priv->cancelled = metric_new(r, "timeouts_cancelled",
            "dequeued during timeout_del*(9)",
            METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
            NULL);
	priv->deleted = metric_new(r, "timeouts_deleted",
            "timeout_del*(9) calls",
            METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
            NULL);
	priv->late = metric_new(r, "timeouts_late",
            "run after deadline",
            METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
            NULL);
	priv->pending = metric_new(r, "timeouts_pending",
            "number currently ONQUEUE",
            METRIC_GAUGE, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
            NULL);
	priv->readded = metric_new(r, "timeouts_readded",
            "timeout_add*(9) + already ONQUEUE",
            METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
            NULL);
	priv->rescheduled = metric_new(r, "timeouts_rescheduled",
            "bucketed + already SCHEDULED",
            METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
            NULL);
	priv->run_softclock = metric_new(r, "timeouts_run_softclock",
            "run from softclock()",
            METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
            NULL);
	priv->run_thread = metric_new(r, "timeouts_run_thread",
            "run from softclock_thread()",
            METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
            NULL);
	priv->scheduled = metric_new(r, "timeouts_scheduled",
            "bucketed during softclock()",
            METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
            NULL);
	priv->softclocks = metric_new(r, "timeouts_softclocks",
            "softclock() calls",
            METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
            NULL);
	priv->thread_wakeups = metric_new(r, "timeouts_thread_wakeups",
            "wakeups in softclock_thread()",
            METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &procs_metric_ops,
            NULL);
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

	size = sizeof(priv->tstats);
	mib[1] = KERN_TIMEOUT_STATS;
	if (sysctl(mib, 2, &priv->tstats, &size, NULL, 0) == -1) {
		tslog("failed to get stats: %s", strerror(errno));
		return (0);
	}
	metric_update(priv->added, priv->tstats.tos_added);
	metric_update(priv->cancelled, priv->tstats.tos_cancelled);
	metric_update(priv->deleted, priv->tstats.tos_deleted);
	metric_update(priv->late, priv->tstats.tos_late);
	metric_update(priv->pending, priv->tstats.tos_pending);
	metric_update(priv->readded, priv->tstats.tos_readded);
	metric_update(priv->rescheduled, priv->tstats.tos_rescheduled);
	metric_update(priv->run_softclock, priv->tstats.tos_run_softclock);
	metric_update(priv->run_thread, priv->tstats.tos_run_thread);
	metric_update(priv->scheduled, priv->tstats.tos_scheduled);
	metric_update(priv->softclocks, priv->tstats.tos_softclocks);
	metric_update(priv->thread_wakeups, priv->tstats.tos_thread_wakeups);

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
