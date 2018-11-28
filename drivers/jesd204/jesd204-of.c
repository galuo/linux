/**
 * JESD204 device-tree handling
 *
 * Copyright (c) 2018 Analog Devices Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/err.h>
#include <linux/of.h>

#include "jesd204-priv.h"

static int of_read_u32_validate_max(struct device *dev,
				    const char *prop, u32 max)
{
	struct device_node *np = dev->of_node;
	u32 value;
	int ret;

	ret = of_property_read_u32(np, prop, &value);
	if (ret) {
		dev_err(dev,
			"%pOF has no valid '%s' property (%d)\n",
			np, prop, ret);
		return ret;
	}

	if (value > max) {
		dev_err(dev,
			"invalid value for '%s' (%u)\n", prop, value);
		return -EINVAL;	
	}

	return value;
}

int of_jesd204_parse_dt(struct jesd204_dev *jdev)
{
	struct jesd204_dev_caps *caps;
	int ret;

	if (!jdev)
		return -EINVAL;

	ret = of_read_u32_validate_max(&jdev->dev, "jesd204_revision",
				       JESD204_REV_C);
	if (ret < 0)
		return ret;
	caps->jesd204_revision = ret;

	ret = of_read_u32_validate_max(&jdev->dev, "jesd204_subclass",
				       JESD204_SUBCLASS_2);
	if (ret < 0)
		return ret;
	caps->jesd204_subclass = ret;

	return 0;
}
EXPORT_SYMBOL_GPL(of_jesd204_parse_dt);
