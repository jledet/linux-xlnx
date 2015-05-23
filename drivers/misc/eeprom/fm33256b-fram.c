/*
 * Cypress FM33256B Processor Companion FRAM Driver
 *
 * Copyright (C) 2015 GomSpace ApS
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/mfd/fm33256b.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

static ssize_t fm33256b_fram_read(struct file *filp, struct kobject *kobj,
				  struct bin_attribute *bin_attr,
				  char *buf, loff_t off, size_t count)
{
	int ret;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct fm33256b *fm33256b = dev_get_drvdata(dev->parent);

	ret = regmap_bulk_read(fm33256b->regmap_fram, off, buf, count);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t fm33256b_fram_write(struct file *filp, struct kobject *kobj,
				   struct bin_attribute *bin_attr,
				   char *buf, loff_t off, size_t count)
{
	int ret;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct fm33256b *fm33256b = dev_get_drvdata(dev->parent);

	ret = regmap_bulk_write(fm33256b->regmap_fram, off, buf, count);
	if (ret < 0)
		return ret;

	return count;
}

static struct bin_attribute fm33256b_fram_attr = {
	.attr = {
		.name = "fram",
		.mode = S_IWUSR | S_IRUGO,
	},
	.size = FM33256B_MAX_FRAM,
	.read = fm33256b_fram_read,
	.write = fm33256b_fram_write,
};

static int fm33256b_fram_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct fm33256b *fm33256b;

	fm33256b = dev_get_drvdata(dev->parent);

	ret = sysfs_create_bin_file(&(dev->kobj), &fm33256b_fram_attr);
	if (ret < 0)
		return ret;

	return 0;
}

static int fm33256b_fram_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fm33256b *fm33256b;

	fm33256b = dev_get_drvdata(dev->parent);

	sysfs_remove_bin_file(&(dev->kobj), &fm33256b_fram_attr);

	return 0;
}

static const struct of_device_id fm33256b_fram_dt_ids[] = {
	{ .compatible = "cypress,fm33256b-fram" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, fm33256b_fram_dt_ids);

static struct platform_driver fm33256b_fram_driver = {
	.driver = {
		.name = "fm33256b-fram",
		.of_match_table = fm33256b_fram_dt_ids,
	},
	.probe = fm33256b_fram_probe,
	.remove = fm33256b_fram_remove,
};
module_platform_driver(fm33256b_fram_driver);

MODULE_ALIAS("platform:fm33256b-fram");
MODULE_AUTHOR("Jeppe Ledet-Pedersen <jlp@gomspace.com>");
MODULE_DESCRIPTION("Cypress FM33256B Processor Companion FRAM Driver");
MODULE_LICENSE("GPL v2");
