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
#define pr_fmt(__fmt) "[drm][%20s] "__fmt, __func__

#include <linux/device.h>
#include <linux/libfdt.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "disp_lib.h"

struct bmp_header {
	u16 magic;
	u32 size;
	u32 unused;
	u32 start;
} __attribute__((__packed__));

struct dib_header {
	u32 size;
	u32 width;
	u32 height;
	u16 planes;
	u16 bpp;
	u32 compression;
	u32 data_size;
	u32 h_res;
	u32 v_res;
	u32 colours;
	u32 important_colours;
	u32 red_mask;
	u32 green_mask;
	u32 blue_mask;
	u32 alpha_mask;
	u32 colour_space;
	u32 unused[12];
} __attribute__((__packed__));

int str_to_u32_array(const char *p, u32 base, u32 array[])
{
	const char *start = p;
	char str[12];
	int length = 0;
	int i, ret;

	pr_info("input: %s", p);

	for (i = 0 ; i < 255; i++) {
		while (*p == ' ')
			p++;
		if (*p == '\0')
			break;
		start = p;

		while ((*p != ' ') && (*p != '\0'))
			p++;

		if ((p - start) >= sizeof(str))
			break;

		memset(str, 0, sizeof(str));
		memcpy(str, start, p - start);

		ret = kstrtou32(str, base, &array[i]);
		if (ret) {
			DRM_ERROR("input format error\n");
			break;
		}

		length++;
	}

	return length;
}
EXPORT_SYMBOL_GPL(str_to_u32_array);

int load_dtb_to_mem(const char *name, void **blob)
{
	ssize_t ret;
	u32 count;
	struct fdt_header dtbhead;
	loff_t pos = 0;
	struct file *fdtb;


	fdtb = filp_open(name, O_RDONLY, 0644);
	if (IS_ERR(fdtb)) {
		DRM_ERROR("%s open file error\n", __func__);
		return PTR_ERR(fdtb);
	}

	ret = kernel_read(fdtb, &dtbhead, sizeof(dtbhead), &pos);
	pos = 0;
	count = ntohl(dtbhead.totalsize);
	*blob = kzalloc(count, GFP_KERNEL);
	if (*blob == NULL) {
		filp_close(fdtb, NULL);
		return -ENOMEM;
	}
	ret = kernel_read(fdtb, *blob, count, &pos);

	if (ret != count) {
		DRM_ERROR("Read to mem fail: ret %zd size%x\n", ret, count);
		kfree(*blob);
		*blob = NULL;
		filp_close(fdtb, NULL);
		return ret < 0 ? ret : -ENODEV;
	}

	filp_close(fdtb, NULL);

	return 0;
}
EXPORT_SYMBOL_GPL(load_dtb_to_mem);

int str_to_u8_array(const char *p, u32 base, u8 array[])
{
	const char *start = p;
	char str[12];
	int length = 0;
	int i, ret;

	pr_info("input: %s", p);

	for (i = 0 ; i < 255; i++) {
		while (*p == ' ')
			p++;
		if (*p == '\0')
			break;
		start = p;

		while ((*p != ' ') && (*p != '\0'))
			p++;

		if ((p - start) >= sizeof(str))
			break;

		memset(str, 0, sizeof(str));
		memcpy(str, start, p - start);

		ret = kstrtou8(str, base, &array[i]);
		if (ret) {
			DRM_ERROR("input format error\n");
			break;
		}

		length++;
	}

	return length;
}
EXPORT_SYMBOL_GPL(str_to_u8_array);

int dump_bmp32(const char *p, u32 width, u32 height,
		bool noflip, const char *filename)
{
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	struct dib_header dib_header = {
		.size = sizeof(dib_header),
		.width = width,
		.height = noflip ? -height : height,
		.planes = 1,
		.bpp = 32,
		.compression = 3,
		.data_size = 4 * width * height,
		.h_res = 0xB13,
		.v_res = 0xB13,
		.colours = 0,
		.important_colours = 0,
		.red_mask = 0x000000FF,
		.green_mask = 0x0000FF00,
		.blue_mask = 0x00FF0000,
		.alpha_mask = 0xFF000000,
		.colour_space = 0x57696E20,
	};
	struct bmp_header bmp_header = {
		.magic = 0x4d42,
		.size = (width * height * 4) +
		sizeof(bmp_header) + sizeof(dib_header),
		.start = sizeof(bmp_header) + sizeof(dib_header),
	};

	fp = filp_open(filename, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		DRM_ERROR("failed to open %s: %ld\n", filename, PTR_ERR(fp));
		return PTR_ERR(fp);
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	vfs_write(fp, (const char *)&bmp_header, sizeof(bmp_header), &pos);
	vfs_write(fp, (const char *)&dib_header, sizeof(dib_header), &pos);
	vfs_write(fp, p, width * height * 4, &pos);

	filp_close(fp, NULL);
	set_fs(fs);

	return 0;
}
EXPORT_SYMBOL_GPL(dump_bmp32);

void *disp_ops_attach(const char *str, struct list_head *head)
{
	struct ops_list *list;
	const char *ver;

	list_for_each_entry(list, head, head) {
		ver = list->entry->ver;
		if (!strcmp(str, ver))
			return list->entry->ops;
	}

	DRM_ERROR("attach disp ops %s failed\n", str);
	return NULL;
}
EXPORT_SYMBOL_GPL(disp_ops_attach);

int disp_ops_register(struct ops_entry *entry, struct list_head *head)
{
	struct ops_list *list;

	list = kzalloc(sizeof(struct ops_list), GFP_KERNEL);
	if (!list)
		return -ENOMEM;

	list->entry = entry;
	list_add(&list->head, head);

	return 0;
}
EXPORT_SYMBOL_GPL(disp_ops_register);

struct device *sprd_disp_pipe_get_by_port(struct device *dev, int port)
{
	struct device_node *np = dev->of_node;
	struct device_node *endpoint;
	struct device_node *remote_node;
	struct platform_device *remote_pdev;

	endpoint = of_graph_get_endpoint_by_regs(np, port, 0);
	if (!endpoint) {
		DRM_ERROR("%s/port%d/endpoint0 was not found\n",
			  np->full_name, port);
		return NULL;
	}

	remote_node = of_graph_get_remote_port_parent(endpoint);
	if (!remote_node) {
		DRM_ERROR("device node was not found by endpoint0\n");
		return NULL;
	}

	remote_pdev = of_find_device_by_node(remote_node);
	if (remote_pdev == NULL) {
		DRM_ERROR("find %s platform device failed\n",
			  remote_node->full_name);
		return NULL;
	}

	return &remote_pdev->dev;
}
EXPORT_SYMBOL_GPL(sprd_disp_pipe_get_by_port);

struct device *sprd_disp_pipe_get_input(struct device *dev)
{
	return sprd_disp_pipe_get_by_port(dev, 1);
}
EXPORT_SYMBOL_GPL(sprd_disp_pipe_get_input);

struct device *sprd_disp_pipe_get_output(struct device *dev)
{
	return sprd_disp_pipe_get_by_port(dev, 0);
}
EXPORT_SYMBOL_GPL(sprd_disp_pipe_get_output);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("leon.he@unisoc.com");
MODULE_DESCRIPTION("display common API library");
