#include <linux/delay.h>
#include <linux/input/adaptive_ts.h>
#include <linux/module.h>
#include <uapi/linux/input.h>

static enum ts_result handle_event(
	struct ts_controller *controller, enum ts_event event, void *data)
{
	unsigned char val;

	switch (event) {
	case TSEVENT_SUSPEND:
		val = 0x03;
		ts_reg_write(0xA5, &val, 1);
		break;
	case TSEVENT_RESUME:
		ts_gpio_set(TSGPIO_INT, 0);
		msleep(10);
		ts_gpio_set(TSGPIO_INT, 1);
		msleep(200);
		break;
	default:
		break;
	}

	return TSRESULT_EVENT_HANDLED;
}

static const unsigned short mAddrs[] = {
	0x38,
};

static const struct ts_virtualkey_info mVirtualkeys[] = {
	DECLARE_VIRTUALKEY(600, 1350, 60, 45, KEY_BACK),
	DECLARE_VIRTUALKEY(360, 1350, 60, 45, KEY_HOMEPAGE),
	DECLARE_VIRTUALKEY(120, 1350, 60, 45, KEY_APPSELECT),
};

static const struct ts_register_info mRegisters[] = {
	DECLARE_REGISTER(TSREG_CHIP_ID, 0xA3),
	DECLARE_REGISTER(TSREG_MOD_ID, 0xA8),
	DECLARE_REGISTER(TSREG_FW_VER, 0xA6),
	DECLARE_REGISTER("frequency", 0x88),
	DECLARE_REGISTER("charger_indicator", 0x8B),
};

static struct ts_controller mController = {
	.name = "dummy_ts",
	.vendor = "sprd",
	.config = TSCONF_ADDR_WIDTH_BYTE
		| TSCONF_POWER_ON_RESET
		| TSCONF_RESET_LEVEL_LOW
		| TSCONF_REPORT_MODE_IRQ
		| TSCONF_IRQ_TRIG_EDGE_FALLING
		| TSCONF_REPORT_TYPE_2,
	.addr_count = ARRAY_SIZE(mAddrs),
	.addrs = mAddrs,
	.virtualkey_count = ARRAY_SIZE(mVirtualkeys),
	.virtualkeys = mVirtualkeys,
	.register_count = ARRAY_SIZE(mRegisters),
	.registers = mRegisters,
	.panel_width = 720,
	.panel_height = 1280,
	.reset_keep_ms = 10,
	.reset_delay_ms = 200,
	.parser = {
		.num_info = DECLARE_FRAG(0x02, 1, 0xFF, 0, 0),
		.point_info = {
			.pressed = DECLARE_FRAG(0, 1, 0xC0, 0, 6),
			.down_check = 0x00,
			.up_check = 0x01,
			.slot = DECLARE_FRAG(2, 1, 0xF0, 0, 4),
			.xh = DECLARE_FRAG(0, 1, 0x0F, 8, 0),
			.xl = DECLARE_FRAG(1, 1, 0xFF, 0, 0),
			.yh = DECLARE_FRAG(2, 1, 0x0F, 8, 0),
			.yl = DECLARE_FRAG(3, 1, 0xFF, 0, 0),
			.pressure_frag1 = DECLARE_FRAG(4, 1, 0xFF, 0, 0),
			.pressure_frag2 = DECLARE_FRAG(0, 0, 0x00, 0, 0),
			.touch_major_frag1 = DECLARE_FRAG(5, 0, 0xF0, 0, 4),
			.touch_major_frag2 = DECLARE_FRAG(0, 0, 0x00, 0, 0),
		},
		.start_addr = 0x03,
		.points_count = 10,
		.bytes_count = 6,
	},
	.match = NULL,
	.fetch_points = NULL,
	.handle_event = handle_event,
	.upgrade_firmware = NULL,
};

REGISTER_CONTROLLER(mController);

MODULE_AUTHOR("joseph.cai@spreadtrum.com");
MODULE_DESCRIPTION("Spreadtrum dummy touchscreen driver");
MODULE_LICENSE("GPL");
