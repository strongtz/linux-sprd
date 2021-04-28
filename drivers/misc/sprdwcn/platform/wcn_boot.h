#ifndef _WCN_BOOT
#define _WCN_BOOT

#include <misc/marlin_platform.h>

#include "rf/rf.h"

struct wcn_sync_info_t {
	unsigned int init_status;
	unsigned int mem_pd_bt_start_addr;
	unsigned int mem_pd_bt_end_addr;
	unsigned int mem_pd_wifi_start_addr;
	unsigned int mem_pd_wifi_end_addr;
	unsigned int prj_type;
	unsigned short tsx_dac_data;
	unsigned short rsved;
} __packed;

struct tsx_data {
	u32 flag; /* cali flag ref */
	u16 dac; /* AFC cali data */
	u16 reserved;
};

struct tsx_cali {
	u32 init_flag;
	struct tsx_data tsxdata;
};

#define WCN_BOUND_CONFIG_NUM	4
struct wcn_pmic_config {
	bool enable;
	char name[32];
	/* index [0]:addr [1]:mask [2]:unboudval [3]boundval */
	u32 config[WCN_BOUND_CONFIG_NUM];
};

struct wcn_clock_info {
	enum wcn_clock_type type;
	enum wcn_clock_mode mode;
	/*
	 * xtal-26m-clk-type-gpio config in the dts.
	 * if xtal-26m-clk-type config in the dts,this gpio unvalid.
	 */
	int gpio;
};

struct marlin_device {
	struct wcn_clock_info clk_xtal_26m;
	int wakeup_ap;
	int reset;
	int chip_en;
	int int_ap;
	/* pmic config */
	struct regmap *syscon_pmic;
	/* sharkl5 vddgen1 */
	struct wcn_pmic_config avdd12_parent_bound_chip;
	struct wcn_pmic_config avdd12_bound_wbreq;
	struct wcn_pmic_config avdd33_bound_wbreq;

	bool bound_avdd12;
	bool bound_dcxo18;
	/* power sequence */
	/* VDDIO->DVDD12->chip_en->rst_N->AVDD12->AVDD33 */
	struct regulator *dvdd12;
	struct regulator *avdd12;
	/* for PCIe */
	struct regulator *avdd18;
	/* for wifi PA, BT TX RX */
	struct regulator *avdd33;
	/* for internal 26M clock */
	struct regulator *dcxo18;
	struct clk *clk_32k;

	struct clk *clk_parent;
	struct clk *clk_enable;
	struct device_node *np;
	struct mutex power_lock;
	struct completion carddetect_done;
	struct completion download_done;
	struct completion gnss_download_done;
	unsigned long power_state;
	char *write_buffer;
	struct delayed_work power_wq;
	struct work_struct download_wq;
	struct work_struct gnss_dl_wq;
	bool keep_power_on;
	bool wait_ge2;
	bool is_btwf_in_sysfs;
	bool is_gnss_in_sysfs;
	int wifi_need_download_ini_flag;
	int first_power_on_ready;
	atomic_t download_finish_flag;
	unsigned char gnss_dl_finish_flag;
	int loopcheck_status_change;
	struct wcn_sync_info_t sync_f;
	struct tsx_cali tsxcali;
	char *btwf_path;
	char *gnss_path;
};

struct wifi_calibration {
	struct wifi_config_t config_data;
	struct wifi_cali_t cali_data;
};

#endif
