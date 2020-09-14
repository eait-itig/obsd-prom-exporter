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

#if !defined(_METRICS_H)
#define _METRICS_H

#include <stdint.h>
#include <stdio.h>

struct metric;
struct label;
struct registry;

struct metrics_module_ops {
	void (*mm_register)(struct registry *db, void **modprivate);
	int (*mm_collect)(void *modprivate);
	void (*mm_free)(void *modprivate);
};

struct metric_ops {
	int (*mo_collect)(struct metric *, void *private);
	void (*mo_free)(void *private);
};

enum metric_val_type {
	METRIC_VAL_STRING,
	METRIC_VAL_INT64,
	METRIC_VAL_UINT64,
	METRIC_VAL_DOUBLE
};

enum metric_type {
	METRIC_GAUGE,
	METRIC_COUNTER
};

struct label *metric_label_new(const char *name, enum metric_val_type type);

struct metric *metric_new(struct registry *r, const char *name,
    const char *help, enum metric_type type, enum metric_val_type vtype,
    void *priv, const struct metric_ops *ops,
    ... /* struct label *, NULL */);

/* Removes all metric values, new and old */
void metric_clear(struct metric *m);
/*
 * Removes all metric values which have not been updated in the current
 * collection cycle (i.e. since registry_collect() was called)
 */
void metric_clear_old_values(struct metric *m);

/* Pushes a metric value, assuming no other value with the same labels exists */
int metric_push(struct metric *m, ... /* label values, metric value */);

/* Increments a metric value */
int metric_inc(struct metric *m, ... /* label values */);
/* Updates a metric value to a new value */
int metric_update(struct metric *m, ... /* label values, metric value */);

struct registry *registry_build(void);
struct registry *registry_new_empty(void);
void registry_free(struct registry *);
int registry_collect(struct registry *r);

void print_metric(FILE *f, const struct metric *m);
void print_registry(FILE *f, const struct registry *r);

#endif /* _METRICS_H */
