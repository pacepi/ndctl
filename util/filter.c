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
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <util/util.h>
#include <ndctl/ndctl.h>
#include <util/filter.h>
#include <ndctl/libndctl.h>
#include <daxctl/libdaxctl.h>

struct ndctl_bus *util_bus_filter(struct ndctl_bus *bus, const char *ident)
{
	char *end = NULL;
	unsigned long bus_id, id;
	const char *provider, *devname;

	if (!ident || strcmp(ident, "all") == 0)
		return bus;

	bus_id = strtoul(ident, &end, 0);
	if (end == ident || end[0])
		bus_id = ULONG_MAX;

	provider = ndctl_bus_get_provider(bus);
	devname = ndctl_bus_get_devname(bus);
	id = ndctl_bus_get_id(bus);

	if (bus_id < ULONG_MAX && bus_id == id)
		return bus;

	if (bus_id == ULONG_MAX && (strcmp(ident, provider) == 0
				|| strcmp(ident, devname) == 0))
		return bus;

	return NULL;
}

struct ndctl_region *util_region_filter(struct ndctl_region *region,
		const char *ident)
{
	char *end = NULL;
	const char *name;
	unsigned long region_id, id;

	if (!ident || strcmp(ident, "all") == 0)
		return region;

	region_id = strtoul(ident, &end, 0);
	if (end == ident || end[0])
		region_id = ULONG_MAX;

	name = ndctl_region_get_devname(region);
	id = ndctl_region_get_id(region);

	if (region_id < ULONG_MAX && region_id == id)
		return region;

	if (region_id == ULONG_MAX && strcmp(ident, name) == 0)
		return region;

	return NULL;
}

struct ndctl_namespace *util_namespace_filter(struct ndctl_namespace *ndns,
		const char *ident)
{
	struct ndctl_region *region = ndctl_namespace_get_region(ndns);
	unsigned long region_id, ndns_id;

	if (!ident || strcmp(ident, "all") == 0)
		return ndns;

	if (strcmp(ident, ndctl_namespace_get_devname(ndns)) == 0)
		return ndns;

	if (sscanf(ident, "%ld.%ld", &region_id, &ndns_id) == 2
			&& ndctl_region_get_id(region) == region_id
			&& ndctl_namespace_get_id(ndns) == ndns_id)
		return ndns;

	return NULL;
}

struct ndctl_dimm *util_dimm_filter(struct ndctl_dimm *dimm, const char *ident)
{
	char *end = NULL;
	const char *name;
	unsigned long dimm_id, id;

	if (!ident || strcmp(ident, "all") == 0)
		return dimm;

	dimm_id = strtoul(ident, &end, 0);
	if (end == ident || end[0])
		dimm_id = ULONG_MAX;

	name = ndctl_dimm_get_devname(dimm);
	id = ndctl_dimm_get_id(dimm);

	if (dimm_id < ULONG_MAX && dimm_id == id)
		return dimm;

	if (dimm_id == ULONG_MAX && strcmp(ident, name) == 0)
		return dimm;

	return NULL;
}

struct ndctl_bus *util_bus_filter_by_dimm(struct ndctl_bus *bus,
		const char *ident)
{
	struct ndctl_dimm *dimm;

	if (!ident || strcmp(ident, "all") == 0)
		return bus;

	ndctl_dimm_foreach(bus, dimm)
		if (util_dimm_filter(dimm, ident))
			return bus;
	return NULL;
}

struct ndctl_bus *util_bus_filter_by_region(struct ndctl_bus *bus,
		const char *ident)
{
	struct ndctl_region *region;

	if (!ident || strcmp(ident, "all") == 0)
		return bus;

	ndctl_region_foreach(bus, region)
		if (util_region_filter(region, ident))
			return bus;
	return NULL;
}


struct ndctl_bus *util_bus_filter_by_namespace(struct ndctl_bus *bus,
		const char *ident)
{
	struct ndctl_region *region;
	struct ndctl_namespace *ndns;

	if (!ident || strcmp(ident, "all") == 0)
		return bus;

	ndctl_region_foreach(bus, region)
		ndctl_namespace_foreach(region, ndns)
			if (util_namespace_filter(ndns, ident))
				return bus;
	return NULL;
}

struct ndctl_region *util_region_filter_by_dimm(struct ndctl_region *region,
		const char *ident)
{
	struct ndctl_dimm *dimm;

	if (!ident || strcmp(ident, "all") == 0)
		return region;

	ndctl_dimm_foreach_in_region(region, dimm)
		if (util_dimm_filter(dimm, ident))
			return region;

	return NULL;
}

struct ndctl_dimm *util_dimm_filter_by_region(struct ndctl_dimm *dimm,
		const char *ident)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_region *region;
	struct ndctl_dimm *check;

	if (!ident || strcmp(ident, "all") == 0)
		return dimm;

	ndctl_region_foreach(bus, region) {
		if (!util_region_filter(region, ident))
			continue;
		ndctl_dimm_foreach_in_region(region, check)
			if (check == dimm)
				return dimm;
	}

	return NULL;
}

struct ndctl_dimm *util_dimm_filter_by_namespace(struct ndctl_dimm *dimm,
		const char *ident)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_namespace *ndns;
	struct ndctl_region *region;
	struct ndctl_dimm *check;

	if (!ident || strcmp(ident, "all") == 0)
		return dimm;

	ndctl_region_foreach(bus, region) {
		ndctl_namespace_foreach(region, ndns) {
			if (!util_namespace_filter(ndns, ident))
				continue;
			ndctl_dimm_foreach_in_region(region, check)
				if (check == dimm)
					return dimm;
		}
	}

	return NULL;
}

struct ndctl_region *util_region_filter_by_namespace(struct ndctl_region *region,
		const char *ident)
{
	struct ndctl_namespace *ndns;

	if (!ident || strcmp(ident, "all") == 0)
		return region;

	ndctl_namespace_foreach(region, ndns)
		if (util_namespace_filter(ndns, ident))
			return region;
	return NULL;
}

struct daxctl_dev *util_daxctl_dev_filter(struct daxctl_dev *dev,
		const char *ident)
{
	struct daxctl_region *region = daxctl_dev_get_region(dev);
	int region_id, dev_id;

	if (!ident || strcmp(ident, "all") == 0)
		return dev;

	if (strcmp(ident, daxctl_dev_get_devname(dev)) == 0)
		return dev;

	if (sscanf(ident, "%d.%d", &region_id, &dev_id) == 2
			&& daxctl_region_get_id(region) == region_id
			&& daxctl_dev_get_id(dev) == dev_id)
		return dev;

	return NULL;
}

static enum ndctl_namespace_mode mode_to_type(const char *mode)
{
	if (!mode)
		return -ENXIO;

	if (strcasecmp(mode, "memory") == 0)
		return NDCTL_NS_MODE_MEMORY;
	else if (strcasecmp(mode, "fsdax") == 0)
		return NDCTL_NS_MODE_MEMORY;
	else if (strcasecmp(mode, "sector") == 0)
		return NDCTL_NS_MODE_SAFE;
	else if (strcasecmp(mode, "safe") == 0)
		return NDCTL_NS_MODE_SAFE;
	else if (strcasecmp(mode, "dax") == 0)
		return NDCTL_NS_MODE_DAX;
	else if (strcasecmp(mode, "devdax") == 0)
		return NDCTL_NS_MODE_DAX;
	else if (strcasecmp(mode, "raw") == 0)
		return NDCTL_NS_MODE_RAW;

	return NDCTL_NS_MODE_UNKNOWN;
}

int util_filter_walk(struct ndctl_ctx *ctx, struct util_filter_ctx *fctx,
		struct util_filter_params *param)
{
	struct ndctl_bus *bus;
	unsigned int type = 0;

	if (param->type && (strcmp(param->type, "pmem") != 0
				&& strcmp(param->type, "blk") != 0)) {
		error("unknown type \"%s\" must be \"pmem\" or \"blk\"\n",
				param->type);
		return -EINVAL;
	}

	if (param->type) {
		if (strcmp(param->type, "pmem") == 0)
			type = ND_DEVICE_REGION_PMEM;
		else
			type = ND_DEVICE_REGION_BLK;
	}

	if (mode_to_type(param->mode) == NDCTL_NS_MODE_UNKNOWN) {
		error("invalid mode: '%s'\n", param->mode);
		return -EINVAL;
	}

	ndctl_bus_foreach(ctx, bus) {
		struct ndctl_region *region;
		struct ndctl_dimm *dimm;

		if (!util_bus_filter(bus, param->bus)
				|| !util_bus_filter_by_dimm(bus, param->dimm)
				|| !util_bus_filter_by_region(bus, param->region)
				|| !util_bus_filter_by_namespace(bus, param->namespace))
			continue;

		if (!fctx->filter_bus(bus, fctx))
			continue;

		ndctl_dimm_foreach(bus, dimm) {
			if (!fctx->filter_dimm)
				break;

			if (!util_dimm_filter(dimm, param->dimm)
					|| !util_dimm_filter_by_region(dimm,
						param->region)
					|| !util_dimm_filter_by_namespace(dimm,
						param->namespace))
				continue;

			fctx->filter_dimm(dimm, fctx);
		}

		ndctl_region_foreach(bus, region) {
			struct ndctl_namespace *ndns;

			if (!util_region_filter(region, param->region)
					|| !util_region_filter_by_dimm(region,
						param->dimm)
					|| !util_region_filter_by_namespace(region,
						param->namespace))
				continue;

			if (type && ndctl_region_get_type(region) != type)
				continue;

			if (!fctx->filter_region(region, fctx))
				continue;

			ndctl_namespace_foreach(region, ndns) {
				enum ndctl_namespace_mode mode;

				if (!fctx->filter_namespace)
					break;
				if (!util_namespace_filter(ndns, param->namespace))
					continue;

				mode = ndctl_namespace_get_mode(ndns);
				if (param->mode && mode_to_type(param->mode) != mode)
					continue;

				fctx->filter_namespace(ndns, fctx);
			}
		}
	}
	return 0;
}
