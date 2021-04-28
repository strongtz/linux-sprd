/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>

#include "disp_lib.h"
#include "sprd_dpu.h"
#include "sprd_panel.h"
#include "sysfs_display.h"

static uint32_t bg_color;

static ssize_t run_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", !dpu->ctx.is_stopped);

	return ret;
}

static ssize_t run_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	int enable;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 10, &enable);
	if (ret) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	if (enable)
		sprd_dpu_run(dpu);
	else
		sprd_dpu_stop(dpu);

	return count;
}
static DEVICE_ATTR_RW(run);

static ssize_t refresh_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	down(&ctx->refresh_lock);

	pr_info("[drm] %s()\n", __func__);

	if (!ctx->is_inited) {
		pr_err("dpu is powered off\n");
		up(&ctx->refresh_lock);
		return -1;
	}

	ctx->disable_flip = false;

	dpu->core->flip(ctx, dpu->layers, dpu->pending_planes);

	up(&ctx->refresh_lock);

	return count;
}
static DEVICE_ATTR_WO(refresh);

static ssize_t bg_color_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%x\n", bg_color);

	return ret;
}

static ssize_t bg_color_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->bg_color)
		return -EIO;

	pr_info("[drm] %s()\n", __func__);

	ret = kstrtou32(buf, 16, &bg_color);
	if (ret) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	down(&ctx->refresh_lock);

	if (!ctx->is_inited) {
		pr_err("dpu is not initialized\n");
		up(&ctx->refresh_lock);
		return -EINVAL;
	}

	ctx->disable_flip = true;
	dpu->core->bg_color(ctx, bg_color);

	up(&ctx->refresh_lock);

	return count;
}
static DEVICE_ATTR_RW(bg_color);

static ssize_t disable_flip_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", ctx->disable_flip);

	return ret;
}
static DEVICE_ATTR_RO(disable_flip);

static ssize_t actual_fps_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct videomode vm = dpu->ctx.vm;
	uint32_t act_fps_int, act_fps_frac;
	uint32_t total_pixels;
	int ret;

	total_pixels = (vm.hsync_len + vm.hback_porch +
			vm.hfront_porch + vm.hactive) *
			(vm.vsync_len + vm.vback_porch +
			vm.vfront_porch + vm.vactive);

	act_fps_int = vm.pixelclock / total_pixels;
	act_fps_frac = vm.pixelclock % total_pixels;
	act_fps_frac = act_fps_frac * 100 / total_pixels;

	ret = snprintf(buf, PAGE_SIZE, "%u.%u\n", act_fps_int, act_fps_frac);

	return ret;
}
static DEVICE_ATTR_RO(actual_fps);

static ssize_t regs_offset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE,
			"dpu reg offset: %x\n"
			"dpu reg length: %x\n",
			dpu->ctx.base_offset[0],
			dpu->ctx.base_offset[1]);

	return ret;
}

static ssize_t regs_offset_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);

	str_to_u32_array(buf, 16, dpu->ctx.base_offset);

	return count;
}
static DEVICE_ATTR_RW(regs_offset);

static ssize_t wr_regs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned int i;
	unsigned int reg;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	unsigned int offset = dpu->ctx.base_offset[0];
	unsigned int length = dpu->ctx.base_offset[1];

	for (i = 0; i < length; i++) {
		reg = readl((void __iomem *)(dpu->ctx.base + offset));
		ret += snprintf(buf + ret, PAGE_SIZE, "%x ", reg);
	}

	return ret;
}

static ssize_t wr_regs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	uint32_t temp = dpu->ctx.base_offset[0];
	uint32_t length = dpu->ctx.base_offset[1];
	uint32_t *value;
	uint32_t i;

	value = kzalloc(length * 4, GFP_KERNEL);
	if (!value)
		return -ENOMEM;

	str_to_u32_array(buf, 16, value);

	for (i = 0; i < length; i++) {
		writel(value[i], (void __iomem *)(dpu->ctx.base + temp));
		temp += 0x04;
	}

	kfree(value);

	return count;
}
static DEVICE_ATTR_RW(wr_regs);

static ssize_t dpu_version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ctx->version);

	return ret;
}
static DEVICE_ATTR_RO(dpu_version);

static ssize_t wb_debug_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;
	void *vaddr = NULL;
	char filename[128];
	struct timex txc;
	struct rtc_time tm;

	if (!ctx->is_inited) {
		pr_err("dpu is not initialized\n");
		return -EINVAL;
	}

	if (dpu->core && dpu->core->write_back) {
		dpu->core->write_back(ctx, 1, true);
		vaddr = __va(ctx->wb_addr_p);
	} else
		return -ENXIO;

	/* FIXME: wait for writeback done isr */
	mdelay(50);

	do_gettimeofday(&(txc.time));
	rtc_time_to_tm(txc.time.tv_sec, &tm);
	sprintf(filename, "/data/dump/wb_%d-%d-%dT%d%d%d.bmp",
			tm.tm_year + 1900, tm.tm_mon, tm.tm_mday, tm.tm_hour,
			tm.tm_min, tm.tm_sec);

	dump_bmp32(vaddr, ctx->vm.hactive, ctx->vm.vactive, true, filename);

	return count;
}
static DEVICE_ATTR_WO(wb_debug);

static ssize_t irq_register_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	uint32_t value;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;
	int ret;

	if (kstrtou32(buf, 10, &value)) {
		pr_err("Invalid input for irq_register\n");
		return -EINVAL;
	}

	if (value > 0 && ctx->irq) {
		down(&ctx->refresh_lock);
		if (!ctx->is_inited) {
			pr_err("dpu is not initialized!\n");
			up(&ctx->refresh_lock);
			return -EINVAL;
		}

		if (dpu->core->enable_vsync)
			dpu->core->enable_vsync(ctx);

		ret = devm_request_irq(&dpu->dev, ctx->irq, ctx->dpu_isr,
			0, "DISPC", dpu);
		if (ret) {
			up(&ctx->refresh_lock);
			pr_err("error: dpu request irq failed\n");
			return ret;
		}

		/*
		 *We request dpu isr on sprd crtc driver and set the IRQ_NOAUTOEN flag,
		 *so if not clear this flag, need to call "enable_irq" enable it.
		 */
		enable_irq(ctx->irq);
		up(&ctx->refresh_lock);
	}

	return count;
}
static DEVICE_ATTR_WO(irq_register);

static ssize_t irq_unregister_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	uint32_t value;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (kstrtou32(buf, 10, &value)) {
		pr_err("Invalid input for irq_unregister\n");
		return -EINVAL;
	}

	if (value > 0 && ctx->irq) {
		down(&ctx->refresh_lock);
		if (!ctx->is_inited) {
			pr_err("dpu is not initialized!\n");
			up(&ctx->refresh_lock);
			return -EINVAL;
		}

		if (dpu->core->disable_vsync)
			dpu->core->disable_vsync(ctx);

		/*
		 *We request dpu isr on sprd crtc driver and set the IRQ_NOAUTOEN flag,
		 *so if not clear this flag, need to call "disable_irq" disable it.
		 */
		disable_irq(ctx->irq);
		devm_free_irq(&dpu->dev, ctx->irq, dpu);
		up(&ctx->refresh_lock);
	}

	return count;
}

static DEVICE_ATTR_WO(irq_unregister);

static struct attribute *dpu_attrs[] = {
	&dev_attr_run.attr,
	&dev_attr_refresh.attr,
	&dev_attr_bg_color.attr,
	&dev_attr_disable_flip.attr,
	&dev_attr_actual_fps.attr,
	&dev_attr_regs_offset.attr,
	&dev_attr_wr_regs.attr,
	&dev_attr_dpu_version.attr,
	&dev_attr_wb_debug.attr,
	&dev_attr_irq_register.attr,
	&dev_attr_irq_unregister.attr,
	NULL,
};
static const struct attribute_group dpu_group = {
	.attrs = dpu_attrs,
};

#define BIN_ATTR_WO(_name, _size) BIN_ATTR(_name, S_IWUSR, NULL,\
					_name##_write, _size)

static ssize_t ltm_read(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_get)
		return -EIO;

	if (off >= attr->size)
		return 0;

	if (off + count > attr->size)
		count = attr->size - off;

	down(&ctx->refresh_lock);
	if (!ctx->is_inited) {
		pr_err("dpu is not initialized\n");
		up(&ctx->refresh_lock);
		return -EINVAL;
	}
	dpu->core->enhance_get(ctx, ENHANCE_CFG_ID_LTM, buf);
	up(&ctx->refresh_lock);

	return count;
}

static ssize_t ltm_write(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_set)
		return -EIO;

	/* I need to get my data in one piece */
	if (off != 0)
		return -EINVAL;

	down(&ctx->refresh_lock);
	dpu->core->enhance_set(ctx, ENHANCE_CFG_ID_LTM, buf);
	up(&ctx->refresh_lock);

	return count;
}
static BIN_ATTR_RW(ltm, 48);

static ssize_t gamma_read(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_get)
		return -EIO;

	if (off >= attr->size)
		return 0;

	if (off + count > attr->size)
		count = attr->size - off;

	down(&ctx->refresh_lock);
	if (!ctx->is_inited) {
		pr_err("dpu is not initialized\n");
		up(&ctx->refresh_lock);
		return -EINVAL;
	}
	dpu->core->enhance_get(ctx, ENHANCE_CFG_ID_GAMMA, buf);
	up(&ctx->refresh_lock);

	return count;
}

static ssize_t gamma_write(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_set)
		return -EIO;

	/* I need to get my data in one piece */
	if (off != 0 || count != attr->size)
		return -EINVAL;

	down(&ctx->refresh_lock);
	dpu->core->enhance_set(ctx, ENHANCE_CFG_ID_GAMMA, buf);
	up(&ctx->refresh_lock);

	return count;
}
static BIN_ATTR_RW(gamma, 1536);

static ssize_t slp_lut_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;
	u32 data[256];
	int ret = 0;
	int i;

	if (!dpu->core->enhance_get)
		return -EIO;

	down(&ctx->refresh_lock);
	if (!ctx->is_inited) {
		pr_err("dpu is not initialized\n");
		up(&ctx->refresh_lock);
		return -EINVAL;
	}

	dpu->core->enhance_get(ctx, ENHANCE_CFG_ID_SLP_LUT, data);
	up(&ctx->refresh_lock);

	for (i = 0; i < 256; i++)
		ret += snprintf(buf + ret, PAGE_SIZE,
			"0x%x: 0x%x\n",
			i, data[i]);

	return ret;
}
static DEVICE_ATTR_RO(slp_lut);

static ssize_t slp_read(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_get)
		return -EIO;

	if (off >= attr->size)
		return 0;

	if (off + count > attr->size)
		count = attr->size - off;

	down(&ctx->refresh_lock);
	if (!ctx->is_inited) {
		pr_err("dpu is not initialized\n");
		up(&ctx->refresh_lock);
		return -EINVAL;
	}
	dpu->core->enhance_get(ctx, ENHANCE_CFG_ID_SLP, buf);
	up(&ctx->refresh_lock);

	return count;
}

static ssize_t slp_write(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_set)
		return -EIO;

	/* I need to get my data in one piece */
	if (off != 0 || count != attr->size)
		return -EINVAL;

	down(&ctx->refresh_lock);
	dpu->core->enhance_set(ctx, ENHANCE_CFG_ID_SLP, buf);
	up(&ctx->refresh_lock);

	return count;
}
static BIN_ATTR_RW(slp, 6);

static ssize_t cm_read(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_get)
		return -EIO;

	if (off >= attr->size)
		return 0;

	if (off + count > attr->size)
		count = attr->size - off;

	down(&ctx->refresh_lock);
	if (!ctx->is_inited) {
		pr_err("dpu is not initialized\n");
		up(&ctx->refresh_lock);
		return -EINVAL;
	}
	dpu->core->enhance_get(ctx, ENHANCE_CFG_ID_CM, buf);
	up(&ctx->refresh_lock);

	return count;
}

static ssize_t cm_write(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_set)
		return -EIO;

	/* I need to get my data in one piece */
	if (off != 0 || count != attr->size)
		return -EINVAL;

	down(&ctx->refresh_lock);
	dpu->core->enhance_set(ctx, ENHANCE_CFG_ID_CM, buf);
	up(&ctx->refresh_lock);

	return count;
}
static BIN_ATTR_RW(cm, 24);

static ssize_t epf_read(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_get)
		return -EIO;

	if (off >= attr->size)
		return 0;

	if (off + count > attr->size)
		count = attr->size - off;

	down(&ctx->refresh_lock);
	if (!ctx->is_inited) {
		pr_err("dpu is not initialized\n");
		up(&ctx->refresh_lock);
		return -EINVAL;
	}
	dpu->core->enhance_get(ctx, ENHANCE_CFG_ID_EPF, buf);
	up(&ctx->refresh_lock);

	return count;
}

static ssize_t epf_write(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_set)
		return -EIO;

	/* I need to get my data in one piece */
	if (off != 0 || count != attr->size)
		return -EINVAL;

	down(&ctx->refresh_lock);
	dpu->core->enhance_set(ctx, ENHANCE_CFG_ID_EPF, buf);
	up(&ctx->refresh_lock);

	return count;
}
static BIN_ATTR_RW(epf, 14);

static ssize_t sr_epf_write(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_set)
		return -EIO;

	/* I need to get my data in one piece */
	if (off != 0 || count != attr->size)
		return -EINVAL;

	down(&ctx->refresh_lock);
	dpu->core->enhance_set(ctx, ENHANCE_CFG_ID_SR_EPF, buf);
	up(&ctx->refresh_lock);

	return count;
}
static BIN_ATTR_WO(sr_epf, 14);

static ssize_t hsv_read(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_get)
		return -EIO;

	if (off >= attr->size)
		return 0;

	if (off + count > attr->size)
		count = attr->size - off;

	down(&ctx->refresh_lock);
	if (!ctx->is_inited) {
		pr_err("dpu is not initialized\n");
		up(&ctx->refresh_lock);
		return -EINVAL;
	}
	dpu->core->enhance_get(ctx, ENHANCE_CFG_ID_HSV, buf);
	up(&ctx->refresh_lock);

	return count;
}

static ssize_t hsv_write(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_set)
		return -EIO;

	/* I need to get my data in one piece */
	if (off != 0 || count != attr->size)
		return -EINVAL;

	down(&ctx->refresh_lock);
	dpu->core->enhance_set(ctx, ENHANCE_CFG_ID_HSV, buf);
	up(&ctx->refresh_lock);

	return count;
}
static BIN_ATTR_RW(hsv, 1440);


static ssize_t scl_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;
	uint32_t param[2] = {};

	if (!dpu->core->enhance_get)
		return -EIO;

	down(&ctx->refresh_lock);
	if (!ctx->is_inited) {
		pr_err("dpu is not initialized\n");
		up(&ctx->refresh_lock);
		return -EINVAL;
	}
	dpu->core->enhance_get(ctx, ENHANCE_CFG_ID_SCL, param);
	up(&ctx->refresh_lock);

	ret = snprintf(buf, PAGE_SIZE, "%d x %d\n", param[0], param[1]);

	return ret;
}

static ssize_t scl_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;
	uint32_t param[2] = {};

	if (!dpu->core->enhance_set)
		return -EIO;

	down(&ctx->refresh_lock);
	str_to_u32_array(buf, 10, param);
	dpu->core->enhance_set(ctx, ENHANCE_CFG_ID_SCL, param);
	up(&ctx->refresh_lock);

	return count;
}
static DEVICE_ATTR_RW(scl);

static ssize_t lut3d_read(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_get)
		return -EIO;

	if (off >= attr->size)
		return 0;

	if (off + count > attr->size)
		count = attr->size - off;

	down(&ctx->refresh_lock);
	if (!ctx->is_inited) {
		pr_err("dpu is not initialized\n");
		up(&ctx->refresh_lock);
		return -EINVAL;
	}
	dpu->core->enhance_get(ctx, ENHANCE_CFG_ID_LUT3D, buf);
	up(&ctx->refresh_lock);

	return count;
}

static ssize_t lut3d_write(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_set)
		return -EIO;

	/* I need to get my data in one piece */
	if (off != 0 || count != attr->size)
		return -EINVAL;

	down(&ctx->refresh_lock);
	dpu->core->enhance_set(ctx, ENHANCE_CFG_ID_LUT3D, buf);
	up(&ctx->refresh_lock);

	return count;
}
static BIN_ATTR_RW(lut3d, 2916);

static ssize_t enable_write(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_set)
		return -EIO;

	/* I need to get my data in one piece */
	if (off != 0 || count != attr->size)
		return -EINVAL;

	down(&ctx->refresh_lock);
	dpu->core->enhance_set(ctx, ENHANCE_CFG_ID_ENABLE, buf);
	up(&ctx->refresh_lock);

	return count;
}
static BIN_ATTR_WO(enable, 4);

static ssize_t disable_write(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;

	if (!dpu->core->enhance_set)
		return -EIO;

	/* I need to get my data in one piece */
	if (off != 0 || count != attr->size)
		return -EINVAL;

	down(&ctx->refresh_lock);
	dpu->core->enhance_set(ctx, ENHANCE_CFG_ID_DISABLE, buf);
	up(&ctx->refresh_lock);

	return count;
}
static BIN_ATTR_WO(disable, 4);

static ssize_t status_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct dpu_context *ctx = &dpu->ctx;
	uint32_t en = 0;
	int ret = 0;

	if (!dpu->core->enhance_get)
		return -EIO;

	down(&ctx->refresh_lock);
	if (!ctx->is_inited) {
		pr_err("dpu is not initialized\n");
		up(&ctx->refresh_lock);
		return -EINVAL;
	}
	dpu->core->enhance_get(ctx, ENHANCE_CFG_ID_ENABLE, &en);
	up(&ctx->refresh_lock);

	ret += snprintf(buf + ret, PAGE_SIZE, "0x%08x\n", en);
	ret += snprintf(buf + ret, PAGE_SIZE, "scl: %d\n", !!(en & BIT(0)));
	ret += snprintf(buf + ret, PAGE_SIZE, "epf: %d\n", !!(en & BIT(1)));
	ret += snprintf(buf + ret, PAGE_SIZE, "hsv: %d\n", !!(en & BIT(2)));
	ret += snprintf(buf + ret, PAGE_SIZE, "cm: %d\n", !!(en & BIT(3)));
	ret += snprintf(buf + ret, PAGE_SIZE, "slp: %d\n", !!(en & BIT(4)));
	ret += snprintf(buf + ret, PAGE_SIZE, "gamma: %d\n", !!(en & BIT(5)));

	return ret;
}
static DEVICE_ATTR_RO(status);

static struct attribute *pq_ascii_attrs[] = {
	&dev_attr_scl.attr,
	&dev_attr_status.attr,
	&dev_attr_slp_lut.attr,
	NULL,
};
static struct bin_attribute *pq_bin_attrs[] = {
	&bin_attr_ltm,
	&bin_attr_gamma,
	&bin_attr_slp,
	&bin_attr_cm,
	&bin_attr_hsv,
	&bin_attr_epf,
	&bin_attr_sr_epf,
	&bin_attr_lut3d,
	&bin_attr_enable,
	&bin_attr_disable,
	NULL,
};
static const struct attribute_group pq_group = {
	.name = "PQ",
	.attrs = pq_ascii_attrs,
	.bin_attrs = pq_bin_attrs,
};

int sprd_dpu_sysfs_init(struct device *dev)
{
	int rc;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);

	rc = sysfs_create_group(&(dev->kobj), &dpu_group);
	if (rc)
		pr_err("create dpu attr node failed, rc=%d\n", rc);

	if (!dpu->core->enhance_set)
		return rc;

	rc = sysfs_create_group(&(dev->kobj), &pq_group);
	if (rc)
		pr_err("create dpu PQ node failed, rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL(sprd_dpu_sysfs_init);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("Add dpu attribute nodes for userspace");
MODULE_LICENSE("GPL v2");
