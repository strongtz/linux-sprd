#ifndef __SPRD_DJTAG__
#define __SPRD_DJTAG__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#define DJTAG_NAME_SIZE		32

struct djtag_master {
	struct djtag_ops *ops;
	struct device dev;
	void *data;
};

struct djtag_device {
	struct djtag_master *master;
	struct device dev;
	u32 sys;
	char modalias[DJTAG_NAME_SIZE];
};

struct djtag_ops {
	void (*mux_sel)(struct djtag_master *, u32, u32);
	void (*write)(struct djtag_master *, u32, u32, u32);
	int (*read)(struct djtag_master *, u32, u32);
	int (*lock)(struct djtag_master *);
	void (*unlock)(struct djtag_master *);
};

struct djtag_driver {
	int (*probe)(struct djtag_device *);
	int (*remove)(struct djtag_device *);
	void (*shutdown)(struct djtag_device *);
	struct device_driver driver;
};

#define to_djtag_driver(d) container_of(d, struct djtag_driver, driver)
static inline void djtag_unregister_driver(struct djtag_driver *ddrv)
{
	if (ddrv)
		driver_unregister(&ddrv->driver);
}
extern int djtag_register_real_driver(struct module *owner,
				   struct djtag_driver *ddrv);
extern int register_djtag_master(struct djtag_master *master);
extern void djtag_unregister_master(struct djtag_master *master);

#define djtag_register_driver(driver) \
	djtag_register_real_driver(THIS_MODULE, driver)

#define module_djtag_driver(ddrv) \
	module_driver(ddrv, djtag_register_driver, \
			djtag_unregister_driver)

#endif /*__SPRD_DJTAG__*/
