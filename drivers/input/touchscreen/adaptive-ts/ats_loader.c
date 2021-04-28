#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/types.h>
#include "ats_core.h"

#define TS_VIRTUALKEY_DATA_LENGTH	5
#define TS_VIRTUALKEY_MAX_COUNT		4
#define TS_RST_INDEX 0
#define TS_INT_INDEX 1
#define TS_PROP_VIRTUALKEY			"virtualkeys"
#define TS_PROP_VIRTUALKEY_REPORT	"virtualkey-report-abs"
#define TS_PROP_AUTO_UPGRADE_FW		"firmware-auto-upgrade"
#define TS_PROP_POWER				"avdd-supply"
#define TS_PROP_CONTROLLER			"controller"
#define TS_PROP_WIDTH				"touchscreen-size-x"
#define TS_PROP_HEIGHT				"touchscreen-size-y"
#define TS_PROP_SURFACE_WIDTH		"surface-width"
#define TS_PROP_SURFACE_HEIGHT		"surface-height"
#define TS_PROP_PRIV_NODE			"private-data"
#define POOL_MAX_SIZE 64

int ts_dbg_level = TS_DEFAULT_DEBUG_LEVEL;
static struct ts_board *g_board;
static LIST_HEAD(controllers);
static DEFINE_SPINLOCK(controller_lock);
static unsigned short address_pool[POOL_MAX_SIZE];
static unsigned int pool_length;
static unsigned short lcd_width;
static unsigned short lcd_height;
static bool cali;

static int ts_parse_lcd_size(char *str)
{
	char *c, buf[32] = { 0 };
	int length;

	if (str != NULL) {
		c = strchr(str, 'x');
		if (c != NULL) {
			/* height */
			length = c - str;
			strncpy(buf, str, length);
			if (kstrtou16(buf, 10, &lcd_height))
				lcd_height = 0;
			/* width */
			length = strlen(str) - (c - str) - 1;
			strncpy(buf, c + 1, length);
			buf[length] = '\0';
			if (kstrtou16(buf, 10, &lcd_width))
				lcd_width = 0;
		} else {
			lcd_width = lcd_height = 0;
		}
	}
	return 1;
}
__setup("lcd_size=", ts_parse_lcd_size);

static int ts_parse_cali_mode(char *str)
{
	if (str != NULL && !strncmp(str, "cali", strlen("cali")))
		cali = true;
	else
		cali = false;
	return 0;
}
__setup("androidboot.mode=", ts_parse_cali_mode);

/* parse key name using key code */
struct ts_virtualkey_pair VIRTUALKEY_PAIRS[] = {
	{
		.name = "BACK",
		.code = KEY_BACK,
	},
	{
		.name = "HOME",
		.code = KEY_HOMEPAGE,
	},
	{
		.name = "APP_SWITCH",
		.code = KEY_APPSELECT,
	},
	{ },
};

/*
 * validate controller
 */
static inline int ts_validate_controller(struct ts_controller *c)
{
	/* controller name and vendor is required */
	if (!c->name || !c->vendor) {
		pr_err("no name or vendor!\n");
		return -EINVAL;
	}

	if (c->addr_count == 0 || !c->addrs) {
		pr_err("should provide at least one valid address!\n");
		return -EINVAL;
	}

	/* TODO: remove this later */
	if (!c->fetch_points)
		return -EINVAL;

	return 0;
}

/*
 * register controller and add its address to address_pool for
 * adaptively matching
 */
int ts_register_controller(struct ts_controller *controller)
{
	bool found = false;
	struct ts_controller *dup;
	unsigned int i;

	if (!controller)
		return -ENODEV;

	if (ts_validate_controller(controller) < 0) {
		pr_warn("ignore controller \"%s\" cuz validation failed!\n", controller->name);
		return -EINVAL;
	}

	/* prevent duplicated controller */
	spin_lock(&controller_lock);
	list_for_each_entry(dup, &controllers, list) {
		if (!strcmp(dup->vendor, controller->vendor)
			&& !strcmp(dup->name, controller->name)) {
			found = true;
			break;
		}
	}
	spin_unlock(&controller_lock);

	if (found) {
		pr_warn("ignore duplicated registration.\n");
		return -EEXIST;
	}

	spin_lock(&controller_lock);
	/* add new controller */
	list_add_tail(&controller->list, &controllers);

	/*
	 * add address to pool
	 * TODO: increase pool size dynamically
	 */
	if (pool_length < POOL_MAX_SIZE) {
		found = false;
		for (i = 0; i < pool_length; i++) {
			/* TODO: change addrs[0] to addr list */
			if (controller->addrs[0] == address_pool[i]) {
				found = true;
				break;
			}
		}
		if (!found)
			address_pool[pool_length++] = controller->addrs[0];
	}
	spin_unlock(&controller_lock);

	pr_debug("register controller: \"%s-%s\"",
		controller->vendor, controller->name);
	return 0;
}

/*
 * when controller unregistered, its addresses are not dropped
 */
void ts_unregister_controller(struct ts_controller *controller)
{
	struct ts_controller *c;
	bool del = false;

	if (!controller)
		return;

	spin_lock(&controller_lock);
	list_for_each_entry(c, &controllers, list) {
		if (!strcmp(c->vendor, controller->vendor)
			&& !strcmp(c->name, controller->name)) {
			list_del_init(&c->list);
			del = true;
			break;
		}
	}
	spin_unlock(&controller_lock);

	if (del)
		pr_debug("unregister controller \"%s-%s\"",
			controller->vendor, controller->name);
	else
		pr_warn("controller \"%s-%s\" not found.",
			controller->vendor, controller->name);
}

/*
 * matches one controller by name or adaptively
 */
struct ts_controller *ts_match_controller(const char *name)
{
	struct ts_controller *c, *target = NULL;
	char *ch, *t, buf[32];
	const char *s;
	int length = 0, i;

	if (unlikely(!g_board))
		return NULL;

	if (name) {
		/* with designated name, just find what they want */
		spin_lock(&controller_lock);
		list_for_each_entry(c, &controllers, list) {
			length = sprintf(buf, "%s,%s", c->vendor, c->name);
			if (!strncmp(name, buf, length)) {
				target = c;
				break;
			}
		}
		spin_unlock(&controller_lock);
		if (!target) {
			ch = strchr(name, ',');
			if (ch) {
				s = name;
				t = buf;
				while (s < ch)
					*t++ = *s++;
				*t = '\0';
				pr_debug("fallback to match vendor \"%s\"", buf);
				spin_lock(&controller_lock);
				list_for_each_entry(c, &controllers, list) {
					if (!strncmp(buf, c->vendor, ch - name)) {
						target = c;
						break;
					}
				}
				spin_unlock(&controller_lock);
			}
		}
	} else {
		/* without designated name. let's see who will be the volunteer */
		spin_lock(&controller_lock);
		list_for_each_entry(c, &controllers, list) {
			if (c->match) {
				for (i = 0; i < c->addr_count; i++) {
					if (g_board->bus->client_addr == c->addrs[i]
						&& c->match(c) == TSRESULT_FULLY_MATCHED) {
						target = c;
						break;
					}
				}
			}
			if (target)
				break;
		}
		spin_unlock(&controller_lock);
	}

	return target;
}

int ts_read(unsigned char *data, unsigned short length)
{
	if (IS_ERR_OR_NULL(g_board) || IS_ERR_OR_NULL(g_board->bus)) {
		pr_err("Touchscreen not ready!\n");
		return -ENODEV;
	}

	return g_board->bus->simple_read(data, length);
}

int ts_write(unsigned char *data, unsigned short length)
{
	if (IS_ERR_OR_NULL(g_board) || IS_ERR_OR_NULL(g_board->bus)) {
		pr_err("Touchscreen not ready!\n");
		return -ENODEV;
	}

	return g_board->bus->simple_write(data, length);
}

int ts_reg_read(unsigned short reg, unsigned char *data, unsigned short length)
{
	if (IS_ERR_OR_NULL(g_board) || IS_ERR_OR_NULL(g_board->bus)) {
		pr_err("Touchscreen not ready!\n");
		return -ENODEV;
	}

	return g_board->bus->read(reg, data, length);
}

int ts_reg_write(unsigned short reg, unsigned char *data, unsigned short length)
{
	if (IS_ERR_OR_NULL(g_board) || IS_ERR_OR_NULL(g_board->bus)) {
		pr_err("Touchscreen not ready!\n");
		return -ENODEV;
	}

	return g_board->bus->write(reg, data, length);
}

/* GPIO operation */
int ts_gpio_get(enum ts_gpio type)
{
	int val = 0;
	struct ts_board *board = g_board;

	if (IS_ERR_OR_NULL(board))
		return -ENODEV;

	if (TSGPIO_INT == type && board->int_gpio > 0) {
		val = gpio_get_value(board->int_gpio);
		if (val < 0)
			pr_err("Failed to get INT gpio(%d), err: %d.\n",
				board->int_gpio, val);
	} else if (TSGPIO_RST == type && board->rst_gpio > 0) {
		val = gpio_get_value(board->rst_gpio);
		if (val < 0)
			pr_err("Failed to set RST gpio(%d), err: %d.\n",
				board->rst_gpio, val);
	} else {
		pr_warn("Unrecognized gpio type, ignore.\n");
	}

	return val;
}
void ts_gpio_set(enum ts_gpio type, int level)
{
	struct ts_board *board = g_board;

	if (IS_ERR_OR_NULL(board))
		return;

	if (TSGPIO_INT == type && board->int_gpio) {
		gpio_set_value(board->int_gpio, level);
		pr_debug("set gpio INT (%d) to %d\n", board->int_gpio, level);
	} else if (TSGPIO_RST == type && board->rst_gpio) {
		gpio_set_value(board->rst_gpio, level);
		pr_debug("set gpio RST (%d) to %d\n", board->rst_gpio, level);
	} else {
		pr_warn("Unrecognized gpio type, ignore.\n");
	}
}
int ts_gpio_input(enum ts_gpio type)
{
	int retval = 0;
	struct ts_board *board = g_board;

	if (IS_ERR_OR_NULL(board))
		return -ENODEV;

	if (TSGPIO_INT == type && board->int_gpio) {
		retval = gpio_direction_input(board->int_gpio);
		if (retval < 0)
			pr_err("Failed to set gpio INT (%d) in, err: %d.\n",
				board->int_gpio, retval);
		else
			pr_debug("set gpio INT (%d) to input\n", board->int_gpio);
	} else if (TSGPIO_RST == type && board->rst_gpio) {
		retval = gpio_direction_input(board->rst_gpio);
		if (retval < 0)
			pr_err("Failed to set gpio RST (%d) in, err: %d.\n",
				board->rst_gpio, retval);
		else
			pr_debug("set gpio RST (%d) to input\n", board->rst_gpio);
	} else {
		pr_warn("Unrecognized gpio type, ignore.\n");
	}

	return retval;
}
int ts_gpio_output(enum ts_gpio type, int level)
{
	int retval = 0;
	struct ts_board *board = g_board;

	if (IS_ERR_OR_NULL(board))
		return -ENODEV;

	if (TSGPIO_INT == type && board->int_gpio) {
		retval = gpio_direction_output(board->int_gpio, level);
		if (retval < 0)
			pr_err("Failed to set gpio INT (%d) out to %d, err: %d.\n",
				board->int_gpio, level, retval);
		else
			pr_debug("set gpio INT (%d) to output, level=%d\n",
				board->int_gpio, level);
	} else if (TSGPIO_RST == type && board->rst_gpio) {
		retval = gpio_direction_output(board->rst_gpio, level);
		if (retval < 0)
			pr_err("Failed to set gpio RST (%d) out to %d, err: %d.\n",
				board->rst_gpio, level, retval);
		else
			pr_debug("set gpio RST (%d) to output, level=%d\n",
				board->rst_gpio, level);
	} else {
		pr_warn("Unrecognized gpio type, ignore.\n");
	}

	return retval;
}

/*
 * parse .dts file to get following info:
 *  - GPIO number for int, rst PIN
 *  - touch panel size
 *  - surface screen size
 *  - if report virtual key as KEY events
 *  - if auto upgrade firmware when boot up
 *  - virtual keys info
 *  - controller used
 *  - regulator name if used
 */
static struct ts_board *ts_parse_dt(struct device_node *pn)
{
	int retval, i, j, irq, rst;
	struct ts_board *board;
	u32 buf[TS_VIRTUALKEY_MAX_COUNT * TS_VIRTUALKEY_DATA_LENGTH];
	size_t size = sizeof(struct ts_board);

	rst = of_get_gpio(pn, TS_RST_INDEX);
	if (rst < 0) {
		pr_err("invalid reset gpio number: %d\n", rst);
		return NULL;
	}
	irq = of_get_gpio(pn, TS_INT_INDEX);
	if (irq < 0) {
		pr_err("invalid irq gpio number: %d\n", irq);
		return NULL;
	}

	i = of_property_count_u32_elems(pn, TS_PROP_VIRTUALKEY);
	if (i > 0) {
		if (i > TS_VIRTUALKEY_DATA_LENGTH * TS_VIRTUALKEY_MAX_COUNT
			|| i % TS_VIRTUALKEY_DATA_LENGTH) {
			i = 0;
			pr_err("invalid virtualkey data count: %d\n", i);
		} else {
			retval = of_property_read_u32_array(pn, TS_PROP_VIRTUALKEY,	buf, i);
			if (!retval) {
				i /= TS_VIRTUALKEY_DATA_LENGTH;
				size += sizeof(struct ts_virtualkey_info) * i;
			} else {
				i = 0;
				pr_err("failed to read virtualkey data, error: %d\n", retval);
			}
		}
	} else {
		i = 0;
	}

	board = kzalloc(size, GFP_KERNEL);
	if (!board) {
		pr_err("failed to allocate board info!!\n");
		return NULL;
	}

	board->int_gpio = irq;
	board->rst_gpio = rst;
	board->virtualkey_count = i;
	for (j = 0; j < i; j++) {
		board->virtualkeys[j].keycode = buf[TS_VIRTUALKEY_DATA_LENGTH * j];
		board->virtualkeys[j].x = buf[TS_VIRTUALKEY_DATA_LENGTH * j + 1] & 0xFFFF;
		board->virtualkeys[j].y = buf[TS_VIRTUALKEY_DATA_LENGTH * j + 2] & 0xFFFF;
		board->virtualkeys[j].width = buf[TS_VIRTUALKEY_DATA_LENGTH * j + 3] & 0xFFFF;
		board->virtualkeys[j].height = buf[TS_VIRTUALKEY_DATA_LENGTH * j + 4] & 0xFFFF;
	}

	retval = of_property_read_u32(pn, TS_PROP_WIDTH, &board->panel_width);
	if (retval < 0)
		board->panel_width = 0;
	retval = of_property_read_u32(pn, TS_PROP_HEIGHT, &board->panel_height);
	if (retval < 0)
		board->panel_height = 0;
	retval = of_property_read_u32(pn,
			TS_PROP_SURFACE_WIDTH, &board->surface_width);
	if (retval < 0)
		board->surface_width = 0;
	retval = of_property_read_u32(pn, TS_PROP_SURFACE_HEIGHT, &board->surface_height);
	board->lcd_width = lcd_width;
	board->lcd_height = lcd_height;
	if (retval < 0)
		board->surface_height = 0;
	board->vkey_report_abs = !!of_get_property(pn, TS_PROP_VIRTUALKEY_REPORT, NULL);
	board->auto_upgrade_fw = !!of_get_property(pn, TS_PROP_AUTO_UPGRADE_FW, NULL);
	retval = of_property_read_string(pn, TS_PROP_CONTROLLER, &board->controller);
	if (retval < 0)
		board->controller = NULL;
	retval = of_property_read_string(pn, TS_PROP_POWER, &board->avdd_supply);
	if (retval < 0)
		board->avdd_supply = NULL;

	/* add private data */
	board->priv = of_get_child_by_name(pn, TS_PROP_PRIV_NODE);

	pr_info("board config: rst_gpio=%d, int_gpio=%d",
			board->rst_gpio, board->int_gpio);
	if (board->panel_width)
		pr_info("board config: report_region=%ux%u",
				board->panel_width, board->panel_height);
	if (board->surface_width)
		pr_info("board config: surface_region=%ux%u",
				board->surface_width, board->surface_height);
	if (board->lcd_width)
		pr_info("from cmdline: lcd_region=%ux%u",
				board->lcd_width, board->lcd_height);
	pr_info("board config: %strying to auto upgrade firmware",
		board->auto_upgrade_fw ? "" : "not ");
	pr_info("board config: report virtual key as %s event",
		board->vkey_report_abs ? "ABS" : "KEY");
	if (board->virtualkey_count) {
		pr_info("board config: read %d virtualkeys",
			board->virtualkey_count);
		for (i = 0; i < board->virtualkey_count; i++) {
			pr_info("board config: x=%u, y=%u, w=%u, h=%u ------ %s",
				board->virtualkeys[i].x, board->virtualkeys[i].y,
				board->virtualkeys[i].width, board->virtualkeys[i].height,
				ts_get_keyname(board->virtualkeys[i].keycode));
		}
	}
	if (board->controller)
		pr_info("board config: requesting controller=\"%s\"", board->controller);
	else
		pr_info("board config: work in auto-detect mode");
	if (board->avdd_supply)
		pr_info("board config: requesting avdd-supply=\"%s\"", board->avdd_supply);

	return board;
}

static void ts_release_platform_dev(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	kfree(pdev);
}

/* register low layer bus access */
int ts_register_bus_dev(struct device *parent)
{
	struct platform_device *pdev;
	struct ts_board *board = g_board;
	struct ts_bus_access *bus;
	int retval;

	if (unlikely(IS_ERR_OR_NULL(board)))
		return -ENODEV;

	bus = dev_get_drvdata(parent);
	if (IS_ERR_OR_NULL(bus) || bus->bus_type == TSBUS_NONE
		|| !bus->read || !bus->write
		|| !bus->simple_read || !bus->simple_write
		|| !bus->simple_read_reg || !bus->simple_write_reg) {
		pr_err("incomplete bus interface!");
		return -ENXIO;
	}
	board->bus = bus;

	pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	if (IS_ERR_OR_NULL(pdev)) {
		pr_err("failed to allocate platform device!");
		return -ENOMEM;
	}

	pdev->name = ATS_PLATFORM_DEV;
	pdev->id = 0;
	pdev->num_resources = 0;
	pdev->dev.parent = parent;
	pdev->dev.platform_data = board;
	pdev->dev.release = ts_release_platform_dev;

	retval = platform_device_register(pdev);
	if (retval < 0) {
		pr_err("failed to register platform device!");
		kfree(pdev);
		return retval;
	}

	board->pdev = pdev;
	pr_debug("succeed to register platform device.");
	return 0;
}

void ts_unregister_bus_dev(void)
{
	struct ts_board *board = g_board;

	if (!IS_ERR_OR_NULL(board) && !IS_ERR_OR_NULL(board->pdev))
		platform_device_unregister(board->pdev);
	board->pdev = NULL;
}

/* initialize bus device according to node type */
static enum ts_bustype ts_bus_init(struct device_node *bus_node, bool adaptive)
{
	if (IS_ERR_OR_NULL(bus_node)) {
		pr_err("cannot decide bus type because of_node is null!");
		return TSBUS_NONE;
	}

	if (!strncmp(bus_node->name, "i2c", 3)) {
		if (adaptive) {
			address_pool[pool_length] = I2C_CLIENT_END;
			return ts_i2c_init(bus_node, address_pool)	?
				TSBUS_NONE : TSBUS_I2C;
		}

		if (!ts_i2c_init(bus_node, NULL))
			return TSBUS_I2C;
	} else if (!strncmp(bus_node->name, "spi", 3)) {
		/* TODO add spi support */
	} else {
		pr_warn("unknown bus type: \"%s\"", bus_node->name);
	}

	return TSBUS_NONE;
}

/*
 * init board related configurations, providing bus access and configs
 * for touchscreen core module
 */
int ts_board_init(void)
{
	struct device_node *pn;
	struct ts_board *board = NULL;
	enum ts_bustype bus_type = TSBUS_I2C;

	pn = of_find_compatible_node(NULL, NULL, ATS_COMPATIBLE);
	if (IS_ERR_OR_NULL(pn)) {
		pr_err("cannot find compatible node \"%s\"", ATS_COMPATIBLE);
		return -ENODEV;
	}

	board = ts_parse_dt(pn);
	if (IS_ERR_OR_NULL(board)) {
		pr_err("parsing board info failed!");
		return -ENODEV;
	}
	board->suspend_on_init = cali;

	g_board = board;
	bus_type = ts_bus_init(pn->parent, board->controller == NULL);
	if (bus_type == TSBUS_NONE) {
		pr_err("bus init failed!");
		kfree(board);
		g_board = NULL;
		return -ENODEV;
	}

	pr_debug("board init OK, bus type is %d.", bus_type);
	return 0;
}

void ts_board_exit(void)
{
	struct ts_board *board = g_board;

	if (IS_ERR_OR_NULL(board))
		return;

	switch (board->bus->bus_type) {
	case TSBUS_I2C:
		ts_i2c_exit();
		break;
	default:
		break;
	}

	kfree(board);
	g_board = NULL;
}

MODULE_AUTHOR("joseph.cai@spreadtrum.com");
MODULE_DESCRIPTION("Spreadtrum touchscreen controller loader");
MODULE_LICENSE("GPL");
