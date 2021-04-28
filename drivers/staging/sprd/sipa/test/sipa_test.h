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


#ifndef _SIPA_TEST_H_
#define _SIPA_TEST_H_

int sipa_loop_test_start(void);
extern void ipa_test_init_callback(void);
extern void sipa_test_enable_periph_int_to_sw(void);

#endif /* _SIPA_TEST_H_ */
