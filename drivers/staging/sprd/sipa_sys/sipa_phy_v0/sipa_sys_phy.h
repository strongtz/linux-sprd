/*
 * Copyright (C) 2018-2019 Unisoc Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef _IPA_SYS_PHY_H_
#define _IPA_SYS_PHY_H_
#include "../sipa_sys.h"

int sipa_sys_force_wakeup(struct sipa_sys_cfg_tag *cfg);
int sipa_sys_set_sipa_enable(struct sipa_sys_cfg_tag *cfg);
int sipa_sys_clear_force_shutdown(struct sipa_sys_cfg_tag *cfg);
int sipa_sys_set_auto_shutdown(struct sipa_sys_cfg_tag *cfg);
int sipa_sys_clear_force_deepsleep(struct sipa_sys_cfg_tag *cfg);
int sipa_sys_set_deepsleep(struct sipa_sys_cfg_tag *cfg);
int sipa_sys_clear_force_lightsleep(struct sipa_sys_cfg_tag *cfg);
int sipa_sys_set_lightsleep(struct sipa_sys_cfg_tag *cfg);
int sipa_sys_set_smart_lightsleep(struct sipa_sys_cfg_tag *cfg);
int sipa_sys_disable_ipacm4(struct sipa_sys_cfg_tag *cfg);
int sipa_sys_auto_gate_enable(struct sipa_sys_cfg_tag *cfg);

void sipa_sys_proc_init(struct sipa_sys_cfg_tag *cfg);

#endif
