#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <net/pfvar.h>

#include "metrics.h"
#include "log.h"

struct pf_modpriv {
	struct pf_status status;
	struct metric *pf_running;
	struct metric *pf_states;
};

struct metric_ops pf_metric_ops = {
	.mo_collect = NULL,
	.mo_free = NULL
};

static void
pf_register(struct registry *r, void **modpriv)
{
	struct pf_modpriv *priv;

	priv = calloc(1, sizeof (struct pf_modpriv));
	*modpriv = priv;

	priv->pf_running = metric_new(r, "pf_running",
	    "Indicates whether pf is running",
	    METRIC_GAUGE, METRIC_VAL_INT64, NULL, &pf_metric_ops, NULL);
	priv->pf_states = metric_new(r, "pf_states",
	    "Number of states currently tracked by pf",
	    METRIC_GAUGE, METRIC_VAL_INT64, NULL, &pf_metric_ops, NULL);
}

static int
pf_collect(void *modpriv)
{
	struct pf_modpriv *priv = modpriv;
	size_t size = sizeof (priv->status);
	int mib[3] = { CTL_KERN, KERN_PFSTATUS };

	metric_clear(priv->pf_running);
	metric_clear(priv->pf_states);

	if (sysctl(mib, 2, &priv->status, &size, NULL, 0) == -1) {
		tslog("failed to get pf status: %s", strerror(errno));
		return (0);
	}

	metric_update(priv->pf_running, priv->status.running);
	metric_update(priv->pf_states, priv->status.states);

	return (0);
}

static void
pf_free(void *modpriv)
{
	struct pf_modpriv *priv = modpriv;
	free(priv);
}

struct metrics_module_ops collect_pf_ops = {
	.mm_register = pf_register,
	.mm_collect = pf_collect,
	.mm_free = pf_free
};
