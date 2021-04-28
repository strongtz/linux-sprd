#ifndef __ADAPTIVE_TS_H__
#define __ADAPTIVE_TS_H__

#include <linux/list.h>

#define TAG "[TS]"
extern int ts_dbg_level;
#define TS_ERR(fmt, ...) \
	pr_err(TAG " %s() " fmt "\n", __func__, ##__VA_ARGS__)
#define TS_WARN(fmt, ...) \
	pr_warn(TAG " %s() " fmt "\n", __func__, ##__VA_ARGS__)
#define TS_INFO(fmt, ...) \
	pr_info(TAG " %s() " fmt "\n", __func__, ##__VA_ARGS__)
#define TS_DBG(fmt, ...) \
	do { \
		if (ts_dbg_level > 0) \
			pr_info(TAG " %s() " fmt "\n", __func__, ##__VA_ARGS__); \
	} while (0)

/*
 * these MACROs define the common register info description
 */
#define TSREG_CHIP_ID "chip_id"
#define TSREG_MOD_ID  "mod_id"
#define TSREG_FW_VER  "fw_ver"

/*
 * these MACROs configure the controller
 */
/* bit 0 */
#define TSCONF_ADDR_WIDTH_MASK       0x01
/* register addr count by byte */
#define TSCONF_ADDR_WIDTH_BYTE       0x00
/* register addr count by word */
#define TSCONF_ADDR_WIDTH_WORD       0x01

/* bit 1 */
#define TSCONF_POWER_ON_RESET_MASK   0x02
/* don't reset when power on */
#define TSCONF_POWER_ON_RESET        0x00
/* need hardware reset when power on */
#define TSCONF_NO_POWER_ON_RESET     0x02

/* bit 2 */
#define TSCONF_RESET_LEVEL_MASK      0x04
/* reset enable level is low */
#define TSCONF_RESET_LEVEL_LOW       0x00
/* reset enable level is high */
#define TSCONF_RESET_LEVEL_HIGH      0x04

/* bit 3 */
#define TSCONF_REPORT_MODE_MASK      0x08
/* works in interrupt mode */
#define TSCONF_REPORT_MODE_IRQ       0x00
/* works in polling mode */
#define TSCONF_REPORT_MODE_POLL      0x08

/* bit 4~5 */
#define TSCONF_IRQ_TRIG_MASK         0x30
/* interrupt is falling-edge trigger */
#define TSCONF_IRQ_TRIG_EDGE_FALLING 0x00
/* interrupt is rising-edge trigger */
#define TSCONF_IRQ_TRIG_EDGE_RISING  0x10
/* interrupt is low-level trigger */
#define TSCONF_IRQ_TRIG_LEVEL_LOW    0x20
/* interrupt is high-level trigger */
#define TSCONF_IRQ_TRIG_LEVEL_HIGH   0x30

/*
 * notes for report type
 *
 * we have 3 types of report type so far:
 *     1. report remaining points on screen WITHOUT hardware tracking id, AKA
 *         type-A report in "Linux Multi-Touch Protocal"
 *     2. report changed events, like DOWN, MOVE or UP, WITH hardware
 *         tracking id for points. This is typically the legacy type-B report
 *         in "Linux Multi-Touch Protocal"
 *     3. report remaining points on screen WITH hardware tracking id, but UP
 *         events may be omitted from raw data since it has gone. This type
 *         of report has an additional tracking id field for each point
 *         compared to type 1 ------ the only difference between type 1.
 */
/* bit 6~7 */
#define TSCONF_REPORT_TYPE_MASK      0XC0
/* report raw data in type A */
#define TSCONF_REPORT_TYPE_1         0x00
/* report raw data in type B */
#define TSCONF_REPORT_TYPE_2         0x40
/* report raw data in private type C */
#define TSCONF_REPORT_TYPE_3         0x80

/*
 * these MACROs is used to check controller configuration
 */
#define TSMASK_ADDR_WIDTH_LEN     0x01
#define TSMASK_POWER_ON_RESET_LEN 0x02
#define TSMASK_RESET_LEVEL_LEN    0x04
#define TSMASK_REPORT_MODE_LEN    0x08
#define TSMASK_IRQ_TRIG_LEN       0x30
#define TSMASK_REPORT_TYPE_LEN    0x40

/*
 * struct ts_point
 *
 * Containing the information of one point
 *
 * slot        : the hareware tracking slot id of this point, can be ignored
 *             : we use type A report protocal
 * pressed     : decides if the point if down or up
 * x           : reports the X coordinate of the point
 * y           : reports the Y coordinate of the point
 * pressure    : reports the physical pressure applied to the tip of the point
 * touch_major : reports the cross-sectional area of the touch contact,
 *               or the length of the longer dimension of the touch contact
 */
struct ts_point {
	unsigned char slot:7;
	unsigned char pressed:1;
	unsigned short x;
	unsigned short y;
	unsigned short pressure;
	unsigned short touch_major;
};

/*
 * struct ts_virtualkey_info
 *
 * Description of the hit box of a virtual key.
 *
 * x       : the x coordinate of the hit box center
 * y       : the y coordinate of the hit box center
 * width   : the width of the hit box
 * height  : the height of the hit box
 * keycode : the keycode this key reports
 */
struct ts_virtualkey_info {
	unsigned short x;
	unsigned short y;
	unsigned short width;
	unsigned short height;
	unsigned int keycode;
};

/*
 * convenient MACRO to declare a virtual key info
 */
#define DECLARE_VIRTUALKEY(val_x, val_y, val_width, val_height, val_keycode) \
	{ \
		.x = val_x, \
		.y = val_y, \
		.width = val_width, \
		.height = val_height, \
		.keycode = val_keycode, \
	}

/*
 * struct ts_register_info
 *
 * Description of register addresses and their meanings.
 *
 * name : the name of this register
 * reg  : the reg address
 */
struct ts_register_info {
	const char *name;
	const unsigned short reg;
};

/*
 * convenient MACRO to declare a register info
 */
#define DECLARE_REGISTER(reg_name, reg_addr) \
	{ \
		.name = reg_name, \
		.reg = reg_addr, \
	}

/*
 * struct ts_value_fragment
 *
 * Describing the fragment of a data, the data must be less than 16 bits
 *
 * addr   : the register address of raw data
 * used   : whether this fragment is used
 * mask   : the mask to be applied to raw data
 * lshift : bits of left shift operation
 * rshift : bits of right shift operation
 */
struct ts_value_fragment {
	const unsigned short addr;
	const unsigned char used;
	const unsigned char mask;
	const unsigned char lshift;
	const unsigned char rshift;
};

/*
 * convenient MACRO to declare a value fragment
 */

#define DECLARE_FRAG(_addr, _used, _mask, _lshift, _rshift) \
	{ \
		.addr = _addr, \
		.used = _used, \
		.mask = _mask, \
		.lshift = _lshift, \
		.rshift = _rshift, \
	}

/*
 * struct ts_point_info
 *
 * Containing information to parse point data which is required to be
 * less than 16 bits.
 *
 * pressed           : pressed or unpressed
 * down_check        : if value from 'pressed' equals, it means a down event
 * up_check          : if value from 'pressed' equals, it means an up event
 * slot              : slot number
 * xh                : high bits of x axis
 * xl                : low bits of x axis
 * yh                : high bits of y axis
 * yl                : low bits of y axis
 * pressure_frag1    : first fragment of pressure bits
 * pressure_frag2    : second fragment of pressure bits
 * touch_major_frag1 : first fragment of touch major
 * touch_major_frag2 : second fragment of touch major
 */
struct ts_point_info {
	const struct ts_value_fragment pressed;
	const unsigned char down_check;
	const unsigned char up_check;
	const struct ts_value_fragment slot;
	const struct ts_value_fragment xh;
	const struct ts_value_fragment xl;
	const struct ts_value_fragment yh;
	const struct ts_value_fragment yl;
	const struct ts_value_fragment pressure_frag1;
	const struct ts_value_fragment pressure_frag2;
	const struct ts_value_fragment touch_major_frag1;
	const struct ts_value_fragment touch_major_frag2;
};

/*
 * struct ts_point_parser
 *
 * This parser help us to parse raw data to point data, including coordinates,
 * pressure, area and id.
 *
 * num_info         : the fragment used to parse point number
 * point_info       : used to parse point data for every points
 * start_addr       : start address of raw data of the first point
 * points_count     : max supported points count
 * bytes_count      : the length of bytes for raw data of per point
 */
struct ts_point_parser {
	const struct ts_value_fragment num_info;
	const struct ts_point_info point_info;
	const unsigned short start_addr;
	const unsigned char points_count;
	const unsigned char bytes_count;
};

/*
 * define several events to be handled by controller
 */
enum ts_event {
	TSEVENT_POWER_ON = 1, /* go through the power on sequence */
	TSEVENT_SUSPEND,      /* go into low power state when screen off */
	TSEVENT_RESUME,       /* come back to normal work mode */
	TSEVENT_NOISE_HIGH,   /* USB plugged in, noise may be high */
	TSEVENT_NOISE_NORMAL, /* USB plugged out, noise be normal */
};

/*
 * define the result of callbacks
 */
enum ts_result {
	TSRESULT_UPGRADE_FINISHED = 0, /* upgrade success */
	TSRESULT_INVALID_BINARY,       /* invalid firmware file */
	TSRESULT_OLDER_VERSION,        /* the firmware file version is older */
	TSRESULT_OTHER_UPGRADE_ERROR,  /* other error */
	TSRESULT_FULLY_MATCHED,         /* controller is perfectly matched */
	TSRESULT_PARTIAL_MATCHED,      /* something's wrong, but may still work */
	TSRESULT_NOT_MATCHED,          /* no matching at all */
	TSRESULT_EVENT_HANDLED,        /* event is handled */
	TSRESULT_EVENT_NOT_HANDLED,    /* event is not handled */
};

/*
 * struct ts_controller
 *
 * The controller description customer should implmented, containing all
 * information that adaptive_ts will use to perform a touch device.
 *
 * name             : the name of this controller
 * vendor           : the vendor name
 * config           : several config macros defining the behavior of this
 *                  : controller
 * addr_count       : number of all possible chip addresses
 * addrs            : pointer to chip addresses
 * virtualkey_count : number of default virtual keys
 * virtualkeys      : pointer to virtual key information
 * register_count   : number of registers whose values can be shown by
 *                  : reading corresponding registers
 * registers        : pointer to register information
 * panel_width      : default max value reported in x-axis
 * panel_height     : default max value reported in y-axis
 * reset_keep_ms    : how long we must keep RST pin to some level
 * reset_delay_ms   : how long we must wait before reading controller
 * parser           : information used when parsing raw data, parser and
 *                  : fetch_points cannot be both NULL
 *
 *
 * match            : (optional) used to identify physical controller, should
 *                  : return the value of TSMATCH_*
 * fetch_points     : (optional) the function that fetching raw data of points
 *                  : from hardware, can be ignored if point info is provided
 * handle_event     : (optional) pointer to the function which handle events
 *                  : such as init, suspend, resume
 * upgrade_firmware : (optional) pointer to the function which handle firmware
 *                  : upgrading request, customer firmware will be loaded by
 *                  : kernel API request_firmware(), and passed through
 *                  : parameter 2 and 3
 */
struct ts_controller {
	const char *name;
	const char *vendor;
	const unsigned int config;
	const unsigned int addr_count;
	const unsigned short *addrs;
	const unsigned int virtualkey_count;
	const struct ts_virtualkey_info *virtualkeys;
	const unsigned int register_count;
	const struct ts_register_info *registers;
	const unsigned short panel_width;
	const unsigned short panel_height;
	const unsigned short reset_keep_ms;
	const unsigned short reset_delay_ms;
	const struct ts_point_parser parser;

	enum ts_result (*match)(struct ts_controller *);
	int (*fetch_points)(struct ts_controller *, struct ts_point *);
	enum ts_result (*handle_event)(
		struct ts_controller *, enum ts_event, void *);
	enum ts_result (*upgrade_firmware)(struct ts_controller *,
		const unsigned char *, size_t, bool);

	struct list_head list;
};

/*
 * convenient MACRO to register init/exit function for a controller
 */
#define REGISTER_CONTROLLER(controller) \
	static int __init controller##_init(void) \
	{ \
		return ts_register_controller(&controller); \
	} \
	static void __exit controller##_exit(void) \
	{ \
		ts_unregister_controller(&controller); \
	} \
	module_init(controller##_init); \
	module_exit(controller##_exit)

/*
 * register controller to our list, these controllers will be used to
 * initialize hardware chip and create a really touchscreen driver
 * call unregister to disable the controller
 */
int ts_register_controller(struct ts_controller *controller);
void ts_unregister_controller(struct ts_controller *controller);
/*
 * a simple read/write function with no retry and no length check,
 * even no register address needed
 */
int ts_read(unsigned char *data, unsigned short length);
int ts_write(unsigned char *data, unsigned short length);
/*
 * read/write function used when getting or setting values from/to
 * the specific register address, auto retry if failure occurred
 */
int ts_reg_read(unsigned short reg, unsigned char *data,
		unsigned short length);
int ts_reg_write(unsigned short reg, unsigned char *data,
		unsigned short length);

/*
 * define the common 2 pins of touchscreen
 */
enum ts_gpio {
	TSGPIO_INT = 1, /* the interrupt pin */
	TSGPIO_RST = 2, /* the reset pin */
};

/*
 * get/set GPIO values
 */
int ts_gpio_get(enum ts_gpio gpio);
void ts_gpio_set(enum ts_gpio gpio, int level);
/*
 * get/set GPIO directions
 */
int ts_gpio_input(enum ts_gpio gpio);
int ts_gpio_output(enum ts_gpio gpio, int level);

#endif
