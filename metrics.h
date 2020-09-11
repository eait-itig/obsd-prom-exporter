#if !defined(_METRICS_H)
#define _METRICS_H

#include <stdint.h>
#include <stdio.h>

struct metric;
struct label;
struct registry;

struct metrics_module_ops {
	void (*mm_register)(struct registry *db, void **modprivate);
	void (*mm_free)(void *modprivate);
};

struct metric_ops {
	int (*mo_collect)(struct metric *, void *private);
	void (*mo_free)(void *private);
};

enum metric_val_type {
	METRIC_VAL_STRING,
	METRIC_VAL_INT64,
	METRIC_VAL_DOUBLE
};

enum metric_type {
	METRIC_GAUGE,
	METRIC_COUNTER
};

struct label *metric_new_label(const char *name, enum metric_val_type type);

struct metric *metric_new(struct registry *r, const char *name,
    const char *help, enum metric_type type, enum metric_val_type vtype,
    void *priv, const struct metric_ops *ops, ...);

void metric_clear(struct metric *m);
int metric_push(struct metric *m, ...);
int metric_inc(struct metric *m, ...);
int metric_update(struct metric *m, ...);

struct registry *registry_build(void);
struct registry *registry_new_empty(void);
void registry_free(struct registry *);
int registry_collect(struct registry *r);

void print_metric(FILE *f, const struct metric *m);
void print_registry(FILE *f, const struct registry *r);

#endif /* _METRICS_H */
