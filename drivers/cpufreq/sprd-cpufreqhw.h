#ifndef SPRD_CPUFREQHW_H
#define SPRD_CPUFREQHW_H

#include "sprd-cpufreq-common.h"

int sprd_hardware_dvfs_device_register(struct platform_device *pdev);
int sprd_hardware_dvfs_device_unregister(struct platform_device *pdev);
struct sprd_cpudvfs_device *sprd_hardware_dvfs_device_get(void);

#endif
