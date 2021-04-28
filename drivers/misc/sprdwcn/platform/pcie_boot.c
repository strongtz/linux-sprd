/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <misc/marlin_platform.h>
#include <linux/delay.h>

#include "pcie.h"
#include "pcie_boot.h"
#include "pcie_dbg.h"
#include "wcn_boot.h"
#include "wcn_log.h"
#include "wcn_gnss.h"

#define BTWF_FIRMWARE_SIZE_MAX	0xf0c00
#define GNSS_FIRMWARE_SIZE_MAX	0x58000
#define GNSS_BASE_ADDR			0x40800000
#define GNSS_CPSTART_OFFSET	0x220000
#define GNSS_CPRESET_OFFSET	0x3c8280

extern struct sprdwcn_gnss_ops *gnss_ops;

static char *load_firmware_data(const char *path, int size)
{
	int read_len;
	char *buffer = NULL, *data = NULL;
	struct file *file;
	loff_t offset = 0;

	WCN_INFO("%s enter,size=0X%x\n", __func__, size);
	file = filp_open(path, O_RDONLY, 0);

	if (IS_ERR(file)) {
		WCN_ERR("%s open fail errno=%ld\n", path, PTR_ERR(file));
		if (PTR_ERR(file) == -ENOENT)
			WCN_ERR("No such file or directory\n");
		if (PTR_ERR(file) == -EACCES)
			WCN_ERR("Permission denied\n");
		return NULL;
	}

	buffer = vmalloc(size);
	if (!buffer) {
		fput(file);
		WCN_INFO("no memory for image\n");
		return NULL;
	}

	data = buffer;
	do {
		read_len = kernel_read(file, buffer, size, &offset);
		if (read_len > 0) {
			size -= read_len;
			buffer += read_len;
		}

		WCN_INFO("size=0X%x, read_len=0X%x\n", size, read_len);
	} while ((read_len > 0) && (size > 0));

	fput(file);
	WCN_INFO("%s finish\n", __func__);

	return data;

}

int gnss_boot_up(struct wcn_pcie_info *pcie_info, const char *path,
		 unsigned int size)
{
	unsigned int reg_val = 0;
	char *buffer = NULL;
	char a[10];
	int i;
	static int dbg_cnt;
	static int cali_flag;
	int ret;
	char gnss_path[255];
	int path_len = 0;

	WCN_INFO("%s enter\n", __func__);
	if (gnss_ops && gnss_ops->write_data) {
		if (gnss_ops->write_data() != 0)
			WCN_ERR("%s gnss_ops write_data error\n", __func__);
	}
retry:
	buffer = load_firmware_data(path, size);
	if (!buffer && ((dbg_cnt++) < 1)) {
		WCN_INFO("%s: can't download firmware, retry %d\n",
			 __func__, dbg_cnt);
		msleep(200);
		goto retry;
	}
	if (!buffer) {
		WCN_ERR("%s: can't open gnss firmware path\n", __func__);
		return -1;
	}

	for (i = 0; i < 10; i++)
		WCN_INFO("buffer[%d]= 0x%x\n", i, buffer[i]);

	/* download firmware */
	sprd_pcie_bar_map(pcie_info, 2, GNSS_BASE_ADDR, 1);
	pcie_bar_write(pcie_info, 2, GNSS_CPSTART_OFFSET,
			buffer, GNSS_FIRMWARE_SIZE_MAX);
	pcie_bar_read(pcie_info, 2, GNSS_CPSTART_OFFSET, a, 10);
	for (i = 0; i < 10; i++)
		WCN_INFO("a[%d]= 0x%x\n", i, a[i]);

	if ((a[0] != buffer[0]) || (a[1] != buffer[1]) || (a[2] != buffer[2])) {
		WCN_ERR("%s: firmware's code transfer error\n", __func__);
		return -1;
	}

	if ((a[0] == 0) && (a[1] == 0) && (a[2] == 0)) {
		WCN_ERR("%s: mmcblk's data is dirty\n", __func__);
		return -1;
	}

	sprd_pcie_bar_map(pcie_info, 2, GNSS_BASE_ADDR, 1);
	/* release cpu */
	pcie_bar_read(pcie_info, 2, GNSS_CPRESET_OFFSET,
			(char *)&reg_val, 0x4);
	WCN_INFO("-->reset reg is %d\n", reg_val);
	reg_val = 0;
	pcie_bar_write(pcie_info, 2, GNSS_CPRESET_OFFSET,
			(char *)&reg_val, 0x4);
	WCN_INFO("<--reset reg is %d\n", reg_val);
	vfree(buffer);

	if (cali_flag == 0) {
		WCN_INFO("gnss start to backup calidata\n");
		if (gnss_ops && gnss_ops->backup_data) {
			ret = gnss_ops->backup_data();
			if (ret == 0)
				cali_flag = 1;
		} else {
			WCN_ERR("%s gnss_ops backup_data error\n", __func__);
		}
	} else {
		WCN_INFO("gnss wait boot finish\n");
		if (gnss_ops && gnss_ops->wait_gnss_boot)
			gnss_ops->wait_gnss_boot();
		else
			WCN_ERR("%s gnss_ops wait boot error\n", __func__);
	}

	if (gnss_ops && gnss_ops->set_file_path) {
		path_len = strlen(path);
		if (path_len > 255)
			path_len = 255;
		memcpy(gnss_path, path, path_len);
		gnss_ops->set_file_path(gnss_path);
	} else {
		WCN_ERR("%s gnss_path error\n", __func__);
	}

	return 0;
}

int btwf_boot_up(struct wcn_pcie_info *pcie_info, const char *path,
		 unsigned int size)
{
	unsigned int reg_val = 0;
	char *buffer = NULL;
	char a[10];
	int i;

	WCN_INFO("%s enter\n", __func__);
	buffer = load_firmware_data(path, size);
	if (!buffer) {
		WCN_ERR("can not open BTWF firmware =%s\n", path);
		return -1;
	}
	/* download firmware */
	sprd_pcie_bar_map(pcie_info, 0, 0x40400000, 0);
	pcie_bar_write(pcie_info, 0, 0x100000, buffer, BTWF_FIRMWARE_SIZE_MAX);
	pcie_bar_read(pcie_info, 0, 0x100000, a, 10);
	for (i = 0; i < 10; i++)
		WCN_INFO("a[%d]= 0x%x\n", i, a[i]);
	if ((a[0] == 0) && (a[1] == 0) && (a[2] == 0)) {
		WCN_ERR("%s: mmcblk's data is dirty\n", __func__);
		return -1;
	}
	sprd_pcie_bar_map(pcie_info, 0, 0x40000000, 0);
	/* release cpu */
	pcie_bar_read(pcie_info, 0, 0x88288, (char *)&reg_val, 0x4);
	WCN_INFO("-->reset reg is %d\n", reg_val);
	reg_val = 0;
	pcie_bar_write(pcie_info, 0, 0x88288, (char *)&reg_val, 0x4);
	WCN_INFO("<--reset reg is %d\n", reg_val);
	vfree(buffer);
	WCN_INFO("%s finished\n", __func__);
	return 0;
}

int handle_gnss_boot(struct wcn_pcie_info *pcie_info,
		     struct marlin_device *marlin_dev,
		     enum marlin_sub_sys subsys)
{
	int temp;

	WCN_INFO("%s enter\n", __func__);

	temp = gnss_boot_up(pcie_info, (const char *)marlin_dev->gnss_path,
			    GNSS_FIRMWARE_SIZE_MAX);
	if (temp < 0) {
		WCN_ERR("GNSS boot up fail\n");
		marlin_dev->gnss_dl_finish_flag = 0;
	} else
		marlin_dev->gnss_dl_finish_flag = 1;

	return 0;
}

int handle_btwf_boot(struct wcn_pcie_info *pcie_info,
		     struct marlin_device *marlin_dev,
		     enum marlin_sub_sys subsys)
{
	int temp;

	WCN_INFO("%s enter\n", __func__);
	temp = btwf_boot_up(pcie_info, (const char *)marlin_dev->btwf_path,
			    BTWF_FIRMWARE_SIZE_MAX);
	if (temp < 0) {
		WCN_ERR("BTWF boot up fail\n");
		atomic_set(&marlin_dev->download_finish_flag, 0);
		return -1;
	}

	atomic_set(&marlin_dev->download_finish_flag, 1);

	return 0;
}

int wcn_boot_init(struct wcn_pcie_info *pcie_info,
		  struct marlin_device *marlin_dev, enum marlin_sub_sys subsys)
{
	int temp = 0;

	WCN_INFO("%s enter\n", __func__);
	if (!atomic_read(&marlin_dev->download_finish_flag)) {
		handle_gnss_boot(pcie_info, marlin_dev, subsys);
		temp = handle_btwf_boot(pcie_info, marlin_dev, subsys);
		return temp;
	}

	if ((subsys == MARLIN_GNSS) && (!marlin_dev->gnss_dl_finish_flag))
		temp = handle_gnss_boot(pcie_info, marlin_dev, subsys);

	return temp;
}
EXPORT_SYMBOL(wcn_boot_init);

int pcie_boot(enum marlin_sub_sys subsys, struct marlin_device *marlin_dev)
{
	struct wcn_pcie_info *pdev;

	pdev = get_wcn_device_info();
	if (!pdev) {
		WCN_ERR("%s:maybe PCIE device link error\n", __func__);
		return -1;
	}
	if (wcn_boot_init(pdev, marlin_dev, subsys) < 0) {
		WCN_ERR("%s: call wcn_boot_init fail\n", __func__);
		return -1;
	}

	return 0;
}

