/**
 * Generic JESD204 device driver
 *
 * Copyright (c) 2018 Analog Devices Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "jesd204-priv.h"

enum {
	JESD204_RX_DEVICE,
	JESD204_TX_DEVICE,
};

static int jesd204_dev_probe(struct platform_device *pdev)
{
	struct jesd204_dev *jdev;

	jdev = devm_jesd204_dev_alloc(&pdev->dev, 0);
	if (!jdev)
		return -ENOMEM;

	jdev->dev.parent = &pdev->dev;

	return devm_jesd204_dev_register(&pdev->dev, jdev);
}

static int jesd204_dev_remove(struct platform_device *pdev)
{
	return 0;
}

/* Match table for of_platform binding */
static const struct of_device_id jesd204_dev_of_match[] = {
	{ .compatible = "jesd204-device-rx",
	  .data = (void *) JESD204_RX_DEVICE },
	{ .compatible = "jesd204-device-tx",
	  .data = (void *) JESD204_TX_DEVICE },
	{ /* end of list */ },
};

static struct platform_driver jesd204_dev_of_driver = {
	.driver = {
		.name = KBUILD_MODNAME,
		.owner = THIS_MODULE,
		.of_match_table = jesd204_dev_of_match,
		.suppress_bind_attrs = true,
	},
	.probe  = jesd204_dev_probe,
	.remove = jesd204_dev_remove,
};

module_platform_driver(jesd204_dev_of_driver);

MODULE_AUTHOR("Alexandru Ardelean <alexandru.ardelean@analog.com>");
MODULE_DESCRIPTION("Generic JESD204 device driver");
MODULE_LICENSE("GPL");
