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

#ifndef __LINUX_MFD_FM33256B_H
#define __LINUX_MFD_FM33256B_H

#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>

/* Opcodes */
#define FM33256B_OP_WREN		0x06
#define FM33256B_OP_WRDI		0x04
#define FM33256B_OP_RDSR		0x05
#define FM33256B_OP_WRSR		0x01
#define FM33256B_OP_READ		0x03
#define FM33256B_OP_WRITE		0x02
#define FM33256B_OP_RDPC		0x13
#define FM33256B_OP_WRPC		0x12

/* RTC/Processor Companion Register Map */
#define FM33256B_ALARM_MONTH		0x1D
#define FM33256B_COMPANION_CONTROL_REG	0x18
#define FM33256B_SERIAL_BYTE0_REG	0x10
#define FM33256B_YEARS_REG		0x08
#define FM33256B_MONTH_REG		0x07
#define FM33256B_DATE_REG		0x06
#define FM33256B_DAY_REG		0x05
#define FM33256B_HOURS_REG		0x04
#define FM33256B_MINUTES_REG		0x03
#define FM33256B_SECONDS_REG		0x02
#define FM33256B_CAL_CONTROL_REG	0x01
#define FM33256B_RTC_ALARM_CONTROL_REG	0x00

/* Companion Control bits */
#define FM33256B_ALSW			BIT(6)
#define FM33256B_VBC			BIT(3)
#define FM33256B_FC			BIT(2)

/* RTC/Alarm Control bits */
#define FM33256B_R			BIT(0)
#define FM33256B_W			BIT(1)
#define FM33256B_CAL			BIT(2)
#define FM33256B_OSCEN			BIT(7)

/* Limits */
#define FM33256B_MAX_REGISTER		FM33256B_ALARM_MONTH
#define FM33256B_MAX_FRAM		(32 * 1024) /* 256 kb */

/**
 * Structure shared by the MFD device and its subdevices.
 *
 * @regmap: register map used to access registers
 */
struct fm33256b {
	struct mutex lock;
	struct regmap *regmap_pc;
	struct regmap *regmap_fram;
};

#endif /* __LINUX_MFD_FM33256B_H */
