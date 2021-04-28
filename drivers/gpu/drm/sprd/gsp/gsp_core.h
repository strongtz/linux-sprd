/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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
#ifndef _GSP_CORE_H
#define _GSP_CORE_H

#include <linux/clk.h>
/* #include <linux/clk-private.h> */
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <drm/gsp_cfg.h>

#define GSP_CORE_SUSPEND_WAIT 3000 /* 3000 ms */
#define GSP_CORE_RESUME_WAIT 3000 /* 3000 ms */
#define GSP_CORE_RELEASE_WAIT 3000 /* 3000 ms */
#define PM_RUNTIME_DELAY_MS 1000
#define GSP_CORE_TIMER_OUT 2800 /* 2800 ms */


struct gsp_core_ops;
struct gsp_dev;
struct gsp_kcfg;
struct gsp_workqueue;

enum gsp_err_code {
	GSP_NO_ERR = 0,

	GSP_K_CTL_CODE_ERR = 0x100,
	GSP_K_CLK_CHK_ERR,

	GSP_K_CREATE_FENCE_ERR,
	GSP_K_PUT_FENCE_TO_USER_ERR,
	GSP_K_GET_FENCE_BY_FD_ERR,
	GSP_K_GET_DMABUF_BY_FD_ERR,
	GSP_K_COPY_FROM_USER_ERR,
	GSP_K_COPY_TO_USER_ERR,
	GSP_K_NOT_ENOUGH_EMPTY_KCMD_ERR,
	GSP_K_CREATE_THREAD_ERR,
	GSP_K_IOMMU_MAP_ERR,
	GSP_K_PARAM_CHK_ERR,

	GSP_K_HW_BUSY_ERR = 0x200,
	GSP_K_HW_HANG_ERR,

	GSP_ERR_MAX,
};

enum gsp_core_state {
	CORE_STATE_ENABLE_ERR = -1,
	CORE_STATE_MAP_ERR,
	CORE_STATE_WAIT_ERR,
	CORE_STATE_TRIGGER_ERR,
	CORE_STATE_HW_HANG_ERR,
	CORE_STATE_IRQ_ERR,
	CORE_STATE_UNMAP_ERR,
	CORE_STATE_RELEASE_ERR,
	CORE_STATE_ERR,
	CORE_STATE_IDLE,
	CORE_STATE_TRIGGER,
	CORE_STATE_BUSY,
	CORE_STATE_IRQ_HANDLED,
	CORE_STATE_SUSPEND
};

enum gsp_core_suspend_state {
	CORE_STATE_SUSPEND_EXIT = 0,
	CORE_STATE_SUSPEND_BEGIN,
	CORE_STATE_SUSPEND_WAIT,
	CORE_STATE_SUSPEND_DONE,
};


/**
 * struct gsp_core - gsp core
 * @sync_wait:		waitqueue header for sync mechanism
 * @wq:			cache kcfg for async mechanism
 * @parent:		be part of struct gsp_dev
 * @gsp_core_ops:	indicate gsp core operations
 */
struct gsp_core {
	char name[32];
	int id;
	int kcfg_num;

	struct gsp_capability *capa;

	struct gsp_dev *parent;
	/* linked with parent header */
	struct list_head list;

	struct device_node *node;
	struct device *dev;

	uint32_t irq;

	/* work queue which manages empty list,
	 * fill list,sep_list to ensure gsp_kcfg
	 * can be passed right
	 */
	struct gsp_workqueue *wq;

	struct kthread_worker kworker;
	struct kthread_work trigger;
	struct kthread_work release;
	struct kthread_work recover;

	struct task_struct *work_thread;

	void __iomem *base;

	/* indicate the kcfg handled by core presently */
	struct gsp_kcfg *current_kcfg;
	struct gsp_core_ops *ops;

	/* to compare which core weight is lighter */
	int weight;

	atomic_t state;
	atomic_t suspend_state;
	struct completion suspend_done;
	struct completion resume_done;
	struct completion release_done;

	struct gsp_sync_timeline *timeline;

	size_t cfg_size;
	/* to indicate whether core kthread priority is real-time */
	bool rt;

	/* used to debug and recover */
	struct list_head kcfgs;

	/* coef related information */
	int force_calc;
	int coef_init;

	/* there must be iommu for gsp core at 64-bit soc */
	bool need_iommu;
	/* to determine whether reset gsp if encounter hung */
	struct timer_list timer;
};




/**
 * struct gsp_core_ops - gsp core operation
 * @reg_set:	base fence class
 * @map:	iommu map dma-buf address
 * @init:	initialize specific gsp core
 */
struct gsp_core_ops {
	int (*parse_dt)(struct gsp_core *core);

	int (*alloc)(struct gsp_core **core, struct device_node *node);
	int (*init)(struct gsp_core *core);

	int (*copy)(struct gsp_kcfg *kcfg, void *arg, int index);

	int (*trigger)(struct gsp_core *core);
	int (*release)(struct gsp_core *core);

	int (*enable)(struct gsp_core *core);
	void (*disable)(struct gsp_core *core);

	int __user *(*intercept)(void __user *arg, int index);
	void (*dump)(struct gsp_core *core);
	void (*reset)(struct gsp_core *core);
};

#define CORE_MAX_KCFG_NUM(core) \
	((core)->kcfg_num)

struct device *gsp_core_to_device(struct gsp_core *core);
int gsp_core_to_id(struct gsp_core *core);
struct gsp_dev *gsp_core_to_parent(struct gsp_core *core);
struct gsp_workqueue *
gsp_core_to_workqueue(struct gsp_core *core);

struct gsp_core *gsp_core_chosen(struct gsp_dev *gsp);

void gsp_core_trigger(struct kthread_work *work);
void gsp_core_release(struct kthread_work *work);

int gsp_core_verify(struct gsp_core *core);

int gsp_core_is_idle(struct gsp_core *core);
int gsp_core_is_suspend(struct gsp_core *core);

void gsp_core_work(struct gsp_core *core);

int gsp_core_alloc(struct gsp_core **core,
		   struct gsp_core_ops *ops,
		   struct device_node *node);
void gsp_core_free(struct gsp_core *core);
int gsp_core_init(struct gsp_core *core);
void gsp_core_deinit(struct gsp_core *core);

void gsp_core_state_set(struct gsp_core *core,
			 enum gsp_core_state st);
enum gsp_core_state gsp_core_state_get(struct gsp_core *core);

struct gsp_core *gsp_core_select(struct gsp_dev *gsp);

void gsp_core_suspend(struct gsp_core *core);
void gsp_core_resume(struct gsp_core *core);

int gsp_core_release_wait(struct gsp_core *core);
int gsp_core_suspend_wait(struct gsp_core *core);
int gsp_core_stop(struct gsp_core *core);

void gsp_core_reg_write(void __iomem *addr, u32 value);
u32 gsp_core_reg_read(void __iomem *addr);
void gsp_core_reg_update(void __iomem *addr, u32 value, u32 mask);

struct gsp_capability *gsp_core_get_capability(struct gsp_core *core);

int gsp_core_get_kcfg_num(struct gsp_core *core);

void gsp_core_dump(struct gsp_core *core);
void gsp_core_reset(struct gsp_core *core);
void gsp_core_hang_handler(unsigned long data);

enum gsp_core_suspend_state gsp_core_suspend_state_get(struct gsp_core *core);
void gsp_core_suspend_state_set(struct gsp_core *core,
			 enum gsp_core_suspend_state value);

#endif
