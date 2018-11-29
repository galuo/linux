/**
 * The JESD204 subsystem core
 *
 * Copyright (c) 2018 Analog Devices Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * Based on elements of the IIO & CoreSight subsystems.
 */

#define pr_fmt(fmt) "jesd204: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "jesd204-priv.h"

/* IDA to assign each registered device a unique id */
static DEFINE_IDA(jesd204_ida);

static dev_t jesd204_devt;

#define JESD204_DEV_MAX 256
static struct bus_type jesd204_bus_type = {
	.name = "jesd204",
};

static void jesd204_dev_release(struct device *device);
static struct device_type jesd204_dev_type = {
	.name = "jesd204_dev",
	.release = jesd204_dev_release,
};

static struct dentry *jesd204_debugfs_dentry;

/**
 * jesd204_dev_alloc() - allocate a jesd204_dev from a driver
 * @sizeof_priv:	Space to allocate for private structure.
 **/
struct jesd204_dev *jesd204_dev_alloc(int sizeof_priv)
{
	struct jesd204_dev_priv *pdev;
	struct jesd204_dev *jdev;
	size_t alloc_size;
	int id;

	/* Try to get an ID before allocating anything */
	id = ida_simple_get(&jesd204_ida, 0, 0, GFP_KERNEL);
	if (id < 0) {
		/* cannot use a dev_err as the name isn't available */
		pr_err("jesd204-core: failed to get device id: %d\n", id);
		return ERR_PTR(id);
	}

	alloc_size = sizeof(struct jesd204_dev_priv);
	if (sizeof_priv) {
		alloc_size = ALIGN(alloc_size, JESD204_ALIGN);
		alloc_size += sizeof_priv;
	}
	/* ensure 32-byte alignment of whole construct ? */
	alloc_size += JESD204_ALIGN - 1;

	pdev = kzalloc(alloc_size, GFP_KERNEL);
	if (!pdev)
		return ERR_PTR(-ENOMEM);

	pdev->id = id;

	jdev = &pdev->jesd204_dev;
	jdev->dev.type = &jesd204_dev_type;
	jdev->dev.bus = &jesd204_bus_type;
	device_initialize(&jdev->dev);
	dev_set_drvdata(&jdev->dev, (void *)jdev);

	dev_set_name(&jdev->dev, "jesd204:%d", pdev->id);

	return jdev;
}
EXPORT_SYMBOL(jesd204_dev_alloc);

/**
 * jesd204_dev_free() - free a jesd204_dev from a driver
 * @jdev:		the jesd204_dev associated with the device
 **/
void jesd204_dev_free(struct jesd204_dev *jdev)
{
	if (jdev)
		put_device(&jdev->dev);
}
EXPORT_SYMBOL(jesd204_dev_free);

static void jesd204_dev_release(struct device *device)
{
	struct jesd204_dev *jdev = dev_to_jesd204_dev(device);
	struct jesd204_dev_priv *pdev = jesd204_dev_to_priv(jdev);

	ida_simple_remove(&jesd204_ida, pdev->id);
	kfree(pdev);
}

static void devm_jesd204_dev_release(struct device *dev, void *res)
{
	jesd204_dev_free(*(struct jesd204_dev **)res);
}

static int devm_jesd204_dev_match(struct device *dev, void *res, void *data)
{
	struct jesd204_dev **r = res;

	if (!r || !*r) {
		WARN_ON(!r || !*r);
		return 0;
	}

	return *r == data;
}

/**
 * devm_jesd204_dev_alloc - Resource-managed jesd204_dev_alloc()
 * @dev:		Device to allocate jesd204_dev for
 * @sizeof_priv:	Space to allocate for private structure.
 *
 * Managed jesd204_dev_alloc. jesd204_dev allocated with this function is
 * automatically freed on driver detach.
 *
 * If an jesd204_dev allocated with this function needs to be freed separately,
 * devm_jesd204_dev_free() must be used.
 *
 * RETURNS:
 * Pointer to allocated jesd204_dev on success, NULL on failure.
 */
struct jesd204_dev *devm_jesd204_dev_alloc(struct device *dev, int sizeof_priv)
{
	struct jesd204_dev **ptr, *jesd204_dev;

	ptr = devres_alloc(devm_jesd204_dev_release, sizeof(*ptr),
			   GFP_KERNEL);
	if (!ptr)
		return NULL;

	jesd204_dev = jesd204_dev_alloc(sizeof_priv);
	if (jesd204_dev) {
		*ptr = jesd204_dev;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return jesd204_dev;
}
EXPORT_SYMBOL_GPL(devm_jesd204_dev_alloc);

/**
 * devm_jesd204_dev_free - Resource-managed jesd204_dev_free()
 * @dev:		Device this jesd204_dev belongs to
 * @jdev:		the jesd204_dev associated with the device
 *
 * Free jesd204_dev allocated with devm_jesd204_dev_alloc().
 */
void devm_jesd204_dev_free(struct device *dev, struct jesd204_dev *jdev)
{
	int rc;

	rc = devres_release(dev, devm_jesd204_dev_release,
			    devm_jesd204_dev_match, jdev);
	WARN_ON(rc);
}
EXPORT_SYMBOL_GPL(devm_jesd204_dev_free);

/**
 * jesd204_chrdev_open() - chrdev file open for buffer access and ioctls
 * @inode:	Inode structure for identifying the device in the file system
 * @filp:	File structure for JESD204 device used to keep and later access
 *		private data
 *
 * Return: 0 on success or -EBUSY if the device is already opened
 **/
static int jesd204_chrdev_open(struct inode *inode, struct file *filp)
{
	struct jesd204_dev_priv *pdev =
		container_of(inode->i_cdev, struct jesd204_dev_priv, chrdev);

	if (test_and_set_bit(JESD204_BUSY_BIT_POS, &pdev->flags))
		return -EBUSY;

	jesd204_dev_get(&pdev->jesd204_dev);

	filp->private_data = pdev;

	return 0;
}

/**
 * jesd204_chrdev_release() - chrdev file close buffer access and ioctls
 * @inode:	Inode structure pointer for the char device
 * @filp:	File structure pointer for the char device
 *
 * Return: 0 for successful release
 */
static int jesd204_chrdev_release(struct inode *inode, struct file *filp)
{
	struct jesd204_dev_priv *pdev =
		 container_of(inode->i_cdev, struct jesd204_dev_priv, chrdev);

	clear_bit(JESD204_BUSY_BIT_POS, &pdev->flags);
	jesd204_dev_put(&pdev->jesd204_dev);

	return 0;
}

static const struct file_operations jesd204_fileops = {
	.release = jesd204_chrdev_release,
	.open = jesd204_chrdev_open,
	.owner = THIS_MODULE,
	.llseek = noop_llseek,
};

int __jesd204_dev_register(struct jesd204_dev *jdev, struct module *this_mod)
{
	struct jesd204_dev_priv *pdev = jesd204_dev_to_priv(jdev);
	int ret;

	pdev->driver_module = this_mod;
	/* If the calling driver did not initialize of_node, do it here */
	if (!jdev->dev.of_node && jdev->dev.parent)
		jdev->dev.of_node = jdev->dev.parent->of_node;

	/* configure elements for the chrdev */
	jdev->dev.devt = MKDEV(MAJOR(jesd204_devt), pdev->id);

	cdev_init(&pdev->chrdev, &jesd204_fileops);
	pdev->chrdev.owner = this_mod;

	ret = cdev_device_add(&pdev->chrdev, &jdev->dev);
	if (ret < 0)
		goto error;

	return 0;
error:
	return ret;
}
EXPORT_SYMBOL(__jesd204_dev_register);

/**
 * jesd204_dev_unregister() - unregister a device from the JESD204 subsystem
 * @jdev:		Device structure representing the device.
 **/
void jesd204_dev_unregister(struct jesd204_dev *jdev)
{
}
EXPORT_SYMBOL(jesd204_dev_unregister);

static void devm_jesd204_dev_unreg(struct device *dev, void *res)
{
	jesd204_dev_unregister(*(struct jesd204_dev **)res);
}

int __devm_jesd204_dev_register(struct device *dev, struct jesd204_dev *jdev,
				struct module *this_mod)
{
	struct jesd204_dev **ptr;
	int ret;

	ptr = devres_alloc(devm_jesd204_dev_unreg, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	*ptr = jdev;
	ret = __jesd204_dev_register(jdev, this_mod);
	if (!ret)
		devres_add(dev, ptr);
	else
		devres_free(ptr);

	return ret;
}
EXPORT_SYMBOL_GPL(__devm_jesd204_dev_register);

/**
 * devm_jesd204_dev_unregister - Resource-managed jesd204_dev_unregister()
 * @dev:	Device this jesd204_dev belongs to
 * @jdev:	the jesd204_dev associated with the device
 *
 * Unregister jesd204_dev registered with devm_jesd204_dev_register().
 */
void devm_jesd204_dev_unregister(struct device *dev, struct jesd204_dev *jdev)
{
	int rc;

	rc = devres_release(dev, devm_jesd204_dev_unreg,
			    devm_jesd204_dev_match, jdev);
	WARN_ON(rc);
}
EXPORT_SYMBOL_GPL(devm_jesd204_dev_unregister);

static int __init jesd204_init(void)
{
	int ret;

	/* Register sysfs bus */
	ret  = bus_register(&jesd204_bus_type);
	if (ret < 0) {
		pr_err("could not register bus type\n");
		goto error_nothing;
	}

	ret = alloc_chrdev_region(&jesd204_devt, 0, JESD204_DEV_MAX, "jesd204");
	if (ret < 0) {
		pr_err("failed to allocate char dev region\n");
		goto error_unregister_bus_type;
	}

	jesd204_debugfs_dentry = debugfs_create_dir("jesd204", NULL);

	return 0;

error_unregister_bus_type:
	bus_unregister(&jesd204_bus_type);
error_nothing:
	return ret;
}

static void __exit jesd204_exit(void)
{
	if (jesd204_devt)
		unregister_chrdev_region(jesd204_devt, JESD204_DEV_MAX);
	bus_unregister(&jesd204_bus_type);
	debugfs_remove_recursive(jesd204_debugfs_dentry);
}

subsys_initcall(jesd204_init);
module_exit(jesd204_exit);

MODULE_AUTHOR("Alexandru Ardelean <alexandru.ardelean@analog.com>");
MODULE_DESCRIPTION("JESD204 core");
MODULE_LICENSE("GPL");
