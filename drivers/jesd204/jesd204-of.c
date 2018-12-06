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

#if 0
static void of_coresight_get_ports(const struct device_node *node,
				   int *nr_inport, int *nr_outport)
{
	struct device_node *ep = NULL;
	int in = 0, out = 0;

	do {
		ep = of_graph_get_next_endpoint(node, ep);
		if (!ep)
			break;

		if (of_property_read_bool(ep, "slave-mode"))
			in++;
		else
			out++;

	} while (ep);

	*nr_inport = in;
	*nr_outport = out;
}

static int of_coresight_alloc_memory(struct device *dev,
			struct coresight_platform_data *pdata)
{
	/* List of output port on this component */
	pdata->outports = devm_kzalloc(dev, pdata->nr_outport *
				       sizeof(*pdata->outports),
				       GFP_KERNEL);
	if (!pdata->outports)
		return -ENOMEM;

	/* Children connected to this component via @outports */
	pdata->child_names = devm_kzalloc(dev, pdata->nr_outport *
					  sizeof(*pdata->child_names),
					  GFP_KERNEL);
	if (!pdata->child_names)
		return -ENOMEM;

	/* Port number on the child this component is connected to */
	pdata->child_ports = devm_kzalloc(dev, pdata->nr_outport *
					  sizeof(*pdata->child_ports),
					  GFP_KERNEL);
	if (!pdata->child_ports)
		return -ENOMEM;

	return 0;
}

int of_coresight_get_cpu(const struct device_node *node)
{
	int cpu;
	bool found;
	struct device_node *dn, *np;

	dn = of_parse_phandle(node, "cpu", 0);

	/* Affinity defaults to CPU0 */
	if (!dn)
		return 0;

	for_each_possible_cpu(cpu) {
		np = of_cpu_device_node_get(cpu);
		found = (dn == np);
		of_node_put(np);
		if (found)
			break;
	}
	of_node_put(dn);

	/* Affinity to CPU0 if no cpu nodes are found */
	return found ? cpu : 0;
}
EXPORT_SYMBOL_GPL(of_coresight_get_cpu);
#endif


/**
 * of_jesd204_dev_populate_topology() - use OF graph to find all elements
 *					of a jesd204_dev (clks, xcvrs, etc)
 * @sizeof_priv:        Space to allocate for private structure.
 **/
int of_jesd204_dev_populate_topology(struct jesd204_dev *jdev)
{
	struct of_endpoint endpoint, rendpoint;
	struct device_node *ep = NULL;
	struct device_node *rparent;
	struct device_node *rport;
	struct device_node *node;
	struct device *rdev;
	int i = 0, ret = 0;

	if (!jdev)
		return -EINVAL;

	node = jdev->dev.of_node;
	/* Iterate through each port to discover topology */
	do {
		pr_err("(%d) %s %d\n", i,  __func__, __LINE__);
		/* Get a handle on a port */
		ep = of_graph_get_next_endpoint(node, ep);
		if (!ep)
			break;

#if 0
		/*
		 * No need to deal with input ports, processing for as
		 * processing for output ports will deal with them.
		 */
		if (of_find_property(ep, "slave-mode", NULL))
			continue;
#endif

		/* Get a handle on the local endpoint */
		ret = of_graph_parse_endpoint(ep, &endpoint);

		if (ret)
			continue;

#if 0
		/* The local out port number */
		pdata->outports[i] = endpoint.port;
#endif
		/*
		 * Get a handle on the remote port and parent
		 * attached to it.
		 */
		rparent = of_graph_get_remote_port_parent(ep);
		rport = of_graph_get_remote_port(ep);

		if (!rparent || !rport)
			continue;

		if (of_graph_parse_endpoint(rport, &rendpoint))
			continue;

		rdev = of_jesd204_get_endpoint_device(rparent);
		if (!rdev)
			return -EPROBE_DEFER;
#if 0
		pdata->child_names[i] = dev_name(rdev);
		pdata->child_ports[i] = rendpoint.id;
#endif

		i++;
	} while (ep);

	return 0;
}
EXPORT_SYMBOL_GPL(of_jesd204_dev_populate_topology);
