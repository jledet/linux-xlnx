/*
 * Cypress FM33256B Processor Companion Driver
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
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/of.h>

static const struct mfd_cell fm33256b_cells[] = {
	{
		.name = "fm33256b-rtc",
		.of_compatible = "cypress,fm33256b-rtc",
	},
	{
		.name = "fm33256b-fram",
		.of_compatible = "cypress,fm33256b-fram",
	},
};

static int fm33256b_io(struct spi_device *spi, bool write_enable,
		       uint8_t *out, uint8_t *in, size_t len)
{
	struct spi_message m;
	struct fm33256b *fm33256b = dev_get_drvdata(&spi->dev);

	uint8_t write_out[1] = {FM33256B_OP_WREN};

	/* Payload transfer */
	struct spi_transfer t = {
		.tx_buf = out,
		.rx_buf = in,
		.len = len,
	};

	mutex_lock(&fm33256b->lock);

	/* CS must go high for the write enable latch to be enabled,
	 * so we have to split this in two transfers.
	 */
	if (write_enable)
		spi_write(spi, write_out, 1);

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	spi_sync(spi, &m);

	mutex_unlock(&fm33256b->lock);

	return 0;
}

static int fm33256b_read_status(struct spi_device *spi, uint8_t *status)
{
	int ret;
	uint8_t out[2] = {FM33256B_OP_RDSR, 0xff};
	uint8_t in[2];

	ret = fm33256b_io(spi, false, out, in, 2);
	if (ret < 0)
		return ret;

	*status = in[1];

	return 0;
}

static int fm33256b_write_status(struct spi_device *spi, uint8_t status)
{
	uint8_t out[2] = {FM33256B_OP_WRSR, status};
	uint8_t in[2];

	return fm33256b_io(spi, true, out, in, 2);
}

static int fm33256b_write_memory(struct spi_device *spi, uint16_t addr,
				 const uint8_t *data, size_t len)
{
	int ret;
	uint8_t *out = NULL, *in = NULL;

	out = devm_kzalloc(&spi->dev, 3 + len, GFP_KERNEL);
	in = devm_kzalloc(&spi->dev, 3 + len, GFP_KERNEL);
	if (!out || !in) {
		ret = -ENOMEM;
		goto out;
	}

	out[0] = FM33256B_OP_WRITE;
	out[1] = (addr >> 8) & 0xff;
	out[2] = addr & 0xff;
	memcpy(&out[3], data, len);

	ret = fm33256b_io(spi, true, out, in, 3 + len);

out:
	devm_kfree(&spi->dev, out);
	devm_kfree(&spi->dev, in);

	return ret;
}

static int fm33256b_read_memory(struct spi_device *spi, uint16_t addr,
				uint8_t *data, size_t len)
{
	int ret;
	uint8_t *out = NULL, *in = NULL;

	out = devm_kzalloc(&spi->dev, 3 + len, GFP_KERNEL);
	in = devm_kzalloc(&spi->dev, 3 + len, GFP_KERNEL);
	if (!out || !in) {
		ret = -ENOMEM;
		goto out;
	}

	out[0] = FM33256B_OP_READ;
	out[1] = (addr >> 8) & 0xff;
	out[2] = addr & 0xff;
	memset(&out[3], 0xff, len);

	ret = fm33256b_io(spi, false, out, in, 3 + len);
	if (ret == 0)
		memcpy(data, &in[3], len);

out:
	devm_kfree(&spi->dev, out);
	devm_kfree(&spi->dev, in);

	return ret;
}

static int fm33256b_write_pc(struct spi_device *spi, uint8_t reg,
			     const uint8_t *data, size_t len)
{
	int ret;
	uint8_t *out = NULL, *in = NULL;

	out = devm_kzalloc(&spi->dev, 2 + len, GFP_KERNEL);
	in = devm_kzalloc(&spi->dev, 2 + len, GFP_KERNEL);
	if (!out || !in) {
		ret = -ENOMEM;
		goto out;
	}

	out[0] = FM33256B_OP_WRPC;
	out[1] = reg;
	memcpy(&out[2], data, len);

	ret = fm33256b_io(spi, true, out, in, 2 + len);

out:
	devm_kfree(&spi->dev, out);
	devm_kfree(&spi->dev, in);

	return ret;
}

static int fm33256b_read_pc(struct spi_device *spi, uint8_t reg,
			    uint8_t *data, size_t len)
{
	int ret;
	uint8_t *out = NULL, *in = NULL;

	out = devm_kzalloc(&spi->dev, 2 + len, GFP_KERNEL);
	in = devm_kzalloc(&spi->dev, 2 + len, GFP_KERNEL);
	if (!out || !in) {
		ret = -ENOMEM;
		goto out;
	}

	out[0] = FM33256B_OP_RDPC;
	out[1] = reg;
	memset(&out[2], 0xff, len);

	ret = fm33256b_io(spi, false, out, in, 2 + len);
	if (ret == 0)
		memcpy(data, &in[2], len);

out:
	devm_kfree(&spi->dev, out);
	devm_kfree(&spi->dev, in);

	return ret;
}

static int fm33256b_pc_regmap_read(void *context, const void *reg,
				   size_t reg_size, void *val,
				   size_t val_size)
{
	struct device *dev = context;
	struct spi_device *spi = to_spi_device(dev);

	if (reg_size != 1)
		return -ENOTSUPP;

	return fm33256b_read_pc(spi, *(uint8_t *)reg, val, val_size);
}

static int fm33256b_pc_regmap_write(void *context, const void *data,
				    size_t count)
{
	struct device *dev = context;
	struct spi_device *spi = to_spi_device(dev);

	const uint8_t *out = data;
	const uint8_t *val = &out[1];

	uint8_t reg = out[0];

	return fm33256b_write_pc(spi, reg, val, count - sizeof(reg));
}

static int fm33256b_fram_regmap_read(void *context, const void *reg,
				     size_t reg_size, void *val,
				     size_t val_size)
{
	struct device *dev = context;
	struct spi_device *spi = to_spi_device(dev);

	const uint8_t *addrp = reg;

	uint16_t addr = ((uint16_t)addrp[0] << 8) | addrp[1];

	if (reg_size != 2)
		return -ENOTSUPP;

	return fm33256b_read_memory(spi, addr, val, val_size);
}

static int fm33256b_fram_regmap_write(void *context, const void *data,
				      size_t count)
{
	struct device *dev = context;
	struct spi_device *spi = to_spi_device(dev);

	const uint8_t *out = data;
	const uint8_t *val = &out[2];

	uint16_t addr = ((uint16_t)out[0] << 8) | out[1];

	return fm33256b_write_memory(spi, addr, val, count - sizeof(addr));
}

static ssize_t fm33256b_bp_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	int ret;
	uint8_t status, bp;

	ret = fm33256b_read_status(spi, &status);
	if (ret < 0)
		return ret;

	bp = (status & 0x0c) >> 2;

	return snprintf(buf, PAGE_SIZE, "%hhu\n", bp);
}

static ssize_t fm33256b_bp_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct spi_device *spi = to_spi_device(dev);
	int ret;
	uint8_t status, bp;
	unsigned long input;

	ret = kstrtoul(buf, 10, &input);
	if (ret < 0)
		return ret;

	if (input > 3)
		return -EINVAL;

	bp = (uint8_t)input << 2;

	ret = fm33256b_read_status(spi, &status);
	if (ret < 0)
		return ret;

	status = (status & 0xf3) | bp;

	ret = fm33256b_write_status(spi, status);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t fm33256b_serial_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	char serial[9];
	int ret;

	ret = fm33256b_read_pc(spi, FM33256B_SERIAL_BYTE0_REG, serial, 8);
	if (ret < 0)
		return ret;

	serial[8] = '\0';

	return snprintf(buf, PAGE_SIZE, "%-8s\n", serial);
}

static ssize_t fm33256b_serial_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct spi_device *spi = to_spi_device(dev);
	char serial[9];
	int ret;

	if (sscanf(buf, "%8s", serial) != 1)
		return -EINVAL;

	ret = fm33256b_write_pc(spi, FM33256B_SERIAL_BYTE0_REG, serial, 8);
	if (ret < 0)
		return ret;

	return count;
}

DEVICE_ATTR(bp, S_IWUSR | S_IRUGO,
	    fm33256b_bp_show, fm33256b_bp_store);
DEVICE_ATTR(serial, S_IWUSR | S_IRUGO,
	    fm33256b_serial_show, fm33256b_serial_store);

/* Processor Companion Register Map */
static const struct regmap_config fm33256b_pc_regmap_conf = {
	.name = "pc",
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = FM33256B_MAX_REGISTER,
};

static struct regmap_bus fm33256b_pc_regmap_bus = {
	.write = fm33256b_pc_regmap_write,
	.read = fm33256b_pc_regmap_read,
};

/* FRAM Register Map */
static const struct regmap_config fm33256b_fram_regmap_conf = {
	.name = "fram",
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = FM33256B_MAX_FRAM,
};

static struct regmap_bus fm33256b_fram_regmap_bus = {
	.write = fm33256b_fram_regmap_write,
	.read = fm33256b_fram_regmap_read,
};

static int fm33256b_setup(struct spi_device *spi, struct fm33256b *fm33256b)
{
	int ret;
	uint8_t companion_ctl = FM33256B_ALSW, rtc_alarm_ctl = 0;

	/* Setup charger control from DT */
	if (of_get_property(spi->dev.of_node, "cypress,charge-enabled", NULL))
		companion_ctl |= FM33256B_VBC;

	if (of_get_property(spi->dev.of_node, "cypress,charge-fast", NULL))
		companion_ctl |= FM33256B_FC;

	/* Setup charging if enabled */
	ret = regmap_write(fm33256b->regmap_pc,
			   FM33256B_COMPANION_CONTROL_REG,
			   companion_ctl);
	if (ret < 0)
		return ret;

	/* Enable 32 kHz oscillator */
	ret = regmap_write(fm33256b->regmap_pc,
			   FM33256B_RTC_ALARM_CONTROL_REG,
			   rtc_alarm_ctl);
	if (ret < 0)
		return ret;

	return 0;
}

static int fm33256b_probe(struct spi_device *spi)
{
	int ret;
	struct device *dev = &spi->dev;
	struct fm33256b *fm33256b;

	fm33256b = devm_kzalloc(dev, sizeof(*fm33256b), GFP_KERNEL);
	if (!fm33256b)
		return -ENOMEM;

	mutex_init(&fm33256b->lock);

	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = spi->max_speed_hz ? : 8000000;

	ret = spi_setup(spi);
	if (ret)
		return ret;

	/* Setup processor companion regmap */
	fm33256b->regmap_pc =
		devm_regmap_init(dev, &fm33256b_pc_regmap_bus,
				 dev, &fm33256b_pc_regmap_conf);
	if (IS_ERR(fm33256b->regmap_pc))
		return PTR_ERR(fm33256b->regmap_pc);

	/* Setup FRAM regmap */
	fm33256b->regmap_fram =
		devm_regmap_init(dev, &fm33256b_fram_regmap_bus,
				 dev, &fm33256b_fram_regmap_conf);
	if (IS_ERR(fm33256b->regmap_fram))
		return PTR_ERR(fm33256b->regmap_fram);

	dev_set_drvdata(dev, fm33256b);

	ret = fm33256b_setup(spi, fm33256b);
	if (ret < 0)
		return ret;

	/* Create sysfs entries */
	ret = device_create_file(&spi->dev, &dev_attr_bp);
	if (ret < 0)
		return ret;

	ret = device_create_file(&spi->dev, &dev_attr_serial);
	if (ret < 0)
		return ret;

	return mfd_add_devices(dev, -1, fm33256b_cells,
			       ARRAY_SIZE(fm33256b_cells),
			       NULL, 0, NULL);
}

static int fm33256b_remove(struct spi_device *spi)
{
	device_remove_file(&spi->dev, &dev_attr_bp);
	device_remove_file(&spi->dev, &dev_attr_serial);
	mfd_remove_devices(&spi->dev);

	return 0;
}

static const struct of_device_id fm33256b_dt_ids[] = {
	{ .compatible = "cypress,fm33256b" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, fm33256b_dt_ids);

static struct spi_driver fm33256b_spi_driver = {
	.driver = {
		.name = "fm33256b",
		.owner = THIS_MODULE,
		.of_match_table = fm33256b_dt_ids,
	},
	.probe = fm33256b_probe,
	.remove = fm33256b_remove,
};
module_spi_driver(fm33256b_spi_driver);

MODULE_ALIAS("spi:fm33256b");
MODULE_DESCRIPTION("Cypress FM33256B Processor Companion Driver");
MODULE_AUTHOR("Jeppe Ledet-Pedersen <jlp@gomspace.com>");
MODULE_LICENSE("GPL v2");
