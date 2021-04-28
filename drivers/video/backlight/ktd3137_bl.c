/*
 * Simple driver for KTD3137 Backlight driver chip
 * Copyright (C) 2019 Spreadtrum.
 *
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU
 * General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include "ktd3137.h"

#define REG_DEV_ID	0x00
#define REG_CHECKSUM	0x01
#define REG_BL_CONF_1	0x02
#define REG_BL_CONF_2	0x03
#define REG_BL_CONF_3	0x04
#define REG_BL_CONF_4	0x05
#define REG_FL_CONF_1	0x06
#define REG_FL_CONF_2	0x07
#define REG_FL_CONF_3	0x08
#define REG_IO_CTRL	0x09
#define REG_ENABLE	0x0A
#define REG_FLAG	0x0B
#define REG_MAX		REG_FLAG

static int ktd3137_read_reg(struct i2c_client *client, int reg, u8 *val)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	else
		*val = ret;

	return ret;
}

static int ktd3137_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int ktd3137_masked_write(struct i2c_client *client, int reg,
			       u8 mask, u8 val)
{
	int rc;
	u8 temp;

	rc = ktd3137_read_reg(client, reg, &temp);
	if (rc < 0)
		dev_err(&client->dev, "failed to read reg=0x%x, rc=%d\n",
		       reg, rc);
	else {
		temp &= ~mask;
		temp |= val & mask;
		rc = ktd3137_write_reg(client, reg, temp);
		if (rc < 0)
			dev_err(&client->dev,
			"failed to write masked data. reg=%03x, rc=%d\n",
			reg, rc);
	}

	return rc;
}

static int ktd3137_update_brightness(struct ktd3137_bl *bl, int brightness)
{
	int ret;
	struct ktd3137_bl_data *pdata = bl->pdata;

	if (bl->brightness == brightness)
		return 0;

	if (!bl->brightness && brightness)
		ktd3137_masked_write(bl->client, REG_MODE, 0x01, 0x01);
	else if (!brightness)
		ktd3137_masked_write(bl->client, REG_MODE, 0x01, 0x00);

	if (brightness > pdata->max_brightness)
		brightness = pdata->max_brightness;

	ret = ktd3137_write_reg(bl->client, REG_RATIO_MSB, brightness);
	if (ret)
		return ret;

	bl->brightness = brightness;

	return 0;
}

static int ktd3137_bled_update_status(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;
	struct ktd3137_bl *bl = bl_get_data(bd);

	if (bd->props.power != FB_BLANK_UNBLANK ||
	   bd->props.fb_blank != FB_BLANK_UNBLANK ||
	   bd->props.state & BL_CORE_FBBLANK)
		brightness = 0;

	return ktd3137_update_brightness(bl, brightness);
}

static const struct backlight_ops ktd3137_bled_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = ktd3137_bled_update_status,
};

static const struct regmap_config ktd3137_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_MAX,
};

static int ktd3137_parse_dt(struct device *dev, struct ktd3137_bl *bl)
{
	struct device_node *np = dev->of_node;
	struct ktd3137_bl_data *pdata = bl->pdata;
	u32 val;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->pwm_mode = of_property_read_bool(np, "ktd,pwm-mode");
	pdata->using_lsb = of_property_read_bool(np, "ktd,using-lsb");
	pdata->using_linear = of_property_read_bool(np, "ktd,using-linear");

	pdata->default_brightness = 0xff;
	pdata->max_brightness = 255;

	if (!of_property_read_u32(np, "ktd,pwm-frequency", &val))
		pdata->pwm_period = val;
	else
		dev_err(dev, "parse pwm-frequency failed\n");

	if (!of_property_read_u32(np, "ktd,bl-fscal-led", &val))
		pdata->full_scale_led = val;
	else
		dev_err(dev, "parse bl-fscal-led failed\n");

	if (!of_property_read_u32(np, "ktd,turn-on-ramp", &val))
		pdata->ramp_on_time = val;
	else
		dev_err(dev, "parse turn-on-ramp failed\n");

	if (!of_property_read_u32(np, "ktd,turn-off-ramp", &val))
		pdata->ramp_off_time = val;
	else
		dev_err(dev, "parse turn-off-ramp failed\n");

	if (!of_property_read_u32(np, "ktd,pwm-trans-dim", &val))
		pdata->pwm_trans_dim = val;
	else
		dev_err(dev, "parse pwm-trans-dim failed\n");

	if (!of_property_read_u32(np, "ktd,i2c-trans-dim", &val))
		pdata->i2c_trans_dim = val;
	else
		dev_err(dev, "parse i2c-trans-dim failed\n");

	if (!of_property_read_u32(np, "ktd,bl-channel", &val))
		pdata->channel = val;
	else
		dev_err(dev, "parse bl-channel failed\n");

	if (!of_property_read_u32(np, "ktd,ovp-level", &val))
		pdata->ovp_level = val;
	else
		dev_err(dev, "parse ovp-level failed\n");

	if (!of_property_read_u32(np, "ktd,switching-frequency", &val))
		pdata->frequency = val;
	else
		dev_err(dev, "parse switching-frequency failed\n");

	if (!of_property_read_u32(np, "ktd,inductor-current", &val))
		pdata->induct_current = val;
	else
		dev_err(dev, "parse inductor-current failed\n");

	if (!of_property_read_u32(np, "ktd,flash-timeout", &val))
		pdata->flash_timeout = val;
	else
		dev_err(dev, "parse flash-timeout failed\n");

	if (!of_property_read_u32(np, "ktd,flash-current", &val))
		pdata->flash_current = val;
	else
		dev_err(dev, "parse flash-current failed\n");

	dev->platform_data = pdata;

	return 0;
}

static int ktd_find_bit(int x)
{
	int i = 0;

	while ((x = x >> 1))
		i++;

	return i + 1;
}

static void ktd3137_ramp_setting(struct ktd3137_bl *bl)
{
	struct ktd3137_bl_data *pdata = bl->pdata;
	const u32 max_time = 16384;
	int temp;

	if (pdata->ramp_on_time == 0)
		ktd3137_masked_write(bl->client, REG_RAMP_ON, 0xf0, 0x00);
	else if (pdata->ramp_on_time > max_time)
		ktd3137_masked_write(bl->client, REG_RAMP_ON, 0xf0, 0xf0);
	else {
		temp = ktd_find_bit(pdata->ramp_on_time);
		ktd3137_masked_write(bl->client, REG_RAMP_ON, 0xf0, temp << 4);
	}

	if (pdata->ramp_off_time == 0)
		ktd3137_masked_write(bl->client, REG_RAMP_ON, 0x0f, 0x00);
	else if (pdata->ramp_off_time > max_time)
		ktd3137_masked_write(bl->client, REG_RAMP_ON, 0x0f, 0x0f);
	else {
		temp = ktd_find_bit(pdata->ramp_off_time);
		ktd3137_masked_write(bl->client, REG_RAMP_ON, 0x0f, temp);
	}
}

static void ktd3137_transition_ramp(struct ktd3137_bl *bl)
{
	struct ktd3137_bl_data *pdata = bl->pdata;
	int reg_i2c, reg_pwm, temp;

	if (pdata->i2c_trans_dim >= 1024)
		reg_i2c = 0xf;
	else if (pdata->i2c_trans_dim < 128)
		reg_i2c = 0x0;
	else {
		temp = pdata->i2c_trans_dim / 64;
		reg_i2c = temp - 1;
	}

	if (pdata->pwm_trans_dim >= 256)
		reg_pwm = 0x7;
	else if (pdata->pwm_trans_dim < 4)
		reg_pwm = 0x0;
	else {
		temp = ktd_find_bit(pdata->pwm_trans_dim);
		reg_pwm = temp - 2;
	}

	ktd3137_masked_write(bl->client, REG_TRANS_RAMP, 0x70, reg_pwm);
	ktd3137_masked_write(bl->client, REG_TRANS_RAMP, 0x0f, reg_i2c);
}

static void ktd3137_backlight_init(struct ktd3137_bl *bl)
{
	struct ktd3137_bl_data *pdata = bl->pdata;
	u8 value;
	u8 update_value;

	update_value = (pdata->ovp_level == 32) ? 0x20 : 0x00;
	(pdata->induct_current == 2600) ? update_value |= 0x08 : update_value;
	(pdata->frequency == 1000) ? update_value |= 0x40 : update_value;
	(pdata->using_linear == 1) ? update_value |= 0x02 : update_value;

	ktd3137_write_reg(bl->client, REG_CONTROL, update_value);
	ktd3137_masked_write(bl->client, REG_PWM, 0x80, 0x80);

	ktd3137_ramp_setting(bl);
	ktd3137_transition_ramp(bl);
	ktd3137_read_reg(bl->client, REG_CONTROL, &value);
	ktd3137_write_reg(bl->client, REG_MODE, pdata->full_scale_led);
	bl->brightness = 0;
}

static int ktd3137_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	int ret;
	struct ktd3137_bl *bl;
	struct ktd3137_bl_data *pdata = dev_get_platdata(&client->dev);
	struct backlight_properties props;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c functionality check fail.\n");
		return -EOPNOTSUPP;
	}

	bl = devm_kzalloc(&client->dev, sizeof(struct ktd3137_bl), GFP_KERNEL);
	if (!bl)
		return -ENOMEM;

	if (!pdata) {
		ret = ktd3137_parse_dt(&client->dev, bl);
		if (ret)
			return ret;
		pdata = dev_get_platdata(&client->dev);
	}
	bl->client = client;
	bl->pdata = pdata;
	bl->dev = &client->dev;
	i2c_set_clientdata(client, bl);

	bl->hwen_gpio = devm_gpiod_get_optional(&client->dev, "hwen",
					   GPIOD_ASIS);
	if (IS_ERR_OR_NULL(bl->hwen_gpio))
	dev_err(&client->dev, "can't get ktd3137 hwen gpio: %ld\n",
	       PTR_ERR(bl->hwen_gpio));

	if (bl->hwen_gpio)
		gpiod_direction_output(bl->hwen_gpio, 1);

	ktd3137_backlight_init(bl);

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = 255;
	bl->bd = devm_backlight_device_register(bl->dev,
						"sprd_backlight", bl->dev, bl,
						&ktd3137_bled_ops, &props);
	if (IS_ERR(bl->bd)) {
		dev_err(&client->dev, "fail : backlight register\n");
		ret = PTR_ERR(bl->bd);
		return ret;
	}

	return 0;
}

static int ktd3137_remove(struct i2c_client *client)
{
	struct ktd3137_bl *bl = i2c_get_clientdata(client);

	ktd3137_update_brightness(bl, 0);
	devm_gpiod_put(&client->dev, bl->hwen_gpio);

	return 0;
}

static const struct of_device_id ktd3137_match_table[] = {
	{ .compatible = "sprd,ktd3137",},
	{ },
};

static const struct i2c_device_id ktd3137_id[] = {
	{KTD3137_NAME, 0},
	{ },
};

MODULE_DEVICE_TABLE(i2c, ktd3137_id);
static struct i2c_driver ktd3137_i2c_driver = {
	.driver = {
		.name = KTD3137_NAME,
		.of_match_table = ktd3137_match_table,
	},
	.probe = ktd3137_probe,
	.remove = ktd3137_remove,
	.id_table = ktd3137_id,
};

module_i2c_driver(ktd3137_i2c_driver);

MODULE_DESCRIPTION("Hisense Backlight driver for ktd3137");
MODULE_AUTHOR("Albert Zhang <albert.zhang@unisoc.com>");
MODULE_LICENSE("GPL v2");
