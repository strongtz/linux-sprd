#ifndef __TS_FUNC_TEST_H
#define __TS_FUNC_TEST_H

struct ts_func_test_device {
	struct device *dev;

	int (*ts_async_suspend_for_lcd_use)(void);
	int (*ts_async_resume_for_lcd_use)(void);
	int (*ts_reset_for_lcd_use)(void);
	int (*ts_suspend_need_lcd_reset_high)(void);
	int (*ts_suspend_need_lcd_power_reset_high)(void);
	int (*check_fw_update_need)(struct device *dev);
	int (*get_fw_update_progress)(struct device *dev);
	int (*proc_fw_update)(struct device *dev, bool force);
	int (*get_rawdata)(struct device *dev, char *buf);
	int (*get_rawdata_info)(struct device *dev, char *buf);
	int (*get_diff)(struct device *dev, char *buf);
	int (*get_selftest)(struct device *dev);
	int (*proc_hibernate_test)(struct device *dev);
	int (*get_ic_fw_version)(struct device *dev, char *buf);
	int (*get_fs_fw_version)(struct device *dev, char *buf);
	int (*get_module_id)(struct device *dev, char *buf);
	int (*get_chip_type)(struct device *dev, char *buf);
	int (*get_calibration_ret)(struct device *dev);

	int (*get_fw_path)(struct device *dev, char *buf, size_t buf_size);
	int (*set_fw_path)(struct device *dev, const char *buf);
	int (*proc_fw_update_with_given_file)(struct device *dev,
					       const char *buf);
	int (*set_gesture_switch)(struct device *dev, const char *buf);
	bool (*get_gesture_switch)(struct device *dev);
	int (*get_open_test)(struct device *dev, char *buf);
	int (*get_short_test)(struct device *dev, char *buf);
	int (*set_test_config_path)(struct device *dev, const char *buf);
	bool (*need_test_config)(struct device *dev);
	int (*get_factory_info)(struct device *dev, char *buf);
	int (*get_cfg_info)(struct device *dev, char *buf);
	int (*get_gesture_pos)(struct device *dev, char *buf);
	int (*get_gesture_fingers)(struct device *dev, char *buf);
	int (*set_hall_switch)(struct device *dev, const char *buf);
	bool (*get_hall_switch)(struct device *dev);
	int (*set_glove_switch)(struct device *dev, bool enable);
	bool (*get_glove_switch)(struct device *dev);
	int (*set_tptype_switch)(struct device *dev, int tptypeswitch);
	int (*get_tptype_switch)(struct device *dev);
	int (*set_tp_enable_switch)(struct device *dev, bool enable);
	bool (*get_tp_enable_switch)(struct device *dev);
	int (*get_tp_settings_info)(struct device *dev, char *buf);
	int (*set_erase_flash_test)(struct device *dev, bool enable);
	bool (*get_tp_irq_awake_switch)(struct device *dev);
	int (*set_tp_irq_awake_switch)(struct device *dev, bool enable);
#if defined(CONFIG_TP_MATCH_HW)
	int (*check_gpio_state)(struct device *dev);
	int (*tp_match_hw)(struct device *dev);
#endif
	int (*get_tp_work_mode)(struct device *dev, char *buf);
	int (*set_tp_work_mode)(struct device *dev, const char *mode);
};

void register_ts_func_test_device(struct ts_func_test_device *device);
void unregister_ts_func_test_device(struct ts_func_test_device *device);
void ts_gen_func_test_init(void);
#endif
