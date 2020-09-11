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
#include <sys/sched.h>

#include "metrics.h"
#include "log.h"

struct cpu_modpriv {
	struct cpustats stats;
	struct metric *cpu_time;
};

struct metric_ops cpu_metric_ops = {
	.mo_collect = NULL,
	.mo_free = NULL
};

static void
cpu_register(struct registry *r, void **modpriv)
{
	struct cpu_modpriv *priv;

	priv = calloc(1, sizeof (struct cpu_modpriv));
	*modpriv = priv;

	priv->cpu_time = metric_new(r, "cpu_time_spent_total",
	    "Time spent in different CPU states",
	    METRIC_COUNTER, METRIC_VAL_UINT64, NULL, &cpu_metric_ops,
	    metric_label_new("state", METRIC_VAL_STRING),
	    NULL);
}

static int
cpu_collect(void *modpriv)
{
	struct cpu_modpriv *priv = modpriv;
	size_t size = sizeof (priv->stats);
	int mib[3] = { CTL_KERN, KERN_CPUSTATS, 0 };

	if (sysctl(mib, 3, &priv->stats, &size, NULL, 0) == -1) {
		tslog("failed to get cpu stats: %s", strerror(errno));
		return (0);
	}

	metric_update(priv->cpu_time, "user", priv->stats.cs_time[CP_USER]);
	metric_update(priv->cpu_time, "nice", priv->stats.cs_time[CP_NICE]);
	metric_update(priv->cpu_time, "sys", priv->stats.cs_time[CP_SYS]);
	metric_update(priv->cpu_time, "spin", priv->stats.cs_time[CP_SPIN]);
	metric_update(priv->cpu_time, "intr", priv->stats.cs_time[CP_INTR]);
	metric_update(priv->cpu_time, "idle", priv->stats.cs_time[CP_IDLE]);

	return (0);
}

static void
cpu_free(void *modpriv)
{
	struct cpu_modpriv *priv = modpriv;
	free(priv);
}

struct metrics_module_ops collect_cpu_ops = {
	.mm_register = cpu_register,
	.mm_collect = cpu_collect,
	.mm_free = cpu_free
};
