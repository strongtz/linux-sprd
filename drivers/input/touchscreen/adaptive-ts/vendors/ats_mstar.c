#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input/adaptive_ts.h>
#include <linux/module.h>
#include <uapi/linux/input.h>

#define MSTAR_POINT_BUF_SIZE 8
#define MSTAR_MAX_POINTS 2

static int mstar_fetch(struct ts_controller *c, struct ts_point *points)
{
	int len, i, count = -1;
	unsigned short dx, dy;
	unsigned char buf[MSTAR_POINT_BUF_SIZE] = { 0 }, checksum = 0;

	len = ts_read(buf, MSTAR_POINT_BUF_SIZE);
	if (len < 0)
		return len;

	if (buf[0] != 0x52) {
		TS_WARN("head error, ignore this packet");
		return -1;
	}

	for (i = 0; i < MSTAR_POINT_BUF_SIZE; i++)
		checksum += buf[i];
	if (checksum != 0) {
		TS_WARN("checksum error, ignore this packet");
		return -1;
	}

	if (buf[1] == 0xFF && buf[2] == 0XFF && buf[3] == 0xFF
		&& buf[4] == 0xFF && buf[6] == 0xFF) {
		/* button event */
		switch (buf[5]) {
		case 0x01:
			points[0].x = 1712;
			points[0].y = 2240;
			count = 1;
			break;
		case 0x02:
			points[0].x = 336;
			points[0].y = 2240;
			count = 1;
			break;
		case 0x04:
			points[0].x = 1024;
			points[0].y = 2240;
			count = 1;
			break;
		case 0x00:
		case 0xFF:
			/* it's an up event */
			count = 0;
			break;
		default:
			/* ignore multi-button or fault event */
			break;
		}
	} else {
		/* touch event */
		points[0].x = ((buf[1] & 0xF0) << 4) | buf[2];
		points[0].y = ((buf[1] & 0x0F) << 8) | buf[3];
		dx = ((buf[4] & 0xF0) << 4) | buf[5];
		dy = ((buf[4] & 0x0F) << 8) | buf[6];
		if (dx == 0 && dy == 0) {
			count = 1;
		} else {
			dx = (dx > 2048) ? (dx - 4096) : dx;
			dy = (dy > 2048) ? (dy - 4096) : dy;
			points[1].x = points[0].x + dx;
			points[1].y = points[0].y + dy;
			count = 2;
		}
	}

	return count;
}

static enum ts_result mstar_handle_event(
	struct ts_controller *c, enum ts_event event, void *data)
{
	switch (event) {
	case TSEVENT_SUSPEND:
		ts_gpio_set(TSGPIO_RST, 0);
		break;
	case TSEVENT_RESUME:
		ts_gpio_set(TSGPIO_RST, 1);
		msleep(50);
		break;
	default:
		break;
	}

	return TSRESULT_EVENT_HANDLED;
}

static const unsigned short mstar_addrs[] = {
	0x26,
};

static const struct ts_virtualkey_info mstar_virtualkeys[] = {
	DECLARE_VIRTUALKEY(336, 2240, 400, 300, KEY_BACK),
	DECLARE_VIRTUALKEY(1024, 2240, 400, 300, KEY_HOMEPAGE),
	DECLARE_VIRTUALKEY(1712, 2240, 400, 300, KEY_APPSELECT),
};

static struct ts_controller msg21xxa = {
	.name = "MSG21xxA",
	.vendor = "mstar",
	.config = TSCONF_ADDR_WIDTH_BYTE
		| TSCONF_NO_POWER_ON_RESET
		| TSCONF_RESET_LEVEL_LOW
		| TSCONF_REPORT_MODE_IRQ
		| TSCONF_IRQ_TRIG_EDGE_RISING
		| TSCONF_REPORT_TYPE_1,
	.addr_count = ARRAY_SIZE(mstar_addrs),
	.addrs = mstar_addrs,
	.virtualkey_count = ARRAY_SIZE(mstar_virtualkeys),
	.virtualkeys = mstar_virtualkeys,
	.register_count = 0,
	.registers = NULL,
	.panel_width = 2048,
	.panel_height = 2048,
	.reset_keep_ms = 10,
	.reset_delay_ms = 50,
	.parser = {
	},
	.match = NULL,
	.fetch_points = mstar_fetch,
	.handle_event = mstar_handle_event,
	.upgrade_firmware = NULL,
};

REGISTER_CONTROLLER(msg21xxa);

MODULE_AUTHOR("joseph.cai@spreadtrum.com");
MODULE_DESCRIPTION("Spreadtrum touchscreen driver for Mstar");
MODULE_LICENSE("GPL");
