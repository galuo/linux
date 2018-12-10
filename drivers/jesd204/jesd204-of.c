/**
 * JESD204 device-tree handling
 *
 * Copyright (c) 2018 Analog Devices Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * Based on elements of the CoreSight subsystem.
 */

#include <linux/types.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/spi/spi.h>

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

static int of_dev_node_match(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static struct device *
of_jesd204_get_endpoint_device(struct device_node *endpoint)
{
	/* List of buses where we may find a JESD204 device */
	static struct bus_type *supported_bus_types[] = {
		&platform_bus_type,
		&iio_bus_type,
		&spi_bus_type,
		&i2c_bus_type,
	};
	struct device *dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(supported_bus_types); i++) {
		dev = bus_find_device(supported_bus_types[i], NULL,
				      endpoint, of_dev_node_match);
		if (dev)
			return dev;
	}

	return NULL;
}

static bool of_jesd204_dev_find_clock(struct jesd204_dev_priv *pdev,
				      struct clk *clk)
{
	struct jesd204_clocks *c;

	list_for_each_entry(c, &pdev->clocks.list, list) {
		if (clk_is_match(c->clk, clk))
			return true;
	}
	return false;
}

static int of_jesd204_dev_collect_clocks(struct jesd204_dev_priv *pdev,
					 struct device_node *np,
					 bool collect)
{
	struct device *dev = &pdev->jesd204_dev.dev;
	struct jesd204_clocks *jclk;
	int i, ret, count, count1;

	count = of_count_phandle_with_args(np, "clocks", "#clock-cells");
	if (count < 0)
		return count;

	/**
	 * If user wants to provide clock-names, that's fine, but they need to
	 * match the number of clock references provided.
	 * These names are very helpful when debugging tons of clocks.
	 */
	count1 = of_property_count_strings(np, "clock-names");
	if ((count1 > 0) && (count != count1)) {
		dev_err(dev, "mismath `clock-names` with `clocks`\n");
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		const char *name = NULL;
		struct clk *c;

		if (count1 > 0) {
			ret = of_property_read_string_index(np, "clock-names",
							    i, &name);
			if (ret < 0)
				return ret;
		}

		c = of_clk_get(np, i);
		if (IS_ERR(c)) {
			ret = PTR_ERR(c);
			if (name)
				dev_err(dev, "of_clk_get(%s) error %d\n",
					name, ret);
			else
				dev_err(dev, "of_clk_get(%d) error %d\n",
					i, ret);
			return ret;
		}

		/**
		 * If we're collecting clocks, we don't want clocks to exist
		 * in list. If we're not collecting, we want clocks to exist
		 * in list. In the latter case, the kref count will be
		 * incremented.
		 */
		if (collect == of_jesd204_dev_find_clock(pdev, c)) {
			if (name)
				dev_err(dev, "Clock '%s' should %s "
					"be in list\n",
					name, collect ? "not" : "");
			else
				dev_err(dev, "Clock '%d' should %s "
					"be in list\n",
					i, collect ? "not" : "");
			return -EINVAL;
		}

		if (!collect)
			continue;

		jclk = devm_kzalloc(dev, sizeof(struct jesd204_clocks),
				    GFP_KERNEL);
		if (!jclk)
			return -ENOMEM;

		jclk->clk = c;

		list_add(&jclk->list, &pdev->clocks.list);
	}

	return count;
}

static int of_jesd204_dev_populate_clocks(struct jesd204_dev_priv *pdev)
{
	struct device_node *ep = NULL;
	struct device_node *rparent;
	struct device_node *rport;
	struct device_node *node;
	struct device *dev;
	int ret;

	INIT_LIST_HEAD(&pdev->clocks.list);

	dev  = &pdev->jesd204_dev.dev;
	node = dev->of_node;
	do {
		ep = of_graph_get_next_endpoint(node, ep);
		if (!ep)
			break;

		rparent = of_graph_get_remote_port_parent(ep);
		rport = of_graph_get_remote_port(ep);

		if (!rparent || !rport)
			continue;

		if (!of_find_property(rport, "output-clocks", NULL))
			continue;

		if (of_find_property(rport, "input-clocks", NULL)) {
			dev_warn(dev, "Endpoint cannot have both "
				 "input & outputs clocks");
			return -EINVAL;
		}

		ret = of_jesd204_dev_collect_clocks(pdev, rport, true);
		if (ret < 0) {
			dev_err(dev, "of_jesd204_dev_populate_clocks(%s) "
				"failed: %d", rparent->name, ret);
			return ret;
		}

	} while (ep);

	return 0;
}

static int of_jesd204_dev_init_clocks(struct jesd204_dev_priv *pdev)
{
	struct device_node *ep = NULL;
	struct device_node *rparent;
	struct device_node *rport;
	struct device_node *node;
	struct device *dev;
	int ret;

	dev  = &pdev->jesd204_dev.dev;
	node = dev->of_node;
	do {
		ep = of_graph_get_next_endpoint(node, ep);
		if (!ep)
			break;

		rparent = of_graph_get_remote_port_parent(ep);
		rport = of_graph_get_remote_port(ep);

		if (!rparent || !rport)
			continue;

		if (!of_find_property(rport, "input-clocks", NULL))
			continue;

		/* No need to do sanity checking for `output-clocks` */

		ret = of_jesd204_dev_collect_clocks(pdev, rport, false);
		if (ret < 0) {
			dev_err(dev, "of_jesd204_dev_init_clocks(%s) "
				"failed: %d", rparent->name, ret);
			return ret;
		}

	} while (ep);

	return 0;
}

/**
 * of_jesd204_dev_populate_topology() - use OF graph to find all elements
 *					of a jesd204_dev (clks, xcvrs, etc)
 **/
int of_jesd204_dev_populate_topology(struct jesd204_dev *jdev)
{
	struct jesd204_dev_priv *pdev = jesd204_dev_to_priv(jdev);
	int ret;

	if (!jdev)
		return -EINVAL;

	/* Collect all clock sources first */
	ret = of_jesd204_dev_populate_clocks(pdev);
	if (ret < 0)
		return ret;

	ret = of_jesd204_dev_init_clocks(pdev);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(of_jesd204_dev_populate_topology);
