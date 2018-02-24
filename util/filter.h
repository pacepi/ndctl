/*
 * Copyright(c) 2015-2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#ifndef _UTIL_FILTER_H_
#define _UTIL_FILTER_H_
#include <stdbool.h>

struct ndctl_bus *util_bus_filter(struct ndctl_bus *bus, const char *ident);
struct ndctl_region *util_region_filter(struct ndctl_region *region,
		const char *ident);
struct ndctl_namespace *util_namespace_filter(struct ndctl_namespace *ndns,
		const char *ident);
struct ndctl_dimm *util_dimm_filter(struct ndctl_dimm *dimm, const char *ident);
struct ndctl_bus *util_bus_filter_by_dimm(struct ndctl_bus *bus,
		const char *ident);
struct ndctl_bus *util_bus_filter_by_region(struct ndctl_bus *bus,
		const char *ident);
struct ndctl_bus *util_bus_filter_by_namespace(struct ndctl_bus *bus,
		const char *ident);
struct ndctl_region *util_region_filter_by_dimm(struct ndctl_region *region,
		const char *ident);
struct ndctl_dimm *util_dimm_filter_by_region(struct ndctl_dimm *dimm,
		const char *ident);
struct ndctl_dimm *util_dimm_filter_by_namespace(struct ndctl_dimm *dimm,
		const char *ident);
struct ndctl_region *util_region_filter_by_namespace(struct ndctl_region *region,
		const char *ident);
struct daxctl_dev *util_daxctl_dev_filter(struct daxctl_dev *dev,
		const char *ident);

struct json_object;

/* json object hierarchy for the util_filter_walk() performed by cmd_list() */
struct list_filter_arg {
	struct json_object *jnamespaces;
	struct json_object *jregions;
	struct json_object *jdimms;
	struct json_object *jbuses;
	struct json_object *jregion;
	struct json_object *jbus;
	unsigned long flags;
};

/*
 * struct util_filter_ctx - control and callbacks for util_filter_walk()
 * ->filter_bus() and ->filter_region() return bool because the
 * child-object filter routines can not be called if the parent context
 * is not established. ->filter_dimm() and ->filter_namespace() are leaf
 * objects, so no child dependencies to check.
 */
struct util_filter_ctx {
	bool (*filter_bus)(struct ndctl_bus *bus, struct util_filter_ctx *ctx);
	void (*filter_dimm)(struct ndctl_dimm *dimm, struct util_filter_ctx *ctx);
	bool (*filter_region)(struct ndctl_region *region,
			struct util_filter_ctx *ctx);
	void (*filter_namespace)(struct ndctl_namespace *ndns,
			struct util_filter_ctx *ctx);
	union {
		void *arg;
		struct list_filter_arg *list;
	};
};

struct util_filter_params {
	const char *bus;
	const char *region;
	const char *type;
	const char *dimm;
	const char *mode;
	const char *namespace;
};

struct ndctl_ctx;
int util_filter_walk(struct ndctl_ctx *ctx, struct util_filter_ctx *fctx,
		struct util_filter_params *param);
#endif
