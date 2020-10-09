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
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <err.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/tree.h>
#include <sys/queue.h>

#include "log.h"
#include "metrics.h"

/*
 * Counters exceeding this value will be wrapped to avoid precision issues
 * in prometheus (it expects exporters to be using double floating point
 * internally).
 */
const uint64_t MAX_COUNTER_MASK = (1ULL << 53) - 1;


extern struct metrics_module_ops
    collect_kstat_ops, collect_proc_ops, collect_zfs_ops;
static struct metrics_module_ops *modops[] = {
	&collect_kstat_ops,
	&collect_proc_ops,
	&collect_zfs_ops,
	NULL
};


struct metrics_module {
	struct metrics_module *next;
	struct metrics_module_ops *ops;
	void *private;
};

struct registry {
	struct metrics_module *mods;
	struct metric *metrics;
	/* cached temporary metric_val for metric_update */
	struct metric_val *tmp;
};

struct metric {
	struct metric *next;
	struct registry *owner;
	char *name;
	char *help;
	enum metric_type type;
	enum metric_val_type val_type;
	struct label *labels;
	RB_HEAD(mvaltree, metric_val) values;
	LIST_HEAD(mvallist, metric_val) old_values;

	void *priv;
	struct metric_ops ops;
};

struct label {
	struct label *next;
	struct metric *owner;
	char *name;
	enum metric_val_type val_type;
};

struct label_val {
	struct label_val *next;
	struct label *label;
	union {
		char *val_string;
		int64_t val_int64;
		uint64_t val_uint64;
		double val_double;
	};
};

struct metric_val {
	RB_ENTRY(metric_val) entry;
	LIST_ENTRY(metric_val) lentry;

	int updated;

	struct metric *metric;
	struct label_val *labels;
	union {
		char *val_string;
		int64_t val_int64;
		uint64_t val_uint64;
		double val_double;
	};
};

const double EPSILON = 1e-8;

static int
compare_label_val(const struct label_val *a, const struct label_val *b)
{
	double delta;
	if (a->label != b->label)
		errx(EXIT_ERROR, "invalid label chain comparison");
	switch (a->label->val_type) {
	case METRIC_VAL_STRING:
		if (a->val_string == NULL && b->val_string != NULL)
			return (-1);
		if (a->val_string != NULL && b->val_string == NULL)
			return (1);
		if (a->val_string == b->val_string)
			return (0);
		return (strcmp(a->val_string, b->val_string));
	case METRIC_VAL_UINT64:
		if (a->val_uint64 < b->val_uint64)
			return (-1);
		if (a->val_uint64 > b->val_uint64)
			return (-1);
		return (0);
	case METRIC_VAL_INT64:
		if (a->val_int64 < b->val_int64)
			return (-1);
		if (a->val_int64 > b->val_int64)
			return (-1);
		return (0);
	case METRIC_VAL_DOUBLE:
		delta = a->val_double - b->val_double;
		if (delta >= 0 && delta < EPSILON)
			return (0);
		if (delta <= 0 && delta > -EPSILON)
			return (0);
		if (delta < 0)
			return (-1);
		if (delta > 0)
			return (1);
		return (0);
	}
	return (0);
}

static int
compare_label_vals(const struct label_val *a, const struct label_val *b)
{
	while (a != NULL && b != NULL) {
		int res = compare_label_val(a, b);
		if (res != 0)
			return (res);
		a = a->next;
		b = b->next;
	}
	return (0);
}

static int
compare_metric_vals(const struct metric_val *a, const struct metric_val *b)
{
	if (a->metric != b->metric) {
		const int namecmp = strcmp(a->metric->name, b->metric->name);
		if (namecmp != 0)
			return (namecmp);
	}
	return (compare_label_vals(a->labels, b->labels));
}

RB_GENERATE_STATIC(mvaltree, metric_val, entry, compare_metric_vals);

static void
free_label(struct label *l)
{
	free(l->name);
	free(l);
}

static void
free_label_val(struct label_val *v)
{
	if (v->label->val_type == METRIC_VAL_STRING)
		free(v->val_string);
	free(v);
}

static void
free_metric_val_content(struct metric_val *v)
{
	struct label_val *lv, *nlv;
	lv = v->labels;
	while (lv != NULL) {
		nlv = lv->next;
		free_label_val(lv);
		lv = nlv;
	}
	if (v->metric->val_type == METRIC_VAL_STRING &&
	    v->val_string != NULL) {
		free(v->val_string);
	}
}

static void
free_metric_val(struct metric_val *v)
{
	free_metric_val_content(v);
	free(v);
}

void
metric_clear(struct metric *m)
{
	struct metric_val *v, *nv;
	v = RB_MIN(mvaltree, &m->values);
	while (v != NULL) {
		nv = RB_NEXT(mvaltree, &m->values, v);
		RB_REMOVE(mvaltree, &m->values, v);
		if (v->updated == 0)
			LIST_REMOVE(v, lentry);
		free_metric_val(v);
		v = nv;
	}
}

static void
free_metric(struct metric *m)
{
	struct label *l, *nl;

	if (m->priv != NULL)
		m->ops.mo_free(m->priv);
	free(m->name);
	free(m->help);

	metric_clear(m);

	l = m->labels;
	while (l != NULL) {
		nl = l->next;
		free_label(l);
		l = nl;
	}

	free(m);
}

struct label *
metric_label_new(const char *name, enum metric_val_type type)
{
	struct label *l;
	l = calloc(1, sizeof (struct label));
	l->name = strdup(name);
	l->val_type = type;
	return (l);
}

struct metric *
metric_new(struct registry *r, const char *name, const char *help,
    enum metric_type type, enum metric_val_type vtype, void *priv,
    const struct metric_ops *ops, ...)
{
	struct metric *m;
	va_list va;
	struct label *l, *pl;

	m = calloc(1, sizeof (struct metric));

	m->name = strdup(name);
	m->help = strdup(help);
	m->type = type;
	m->val_type = vtype;
	m->owner = r;

	RB_INIT(&m->values);
	LIST_INIT(&m->old_values);

	m->priv = priv;
	m->ops = *ops;

	m->next = r->metrics;
	r->metrics = m;

	pl = NULL;
	va_start(va, ops);
	while ((l = va_arg(va, struct label *)) != NULL) {
		if (pl != NULL)
			pl->next = l;
		else
			m->labels = l;
		l->owner = m;
		pl = l;
	}
	if (pl != NULL)
		pl->next = NULL;
	va_end(va);

	return (m);
}

static struct label_val *
vlabels(struct metric *m, va_list va)
{
	struct label *lbl;
	struct label_val *v;
	struct label_val *firstv = NULL, *lastv = NULL;
	const char *strv;

	lbl = m->labels;
	while (lbl != NULL) {
		v = calloc(1, sizeof (struct label_val));
		v->label = lbl;
		switch (lbl->val_type) {
		case METRIC_VAL_STRING:
			strv = va_arg(va, const char *);
			v->val_string = (strv == NULL) ? NULL : strdup(strv);
			break;
		case METRIC_VAL_INT64:
			v->val_int64 = va_arg(va, int64_t);
			break;
		case METRIC_VAL_UINT64:
			v->val_uint64 = va_arg(va, uint64_t);
			break;
		case METRIC_VAL_DOUBLE:
			v->val_double = va_arg(va, double);
			break;
		}
		if (firstv == NULL)
			firstv = v;
		if (lastv != NULL)
			lastv->next = v;
		lastv = v;

		lbl = lbl->next;
	}

	return (firstv);
}

int
metric_push(struct metric *m, ...)
{
	struct metric_val *mv;
	va_list va;

	mv = calloc(1, sizeof (struct metric_val));
	mv->metric = m;
	mv->updated = 1;

	va_start(va, m);
	mv->labels = vlabels(m, va);
	switch (m->val_type) {
	case METRIC_VAL_STRING:
		mv->val_string = strdup(va_arg(va, const char *));
		break;
	case METRIC_VAL_INT64:
		mv->val_int64 = va_arg(va, int64_t);
		break;
	case METRIC_VAL_UINT64:
		mv->val_uint64 = va_arg(va, uint64_t);
		break;
	case METRIC_VAL_DOUBLE:
		mv->val_double = va_arg(va, double);
		break;
	}
	va_end(va);

	RB_INSERT(mvaltree, &m->values, mv);

	return (0);
}

int
metric_inc(struct metric *m, ...)
{
	struct metric_val *mv, *omv;
	va_list va;

	mv = calloc(1, sizeof (struct metric_val));
	mv->metric = m;
	mv->updated = 1;

	va_start(va, m);
	mv->labels = vlabels(m, va);
	switch (m->val_type) {
	case METRIC_VAL_INT64:
		mv->val_int64 = 1;
		break;
	case METRIC_VAL_UINT64:
		mv->val_uint64 = 1;
		break;
	case METRIC_VAL_DOUBLE:
		mv->val_double = 1.0;
		break;
	case METRIC_VAL_STRING:
		return (EINVAL);
	}
	va_end(va);

	omv = RB_FIND(mvaltree, &m->values, mv);
	if (omv != NULL) {
		if (omv->updated == 0) {
			LIST_REMOVE(omv, lentry);
			omv->updated = 1;
		}
		switch (m->val_type) {
		case METRIC_VAL_INT64:
			omv->val_int64++;
			break;
		case METRIC_VAL_UINT64:
			omv->val_uint64++;
			break;
		case METRIC_VAL_DOUBLE:
			omv->val_double += 1.0;
			break;
		case METRIC_VAL_STRING:
			return (EINVAL);
		}
		free_metric_val(mv);
		return (0);
	}

	RB_INSERT(mvaltree, &m->values, mv);

	return (0);
}

int
metric_inc_by(struct metric *m, ...)
{
	struct metric_val *mv, *omv;
	va_list va;

	mv = calloc(1, sizeof (struct metric_val));
	mv->metric = m;
	mv->updated = 1;

	va_start(va, m);
	mv->labels = vlabels(m, va);
	switch (m->val_type) {
	case METRIC_VAL_STRING:
		return (EINVAL);
	case METRIC_VAL_INT64:
		mv->val_int64 = va_arg(va, int64_t);
		break;
	case METRIC_VAL_UINT64:
		mv->val_uint64 = va_arg(va, uint64_t);
		break;
	case METRIC_VAL_DOUBLE:
		mv->val_double = va_arg(va, double);
		break;
	}
	va_end(va);

	omv = m->values;
	while (omv != NULL) {
		if (compare_label_vals(mv->labels, omv->labels) == 0) {
			omv->updated = 1;
			switch (m->val_type) {
			case METRIC_VAL_INT64:
				omv->val_int64 += mv->val_int64;
				break;
			case METRIC_VAL_UINT64:
				omv->val_uint64 += mv->val_uint64;
				break;
			case METRIC_VAL_DOUBLE:
				omv->val_double += mv->val_double;
				break;
			case METRIC_VAL_STRING:
				return (EINVAL);
			}
			free_metric_val(mv);
			return (0);
		}
		omv = omv->next;
	}

	mv->next = m->values;
	m->values = mv;

	return (0);
}

int
metric_update(struct metric *m, ...)
{
	struct metric_val *mv;
	struct metric_val *tmp;
	struct metric_val *omv;
	struct registry *r;
	va_list va;

	r = m->owner;
	if (r->tmp == NULL)
		r->tmp = malloc(sizeof (struct metric_val));
	mv = r->tmp;
	bzero(mv, sizeof (struct metric_val));

	mv->metric = m;
	mv->updated = 1;

	va_start(va, m);
	mv->labels = vlabels(m, va);
	switch (m->val_type) {
	case METRIC_VAL_STRING:
		mv->val_string = strdup(va_arg(va, const char *));
		break;
	case METRIC_VAL_INT64:
		mv->val_int64 = va_arg(va, int64_t);
		break;
	case METRIC_VAL_UINT64:
		mv->val_uint64 = va_arg(va, uint64_t);
		break;
	case METRIC_VAL_DOUBLE:
		mv->val_double = va_arg(va, double);
		break;
	}
	va_end(va);

	omv = RB_FIND(mvaltree, &m->values, mv);
	if (omv != NULL) {
		if (omv->updated == 0) {
			LIST_REMOVE(omv, lentry);
			omv->updated = 1;
		}
		switch (m->val_type) {
		case METRIC_VAL_INT64:
			omv->val_int64 = mv->val_int64;
			break;
		case METRIC_VAL_UINT64:
			omv->val_uint64 = mv->val_uint64;
			break;
		case METRIC_VAL_DOUBLE:
			omv->val_double = mv->val_double;
			break;
		case METRIC_VAL_STRING:
			free(omv->val_string);
			omv->val_string = strdup(mv->val_string);
			break;
		}
		free_metric_val_content(mv);
		return (0);
	}

	tmp = malloc(sizeof (struct metric_val));
	bcopy(mv, tmp, sizeof (struct metric_val));
	RB_INSERT(mvaltree, &m->values, tmp);

	return (0);
}

static void
print_metric_val(FILE *f, const struct metric_val *mv)
{
	const struct metric *m = mv->metric;
	const struct label_val *lv, *plv = NULL;
	uint64_t uv;

	fprintf(f, "%s", m->name);
	lv = mv->labels;
	if (lv != NULL) {
		fprintf(f, "{");
		for (; lv != NULL; lv = lv->next) {
			if (lv->label->val_type == METRIC_VAL_STRING &&
			    lv->val_string == NULL)
				continue;
			if (plv != NULL)
				fprintf(f, ", ");
			fprintf(f, "%s=", lv->label->name);
			switch (lv->label->val_type) {
			case METRIC_VAL_STRING:
				fprintf(f, "\"%s\"", lv->val_string);
				break;
			case METRIC_VAL_INT64:
				fprintf(f, "\"%" PRId64 "\"", lv->val_int64);
				break;
			case METRIC_VAL_UINT64:
				uv = lv->val_uint64;
				fprintf(f, "\"%" PRIu64 "\"", uv);
				break;
			case METRIC_VAL_DOUBLE:
				fprintf(f, "\"%f\"", lv->val_double);
				break;
			}
			plv = lv;
		}
		fprintf(f, "}");
	}
	fprintf(f, "\t");
	switch (m->val_type) {
	case METRIC_VAL_STRING:
		fprintf(f, "%s\n", mv->val_string);
		break;
	case METRIC_VAL_INT64:
		fprintf(f, "%" PRId64 "\n", mv->val_int64);
		break;
	case METRIC_VAL_UINT64:
		uv = mv->val_uint64;
		if (m->type == METRIC_COUNTER)
			uv &= MAX_COUNTER_MASK;
		fprintf(f, "%" PRIu64 "\n", uv);
		break;
	case METRIC_VAL_DOUBLE:
		fprintf(f, "%f\n", mv->val_double);
		break;
	}
}

void
print_metric(FILE *f, const struct metric *m)
{
	const struct metric_val *mv;
	const char *type;

	fprintf(f, "# HELP %s %s\n", m->name, m->help);
	switch (m->type) {
	case METRIC_GAUGE:
		type = "gauge";
		break;
	case METRIC_COUNTER:
		type = "counter";
		break;
	}
	fprintf(f, "# TYPE %s %s\n", m->name, type);

	mv = RB_MIN(mvaltree, (struct mvaltree *)&m->values);
	while (mv != NULL) {
		print_metric_val(f, mv);
		mv = RB_NEXT(mvaltree, (struct mvaltree *)&m->values,
		    (struct metric_val *)mv);
	}
}

void
print_registry(FILE *f, const struct registry *r)
{
	const struct metric *m;

	m = r->metrics;
	while (m != NULL) {
		print_metric(f, m);
		m = m->next;
	}
}

struct registry *
registry_new_empty(void)
{
	struct registry *r;

	r = calloc(1, sizeof (struct registry));
	return (r);
}

void
registry_free(struct registry *r)
{
	struct metric *m, *nm;
	struct metrics_module *mod, *nmod;

	free(r->tmp);

	m = r->metrics;
	while (m != NULL) {
		nm = m->next;
		free_metric(m);
		m = nm;
	}

	mod = r->mods;
	while (mod != NULL) {
		nmod = mod->next;
		if (mod->private != NULL)
			mod->ops->mm_free(mod->private);
		free(mod);
		mod = nmod;
	}

	free(r);
}

struct registry *
registry_build(void)
{
	struct registry *r;
	struct metrics_module *mod;
	size_t i;

	r = calloc(1, sizeof (struct registry));

	for (i = 0; modops[i] != NULL; ++i) {
		mod = calloc(1, sizeof (struct metrics_module));
		mod->ops = modops[i];

		mod->next = r->mods;
		r->mods = mod;

		mod->ops->mm_register(r, &mod->private);
	}

	return (r);
}

int
registry_collect(struct registry *r)
{
	struct metric *m;
	struct metric_val *mv;
	struct metrics_module *mod;
	int rc;

	m = r->metrics;
	while (m != NULL) {
		mv = RB_MIN(mvaltree, &m->values);
		while (mv != NULL) {
			mv->updated = 0;
			LIST_INSERT_HEAD(&m->old_values, mv, lentry);
			mv = RB_NEXT(mvaltree, &m->values, mv);
		}
		m = m->next;
	}

	mod = r->mods;
	while (mod != NULL) {
		if (mod->ops->mm_collect != NULL) {
			rc = mod->ops->mm_collect(mod->private);
			if (rc != 0)
				return (rc);
		}
		mod = mod->next;
	}

	m = r->metrics;
	while (m != NULL) {
		if (m->ops.mo_collect != NULL) {
			rc = m->ops.mo_collect(m, m->priv);
			if (rc != 0)
				return (rc);
		}
		m = m->next;
	}

	return (0);
}

void
metric_clear_old_values(struct metric *m)
{
	struct metric_val *mv;
	while (!LIST_EMPTY(&m->old_values)) {
		mv = LIST_FIRST(&m->old_values);
		LIST_REMOVE(mv, lentry);
		RB_REMOVE(mvaltree, &m->values, mv);
		free_metric_val(mv);
	}
}

