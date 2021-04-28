#ifndef _SPRD_GPU_DEVICE_H_
#define _SPRD_GPU_DEVICE_H_

int create_gpu_cooling_device(struct devfreq *gpudev, u64 *mask);

int destroy_gpu_cooling_device(void);

#endif
