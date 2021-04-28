/*
 * sound/soc/sprd/sprd-asoc-debug.h
 *
 * SPRD ASoC Debug include -- SpreadTrum ASOC Debug.
 *
 * Copyright (C) 2015 SpreadTrum Ltd.
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
#ifndef __SPRD_ASOC_DEBUG_H
#define __SPRD_ASOC_DEBUG_H

#define pr_color_terminal 0
#if pr_color_terminal
#define pr_col_s "\e[35m"
#define pr_col_e "\e[0m"
#else
#define pr_col_s
#define pr_col_e
#endif

#define pr_id "ASoC:"
#define pr_s pr_col_s"["pr_id
#define pr_e "] "pr_col_e

#define pr_sprd_fmt(id) pr_s""id""pr_e

#endif /* __SPRD_ASOC_DEBUG_H */
