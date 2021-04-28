/*
 * Backlight device driver for SC2703
 * Copyright (c) 2018 Dialog Semiconductor.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/mfd/sc2703/sc2703-disp.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regmap.h>

#define LCD_IRQ_NUM	3
#define DISP_SYSCTRL_IRQ_MASK\
		(SC2703_M_VSYS_UV_OT_VREF_FLT_MASK |\
		SC2703_M_VSYS_UV_OT_VREF_FLT_MASK |\
		SC2703_M_CHCR_MASK |\
		SC2703_M_VDDIO_FLT_MASK |\
		SC2703_M_SYS_WDT_MASK)
#define DISP_LCD_IRQ_MASK\
		(SC2703_M_BOOST_OVP_MASK |\
		SC2703_M_BOOST_OCP_MASK |\
		SC2703_M_BOOST_UVP_MASK |\
		SC2703_M_DLDO_SC_MASK |\
		SC2703_M_CP_OVP_MASK |\
		SC2703_M_CP_UVP_MASK)
#define DISP_WLED_IRQ_MASK\
		(SC2703_M_WLED_OC_MASK |\
		SC2703_M_WLED_OV_MASK |\
		SC2703_M_WLED_UV_MASK)
#define SC2703_D_IRQ_M_CLR 0
#define SC2703_D_IRQ_M_SET 1

/* Default pwm freq to 20kHz and Max 33kHz */
#define SC2703_WLED_FREQ_DEF	0 /* 20kHz */
#define SC2703_WLED_FREQ_MIN	20480
#define SC2703_WLED_FREQ_MAX	34816

/* pwm output duty range 0 ~ 256 */
#define SC2703_WLED_PWM_DUTY_MAX 255 /* 100% */

/* IDAC, Max = 40960 uA = 160uA * 255 steps */
#define SC2703_WLED_IDAC_MAX 40960
#define SC2703_WLED_IDAC_DEF 0

/* This is for Direct mode/GPIO mode to set PWM default*/
#define SC2703_WLED_PWM_DEFAULT 1000

#define SC2703_WLED_VDAC_MAX 1600000	/* uA */
#define SC2703_WLED_VDAC_MIN 20000	/* uA */
#define SC2703_WLED_VDAC_DEF 0x95
#define SC2703_WLED_VDAC_SEL_DEF 0x0

#define SC2703_PWM_DUTY_THRESH_MAX 255
#define SC2703_PWM_DUTY_THRESH_DEF 50

#define SC2703_PWM_BRIGHTNESS_MAX 0xFFFF
#define SC2703_PWM_BRIGHTNESS_DEF 100

#define SC2703_WLED_MIN_PWM_FREQ	25	/* kHz */
#define SC2703_WLED_IN_PWM_FREQ_STD	30	/* kHz */
#define SC2703_WLED_MAX_PWM_FREQ	35	/* kHz */

enum sc2703_wled_mode {
	SC2703_WLED_BYPASS = 0,
	SC2703_WLED_DIRECT = 1,
	SC2703_WLED_DUTY_DET_PWM = 2,
	SC2703_WLED_DUTY_DET_ANALOG = 3,
	SC2703_WLED_DUTY_DET_MIXED = 4,
	SC2703_WLED_EXT_R_SINK = 5,
	SC2703_WLED_MODE_MAX,
};

enum sc2703_ctrl_mode {
	SC2703_IDAC_MODE = 0,
	SC2703_PWM_MODE = 1,
	SC2703_LEVEL_CTRL_DEF = SC2703_PWM_MODE,
};

struct sc2703_bl {
	struct device *dev;
	struct backlight_device *bd;
	struct regmap *regmap;
	struct pwm_device *pwm;
	struct gpio_desc *gpio;
	int irq;
	int brightness;
	u8 wled_mode;
	u8 level_mode;
	uint out_freq_step;
	uint pwm_out_duty;
	u32 wled_idac;
	u8 pwm_in_duty_thresh;
	uint max_brightness;
	u8 idac_ramp_disable;
	u8 vdac_fs_ext_r;
	u8 vdac_sel;
};

struct i2c_sysfs {
	bool reg_flag;
	unsigned int addr;
	unsigned int reg;
	unsigned int val;
	struct regmap *regmap;
};

static struct i2c_sysfs packet;

static ssize_t i2c_clear_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "reg = 0x%x val = 0x%x\n",
			packet.reg, packet.val);
}

static ssize_t i2c_clear_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	packet.reg = 0;
	packet.val = 0;
	packet.reg_flag = false;

	return count;
}
static DEVICE_ATTR_RW(i2c_clear);

static ssize_t i2c_reg_show(struct device *dev,
		  struct device_attribute *attr,
		  char *buf)
{
	return sprintf(buf, "reg = 0x%x, ready = 0x%x\n",
			packet.reg, packet.reg_flag);
}

static ssize_t i2c_reg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret = -EINVAL;

	memset(&packet.reg, 0, sizeof(unsigned int));

	ret = kstrtouint(buf, 16, &packet.reg);
	if (ret < 0) {
		packet.reg_flag = false;
		dev_err(dev, "i2c reg formal error\n");
		return ret;
	}

	packet.reg_flag = true;
	return count;
}
static DEVICE_ATTR_RW(i2c_reg);

static ssize_t i2c_val_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int ret = -EINVAL;

	if (packet.regmap == NULL) {
		dev_err(dev, "i2c address map error\n");
		return ret;
	}

	if (!packet.reg_flag) {
		dev_err(dev, "plase input reg to i2c\n");
		return ret;
	}

	packet.val = 0;

	ret = regmap_read(packet.regmap, packet.reg, &packet.val);
	if (ret) {
		dev_err(dev, "I2C read error(%d)\n", ret);
		return ret;
	}

	return sprintf(buf, "0x%x\n", packet.val);
}

static ssize_t i2c_val_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret = -EINVAL;

	if (packet.regmap == NULL) {
		dev_err(dev, "i2c address map error\n");
		return ret;
	}

	if (!packet.reg_flag) {
		dev_err(dev, "plase input reg to i2c\n");
		return ret;
	}

	packet.val = 0;

	ret = kstrtouint(buf, 16, &packet.val);
	if (ret < 0) {
		dev_err(dev, "i2c addr formal error\n");
		return ret;
	}

	ret = regmap_write(packet.regmap, packet.reg, packet.val);
	if (ret) {
		dev_err(dev, "I2C write error(%d)\n", ret);
		return ret;
	}

	return count;
}
static DEVICE_ATTR_RW(i2c_val);

static struct attribute *i2c_attrs[] = {
	&dev_attr_i2c_clear.attr,
	&dev_attr_i2c_reg.attr,
	&dev_attr_i2c_val.attr,
	NULL,
};
ATTRIBUTE_GROUPS(i2c);

static int sc2703_i2c_sysfs_init(struct device *dev)
{
	int ret;

	ret = sysfs_create_groups(&dev->kobj, i2c_groups);
	if (ret)
		dev_err(dev, "create i2c sysfs node failed, ret=%d\n", ret);

	return ret;
}

static void sc2703_i2c_sysfs_exit(struct device *dev)
{
	sysfs_remove_groups(&dev->kobj, i2c_groups);
}

static int sc2703_bl_set_pwm(struct sc2703_bl *bl, int brightness)
{
	u32 pwm_duty;
	int ret;

	if (brightness > bl->max_brightness)
		brightness = bl->max_brightness;
	else if (brightness < 0)
		brightness = 0;

	/*
	 * The formula to convert level(from host) to pwm duty cycle as below:
	 * pwm_duty range (0 ~ 100%) = (level * pwm_period) / MAXIMUM
	 */
	pwm_duty = bl->pwm->args.period * brightness / bl->max_brightness;

	ret = pwm_config(bl->pwm, pwm_duty, bl->pwm->args.period);
	if (ret) {
		dev_err(bl->dev, "failed to set pwm duty cycle: %d\n", ret);
		return ret;
	}

	if (brightness > 0)
		pwm_enable(bl->pwm);
	else
		pwm_disable(bl->pwm);

	return 0;
}

static int sc2703_update_brightness(struct sc2703_bl *bl, int brightness)
{
	int ret;

	if (bl->brightness == brightness)
		return 0;

	if (bl->wled_mode != SC2703_WLED_DIRECT) {
		ret = sc2703_bl_set_pwm(bl, brightness);
		if (ret)
			return ret;
	}

	bl->brightness = brightness;

	dev_info(bl->dev, "wled_mode = %d, brightness = %d\n",
		bl->wled_mode, brightness);

	return 0;
}

static int sc2703_bl_update_status(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;
	struct sc2703_bl *bl = bl_get_data(bd);

	if (bd->props.power != FB_BLANK_UNBLANK ||
	    bd->props.fb_blank != FB_BLANK_UNBLANK ||
	    bd->props.state & BL_CORE_FBBLANK)
		brightness = 0;

	return sc2703_update_brightness(bl, brightness);
}

static int sc2703_bl_get_brightness(struct backlight_device *bd)
{
	struct sc2703_bl *bl = bl_get_data(bd);

	return bl->brightness;
}

static const struct backlight_ops sc2703_bl_ops = {
	.update_status = sc2703_bl_update_status,
	.get_brightness = sc2703_bl_get_brightness,
};

static u8 str_to_wled_mode(struct device *dev, const char *str)
{
	if (!strcmp(str, "bypass-mode"))
		return SC2703_WLED_BYPASS;
	else if (!strcmp(str, "direct-mode"))
		return SC2703_WLED_DIRECT;
	else if (!strcmp(str, "duty-det-pwm-mode"))
		return SC2703_WLED_DUTY_DET_PWM;
	else if (!strcmp(str, "duty-det-analog-mode"))
		return SC2703_WLED_DUTY_DET_ANALOG;
	else if (!strcmp(str, "duty-det-mixed-mode"))
		return SC2703_WLED_DUTY_DET_MIXED;
	else if (!strcmp(str, "ext-r-sink-mode"))
		return SC2703_WLED_EXT_R_SINK;

	dev_err(dev, "Invalid mode - set to default\n");
	return SC2703_WLED_DUTY_DET_ANALOG;
}

static u8 str_to_level_mode(struct device *dev, const char *str)
{
	if (!strcmp(str, "idac-control"))
		return SC2703_IDAC_MODE;
	else if (!strcmp(str, "pwm-control"))
		return SC2703_PWM_MODE;

	dev_err(dev, "Invalid WLED mode - set to default\n");
	return SC2703_LEVEL_CTRL_DEF;
}

static int sc2703_bl_wled_init(struct sc2703_bl *bl)
{
	int ret = 0;
	bool fix_wled_idac = 0;
	bool fix_pwm_out_freq = 0;
	bool fix_pwm_out_duty = 0;
	bool set_pwm_in_range = 0;

	/* WLED power control selected by (pwm) gpio */
	ret = regmap_update_bits(bl->regmap,
		SC2703_SYSCTRL_SEQ_MODE_CONTROL1,
		SC2703_WLED_POWER_CTRL_SELECT_MASK,
		SC2703_WLED_POWER_CTRL_SELECT_MASK);
	if (ret)
		goto err_i2c;

	/* WLED mode init */
	ret = regmap_update_bits(bl->regmap,
		SC2703_WLED_CONFIG1,
		SC2703_WLED_MODE_MASK | SC2703_WLED_IDAC_EN_MASK,
		bl->wled_mode << SC2703_WLED_MODE_SHIFT |
		SC2703_WLED_IDAC_EN_MASK);
	if (ret)
		goto err_i2c;

	switch (bl->wled_mode) {
	case SC2703_WLED_BYPASS:
		set_pwm_in_range = 1;
		fix_wled_idac = 1;
		break;
	case SC2703_WLED_DIRECT:
		if (bl->level_mode == SC2703_IDAC_MODE)
			fix_pwm_out_duty = 1;
		else
			fix_wled_idac = 1;
		break;
	case SC2703_WLED_DUTY_DET_PWM:
		fix_pwm_out_freq = 1;
		fix_wled_idac = 1;
		set_pwm_in_range = 1;
		break;
	case SC2703_WLED_DUTY_DET_ANALOG:
		set_pwm_in_range = 1;
		break;
	case SC2703_WLED_DUTY_DET_MIXED:
		set_pwm_in_range = 1;
		fix_pwm_out_freq = 1;
		ret = regmap_write(bl->regmap,
				SC2703_WLED_CONFIG4,
				bl->pwm_in_duty_thresh & 0xFF);
		if (ret)
			goto err_i2c;
		break;
	case SC2703_WLED_EXT_R_SINK:
		set_pwm_in_range = 1;
		fix_wled_idac = 1;
		bl->wled_idac = SC2703_WLED_IDAC_DEF;

		/* check if need to update from time to time */
		ret = regmap_write(bl->regmap,
				SC2703_WLED_CONFIG9,
				bl->vdac_fs_ext_r & 0xFF);
		if (ret)
			goto err_i2c;
		break;
	default:
		dev_err(bl->dev, "Invalid WLED Mode : %d\n", bl->wled_mode);
		break;
	}

	if (fix_pwm_out_freq) {
		ret = regmap_update_bits(bl->regmap,
			SC2703_WLED_CONFIG3,
			SC2703_PWM_OUT_FREQ_STEP_MASK |
			SC2703_IDAC_RAMP_RATE_MASK,
			(bl->out_freq_step << SC2703_PWM_OUT_FREQ_STEP_SHIFT) |
			(0 << SC2703_IDAC_RAMP_RATE_SHIFT));

		if (ret)
			goto err_i2c;
	}

	if (fix_pwm_out_duty) {
		ret = regmap_write(bl->regmap,
			SC2703_WLED_CONFIG5,
			bl->pwm_out_duty);
		if (ret)
			goto err_i2c;
	}

	if (fix_wled_idac) {
		/* Fix IDAC and control by internal PWM duty */
		ret = regmap_write(bl->regmap,
			SC2703_WLED_CONFIG2,
			bl->wled_idac <= 160 ?
			 0 : (bl->wled_idac / 160 - 1) & 0xFF);
		if (ret)
			goto err_i2c;
	}

	if (set_pwm_in_range) {
		unsigned int period2Freq = 1000000 / bl->pwm->args.period;

		/* Check PWM freq and set range bit */
		if (period2Freq > SC2703_WLED_MAX_PWM_FREQ ||
			period2Freq < SC2703_WLED_MIN_PWM_FREQ) {
			dev_err(bl->dev,
				"Invalid pwm input: %dkHz(must be 25 ~ 35kHz)\n",
				period2Freq);
			return -EINVAL;
		}

		ret = regmap_update_bits(bl->regmap,
			SC2703_WLED_CONFIG3,
			SC2703_PWM_IN_FREQ_RANGE_MASK,
			(period2Freq < SC2703_WLED_IN_PWM_FREQ_STD) ?
			0 : SC2703_PWM_IN_FREQ_RANGE_MASK);

		if (ret)
			goto err_i2c;
	}

	if (bl->idac_ramp_disable) {
		ret = regmap_update_bits(bl->regmap,
			SC2703_WLED_CONFIG7,
			SC2703_IDAC_RAMP_DIS_MASK,
			SC2703_IDAC_RAMP_DIS_MASK);

		if (ret)
			goto err_i2c;
	}

	ret = regmap_update_bits(bl->regmap,
		SC2703_WLED_CONFIG6,
		SC2703_WLED_PANIC_VTH_MASK,
		SC2703_WLED_PANIC_VTH_MASK);
	if (ret)
		goto err_i2c;

	ret = regmap_update_bits(bl->regmap,
		SC2703_WLED_BOOST_CONTROL1,
		SC2703_PANIC_FB_SEL_MASK,
		SC2703_PANIC_FB_SEL_MASK);
	if (ret)
		goto err_i2c;

	bl->brightness = 0;
	return 0;

err_i2c:
	dev_err(bl->dev, "I2C error(%d)\n", ret);
	return ret;
}

int sc2703_bl_irq_mask(struct sc2703_bl *bl, u8 set)
{
	unsigned int regs[] = {
		SC2703_SYSCTRL_IRQ_MASK,
		SC2703_DISPLAY_IRQ_MASK_A,
		SC2703_WLED_IRQ_MASK,
	};
	unsigned int masks[] = {
		DISP_SYSCTRL_IRQ_MASK,
		DISP_LCD_IRQ_MASK,
		DISP_WLED_IRQ_MASK };
	int ret = 0, i;

	for (i = 0; i < LCD_IRQ_NUM; i++) {
		ret = regmap_write(bl->regmap, regs[i],
				set ? masks[i] : 0);
		if (ret) {
			dev_err(bl->dev, "I2C error(%d)\n", ret);
			return ret;
		}
	}
	return 0;
}

static irqreturn_t sc2703_bl_irq_handler(int irq, void *data)
{
	struct sc2703_bl *bl = data;
	unsigned int regs[] = {
		SC2703_SYSCTRL_EVENT,
		SC2703_DISPLAY_EVENT_A,
		SC2703_WLED_EVENT,
	};
	unsigned int events[LCD_IRQ_NUM];
	int ret = 0, i;

	/* Check what events have happened */
	for (i = 0; i < LCD_IRQ_NUM; i++) {
		ret = regmap_read(bl->regmap, regs[i], &events[i]);
		if (ret)
			return IRQ_RETVAL(ret);
	}

	/* Empty check due to shared interrupt */
	if ((events[0] | events[1] | events[2]) == 0x00)
		return IRQ_HANDLED;

	/* Clear events */
	for (i = 0; i < LCD_IRQ_NUM; i++) {
		if (events[i]) {
			ret = regmap_write(bl->regmap,
				regs[i], events[i]);
			if (ret)
				return IRQ_RETVAL(ret);
			dev_info(bl->dev, "event(0x%02x): 0x%02x\n",
				regs[i], events[i]);
		}
	}

	return IRQ_HANDLED;
}

static int sc2703_bl_irq_init(struct sc2703_bl *bl)
{
	int ret;

	bl->gpio = devm_gpiod_get(bl->dev, "int", GPIOD_IN);
	if (IS_ERR(bl->gpio)) {
		dev_warn(bl->dev, "get int-gpio failed\n");
		return 0;
	}

	bl->irq = gpiod_to_irq(bl->gpio);
	if (!bl->irq) {
		dev_err(bl->dev, "map gpio irq failed\n");
		return -EINVAL;
	}

	ret = sc2703_bl_irq_mask(bl, SC2703_D_IRQ_M_SET);
	if (ret)
		return ret;

	ret = devm_request_threaded_irq(bl->dev, bl->irq, NULL,
		sc2703_bl_irq_handler, IRQF_TRIGGER_LOW |
		IRQF_ONESHOT | IRQF_SHARED, "sc2703-bl", bl);
	if (ret) {
		dev_err(bl->dev, "failed to request IRQ: %d\n", bl->irq);
		return ret;
	}

	ret = sc2703_bl_irq_mask(bl, SC2703_D_IRQ_M_CLR);
	if (ret)
		return ret;

	return 0;
}

static int sc2703_bl_parse_dt(struct device_node *np, struct sc2703_bl *bl)
{
	u32 val;
	const char *str;

	if (!of_property_read_string(np, "sprd,wled-mode", &str))
		bl->wled_mode = str_to_wled_mode(bl->dev, str);
	else
		bl->wled_mode = SC2703_WLED_BYPASS;

	/* In case of direct mode, this must be selected */
	if (bl->wled_mode == SC2703_WLED_DIRECT) {
		if (!of_property_read_string(np, "sprd,level-ctrl-by", &str))
			bl->level_mode = str_to_level_mode(bl->dev, str);
		else
			bl->level_mode = SC2703_LEVEL_CTRL_DEF;
	}

	if (bl->wled_mode != SC2703_WLED_DIRECT) {
		bl->pwm = of_pwm_get(np, "wled-pwm");
		if (IS_ERR(bl->pwm)) {
			dev_err(bl->dev, "unable to request PWM\n");
			return PTR_ERR(bl->pwm);
		}
	}

	if (!of_property_read_u32(np, "sprd,pwm-out-freq-hz", &val)) {
		if (val >= SC2703_WLED_FREQ_MIN
			&& val <= SC2703_WLED_FREQ_MAX)
			bl->out_freq_step = (val / 1024 - 20) / 2;
		else
			bl->out_freq_step = SC2703_WLED_FREQ_DEF;
	} else
		bl->out_freq_step = SC2703_WLED_FREQ_DEF;

	if (!of_property_read_u32(np, "sprd,pwm-out-duty", &val)) {
		if (val <= SC2703_WLED_PWM_DUTY_MAX)
			bl->pwm_out_duty = val;
		else
			bl->pwm_out_duty = SC2703_WLED_PWM_DUTY_MAX / 2;
	} else
		bl->pwm_out_duty = SC2703_WLED_PWM_DUTY_MAX / 2;

	if (!of_property_read_u32(np, "sprd,wled-idac-microamp", &val)) {
		if (val <= SC2703_WLED_IDAC_MAX)
			bl->wled_idac = val;
		else
			bl->wled_idac = SC2703_WLED_IDAC_DEF;
	} else
		bl->wled_idac = SC2703_WLED_IDAC_DEF;

	if (!of_property_read_u32(np, "sprd,pwm-in-duty-thresh", &val)) {
		if (val <= SC2703_PWM_DUTY_THRESH_MAX)
			bl->pwm_in_duty_thresh = (u8)(val & 0xFF);
		else
			bl->pwm_in_duty_thresh = SC2703_PWM_DUTY_THRESH_DEF;
	} else
		bl->pwm_in_duty_thresh = SC2703_PWM_DUTY_THRESH_DEF;

	/* Set the max_brightness by the wled_mode and control factor */
	if (bl->wled_mode != SC2703_WLED_DIRECT) {
		if (!of_property_read_u32(np, "sprd,max-brightness", &val)) {
			if (val <= SC2703_PWM_BRIGHTNESS_MAX)
				bl->max_brightness = val;
			else
				bl->max_brightness = SC2703_PWM_BRIGHTNESS_DEF;
		} else
			bl->max_brightness = SC2703_PWM_BRIGHTNESS_DEF;
	} else {
		if (bl->level_mode == SC2703_IDAC_MODE)
			bl->max_brightness = SC2703_WLED_IDAC_MAX;
		else
			bl->max_brightness = SC2703_WLED_PWM_DUTY_MAX;
	}

	if (!of_property_read_u32(np, "sprd,vdac-code-microvolt", &val)) {
		/* the case less than 20mV */
		if (val < SC2703_WLED_VDAC_MIN)
			bl->vdac_fs_ext_r = 0;
		else if (val <= SC2703_WLED_VDAC_MAX)
			bl->vdac_fs_ext_r = (val / 10000) - 1;
		else /* the case more than 1.6V */
			bl->vdac_fs_ext_r = SC2703_WLED_VDAC_DEF;
	} else
		bl->vdac_fs_ext_r = SC2703_WLED_VDAC_DEF;

	if (!of_property_read_u32(np, "sprd,vdac-sel-microvolt", &val)) {
		/* the case less than 20mV */
		if (val < SC2703_WLED_VDAC_MIN)
			bl->vdac_sel = 0;
		else if (val <= SC2703_WLED_VDAC_MAX)
			bl->vdac_sel = (val / 10000) - 1;
		else /* the case more than 1.6V */
			bl->vdac_sel = SC2703_WLED_VDAC_SEL_DEF;
	} else
		bl->vdac_sel = SC2703_WLED_VDAC_SEL_DEF;

	bl->idac_ramp_disable =
		of_property_read_bool(np, "sprd,idac-ramp-disable");

	return 0;
}

static int sc2703_backlight_probe(struct platform_device *pdev)
{
	struct backlight_properties props = {};
	struct sc2703_bl *bl;
	int ret;

	bl = devm_kzalloc(&pdev->dev, sizeof(*bl), GFP_KERNEL);
	if (!bl)
		return -ENOMEM;
	bl->dev = &pdev->dev;

	bl->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!bl->regmap) {
		dev_err(&pdev->dev, "get i2c regmap failed\n");
		return -ENODEV;
	}

	ret = sc2703_bl_parse_dt(pdev->dev.of_node, bl);
	if (ret)
		return ret;

	props.type = BACKLIGHT_RAW;
	props.max_brightness = bl->max_brightness;
	bl->bd = devm_backlight_device_register(&pdev->dev,
					"sprd_backlight", &pdev->dev,
					bl, &sc2703_bl_ops, &props);
	if (IS_ERR(bl->bd)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		return PTR_ERR(bl->bd);
	}

	ret = sc2703_i2c_sysfs_init(&bl->bd->dev);
	if (ret)
		return ret;
	packet.regmap = bl->regmap;

	ret = sc2703_bl_wled_init(bl);
	if (ret)
		return ret;

	ret = sc2703_bl_irq_init(bl);
	if (ret)
		return ret;

	return sc2703_update_brightness(bl, props.max_brightness / 10);
}

static int sc2703_backlight_remove(struct platform_device *pdev)
{
	struct sc2703_bl *bl = platform_get_drvdata(pdev);

	sc2703_update_brightness(bl, 0);

	sc2703_i2c_sysfs_exit(&bl->bd->dev);

	return 0;
}

static const struct of_device_id sc2703_bl_of_match[] = {
	{ .compatible = "sprd,sc2703-backlight", },
	{ }
};
MODULE_DEVICE_TABLE(of, sc2703_bl_of_match);

static struct platform_driver sc2703_backlight_driver = {
	.driver		= {
		.name		= "sc2703-backlight",
		.of_match_table	= sc2703_bl_of_match,
	},
	.probe		= sc2703_backlight_probe,
	.remove		= sc2703_backlight_remove,
};

module_platform_driver(sc2703_backlight_driver);

MODULE_DESCRIPTION("Backlight device driver for SC2703");
MODULE_AUTHOR("Roy Im <Roy.Im.Opensource@diasemi.com>");
MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_LICENSE("GPL v2");
