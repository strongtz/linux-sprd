#ifndef __ATS_CORE_H__
#define __ATS_CORE_H__

#include <linux/input.h>
#include <linux/input/adaptive_ts.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <uapi/linux/time.h>

/* ================================================== *
 *                       MACROs                       *
 * ================================================== */

/* names to recognize adaptive-ts in system */
#define ATS_COMPATIBLE    "adaptive-touchscreen"
#define ATS_PLATFORM_DEV  "adaptive_ts_drv"
#define ATS_I2C_DEV       "adaptive_ts_i2c_drv"
#define ATS_INPUT_DEV     "adaptive_ts"
#define ATS_INT_LABEL     "adaptive_ts_int"
#define ATS_RST_LABEL     "adaptive_ts_rst"
#define ATS_WORKQUEUE     "adaptive_ts_workqueue"
#define ATS_WL_UPGRADE_FW "adaptive_ts_fw_upgrade"
#define ATS_IRQ_HANDLER   "adaptive_ts-irq"
#define ATS_NOTIFIER_WORKQUEUE     "adaptive_ts_notifier_workqueue"

/* default point buffer size - support max 10 points */
#define TS_MAX_POINTS 10
/* default polling frequency 100Hz */
#define TS_POLL_INTERVAL 10000
/* default debug level */
#define TS_DEFAULT_DEBUG_LEVEL 1
/* use adf notifier to control suspend/resume */
#ifdef CONFIG_ADF_SPRD
#define TS_USE_ADF_NOTIFIER
#endif
/* use legacy linux suspend/resume */
#define TS_USE_LEGACY_SUSPEND 0

/* key map version for Android */
#define ANDROID_KEYMAP_VERSION 0x01

#define TSMODE_CONTROLLER_EXIST     0       /* bit0: controller existence status */
#define TSMODE_POLLING_MODE         1       /* bit1: polling mode status */
#define TSMODE_POLLING_STATUS       2       /* bit2: polling enable status */
#define TSMODE_IRQ_MODE             3       /* bit3: irq mode status */
#define TSMODE_IRQ_STATUS           4       /* bit4: irq enable status */
#define TSMODE_SUSPEND_STATUS       5       /* bit5: suspend status */
#define TSMODE_CONTROLLER_STATUS    6       /* bit6: whether controller is available */
#define TSMODE_VKEY_REPORT_ABS      7       /* bit7: report virtualkey as ABS or KEY event */
#define TSMODE_WORKQUEUE_STATUS     8       /* bit8: workqueue status */
#define TSMODE_AUTO_UPGRADE_FW      9       /* bit9: auto upgrade firmware when boot up */
#define TSMODE_NOISE_STATUS         10      /* bit10: noise status */
#define TSMODE_DEBUG_UPDOWN         16      /* bit16: print point up & down info */
#define TSMODE_DEBUG_RAW_DATA       17      /* bit17: print raw data */
#define TSMODE_DEBUG_IRQTIME        18      /* bit18: print irq time info */


/* ================================================== *
 *                      structs                       *
 * ================================================== */

/*
 * defines supported low layer bus type
 */
enum ts_bustype {
	TSBUS_NONE = 0,
	TSBUS_I2C,
};

/*
 * struct ts_bus_access
 *
 * Provide a set of methods to access the bus.
 *
 * bus_type         : bus type
 * client_addr      : controller slave address
 * reg_width        : width of controller's registers
 * simple_read      : read data from bus, length limited
 * simple_write     : write data to bus, length limited
 * read             : read data from some register address(full version)
 * write            : write data to some register address(full version)
 * simple_read_reg  : read data from some register, length limited
 * simple_write_reg : write data to some register, length limited
 *
 */
struct ts_bus_access {
	enum ts_bustype bus_type;
	unsigned short client_addr;
	unsigned char reg_width;
	int (*simple_read)(unsigned char *, unsigned short);
	int (*simple_write)(unsigned char *, unsigned short);
	int (*read)(unsigned short, unsigned char *, unsigned short);
	int (*write)(unsigned short, unsigned char *, unsigned short);
	int (*simple_read_reg)(unsigned short, unsigned char *, unsigned short);
	int (*simple_write_reg)(unsigned short, unsigned char *, unsigned short);
};

/*
 * struct ts_board
 *
 * contains board info in .dts file
 *
 * bus              : bus access info
 * pdev             : platform device
 * priv             : private data node
 * int_gpio         : interrupt GPIO number
 * rst_gpio         : reset GPIO number
 * panel_width      : touch panel width
 * panel_height     : touch panel height
 * surface_width    : surface width of screen
 * surface_height   : surface height of screen
 * lcd_width        : lcd panel width
 * lcd_height       : lcd panel height
 * avdd_supply      : some controller may require special power supply
 * controller       : controller name
 * vkey_report_abs  : whether we treat virtual keys as abs events
 * auto_upgrade_fw  : whether upgrade firmware when boot up
 * suspend_on_init  : suspend touch panel after init complete
 * virtualkey_count : number of virtual key defined
 * virtualkeys      : virtual keys info
 *
 */
struct ts_board {
	struct ts_bus_access *bus;
	struct platform_device *pdev;
	struct device_node *priv;
	int int_gpio;
	int rst_gpio;
	unsigned int panel_width;
	unsigned int panel_height;
	unsigned int surface_width;
	unsigned int surface_height;
	unsigned int lcd_width;
	unsigned int lcd_height;
	const char *avdd_supply;
	const char *controller;
	bool vkey_report_abs;
	bool auto_upgrade_fw;
	bool suspend_on_init;
	int virtualkey_count;
	struct ts_virtualkey_info virtualkeys[0];
};

/*
 * struct ts_virtualkey_hitbox
 */
struct ts_virtualkey_hitbox {
	unsigned short top;
	unsigned short bottom;
	unsigned short left;
	unsigned short right;
};

/*
 * struct ts_virtualkey_pair
 *
 * stores virtual key name and keycode
 */
struct ts_virtualkey_pair {
	const char *name;
	const unsigned int code;
};

enum ts_stashed_status {
	TSSTASH_INVALID = 0,
	TSSTASH_NEW,
	TSSTASH_CONSUMED,
};

/*
 * struct ts_data
 *
 * irq            : interrupt number for touch
 * status         : working status, defined by TSMODE_xxx
 * stashed_reg    : stashed register address, used in sysfs node register
 * width          : reportint width for input device
 * height         : reporting height for input device
 * pdev           : platform device
 * board          : contains board info
 * controller     : contains controller info
 * input          : input device
 * power          : regulator used
 * vkey_obj       : the kernel object corresponding to /sys/board_properties
 * vkey_attr      : the kernel attribute used for virtual keys
 * vkey_count     : number of virtual keys
 * vkey_list      : virtual keys info
 * vkey_hitbox    : virtual key hit boxes
 * poll_interval  : the reporting interval if we're in polling mode
 * poll_timer     : polling mode timer
 * resume_work    : resuming work struct
 * workqueue      : workqueue used for async task
 * upgrade_lock   : hold wake lock when firmware upgrading
 * stashed_points : stashed points to filter changed fields when using type B
 *                : store every point and use slot as array index, we assume
 *                : slot for reported points range from 0 to TS_MAX_POINTS
 * suspend        : track lcd backlight status to do suspend and resume
 *                : following display
 *
 */
struct ts_data {
	int irq;
	unsigned long status;
	unsigned short stashed_reg;
	unsigned int width;
	unsigned int height;

	struct platform_device *pdev;
	struct ts_board *board;
	struct ts_controller *controller;
	struct input_dev *input;
	struct regulator *power;

	struct kobject *vkey_obj;
	struct kobj_attribute vkey_attr;
	unsigned int vkey_count;
	struct ts_virtualkey_info *vkey_list;
	struct ts_virtualkey_hitbox *vkey_hitbox;

	int poll_interval;
	struct timer_list poll_timer;
	struct work_struct resume_work;
	struct workqueue_struct *workqueue;
	struct work_struct notifier_work;
	struct workqueue_struct *notifier_workqueue;

	struct wakeup_source upgrade_lock;
	/*
	 * stash points to filter changed fields when using type 3
	 * store every point using slot as array index, we assume
	 * the reported slot ranges from 0 to 9
	 */
	struct ts_point stashed_points[TS_MAX_POINTS]; /* TODO need two buffer? */
	enum ts_stashed_status stashed_status[TS_MAX_POINTS];
	bool suspend;
};


/* ================================================== *
 *                    functions                       *
 * ================================================== */

/*
 * initialize I2C device
 */
int ts_i2c_init(struct device_node *, unsigned short *);
void ts_i2c_exit(void);

/*
 * initialize board settings
 */
int ts_board_init(void);
void ts_board_exit(void);

/*
 * register bus device to board settings
 */
int ts_register_bus_dev(struct device *);
void ts_unregister_bus_dev(void);

/*
 * handler from core module to handle our inner events
 */
typedef void (*event_handler_t)(struct ts_data *, enum ts_event, void *);
int ts_register_ext_event_handler(struct ts_data *, event_handler_t);
void ts_unregister_ext_event_handler(void);

/*
 * match controller from name, if no name provided, choose a controller
 */
struct ts_controller *ts_match_controller(const char *);

/*
 * transform keycode to key name
 */
static inline const char *ts_get_keyname(const unsigned int keycode)
{
	extern struct ts_virtualkey_pair VIRTUALKEY_PAIRS[];
	struct ts_virtualkey_pair *pair = VIRTUALKEY_PAIRS;

	while (pair->code && pair->code != keycode)
		pair++;

	return pair->name;
}

#endif
