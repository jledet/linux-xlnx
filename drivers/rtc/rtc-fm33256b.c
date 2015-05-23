/*
 * Cypress FM33256B Processor Companion RTC Driver
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
#include <linux/rtc.h>
#include <linux/bcd.h>

struct fm33256b_rtc {
	struct fm33256b *fm33256b;
	struct rtc_device *rtcdev;
};

static int fm33256b_rtc_readtime(struct device *dev, struct rtc_time *tm)
{
	int ret;
	struct fm33256b_rtc *rtc = dev_get_drvdata(dev);
	uint8_t time[7];

	/* Lock time update */
	ret = regmap_update_bits(rtc->fm33256b->regmap_pc,
				 FM33256B_RTC_ALARM_CONTROL_REG,
				 FM33256B_R, FM33256B_R);
	if (ret < 0)
		return ret;

	ret = regmap_bulk_read(rtc->fm33256b->regmap_pc,
			       FM33256B_SECONDS_REG, time, sizeof(time));
	if (ret < 0)
		return ret;

	/* Unlock time update */
	ret = regmap_update_bits(rtc->fm33256b->regmap_pc,
				 FM33256B_RTC_ALARM_CONTROL_REG,
				 FM33256B_R, 0);
	if (ret < 0)
		return ret;

	tm->tm_sec	= bcd2bin(time[0]);
	tm->tm_min	= bcd2bin(time[1]);
	tm->tm_hour	= bcd2bin(time[2]);
	tm->tm_wday	= bcd2bin(time[3]) - 1;
	tm->tm_mday	= bcd2bin(time[4]);
	tm->tm_mon	= bcd2bin(time[5]) - 1;
	tm->tm_year	= bcd2bin(time[6]);

	if (tm->tm_year < 70)
		tm->tm_year += 100;

	return rtc_valid_tm(tm);
}

static int fm33256b_rtc_settime(struct device *dev, struct rtc_time *tm)
{
	int ret;
	struct fm33256b_rtc *rtc = dev_get_drvdata(dev);
	uint8_t time[7];

	time[0] = bin2bcd(tm->tm_sec);
	time[1] = bin2bcd(tm->tm_min);
	time[2] = bin2bcd(tm->tm_hour);
	time[3] = bin2bcd(tm->tm_wday + 1);
	time[4] = bin2bcd(tm->tm_mday);
	time[5] = bin2bcd(tm->tm_mon + 1);
	time[6] = bin2bcd(tm->tm_year % 100);

	/* Unlock time update */
	ret = regmap_update_bits(rtc->fm33256b->regmap_pc,
				 FM33256B_RTC_ALARM_CONTROL_REG,
				 FM33256B_W, FM33256B_W);
	if (ret < 0)
		return ret;

	ret = regmap_bulk_write(rtc->fm33256b->regmap_pc,
				FM33256B_SECONDS_REG, time, sizeof(time));
	if (ret < 0)
		return ret;

	/* Lock time update */
	ret = regmap_update_bits(rtc->fm33256b->regmap_pc,
				 FM33256B_RTC_ALARM_CONTROL_REG,
				 FM33256B_W, 0);
	if (ret < 0)
		return ret;

	return 0;
}

static const struct rtc_class_ops fm33256b_rtc_ops = {
	.read_time	= fm33256b_rtc_readtime,
	.set_time	= fm33256b_rtc_settime,
};

static int fm33256b_rtc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fm33256b *fm33256b;
	struct fm33256b_rtc *rtc;

	fm33256b = dev_get_drvdata(dev->parent);

	rtc = devm_kzalloc(dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	platform_set_drvdata(pdev, rtc);

	rtc->fm33256b = fm33256b;
	rtc->rtcdev = devm_rtc_device_register(&pdev->dev, KBUILD_MODNAME,
					       &fm33256b_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtcdev))
		return PTR_ERR(rtc->rtcdev);


	return 0;
}

static int fm33256b_rtc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fm33256b *fm33256b;
	struct fm33256b_rtc *rtc = platform_get_drvdata(pdev);

	fm33256b = dev_get_drvdata(dev->parent);

	devm_rtc_device_unregister(&pdev->dev, rtc->rtcdev);

	return 0;
}

static const struct of_device_id fm33256b_rtc_dt_ids[] = {
	{ .compatible = "cypress,fm33256b-rtc" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, fm33256b_rtc_dt_ids);

static struct platform_driver fm33256b_rtc_driver = {
	.driver = {
		.name = "fm33256b-rtc",
		.of_match_table = fm33256b_rtc_dt_ids,
	},
	.probe = fm33256b_rtc_probe,
	.remove = fm33256b_rtc_remove,
};
module_platform_driver(fm33256b_rtc_driver);

MODULE_ALIAS("platform:fm33256b-rtc");
MODULE_AUTHOR("Jeppe Ledet-Pedersen <jlp@gomspace.com>");
MODULE_DESCRIPTION("Cypress FM33256B Processor Companion RTC Driver");
MODULE_LICENSE("GPL v2");
