/*
 * SPRD external modem control driver in AP side.
 *
 * Copyright (C) 2019 Spreadtrum Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 of
 * the License as published by the Free Software Foundation.
 *
 * This program is used to control external modem in AP side for
 * Spreadtrum SoCs.
 */

#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sipc.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/gpio/consumer.h>

#include "sprd_modem_loader.h"

/* modem io cmd */
#define MODEM_MAGIC 'M'

#define MODEM_READ_LOCK_CMD _IO(MODEM_MAGIC, 0x1)
#define MODEM_READ_UNLOCK_CMD _IO(MODEM_MAGIC, 0x2)

#define MODEM_WRITE_LOCK_CMD _IO(MODEM_MAGIC, 0x3)
#define MODEM_WRITE_UNLOCK_CMD _IO(MODEM_MAGIC, 0x4)

#define MODEM_GET_LOAD_INFO_CMD _IOR(MODEM_MAGIC, 0x5, struct modem_load_info)
#define MODEM_SET_LOAD_INFO_CMD _IOW(MODEM_MAGIC, 0x6, struct modem_load_info)

#define MODEM_SET_READ_REGION_CMD _IOR(MODEM_MAGIC, 0x7, int)
#define MODEM_SET_WRITE_GEGION_CMD _IOW(MODEM_MAGIC, 0x8, int)

#ifdef CONFIG_SPRD_EXT_MODEM
#define MODEM_GET_REMOTE_FLAG_CMD _IOR(MODEM_MAGIC, 0x9, int)
#define MODEM_SET_REMOTE_FLAG_CMD _IOW(MODEM_MAGIC, 0xa, int)
#define MODEM_CLR_REMOTE_FLAG_CMD _IOW(MODEM_MAGIC, 0xb, int)
#endif

#define MODEM_STOP_CMD _IO(MODEM_MAGIC, 0xc)
#define MODEM_START_CMD _IO(MODEM_MAGIC, 0xd)

#ifdef CONFIG_SPRD_EXT_MODEM_POWER_CTRL
#define MODEM_REBOOT_EXT_MODEM_CMD _IO(MODEM_MAGIC, 0xe)
#define MODEM_POWERON_EXT_MODEM_CMD _IO(MODEM_MAGIC, 0xf)
#define MODEM_POWEROFF_EXT_MODEM_CMD _IO(MODEM_MAGIC, 0x10)
#endif

#define	MODEM_READ_ALL_MEM 0xff
#define	MODEM_READ_MODEM_MEM 0xfe
#define RUN_STATE_INVALID 0xff

enum {
	SPRD_5G_MODEM_DP = 0,
	SPRD_5G_MODEM_PS,
	SPRD_5G_MODEM_NR_PHY,
	SPRD_5G_MODEM_V3_PHY,
	SPRD_5G_MODEM_CNT
};

enum {
	SPRD_4G_MODEM_PM = 0,
	SPRD_4G_MODEM_PUBCP,
	SPRD_4G_MODEM_CNT,
};

#ifdef CONFIG_SPRD_EXT_MODEM
const struct ext_modem_operations *ext_modem_ops;
#endif

const char *modem_ctrl_args[MODEM_CTRL_NR] = {
	"shutdown",
	"deepsleep",
	"corereset",
	"sysreset",
	"getstatus"
};

const char *modem_5g_name[SPRD_5G_MODEM_CNT] = {
	"dpsys",
	"modem",
	"nrphy",
	"v3phy"
};

const char *modem_4g_name[SPRD_4G_MODEM_CNT] = {
	"pmsys",
	"pubcp"
};

typedef int (*MODEM_PARSE_FUN)(struct modem_device *modem,
			       struct device_node *np);

static struct class *modem_class;

static int modem_open(struct inode *inode, struct file *filp)
{
	struct modem_device *modem;

	modem = container_of(inode->i_cdev, struct modem_device, cdev);
	filp->private_data = modem;

	return 0;
}

static int modem_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static void modem_get_base_range(struct modem_device *modem,
				 phys_addr_t *p_base,
				 size_t *p_size, int b_read_region)
{
	phys_addr_t base = 0;
	size_t size = 0;
	u8 index;
	struct modem_region_info *region;

	index = b_read_region ? modem->read_region : modem->write_region;
	if (b_read_region) {
		switch (modem->read_region) {
		case MODEM_READ_ALL_MEM:
			base = modem->all_base;
			size = modem->all_size;
			break;

		case  MODEM_READ_MODEM_MEM:
			base = modem->modem_base;
			size = modem->modem_size;
			break;

		default:
			if (index < modem->load->region_cnt) {
				region = &modem->load->regions[index];
				base = region->address;
				size = region->size;
			}
			break;
		}
	} else if (index < MAX_REGION_CNT) {
		region = &modem->load->regions[index];
		base = region->address;
		size = region->size;
	}

	dev_dbg(modem->p_dev, "get base 0x%llx, size = 0x%lx!\n", base, size);

	*p_base = base;
	*p_size = size;
}

static void *modem_map_memory(struct modem_device *modem, phys_addr_t start,
			      size_t size, size_t *map_size_ptr)
{
	size_t map_size = size;
	void *map;

	do {
		map_size = PAGE_ALIGN(map_size);
		map = modem_ram_vmap_nocache(modem->modem_type,
					     start, map_size);
		if (map) {
			if (map_size_ptr)
				*map_size_ptr = map_size;

			return map;
		}
		map_size /= 2;
	} while (map_size >= PAGE_SIZE);

	return NULL;
}

static ssize_t modem_read(struct file *filp,
			  char __user *buf, size_t count, loff_t *ppos)
{
	phys_addr_t base;
	size_t size, offset, copy_size, map_size, r;
	void *vmem;
	struct modem_device *modem = filp->private_data;
	phys_addr_t addr;

	dev_dbg(modem->p_dev, "read, %s!\n", modem->modem_name);

	/* only get read lock task can be read */
	if (strcmp(current->comm, modem->rd_lock_name) != 0) {
		dev_err(modem->p_dev,  "read, task %s need get rd lock!\n",
			current->comm);
		return -EACCES;
	}

	modem_get_base_range(modem, &base, &size, 1);
	offset = *ppos;
	dev_dbg(modem->p_dev, "read, offset = 0x%lx, count = 0x%lx!\n",
		offset, count);

	if (size <= offset)
		return -EINVAL;

	count = min_t(size_t, size - offset, count);
	r = count;
	do {
		addr = base + offset + (count - r);
		vmem = modem_map_memory(modem, addr, r, &map_size);
		if (!vmem) {
			dev_err(modem->p_dev,
				"read, Unable to map  base: 0x%llx\n", addr);
			return -ENOMEM;
		}

		copy_size = min_t(size_t, r, map_size);
		if (unalign_copy_to_user(buf, vmem, copy_size)) {
			dev_err(modem->p_dev,
				"read, copy data from user err!\n");
			modem_ram_unmap(modem->modem_type, vmem);
			return -EFAULT;
		}
		modem_ram_unmap(modem->modem_type, vmem);
		r -= copy_size;
		buf += copy_size;
	} while (r > 0);

	*ppos += (count - r);
	return count - r;
}

static ssize_t modem_write(struct file *filp,
			   const char __user *buf,
			   size_t count, loff_t *ppos)
{
	phys_addr_t base;
	size_t size, offset, copy_size, map_size, r;
	void *vmem;
	struct modem_device *modem = filp->private_data;
	phys_addr_t addr;

	dev_dbg(modem->p_dev, "write, %s!\n", modem->modem_name);

	/* only get write lock task can be write */
	if (strcmp(current->comm, modem->wt_lock_name) != 0) {
		dev_err(modem->p_dev, "write, task %s need get wt lock!\n",
			current->comm);
		return -EACCES;
	}

	modem_get_base_range(modem, &base, &size, 0);
	offset = *ppos;
	dev_dbg(modem->p_dev, "write, offset 0x%lx, count = 0x%lx!\n",
		offset, count);

	if (size <= offset)
		return -EINVAL;

	count = min_t(size_t, size - offset, count);
	r = count;
	do {
		addr = base + offset + (count - r);
		vmem = modem_map_memory(modem, addr, r, &map_size);
		if (!vmem) {
			dev_err(modem->p_dev,
				"write, Unable to map  base: 0x%llx\n",
				addr);
			return -ENOMEM;
		}
		copy_size = min_t(size_t, r, map_size);
		if (unalign_copy_from_user(vmem, buf, copy_size)) {
			dev_err(modem->p_dev,
				"write, copy data from user err!\n");
			modem_ram_unmap(modem->modem_type, vmem);
			return -EFAULT;
		}
		modem_ram_unmap(modem->modem_type, vmem);
		r -= copy_size;
		buf += copy_size;
	} while (r > 0);

	*ppos += (count - r);
	return count - r;
}

static loff_t modem_lseek(struct file *filp, loff_t off, int whence)
{
	switch (whence) {
	case SEEK_SET:
		filp->f_pos = off;
		break;

	default:
		return -EINVAL;
	}
	return off;
}

static int modem_cmd_lock(struct file *filp,
			  struct modem_device *modem, int b_rx)
{
	struct mutex *mut; /* mutex point to rd_mutex or wt_mutex*/
	struct wakeup_source *ws;
	char *name;

	mut = b_rx ? &modem->rd_mutex : &modem->wt_mutex;
	ws = b_rx ? &modem->rd_ws : &modem->wt_ws;
	name = b_rx ? modem->rd_lock_name : modem->wt_lock_name;

	if (filp->f_flags & O_NONBLOCK) {
		if (!mutex_trylock(mut)) {
			dev_err(modem->p_dev, "lock, %s get lock %d busy!\n",
				current->comm, b_rx);
			return -EBUSY;
		}
	} else {
		dev_dbg(modem->p_dev, "lock, %s has get lock %d !\n",
			current->comm, b_rx);
		mutex_lock(mut);
	}

	/* lock, get wake lock, cpy task to name */
	__pm_stay_awake(ws);
	strcpy(name, current->comm);
	return 0;
}

static int modem_cmd_unlock(struct modem_device *modem, int b_rx)
{
	struct mutex *mut; /* mutex point to rd_mutex or wt_mutex*/
	struct wakeup_source *ws;
	char *name;

	mut = b_rx ? &modem->rd_mutex : &modem->wt_mutex;
	ws = b_rx ? &modem->rd_ws : &modem->wt_ws;
	name = b_rx ? modem->rd_lock_name : modem->wt_lock_name;

	if (strlen(name) == 0)
		/* means no lock, so don't unlock */
		return 0;

	/* unlock, release wake lock, set name[0] to 0 */
	name[0] = 0;
	__pm_relax(ws);
	mutex_unlock(mut);

	dev_dbg(modem->p_dev,
		"unlock, %s has unlock %d!\n",
		current->comm, b_rx);

	return 0;
}

static int modem_get_something(struct modem_device *modem,
			       void *from,
			       unsigned int cmd,
			       unsigned long arg)
{
	if (strcmp(current->comm, modem->rd_lock_name) != 0) {
		dev_err(modem->p_dev, "get, task %s need get rd lock!\n",
			current->comm);
		return -EBUSY;
	}

	if (copy_to_user((void __user *)arg, from, _IOC_SIZE(cmd)))
		return -EFAULT;

	dev_dbg(modem->p_dev, "get, %s arg 0x%lx!\n", current->comm, arg);

	return 0;
}

static int modem_set_something(struct modem_device *modem,
			       void *to, unsigned int cmd, unsigned long arg)
{
	dev_dbg(modem->p_dev, "set, %s cmd 0x%x!\n", current->comm, cmd);

	if (strcmp(current->comm, modem->wt_lock_name) != 0) {
		dev_err(modem->p_dev, "set, task %s need get wt lock!\n",
		       current->comm);
		return -EBUSY;
	}

	if (copy_from_user(to, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	return 0;
}

static void modem_reg_ctrl(struct modem_device *modem, u32 index, int b_clear)
{
	struct regmap *map;
	u32 reg, mask, val;
	struct modem_ctrl *ctrl = modem->modem_ctrl;

	reg = ctrl->ctrl_reg[index];
	if (reg == MODEM_INVALID_REG)
		return;

	map = ctrl->ctrl_map[index];
	mask = ctrl->ctrl_mask[index];
	val = b_clear ? ~mask : mask;
	dev_dbg(modem->p_dev, "ctrl reg = 0x%x, mask =0x%x, val =0x%x\n",
		reg, mask, val);

	regmap_update_bits(map, reg, mask, val);
}

static void soc_modem_start(struct modem_device *modem)
{
	/* clear cp force shutdown */
	modem_reg_ctrl(modem, MODEM_CTRL_SHUT_DOWN, 1);

	/* clear cp force deep sleep */
	modem_reg_ctrl(modem, MODEM_CTRL_DEEP_SLEEP, 1);

	/* waiting for power on stably */
	msleep(50);

	/* clear sys reset */
	modem_reg_ctrl(modem, MODEM_CTRL_SYS_RESET, 1);

	/* clear core reset */
	modem_reg_ctrl(modem, MODEM_CTRL_CORE_RESET, 1);

	/* waiting for core reset release stably */
	msleep(50);

	dev_info(modem->p_dev, "start over\n");
}

static void soc_modem_stop(struct modem_device *modem)
{
	/* set core reset */
	modem_reg_ctrl(modem, MODEM_CTRL_CORE_RESET, 0);

	/* set sys reset */
	modem_reg_ctrl(modem, MODEM_CTRL_SYS_RESET, 0);

	/* waiting for core reset hold stably */
	msleep(50);

	/* set cp force deep sleep */
	modem_reg_ctrl(modem, MODEM_CTRL_DEEP_SLEEP, 0);

	/* set cp force shutdown */
	modem_reg_ctrl(modem, MODEM_CTRL_SHUT_DOWN, 0);

	/* waiting for power off stably */
	msleep(50);

	dev_info(modem->p_dev, "stop over\n");
}

static int modem_run(struct modem_device *modem, u8 b_run)
{
	dev_info(modem->p_dev, "%s modem run = %d!\n", current->comm, b_run);

	if (modem->run_state == b_run)
		return -EINVAL;

	modem->run_state = b_run;
	if (modem->modem_type == SOC_MODEM) {
		if (b_run)
			soc_modem_start(modem);
		else
			soc_modem_stop(modem);
	}

	return 0;
}

#ifdef CONFIG_SPRD_EXT_MODEM
static void modem_get_remote_flag(struct modem_device *modem)
{
	ext_modem_ops->get_remote_flag(modem);
	dev_info(modem->p_dev, "get remote flag = 0x%x!\n", modem->remote_flag);
}

static void modem_set_remote_flag(struct modem_device *modem, u8 b_clear)
{
	ext_modem_ops->set_remote_flag(modem, b_clear);
	dev_info(modem->p_dev, "set remote flag = 0x%x, b_clear = %d!\n",
		 modem->remote_flag, b_clear);
}
#endif

#ifdef CONFIG_SPRD_EXT_MODEM_POWER_CTRL
static int modem_reboot_ext_modem(struct modem_device *modem, u8 b_reset)
{
	return ext_modem_ops->reboot(modem, b_reset);
}

static int modem_poweroff_ext_modem(struct modem_device *modem)
{
	return ext_modem_ops->poweroff(modem);
}
#endif

static long modem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = -EINVAL;
	int access = 0;
	int param = 0;
	u8 b_clear;

	struct modem_device *modem = (struct modem_device *)filp->private_data;

	dev_dbg(modem->p_dev, "ioctl, cmd=0x%x (%c nr=%d len=%d dir=%d)\n", cmd,
		_IOC_TYPE(cmd), _IOC_NR(cmd), _IOC_SIZE(cmd), _IOC_DIR(cmd));

	if (_IOC_DIR(cmd) & _IOC_READ)
		access = !access_ok(VERIFY_WRITE,
				 (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		access = !access_ok(VERIFY_READ,
				 (void __user *)arg, _IOC_SIZE(cmd));

	if (access) {
		dev_err(modem->p_dev, "ioctl, access isn't ok! ret=%d\n", ret);
		return -EFAULT;
	}

	dev_dbg(modem->p_dev, "ioctl, arg = 0x%lx!", arg);

	switch (cmd) {
	case MODEM_READ_LOCK_CMD:
		ret = modem_cmd_lock(filp, modem, 1);
		break;

	case MODEM_READ_UNLOCK_CMD:
		ret = modem_cmd_unlock(modem, 1);
		break;

	case MODEM_WRITE_LOCK_CMD:
		ret = modem_cmd_lock(filp, modem, 0);
		break;

	case MODEM_WRITE_UNLOCK_CMD:
		ret = modem_cmd_unlock(modem, 0);
		break;

	case MODEM_GET_LOAD_INFO_CMD:
		ret = modem_get_something(modem, modem->load,
			      cmd, arg);
		break;

	case MODEM_SET_LOAD_INFO_CMD:
		ret = modem_set_something(modem, modem->load,
					  cmd, arg);
		if (!ret) {
			modem->all_base = (phys_addr_t)modem->load->all_base;
			modem->all_size = (size_t)modem->load->all_size;
			modem->modem_base = (phys_addr_t)
					    modem->load->modem_base;
			modem->modem_size = (size_t)modem->load->modem_size;
		}
		break;

	case MODEM_SET_READ_REGION_CMD:
		ret = modem_set_something(modem,
					  &param,
					  cmd, arg);
		modem->read_region = (u8)param;
		break;

	case MODEM_SET_WRITE_GEGION_CMD:
		ret = modem_set_something(modem,
					  &param,
					  cmd, arg);
		modem->write_region = (u8)param;
		break;

#ifdef CONFIG_SPRD_EXT_MODEM
	case MODEM_GET_REMOTE_FLAG_CMD:
		modem_get_remote_flag(modem);
		param = (int)modem->remote_flag;
		ret = modem_get_something(modem,
					  &param,
					  cmd, arg);
		break;

	case MODEM_SET_REMOTE_FLAG_CMD:
	case MODEM_CLR_REMOTE_FLAG_CMD:
		b_clear = cmd == MODEM_CLR_REMOTE_FLAG_CMD ? 1 : 0;
		ret = modem_set_something(modem,
					  &param,
					  cmd, arg);
		if (ret == 0) {
			modem->remote_flag = param;
			modem_set_remote_flag(modem, b_clear);
		}
		break;
#endif

	case MODEM_STOP_CMD:
		ret = modem_run(modem, 0);
		break;

	case MODEM_START_CMD:
		ret = modem_run(modem, 1);
		break;

#ifdef CONFIG_SPRD_EXT_MODEM_POWER_CTRL
	case MODEM_REBOOT_EXT_MODEM_CMD:
		ret = modem_reboot_ext_modem(modem, 1);
		break;

	case MODEM_POWERON_EXT_MODEM_CMD:
		ret = modem_reboot_ext_modem(modem, 0);
		break;

	case MODEM_POWEROFF_EXT_MODEM_CMD:
		ret = modem_poweroff_ext_modem(modem);
			break;
#endif

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int modem_get_device_name(struct modem_device *modem,
			       struct device_node *np)
{
	int modem_id;

	if (of_property_read_bool(np, "5g-modem-support")) {
		modem_id = of_alias_get_id(np, "nr-modem");
		if (modem_id == -ENODEV || modem_id >= SPRD_5G_MODEM_CNT) {
			dev_err(modem->p_dev, "fail to get id\n");
			return -ENODEV;
		}
		modem->modem_name = modem_5g_name[modem_id];
	} else {
		modem_id = of_alias_get_id(np, "lte-modem");
		if (modem_id == -ENODEV || modem_id >= SPRD_4G_MODEM_CNT) {
			dev_err(modem->p_dev, "fail to get id\n");
			return -ENODEV;
		}
		modem->modem_name = modem_4g_name[modem_id];
	}

	return 0;
}

static int pcie_modem_parse_dt(struct modem_device *modem,
			       struct device_node *np)
{
	int ret;

	modem->modem_type = PCIE_MODEM;

	ret = modem_get_device_name(modem, np);
	if (ret)
		return ret;

#ifdef CONFIG_SPRD_EXT_MODEM_POWER_CTRL
	modem->modem_power = devm_gpiod_get(modem->p_dev,
					    "poweron",
					    GPIOD_OUT_HIGH);
	if (IS_ERR(modem->modem_power)) {
		dev_err(modem->p_dev, "get poweron gpio failed!\n");
		return PTR_ERR(modem->modem_power);
	}


	modem->modem_reset = devm_gpiod_get(modem->p_dev,
					    "reset",
					    GPIOD_OUT_HIGH);
	if (IS_ERR(modem->modem_reset)) {
		dev_err(modem->p_dev, "get reset gpio failed!\n");
		return PTR_ERR(modem->modem_reset);
	}
#endif

	return 0;
}

static int soc_modem_parse_dt(struct modem_device *modem,
			      struct device_node *np)
{
	int ret, cr_num;
	struct modem_ctrl *modem_ctl;
	u32 syscon_args[2];

	modem->modem_type = SOC_MODEM;

	ret = modem_get_device_name(modem, np);
	if (ret)
		return ret;

	modem_ctl = devm_kzalloc(modem->p_dev,
				 sizeof(struct modem_ctrl),
				 GFP_KERNEL);
	if (!modem_ctl)
		return -ENOMEM;

	cr_num = 0;

	do {
		/* get apb & pmu reg handle */
		modem_ctl->ctrl_map[cr_num] =
			syscon_regmap_lookup_by_name(np,
						     modem_ctrl_args[cr_num]);
		if (IS_ERR(modem_ctl->ctrl_map[cr_num])) {
			dev_err(modem->p_dev, "failed to find %s\n",
				modem_ctrl_args[cr_num]);
			return -EINVAL;
		}

		/**
		 * 1.get ctrl_reg offset, the ctrl-reg variable number, so need
		 * to start reading from the largest until success.
		 * 2.get ctrl_mask
		 */
		ret = syscon_get_args_by_name(np,
					      modem_ctrl_args[cr_num],
					      2,
					      (u32 *)syscon_args);
		if (ret == 2) {
			modem_ctl->ctrl_reg[cr_num] = syscon_args[0];
			modem_ctl->ctrl_mask[cr_num] = syscon_args[1];
		} else {
			dev_err(modem->p_dev, "failed to map ctrl reg\n");
			return -EINVAL;
		}

		cr_num++;
	} while (modem_ctrl_args[cr_num] != NULL);

	modem->modem_ctrl = modem_ctl;
	return 0;
}

#ifdef CONFIG_DEBUG_FS
static const struct file_operations modem_debug_fops;
static struct dentry *modem_root;

static void modem_debug_putline(struct seq_file *m, char c, int n)
{
	char buf[300];
	int i, max, len;

	/* buf will end with '\n' and 0 */
	max = ARRAY_SIZE(buf) - 2;
	len = n > max ? max : n;

	for (i = 0; i < len; i++)
		buf[i] = c;

	buf[i] = '\n';
	buf[i + 1] = 0;

	seq_puts(m, buf);
}

static int modem_debug_show(struct seq_file *m, void *private)
{
	u32 region_cnt, i;
	struct modem_region_info *regions;
	struct modem_device *modem = (struct modem_device *)m->private;

	region_cnt = modem->load->region_cnt;
	regions = modem->load->regions;

	modem_debug_putline(m, '*', 100);

	seq_printf(m, "%s info:\n", modem->modem_name);
	modem_debug_putline(m, '-', 80);
	seq_printf(m, "read_region: %d, write_region: %d\n",
		   modem->read_region, modem->write_region);
	seq_printf(m, "run_state: %d, remote_flag: %d\n",
		   modem->run_state, modem->remote_flag);
	seq_printf(m, "modem_base: 0x%llx, size: 0x%lx\n",
		   modem->modem_base, modem->modem_size);
	seq_printf(m, "all_base: 0x%llx, size: 0x%lx\n",
		   modem->all_base, modem->all_size);

	modem_debug_putline(m, '-', 80);
	seq_puts(m, "region list:\n");

	for (i = 0; i < region_cnt; i++)
		seq_printf(m, "region[%2d]:address=0x%llx, size=0x%lx, name=%s\n",
			   i,
			   (phys_addr_t)regions[i].address,
			   (size_t)regions[i].size,
			   regions[i].name);

	if (modem->modem_ctrl) {
		struct modem_ctrl *ctrl = modem->modem_ctrl;

		modem_debug_putline(m, '-', 80);
		seq_puts(m, "modem ctl info:\n");

		for (i = 0; i < MODEM_CTRL_NR; i++)
			seq_printf(m, "region[%2d]:reg=0x%x, mask=0x%x\n",
				   i,
				   ctrl->ctrl_reg[i],
				   ctrl->ctrl_mask[i]);
	}
	modem_debug_putline(m, '*', 100);

	return 0;
}

static int modem_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, modem_debug_show, inode->i_private);
}

static const struct file_operations modem_debug_fops = {
	.open = modem_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void modem_init_debugfs(struct modem_device *modem)
{
	modem->debug_file = debugfs_create_file(modem->modem_name, 0444,
				modem_root,
				modem,
				&modem_debug_fops);
}

static void modem_remove_debugfs(struct modem_device *modem)
{
	debugfs_remove(modem->debug_file);
}
#endif

static const struct file_operations modem_fops = {
	.open = modem_open,
	.release = modem_release,
	.llseek = modem_lseek,
	.read = modem_read,
	.write = modem_write,
	.unlocked_ioctl = modem_ioctl,
	.owner = THIS_MODULE
};

static const struct of_device_id modem_match_table[] = {
	{.compatible = "sprd,modem", .data = soc_modem_parse_dt},
	{.compatible = "sprd,pcie-modem", .data = pcie_modem_parse_dt},
	{ },
};

static int modem_probe(struct platform_device *pdev)
{
	struct modem_device *modem;
	int ret = 0;
	MODEM_PARSE_FUN dt_parse;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev;

	modem = devm_kzalloc(&pdev->dev,
			     sizeof(struct modem_device),
			     GFP_KERNEL);
	if (!modem)
		return -ENOMEM;

	modem->p_dev = &pdev->dev;
	modem->run_state = RUN_STATE_INVALID;
	modem->load = devm_kzalloc(modem->p_dev,
				   sizeof(struct modem_load_info),
				   GFP_KERNEL);
	if (!modem->load)
		return -ENOMEM;

	dt_parse = of_device_get_match_data(modem->p_dev);
	if (!dt_parse) {
		dev_err(modem->p_dev, "cat't get parse fun!\n");
		return -EINVAL;
	}

	ret = dt_parse(modem, np);
	if (ret)
		return ret;

	ret = alloc_chrdev_region(&modem->devid, 0, 1, modem->modem_name);
	if (ret != 0) {
		dev_err(modem->p_dev, "get name fail, ret = %d!\n", ret);
		return ret;
	}

	cdev_init(&modem->cdev, &modem_fops);
	ret = cdev_add(&modem->cdev, modem->devid, 1);
	if (ret != 0) {
		unregister_chrdev_region(modem->devid, 1);
		dev_err(modem->p_dev, "add dev fail, ret = %d!\n", ret);
		return ret;
	}

	dev = device_create(modem_class, NULL,
		      modem->devid,
		      NULL, "%s", modem->modem_name);
	if (IS_ERR(dev))
		dev_err(modem->p_dev, "device_create fail,ERRNO = %ld!\n",
			PTR_ERR(dev));

	mutex_init(&modem->rd_mutex);
	mutex_init(&modem->wt_mutex);
	wakeup_source_init(&modem->rd_ws, "modem_read");
	wakeup_source_init(&modem->wt_ws, "modem_write");

	platform_set_drvdata(pdev, modem);

#if defined(CONFIG_DEBUG_FS)
	modem_init_debugfs(modem);
#endif

	return 0;
}

static int  modem_remove(struct platform_device *pdev)
{
	struct modem_device *modem = platform_get_drvdata(pdev);

	if (modem) {
		wakeup_source_trash(&modem->rd_ws);
		wakeup_source_trash(&modem->wt_ws);
		mutex_destroy(&modem->rd_mutex);
		mutex_destroy(&modem->wt_mutex);
		device_destroy(modem_class, modem->devid);
		cdev_del(&modem->cdev);
		unregister_chrdev_region(modem->devid, 1);
#if defined(CONFIG_DEBUG_FS)
		modem_remove_debugfs(modem);
#endif
		platform_set_drvdata(pdev, NULL);
	}

	return 0;
}

static struct platform_driver modem_driver = {
	.driver = {
		.name = "modem",
		.of_match_table = modem_match_table,
	},
	.probe = modem_probe,
	.remove = modem_remove,
};

static int __init modem_init(void)
{
	modem_class = class_create(THIS_MODULE, "ext_modem");
	if (IS_ERR(modem_class))
		return PTR_ERR(modem_class);

#if defined(CONFIG_DEBUG_FS)
	modem_root = debugfs_create_dir("modem", NULL);
	if (IS_ERR(modem_root))
		return PTR_ERR(modem_root);
#endif

#ifdef CONFIG_SPRD_EXT_MODEM
	 modem_get_ext_modem_ops(&ext_modem_ops);
#endif

	return platform_driver_register(&modem_driver);
}

static void __exit modem_exit(void)
{
	class_destroy(modem_class);

#if defined(CONFIG_DEBUG_FS)
	debugfs_remove_recursive(modem_root);
#endif

	platform_driver_unregister(&modem_driver);
}

module_init(modem_init);
module_exit(modem_exit);

MODULE_AUTHOR("Wenping zhou");
MODULE_DESCRIPTION("External modem driver");
MODULE_LICENSE("GPL v2");
