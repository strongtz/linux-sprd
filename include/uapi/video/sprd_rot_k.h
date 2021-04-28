/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
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
#ifndef _SPRD_ROT_K_H_
#define _SPRD_ROT_K_H_

enum {
	ROT_YUV422 = 0,
	ROT_YUV420,
	ROT_YUV400,
	ROT_RGB888,
	ROT_RGB666,
	ROT_RGB565,
	ROT_RGB555,
	ROT_FMT_MAX
};

enum {
	ROT_90 = 0,
	ROT_270,
	ROT_180,
	ROT_MIRROR,
	ROT_ANGLE_MAX
};

enum {
	ROT_ENDIAN_BIG = 0,
	ROT_ENDIAN_LITTLE,
	ROT_ENDIAN_HALFBIG,
	ROT_ENDIAN_HALFLITTLE,
	ROT_ENDIAN_MAX
};

struct rot_size_tag {
	uint16_t w;
	uint16_t h;
};

struct rot_addr_tag {
	uint32_t y_addr;
	uint32_t u_addr;
	uint32_t v_addr;
	unsigned int mfd[3];
};

struct rot_cfg_tag {
	struct rot_size_tag img_size;
	uint32_t format;
	uint32_t angle;
	struct rot_addr_tag src_addr;
	struct rot_addr_tag dst_addr;
	uint32_t src_endian;
	uint32_t dst_endian;
};


#define SPRD_ROT_IOCTL_MAGIC 'm'
#define ROT_IO_START		\
		_IOW(SPRD_ROT_IOCTL_MAGIC, 1, struct rot_cfg_tag)
#define ROT_IO_DATA_COPY	\
		_IOW(SPRD_ROT_IOCTL_MAGIC, 2, struct rot_cfg_tag)
#define ROT_IO_DATA_COPY_TO_VIRTUAL	\
		_IOW(SPRD_ROT_IOCTL_MAGIC, 3, struct rot_cfg_tag)
#define ROT_IO_DATA_COPY_FROM_VIRTUAL	\
		_IOW(SPRD_ROT_IOCTL_MAGIC, 4, struct rot_cfg_tag)
#endif
