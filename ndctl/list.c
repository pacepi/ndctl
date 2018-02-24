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
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include <util/json.h>
#include <util/filter.h>
#include <json-c/json.h>
#include <ndctl/libndctl.h>
#include <util/parse-options.h>
#include <ccan/array_size/array_size.h>

#include <ndctl.h>

static struct {
	bool buses;
	bool dimms;
	bool regions;
	bool namespaces;
	bool idle;
	bool health;
	bool dax;
	bool media_errors;
	bool human;
	bool firmware;
} list;

static unsigned long listopts_to_flags(void)
{
	unsigned long flags = 0;

	if (list.idle)
		flags |= UTIL_JSON_IDLE;
	if (list.media_errors)
		flags |= UTIL_JSON_MEDIA_ERRORS;
	if (list.dax)
		flags |= UTIL_JSON_DAX | UTIL_JSON_DAX_DEVS;
	if (list.human)
		flags |= UTIL_JSON_HUMAN;
	return flags;
}

struct util_filter_params param;

static int did_fail;
static int jflag = JSON_C_TO_STRING_PRETTY;

#define fail(fmt, ...) \
do { \
	did_fail = 1; \
	fprintf(stderr, "ndctl-%s:%s:%d: " fmt, \
			VERSION, __func__, __LINE__, ##__VA_ARGS__); \
} while (0)

static struct json_object *region_to_json(struct ndctl_region *region,
		unsigned long flags)
{
	struct json_object *jregion = json_object_new_object();
	struct json_object *jobj, *jbbs, *jmappings = NULL;
	struct ndctl_interleave_set *iset;
	struct ndctl_mapping *mapping;
	unsigned int bb_count = 0;

	if (!jregion)
		return NULL;

	jobj = json_object_new_string(ndctl_region_get_devname(region));
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "dev", jobj);

	jobj = util_json_object_size(ndctl_region_get_size(region), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "size", jobj);

	jobj = util_json_object_size(ndctl_region_get_available_size(region),
			flags);
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "available_size", jobj);

	switch (ndctl_region_get_type(region)) {
	case ND_DEVICE_REGION_PMEM:
		jobj = json_object_new_string("pmem");
		break;
	case ND_DEVICE_REGION_BLK:
		jobj = json_object_new_string("blk");
		break;
	default:
		jobj = NULL;
	}
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "type", jobj);

	iset = ndctl_region_get_interleave_set(region);
	if (iset) {
		jobj = util_json_object_hex(
				ndctl_interleave_set_get_cookie(iset), flags);
		if (!jobj)
			fail("\n");
		else
			json_object_object_add(jregion, "iset_id", jobj);
	}

	ndctl_mapping_foreach(region, mapping) {
		struct ndctl_dimm *dimm = ndctl_mapping_get_dimm(mapping);
		struct json_object *jmapping;

		if (!list.dimms)
			break;

		if (!util_dimm_filter(dimm, param.dimm))
			continue;

		if (!list.idle && !ndctl_dimm_is_enabled(dimm))
			continue;

		if (!jmappings) {
			jmappings = json_object_new_array();
			if (!jmappings) {
				fail("\n");
				continue;
			}
			json_object_object_add(jregion, "mappings", jmappings);
		}

		jmapping = util_mapping_to_json(mapping, listopts_to_flags());
		if (!jmapping) {
			fail("\n");
			continue;
		}
		json_object_array_add(jmappings, jmapping);
	}

	if (!ndctl_region_is_enabled(region)) {
		jobj = json_object_new_string("disabled");
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "state", jobj);
	}

	jbbs = util_region_badblocks_to_json(region, &bb_count, flags);
	if (bb_count) {
		jobj = json_object_new_int(bb_count);
		if (!jobj) {
			json_object_put(jbbs);
			goto err;
		}
		json_object_object_add(jregion, "badblock_count", jobj);
	}
	if ((flags & UTIL_JSON_MEDIA_ERRORS) && jbbs)
		json_object_object_add(jregion, "badblocks", jbbs);

	return jregion;
 err:
	fail("\n");
	json_object_put(jregion);
	return NULL;
}

static void filter_namespace(struct ndctl_namespace *ndns,
		struct util_filter_ctx *ctx)
{
	struct json_object *jndns;
	struct list_filter_arg *lfa = ctx->list;
	struct json_object *container = lfa->jregion ? lfa->jregion : lfa->jbus;

	if (!list.idle && !ndctl_namespace_is_active(ndns))
		return;

	if (!lfa->jnamespaces) {
		lfa->jnamespaces = json_object_new_array();
		if (!lfa->jnamespaces) {
			fail("\n");
			return;
		}

		if (container)
			json_object_object_add(container, "namespaces",
					lfa->jnamespaces);
	}

	jndns = util_namespace_to_json(ndns, lfa->flags);
	if (!jndns) {
		fail("\n");
		return;
	}

	json_object_array_add(lfa->jnamespaces, jndns);
}

static bool filter_region(struct ndctl_region *region,
		struct util_filter_ctx *ctx)
{
	struct list_filter_arg *lfa = ctx->list;
	struct json_object *jbus = lfa->jbus;
	struct json_object *jregion;

	if (!list.regions)
		return true;

	if (!list.idle && !ndctl_region_is_enabled(region))
		return true;

	if (!lfa->jregions) {
		lfa->jregions = json_object_new_array();
		if (!lfa->jregions) {
			fail("\n");
			return false;
		}

		if (jbus)
			json_object_object_add(jbus, "regions",
					lfa->jregions);
	}

	jregion = region_to_json(region, lfa->flags);
	if (!jregion) {
		fail("\n");
		return false;
	}
	lfa->jregion = jregion;

	/*
	 * Without a bus we are collecting regions anonymously across
	 * the platform.
	 */
	json_object_array_add(lfa->jregions, jregion);
	return true;
}

static void filter_dimm(struct ndctl_dimm *dimm, struct util_filter_ctx *ctx)
{
	struct list_filter_arg *lfa = ctx->list;
	struct json_object *jdimm;

	if (!list.idle && !ndctl_dimm_is_enabled(dimm))
		return;

	if (!lfa->jdimms) {
		lfa->jdimms = json_object_new_array();
		if (!lfa->jdimms) {
			fail("\n");
			return;
		}

		if (lfa->jbus)
			json_object_object_add(lfa->jbus, "dimms", lfa->jdimms);
	}

	jdimm = util_dimm_to_json(dimm, lfa->flags);
	if (!jdimm) {
		fail("\n");
		return;
	}

	if (list.health) {
		struct json_object *jhealth;

		jhealth = util_dimm_health_to_json(dimm);
		if (jhealth)
			json_object_object_add(jdimm, "health", jhealth);
		else if (ndctl_dimm_is_cmd_supported(dimm, ND_CMD_SMART)) {
			/*
			 * Failed to retrieve health data from a dimm
			 * that otherwise supports smart data retrieval
			 * commands.
			 */
			fail("\n");
			return;
		}
	}

	if (list.firmware) {
		struct json_object *jfirmware;

		jfirmware = util_dimm_firmware_to_json(dimm, lfa->flags);
		if (jfirmware)
			json_object_object_add(jdimm, "firmware", jfirmware);
	}

	/*
	 * Without a bus we are collecting dimms anonymously across the
	 * platform.
	 */
	json_object_array_add(lfa->jdimms, jdimm);
}

static bool filter_bus(struct ndctl_bus *bus, struct util_filter_ctx *ctx)
{
	struct list_filter_arg *lfa = ctx->list;

	/*
	 * These sub-objects are local to a bus and, if present, have
	 * been added as a child of a parent object on the last
	 * iteration.
	 */
	if (lfa->jbuses) {
		lfa->jdimms = NULL;
		lfa->jregion = NULL;
		lfa->jregions = NULL;
		lfa->jnamespaces = NULL;
	}

	if (!list.buses)
		return true;

	if (!lfa->jbuses) {
		lfa->jbuses = json_object_new_array();
		if (!lfa->jbuses) {
			fail("\n");
			return false;
		}
	}

	lfa->jbus = util_bus_to_json(bus);
	if (!lfa->jbus) {
		fail("\n");
		return false;
	}
	json_object_array_add(lfa->jbuses, lfa->jbus);
	return true;
}

static int list_display(struct list_filter_arg *lfa)
{
	struct json_object *jnamespaces = lfa->jnamespaces;
	struct json_object *jregions = lfa->jregions;
	struct json_object *jdimms = lfa->jdimms;
	struct json_object *jbuses = lfa->jbuses;

	if (jbuses)
		util_display_json_array(stdout, jbuses, jflag);
	else if ((!!jdimms + !!jregions + !!jnamespaces) > 1) {
		struct json_object *jplatform = json_object_new_object();

		if (!jplatform) {
			fail("\n");
			return -ENOMEM;
		}

		if (jdimms)
			json_object_object_add(jplatform, "dimms", jdimms);
		if (jregions)
			json_object_object_add(jplatform, "regions", jregions);
		if (jnamespaces)
			json_object_object_add(jplatform, "namespaces",
					jnamespaces);
		printf("%s\n", json_object_to_json_string_ext(jplatform,
					jflag));
		json_object_put(jplatform);
	} else if (jdimms)
		util_display_json_array(stdout, jdimms, jflag);
	else if (jregions)
		util_display_json_array(stdout, jregions, jflag);
	else if (jnamespaces)
		util_display_json_array(stdout, jnamespaces, jflag);
	return 0;
}

static int num_list_flags(void)
{
	return list.buses + list.dimms + list.regions + list.namespaces;
}

int cmd_list(int argc, const char **argv, void *ctx)
{
	const struct option options[] = {
		OPT_STRING('b', "bus", &param.bus, "bus-id", "filter by bus"),
		OPT_STRING('r', "region", &param.region, "region-id",
				"filter by region"),
		OPT_STRING('d', "dimm", &param.dimm, "dimm-id",
				"filter by dimm"),
		OPT_STRING('n', "namespace", &param.namespace, "namespace-id",
				"filter by namespace id"),
		OPT_STRING('m', "mode", &param.mode, "namespace-mode",
				"filter by namespace mode"),
		OPT_STRING('t', "type", &param.type, "region-type",
				"filter by region-type"),
		OPT_BOOLEAN('B', "buses", &list.buses, "include bus info"),
		OPT_BOOLEAN('D', "dimms", &list.dimms, "include dimm info"),
		OPT_BOOLEAN('F', "firmware", &list.firmware, "include firmware info"),
		OPT_BOOLEAN('H', "health", &list.health, "include dimm health"),
		OPT_BOOLEAN('R', "regions", &list.regions,
				"include region info"),
		OPT_BOOLEAN('N', "namespaces", &list.namespaces,
				"include namespace info (default)"),
		OPT_BOOLEAN('X', "device-dax", &list.dax,
				"include device-dax info"),
		OPT_BOOLEAN('i', "idle", &list.idle, "include idle devices"),
		OPT_BOOLEAN('M', "media-errors", &list.media_errors,
				"include media errors"),
		OPT_BOOLEAN('u', "human", &list.human,
				"use human friendly number formats "),
		OPT_END(),
	};
	const char * const u[] = {
		"ndctl list [<options>]",
		NULL
	};
	struct util_filter_ctx fctx = { 0 };
	struct list_filter_arg lfa = { 0 };
	int i, rc;

        argc = parse_options(argc, argv, options, u, 0);
	for (i = 0; i < argc; i++)
		error("unknown parameter \"%s\"\n", argv[i]);
	if (argc)
		usage_with_options(u, options);

	if (num_list_flags() == 0) {
		list.buses = !!param.bus;
		list.regions = !!param.region;
		list.dimms = !!param.dimm;
		if (list.dax && !param.mode)
			param.mode = "dax";
	}

	if (num_list_flags() == 0)
		list.namespaces = true;

	fctx.filter_bus = filter_bus;
	fctx.filter_dimm = list.dimms ? filter_dimm : NULL;
	fctx.filter_region = filter_region;
	fctx.filter_namespace = list.namespaces ? filter_namespace : NULL;
	fctx.list = &lfa;
	lfa.flags = listopts_to_flags();

	rc = util_filter_walk(ctx, &fctx, &param);
	if (rc)
		return rc;

	if (list_display(&lfa) || did_fail)
		return -ENOMEM;
	return 0;
}
