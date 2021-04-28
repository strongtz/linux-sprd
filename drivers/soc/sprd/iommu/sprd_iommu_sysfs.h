#ifndef __SPRD_IOMMU_SYSFS_H__
#define __SPRD_IOMMU_SYSFS_H__

#include <linux/device.h>

int sprd_iommu_sysfs_create(struct sprd_iommu_dev *device,
							const char *dev_name);

int sprd_iommu_sysfs_destroy(struct sprd_iommu_dev *device,
							const char *dev_name);

#endif
