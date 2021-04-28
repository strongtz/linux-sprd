/*
 ** Copyright (C) 2019 Spreadtrum Communications Inc.
 **
 ** This software is licensed under the terms of the GNU General Public
 ** License version 2, as published by the Free Software Foundation, and
 ** may be copied, distributed, and modified under those terms.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h>

#include "core.h"
#include "../../../gpio/gpiolib.h"

#define SPRD_GPIO_TEST_NUM_MSK		GENMASK(15, 0)
#define SPRD_GPIO_TEST_DIR_MSK		GENMASK(16, 16)
#define SPRD_GPIO_TEST_VAL_MSK		GENMASK(17, 17)
#define SPRD_GPIO_TEST_VAL_SHIFT	17

#define SPRD_GPIO_TEST_NUM(i)		((i) & SPRD_GPIO_TEST_NUM_MSK)
#define SPRD_GPIO_TEST_DIR(i)		(!!((i) & SPRD_GPIO_TEST_DIR_MSK))
#define SPRD_GPIO_TEST_VAL(i)		(!!((i) & SPRD_GPIO_TEST_VAL_MSK))

static int gpio_test(struct autotest_handler *handler, void *arg)
{
	int gpio_data, num, dir, val, ret = 0;
	struct gpio_desc *desc;
	struct gpio_chip *chip;

	if (get_user(gpio_data, (int __user *)arg))
		return -EFAULT;

	num = SPRD_GPIO_TEST_NUM(gpio_data);
	dir = SPRD_GPIO_TEST_DIR(gpio_data);
	val = SPRD_GPIO_TEST_VAL(gpio_data);
	pr_info("num=%d, dir=%d, val=%d\n", num, dir, val);

	desc = gpio_to_desc(num);
	chip = gpiod_to_chip(desc);
	if (!chip) {
		pr_err("get gpio chip failed.\n");
		return -EINVAL;
	}

	ret = gpiod_request(desc, "autotest-gpio");
	if (ret < 0 && ret != -EBUSY) {
		pr_err("gpio request failed.\n");
		return ret;
	}

	if (dir) {
		if (gpiochip_line_is_irq(chip, num - chip->base)) {
			ret = chip->direction_output(chip,
						     num - chip->base, val);
		} else {
			ret = gpiod_direction_output(desc, val);
		}

		if (ret < 0) {
			pr_err("set direction failed, %d", ret);
			return ret;
		}
	} else {
		gpiod_direction_input(desc);
		val = gpiod_get_value(desc) ? 1 : 0;
		gpio_data &= ~SPRD_GPIO_TEST_VAL_MSK;
		gpio_data |= (val << SPRD_GPIO_TEST_VAL_SHIFT) &
			SPRD_GPIO_TEST_VAL_MSK;
		ret = put_user(gpio_data, (int __user *)arg);
		if (ret < 0) {
			pr_err("write to user failed.\n");
			return -EFAULT;
		}
	}

	return ret;
}

static struct autotest_handler gpio_handler = {
	.label = "gpio",
	.type = AT_GPIO,
	.start_test = gpio_test,
};

static int __init gpio_test_init(void)
{
	return sprd_autotest_register_handler(&gpio_handler);
}

static void __exit gpio_test_exit(void)
{
	sprd_autotest_unregister_handler(&gpio_handler);
}

late_initcall(gpio_test_init);
module_exit(gpio_test_exit);
