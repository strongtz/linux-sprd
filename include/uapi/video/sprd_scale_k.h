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
#ifndef _SPRD_SCALE_H_
#define _SPRD_SCALE_H_

enum scale_fmt_e {
	SCALE_YUV422 = 0,
	SCALE_YUV420,
	SCALE_YUV400,
	SCALE_YUV420_3FRAME,
	SCALE_RGB565,
	SCALE_RGB888,
	SCALE_FTM_MAX
};

enum scale_data_endian_e {
	SCALE_ENDIAN_BIG = 0,
	SCALE_ENDIAN_LITTLE,
	SCALE_ENDIAN_HALFBIG,
	SCALE_ENDIAN_HALFLITTLE,
	SCALE_ENDIAN_MAX
};

enum scale_mode_e {
	SCALE_MODE_NORMAL = 0,
	SCALE_MODE_SLICE,
	SCALE_MODE_SLICE_READDR,
	SCALE_MODE_MAX
};

enum scale_process_e {
	SCALE_PROCESS_SUCCESS = 0,
	SCALE_PROCESS_EXIT = -1,
	SCALE_PROCESS_SYS_BUSY = -2,
	SCALE_PROCESS_MAX = 0xFF
};

struct scale_size_t {
	uint32_t w;
	uint32_t h;
};

struct scale_rect_t {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

struct scale_addr_t {
	uint32_t yaddr;
	uint32_t uaddr;
	uint32_t vaddr;
	unsigned int mfd[3];
};

struct scale_endian_sel_t {
	uint8_t y_endian;
	uint8_t uv_endian;
	uint8_t reserved0;
	uint8_t reserved1;
};

struct scale_slice_param_t {
	uint32_t slice_height;
	struct scale_rect_t input_rect;
	struct scale_addr_t input_addr;
	struct scale_addr_t output_addr;
};

struct scale_frame_param_t {
	struct scale_size_t input_size;
	struct scale_rect_t input_rect;
	enum scale_fmt_e input_format;
	struct scale_addr_t input_addr;
	struct scale_endian_sel_t input_endian;
	struct scale_size_t output_size;
	enum scale_fmt_e output_format;
	struct scale_addr_t output_addr;
	struct scale_endian_sel_t output_endian;
	enum scale_mode_e scale_mode;
	uint32_t slice_height;
	void  *coeff_addr;
};

struct scale_frame_info_t {
	uint32_t type;
	uint32_t lock;
	uint32_t flags;
	uint32_t fid;
	uint32_t width;
	uint32_t height;
	uint32_t height_uv;
	uint32_t yaddr;
	uint32_t uaddr;
	uint32_t vaddr;
	struct scale_endian_sel_t endian;
	enum scale_process_e scale_result;
};


#define SCALE_IO_MAGIC      'S'
#define SCALE_IO_START     _IOW(SCALE_IO_MAGIC, 0, struct scale_frame_param_t)
#define SCALE_IO_CONTINUE  _IOW(SCALE_IO_MAGIC, 1, struct scale_slice_param_t)
#define SCALE_IO_DONE      _IOW(SCALE_IO_MAGIC, 2, struct scale_slice_param_t)
#if 0
#define SPRD_CPP_IO_OPEN_SCALE \
	_IOW(SCALE_IO_MAGIC, 0, unsigned int)
#define SPRD_CPP_IO_CLOSE_SCALE \
	_IOW(SCALE_IO_MAGIC, 1, unsigned int)
#define SPRD_CPP_IO_START_SCALE \
	_IOW(SCALE_IO_MAGIC, 2, struct scale_frame_param_t)
#define SPRD_CPP_IO_CONTINUE_SCALE \
	_IOW(SCALE_IO_MAGIC, 3, struct scale_slice_param_t)
#define SPRD_CPP_IO_STOP_SCALE \
	_IOW(SCALE_IO_MAGIC, 4, unsigned int)
#endif
#endif
