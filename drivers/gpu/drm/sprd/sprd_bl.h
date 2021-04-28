/*
 *Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 */

#ifndef _SPRD_BL_H_
#define _SPRD_BL_H_

struct sprd_backlight {
	/* pwm backlight parameters */
	struct pwm_device *pwm;
	u32 max_level;
	u32 min_level;
	u32 dft_level;
	u32 scale;

	/* cabc backlight parameters */
	bool cabc_en;
	u32 cabc_level;
	u32 cabc_refer_level;
};

int sprd_cabc_backlight_update(struct backlight_device *bd);

#endif
