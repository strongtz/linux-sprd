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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/firmware.h>
#include <linux/file.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <misc/marlin_platform.h>
#include <linux/mfd/syscon.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/unistd.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>

#include "gnss/gnss_common.h"
#include "rf/rf.h"
#include "../sleep/sdio_int.h"
#include "../sleep/slp_mgr.h"
#include "mem_pd_mgr.h"
#include <misc/wcn_bus.h>
#include "wcn_op.h"
#include "wcn_parn_parser.h"
#include "pcie_boot.h"
#include "rdc_debug.h"
#include "wcn_boot.h"
#include "wcn_dump.h"
#include "wcn_log.h"
#include "wcn_misc.h"
#include "wcn_procfs.h"
#include "wcn_gnss.h"
#include "wcn_txrx.h"
#include "mdbg_type.h"
#include "../include/wcn_dbg.h"
#include "../include/wcn_glb_reg.h"

#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX	"marlin."

static int clktype = -1;
module_param(clktype, int, 0444);

#ifndef REG_PMU_APB_XTL_WAIT_CNT0
#define REG_PMU_APB_XTL_WAIT_CNT0 0xe42b00ac
#endif
static char BTWF_FIRMWARE_PATH[255];
static char GNSS_FIRMWARE_PATH[255];

static struct wifi_calibration wifi_data;
struct completion ge2_completion;
static int first_call_flag = 1;
marlin_reset_callback marlin_reset_func;
void *marlin_callback_para;

static struct marlin_device *marlin_dev;
struct sprdwcn_gnss_ops *gnss_ops;

unsigned char  flag_reset;
char functionmask[8];
static unsigned int reg_val;
static unsigned int clk_wait_val;
static unsigned int cp_clk_wait_val;
static unsigned int marlin2_clk_wait_reg;

/* temp for rf pwm mode */
/* static struct regmap *pwm_regmap; */

#define IMG_HEAD_MAGIC "WCNM"
#define IMG_MARLINAA_TAG "MLAA"
#define IMG_MARLINAB_TAG "MLAB"
#define IMG_MARLINAC_TAG "MLAC"
#define MARLIN_MASK 0x27F
#define GNSS_MASK 0x080
#define AUTO_RUN_MASK 0X100

#define AFC_CALI_FLAG 0x54463031 /* cali flag */
#define AFC_CALI_READ_FINISH 0x12121212
#define WCN_AFC_CALI_PATH "/productinfo/wcn/tsx_bt_data.txt"

/* #define E2S(x) { case x: return #x; } */

struct head {
	char magic[4];
	u32 version;
	u32 img_count;
} __packed;

struct imageinfo {
	char tag[4];
	u32 offset;
	u32 size;
} __packed;

static unsigned long int chip_id;

unsigned int marlin_get_wcn_chipid(void)
{
	int ret;

	if (unlikely(chip_id != 0))
		return chip_id;

	ret = sprdwcn_bus_reg_read(CHIPID_REG, &chip_id, 4);
	if (ret < 0) {
		WCN_ERR("marlin read chip ID fail\n");
		return 0;
	}
	WCN_INFO("marlin: chipid=%lx, %s\n", chip_id, __func__);

	return chip_id;
}

enum wcn_chip_id_type wcn_get_chip_type(void)
{
	static enum wcn_chip_id_type chip_type;

	if (likely(chip_type))
		return chip_type;

	switch (marlin_get_wcn_chipid()) {
	case MARLIN_AA_CHIPID:
		chip_type = WCN_CHIP_ID_AA;
		break;
	case MARLIN_AB_CHIPID:
		chip_type = WCN_CHIP_ID_AB;
		break;
	case MARLIN_AC_CHIPID:
		chip_type = WCN_CHIP_ID_AC;
		break;
	case MARLIN_AD_CHIPID:
		chip_type = WCN_CHIP_ID_AD;
		break;
	default:
		chip_type = WCN_CHIP_ID_INVALID;
		break;
	}

	return chip_type;
}
EXPORT_SYMBOL_GPL(wcn_get_chip_type);

#if defined(CONFIG_SC2355)
#define WCN_CHIP_NAME_PRE "Marlin3_"
#elif defined(CONFIG_UMW2652)
#define WCN_CHIP_NAME_PRE "Marlin3Lite_"
#else
#define WCN_CHIP_NAME_PRE "ERRO_"
#endif

#define _WCN_STR(a) #a
#define WCN_STR(a) _WCN_STR(a)
#define WCN_CON_STR(a, b, c) (a b WCN_STR(c))

const char *wcn_get_chip_name(void)
{
	enum wcn_chip_id_type chip_type;
	static char *wcn_chip_name;
	static char * const chip_name[] = {
		"UNKNOWN",
		WCN_CON_STR(WCN_CHIP_NAME_PRE, "AA_", MARLIN_AA_CHIPID),
		WCN_CON_STR(WCN_CHIP_NAME_PRE, "AB_", MARLIN_AB_CHIPID),
		WCN_CON_STR(WCN_CHIP_NAME_PRE, "AC_", MARLIN_AC_CHIPID),
		WCN_CON_STR(WCN_CHIP_NAME_PRE, "AD_", MARLIN_AD_CHIPID),
	};

	if (likely(wcn_chip_name))
		return wcn_chip_name;

	chip_type = wcn_get_chip_type();
	if (chip_type != WCN_CHIP_ID_INVALID)
		wcn_chip_name = chip_name[chip_type];

	return chip_name[chip_type];
}
EXPORT_SYMBOL_GPL(wcn_get_chip_name);

static char *wcn_get_chip_tag(void)
{
	enum wcn_chip_id_type chip_type;
	static char *wcn_chip_tag;
	static char * const magic_tag[] = {
		"NULL",
		IMG_MARLINAA_TAG,
		IMG_MARLINAB_TAG,
		IMG_MARLINAC_TAG,
		IMG_MARLINAC_TAG,
	};

	if (likely(wcn_chip_tag))
		return wcn_chip_tag;

	chip_type = wcn_get_chip_type();
	if (chip_type != WCN_CHIP_ID_INVALID)
		wcn_chip_tag = magic_tag[chip_type];

	return wcn_chip_tag;
}

/* get the subsys string */
const char *strno(int subsys)
{
	switch (subsys) {
	case MARLIN_BLUETOOTH:
		return "MARLIN_BLUETOOTH";
	case MARLIN_FM:
		return "MARLIN_FM";
	case MARLIN_WIFI:
		return "MARLIN_WIFI";
	case MARLIN_WIFI_FLUSH:
		return "MARLIN_WIFI_FLUSH";
	case MARLIN_SDIO_TX:
		return "MARLIN_SDIO_TX";
	case MARLIN_SDIO_RX:
		return "MARLIN_SDIO_RX";
	case MARLIN_MDBG:
		return "MARLIN_MDBG";
	case MARLIN_GNSS:
		return "MARLIN_GNSS";
	case WCN_AUTO:
		return "WCN_AUTO";
	case MARLIN_ALL:
		return "MARLIN_ALL";
	default: return "MARLIN_SUBSYS_UNKNOWN";
	}
/* #undef E2S */
}

/* tsx/dac init */
int marlin_tsx_cali_data_read(struct tsx_data *p_tsx_data)
{
	u32 size = 0;
	u32 read_len = 0;
	struct file *file;
	loff_t offset = 0;
	char *pdata;

	file = filp_open(WCN_AFC_CALI_PATH, O_RDONLY, 0);
	if (IS_ERR(file)) {
		WCN_ERR("open file error\n");
		return -1;
	}
	WCN_INFO("open image "WCN_AFC_CALI_PATH" successfully\n");

	/* read file to buffer */
	size = sizeof(struct tsx_data);
	pdata = (char *)p_tsx_data;
	do {
		read_len = kernel_read(file, pdata, size, &offset);
		if (read_len > 0) {
			size -= read_len;
			pdata += read_len;
		}
	} while ((read_len > 0) && (size > 0));
	fput(file);
	WCN_INFO("After read, data =%p dac value %02x\n", pdata,
			 p_tsx_data->dac);

	return 0;
}

static u16 marlin_tsx_cali_data_get(void)
{
	int ret;

	WCN_INFO("tsx cali init flag %d\n", marlin_dev->tsxcali.init_flag);

	if (marlin_dev->tsxcali.init_flag == AFC_CALI_READ_FINISH)
		return marlin_dev->tsxcali.tsxdata.dac;

	ret = marlin_tsx_cali_data_read(&marlin_dev->tsxcali.tsxdata);
	marlin_dev->tsxcali.init_flag = AFC_CALI_READ_FINISH;
	if (ret != 0) {
		marlin_dev->tsxcali.tsxdata.dac = 0xffff;
		WCN_INFO("tsx cali read fail! default 0xffff\n");
		return marlin_dev->tsxcali.tsxdata.dac;
	}

	if (marlin_dev->tsxcali.tsxdata.flag != AFC_CALI_FLAG) {
		marlin_dev->tsxcali.tsxdata.dac = 0xffff;
		WCN_INFO("tsx cali flag fail! default 0xffff\n");
		return marlin_dev->tsxcali.tsxdata.dac;
	}
	WCN_INFO("dac flag %d value:0x%x\n",
			    marlin_dev->tsxcali.tsxdata.flag,
			    marlin_dev->tsxcali.tsxdata.dac);

	return marlin_dev->tsxcali.tsxdata.dac;
}

static int marlin_judge_imagepack(char *buffer)
{
	struct head *imghead;

	if (buffer == NULL)
		return -1;

	imghead = (struct head *)buffer;

	return strncmp(IMG_HEAD_MAGIC, imghead->magic, 4);
}


static struct imageinfo *marlin_judge_images(char *buffer)
{

	struct imageinfo *imginfo = NULL;
	unsigned char *magic_str;

	magic_str = wcn_get_chip_tag();
	if (!magic_str) {
		WCN_ERR("%s chip id erro\n", __func__);
		return NULL;
	}

	imginfo = kzalloc(sizeof(*imginfo), GFP_KERNEL);
	if (!imginfo) {
		WCN_ERR("%s no memory\n", __func__);
		return NULL;
	}
	memcpy(imginfo, (buffer + sizeof(struct head)),
	       sizeof(*imginfo));

	if (!strncmp(magic_str, imginfo->tag, 4)) {
		WCN_INFO("%s: marlin imginfo1 type is %s\n",
			 __func__, magic_str);
		return imginfo;
	}
	memcpy(imginfo, buffer + sizeof(*imginfo) + sizeof(struct head),
	       sizeof(*imginfo));
	if (!strncmp(magic_str, imginfo->tag, 4)) {
		WCN_INFO("%s: marlin imginfo2 type is %s\n",
			 __func__, magic_str);
		return imginfo;
	}

	WCN_ERR("Marlin can't find marlin chip image!!!\n");
	kfree(imginfo);

	return  NULL;
}

static char *btwf_load_firmware_data(loff_t off, unsigned long int imag_size)
{
	int read_len, size, i, opn_num_max = 15;
	char *buffer = NULL;
	char *data = NULL;
	struct file *file;
	loff_t offset = 0, pos = 0;

	WCN_LOG("%s entry\n", __func__);

	file = filp_open(BTWF_FIRMWARE_PATH, O_RDONLY, 0);
	for (i = 1; i <= opn_num_max; i++) {
		if (IS_ERR(file)) {
			WCN_INFO("try open file %s,count_num:%d,%s\n",
				BTWF_FIRMWARE_PATH, i, __func__);
			ssleep(1);
			file = filp_open(BTWF_FIRMWARE_PATH, O_RDONLY, 0);
		} else {
			break;
		}
	}
	if (IS_ERR(file)) {
		WCN_ERR("%s open file %s error\n",
			BTWF_FIRMWARE_PATH, __func__);
		return NULL;
	}
	WCN_LOG("marlin %s open image file  successfully\n",
		__func__);
	size = imag_size;
	buffer = vmalloc(size);
	if (!buffer) {
		fput(file);
		WCN_ERR("no memory for image\n");
		return NULL;
	}

	read_len = kernel_read(file, functionmask, 8, &pos);
	if ((functionmask[0] == 0x00) && (functionmask[1] == 0x00))
		offset = offset + 8;
	else
		functionmask[7] = 0;

	data = buffer;
	offset += off;
	do {
		read_len = kernel_read(file, buffer, size, &offset);
		if (read_len > 0) {
			size -= read_len;
			buffer += read_len;
		}
	} while ((read_len > 0) && (size > 0));
	fput(file);
	WCN_INFO("%s finish read_Len:%d\n", __func__, read_len);
	if (read_len <= 0) {
		vfree(buffer);
		return NULL;
	}

	return data;
}

static int marlin_download_from_partition(void)
{
	int err, len, trans_size, ret;
	unsigned long int img_size;
	char *buffer = NULL;
	char *temp = NULL;
	struct imageinfo *imginfo = NULL;

	img_size = FIRMWARE_MAX_SIZE;

	WCN_INFO("%s entry\n", __func__);
	buffer = btwf_load_firmware_data(0, img_size);
	if (!buffer) {
		WCN_INFO("%s buff is NULL\n", __func__);
		return -1;
	}
	temp = buffer;

	ret = marlin_judge_imagepack(buffer);
	if (!ret) {
		WCN_INFO("marlin %s imagepack is WCNM type,need parse it\n",
			__func__);
		marlin_get_wcn_chipid();

		imginfo = marlin_judge_images(buffer);
		vfree(temp);
		if (!imginfo) {
			WCN_ERR("marlin:%s imginfo is NULL\n", __func__);
			return -1;
		}
		img_size = imginfo->size;
		if (img_size > FIRMWARE_MAX_SIZE)
			WCN_INFO("%s real size %ld is large than the max:%d\n",
				 __func__, img_size, FIRMWARE_MAX_SIZE);
		buffer = btwf_load_firmware_data(imginfo->offset, img_size);
		if (!buffer) {
			WCN_ERR("marlin:%s buffer is NULL\n", __func__);
			kfree(imginfo);
			return -1;
		}
		temp = buffer;
		kfree(imginfo);
	}

	len = 0;
	while (len < img_size) {
		trans_size = (img_size - len) > PACKET_SIZE ?
				PACKET_SIZE : (img_size - len);
		memcpy(marlin_dev->write_buffer, buffer + len, trans_size);
		err = sprdwcn_bus_direct_write(CP_START_ADDR + len,
			marlin_dev->write_buffer, trans_size);
		if (err < 0) {
			WCN_ERR(" %s: dt write SDIO error:%d\n", __func__, err);
			vfree(temp);
			return -1;
		}
		len += PACKET_SIZE;
	}
	vfree(temp);
	WCN_INFO("%s finish and successful\n", __func__);

	return 0;
}

int wcn_gnss_ops_register(struct sprdwcn_gnss_ops *ops)
{
	if (gnss_ops) {
		WARN_ON(1);
		return -EBUSY;
	}

	gnss_ops = ops;

	return 0;
}

void wcn_gnss_ops_unregister(void)
{
	gnss_ops = NULL;
}

static char *gnss_load_firmware_data(unsigned long int imag_size)
{
	int read_len, size, i, opn_num_max = 15;
	char *buffer = NULL;
	char *data = NULL;
	struct file *file;
	loff_t pos = 0;

	WCN_LOG("%s entry\n", __func__);
	if (gnss_ops && (gnss_ops->set_file_path))
		gnss_ops->set_file_path(&GNSS_FIRMWARE_PATH[0]);
	else
		WCN_ERR("%s gnss_ops set_file_path error\n", __func__);
	file = filp_open(GNSS_FIRMWARE_PATH, O_RDONLY, 0);
	for (i = 1; i <= opn_num_max; i++) {
		if (IS_ERR(file)) {
			WCN_INFO("try open file %s,count_num:%d,errno=%ld,%s\n",
				 GNSS_FIRMWARE_PATH, i,
				 PTR_ERR(file), __func__);
			if (PTR_ERR(file) == -ENOENT)
				WCN_ERR("No such file or directory\n");
			if (PTR_ERR(file) == -EACCES)
				WCN_ERR("Permission denied\n");
			ssleep(1);
			file = filp_open(GNSS_FIRMWARE_PATH, O_RDONLY, 0);
		} else {
			break;
		}
	}

	if (IS_ERR(file)) {
		WCN_ERR("%s marlin3 gnss open file %s error\n",
			GNSS_FIRMWARE_PATH, __func__);
		return NULL;
	}
	WCN_LOG("%s open image file  successfully\n", __func__);
	size = imag_size;
	buffer = vmalloc(size);
	if (!buffer) {
		fput(file);
		WCN_ERR("no memory for gnss img\n");
		return NULL;
	}

	data = buffer;
	do {
		read_len = kernel_read(file, buffer, size, &pos);
		if (read_len > 0) {
			size -= read_len;
			buffer += read_len;
		}
	} while ((read_len > 0) && (size > 0));
	fput(file);
	WCN_INFO("%s finish read_Len:%d\n", __func__, read_len);

	if (read_len <= 0) {
		vfree(data);
		return NULL;
	}

	return data;
}

static int gnss_download_from_partition(void)
{
	int err, len, trans_size;
	unsigned long int imgpack_size, img_size;
	char *buffer = NULL;
	char *temp = NULL;

	img_size = imgpack_size =  GNSS_FIRMWARE_MAX_SIZE;

	WCN_INFO("GNSS %s entry\n", __func__);
	temp = buffer = gnss_load_firmware_data(imgpack_size);
	if (!buffer) {
		WCN_INFO("%s gnss buff is NULL\n", __func__);
		return -1;
	}

	len = 0;
	while (len < img_size) {
		trans_size = (img_size - len) > PACKET_SIZE ?
				PACKET_SIZE : (img_size - len);
		memcpy(marlin_dev->write_buffer, buffer + len, trans_size);
		err = sprdwcn_bus_direct_write(GNSS_CP_START_ADDR + len,
			marlin_dev->write_buffer, trans_size);
		if (err < 0) {
			WCN_ERR("gnss dt write %s error:%d\n", __func__, err);
			vfree(temp);
			return -1;
		}
		len += PACKET_SIZE;
	}
	vfree(temp);
	WCN_INFO("%s gnss download firmware finish\n", __func__);

	return 0;
}

static int gnss_download_firmware(void)
{
	const struct firmware *firmware;
	char *buf;
	int err;
	int i, len, count, trans_size;

	if (marlin_dev->is_gnss_in_sysfs) {
		err = gnss_download_from_partition();
		return err;
	}

	WCN_INFO("%s start from /system/etc/firmware/\n", __func__);
	buf = marlin_dev->write_buffer;
	err = request_firmware_direct(&firmware, "gnssmodem.bin", NULL);
	if (err < 0) {
		WCN_ERR("%s no find gnssmodem.bin err:%d(ignore)\n",
			__func__, err);
		marlin_dev->is_gnss_in_sysfs = true;
		err = gnss_download_from_partition();

		return err;
	}
	count = (firmware->size + PACKET_SIZE - 1) / PACKET_SIZE;
	len = 0;
	for (i = 0; i < count; i++) {
		trans_size = (firmware->size - len) > PACKET_SIZE ?
				PACKET_SIZE : (firmware->size - len);
		memcpy(buf, firmware->data + len, trans_size);
		err = sprdwcn_bus_direct_write(GNSS_CP_START_ADDR + len, buf,
				trans_size);
		if (err < 0) {
			WCN_ERR("gnss dt write %s error:%d\n", __func__, err);
			release_firmware(firmware);

			return err;
		}
		len += trans_size;
	}
	release_firmware(firmware);
	WCN_INFO("%s successfully through request_firmware!\n", __func__);

	return 0;
}

/* BT WIFI FM download */
static int btwifi_download_firmware(void)
{
	const struct firmware *firmware;
	char *buf;
	int err;
	int i, len, count, trans_size;

	if (marlin_dev->is_btwf_in_sysfs) {
		err = marlin_download_from_partition();
		return err;
	}

	WCN_INFO("marlin %s from /system/etc/firmware/ start!\n", __func__);
	buf = marlin_dev->write_buffer;
	err = request_firmware_direct(&firmware, "wcnmodem.bin", NULL);
	if (err < 0) {
		WCN_ERR("no find wcnmodem.bin errno:(%d)(ignore!!)\n", err);
		marlin_dev->is_btwf_in_sysfs = true;
		err = marlin_download_from_partition();

		return err;
	}

	count = (firmware->size + PACKET_SIZE - 1) / PACKET_SIZE;
	len = 0;

	for (i = 0; i < count; i++) {
		trans_size = (firmware->size - len) > PACKET_SIZE ?
				PACKET_SIZE : (firmware->size - len);
		memcpy(buf, firmware->data + len, trans_size);
		WCN_INFO("download count=%d,len =%d,trans_size=%d\n", count,
			 len, trans_size);
		err = sprdwcn_bus_direct_write(CP_START_ADDR + len,
					       buf, trans_size);
		if (err < 0) {
			WCN_ERR("marlin dt write %s error:%d\n", __func__, err);
			release_firmware(firmware);
			return err;
		}
		len += trans_size;
	}

	release_firmware(firmware);
	WCN_INFO("marlin %s successfully!\n", __func__);

	return 0;
}

static int wcn_get_syscon_regmap(void)
{
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		WCN_ERR("unable to get syscon node\n");
		return -ENODEV;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		WCN_ERR("unable to get syscon platform device\n");
		return -ENODEV;
	}

	marlin_dev->syscon_pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!marlin_dev->syscon_pmic)
		WCN_ERR("unable to get pmic regmap device\n");

	of_node_put(regmap_np);

	return 0;
}

static void wcn_get_pmic_config(struct device_node *np)
{
	int ret;
	struct wcn_pmic_config *pmic;

	if (wcn_get_syscon_regmap())
		return;

	pmic = &marlin_dev->avdd12_parent_bound_chip;
	strcpy(pmic->name, "avdd12-parent-bound-chip");
	ret = of_property_read_u32_array(np, pmic->name,
					 (u32 *)pmic->config,
					 WCN_BOUND_CONFIG_NUM);
	pmic->enable = !ret;
	WCN_INFO("vddgen1-bound-chip config enable:%d\n", pmic->enable);

	pmic = &marlin_dev->avdd12_bound_wbreq;
	strcpy(pmic->name, "avdd12-bound-wbreq");
	ret = of_property_read_u32_array(np, pmic->name,
					 (u32 *)pmic->config,
					 WCN_BOUND_CONFIG_NUM);
	pmic->enable = !ret;
	WCN_INFO("avdd12-bound-wbreq config status:%d\n", pmic->enable);

	pmic = &marlin_dev->avdd33_bound_wbreq;
	strcpy(pmic->name, "avdd33-bound-wbreq");
	ret = of_property_read_u32_array(np, pmic->name,
					 (u32 *)pmic->config,
					 WCN_BOUND_CONFIG_NUM);
	pmic->enable = !ret;
	WCN_INFO("avdd33-bound-wbreq config status:%d\n", pmic->enable);
}

static int wcn_pmic_do_bound(struct wcn_pmic_config *pmic, bool bound)
{
	int ret;
	u32 *chip;

	if (!marlin_dev->syscon_pmic || !pmic->enable)
		return -1;

	chip = pmic->config;

	if (bound) {
		WCN_INFO("%s bound\n", pmic->name);
		ret = regmap_update_bits(marlin_dev->syscon_pmic,
					 chip[0], chip[1], chip[3]);
		if (ret)
			WCN_ERR("%s bound:%d\n", pmic->name, ret);
	} else {
		WCN_INFO("%s unbound\n", pmic->name);
		ret = regmap_update_bits(marlin_dev->syscon_pmic,
					 chip[0], chip[1], chip[2]);
		if (ret)
			WCN_ERR("%s unbound:%d\n", pmic->name, ret);
	}
	usleep_range(1000, 2000);

	return 0;
}

static inline int wcn_avdd12_parent_bound_chip(bool enable)
{
	return wcn_pmic_do_bound(&marlin_dev->avdd12_parent_bound_chip, enable);
}

static inline int wcn_avdd12_bound_xtl(bool enable)
{
	return wcn_pmic_do_bound(&marlin_dev->avdd12_bound_wbreq, enable);
}

/* wifipa bound XTLEN3, gnss not need wifipa bound */
static inline int wcn_wifipa_bound_xtl(bool enable)
{
	return wcn_pmic_do_bound(&marlin_dev->avdd33_bound_wbreq, enable);
}

static int marlin_parse_dt(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct regmap *pmu_apb_gpr;
	int ret;
	char *buf;
	struct wcn_clock_info *clk;

	if (!marlin_dev)
		return -1;

	wcn_get_pmic_config(np);

	marlin_dev->wakeup_ap = of_get_named_gpio(np,
			"m2-wakeup-ap-gpios", 0);
	if (!gpio_is_valid(marlin_dev->wakeup_ap))
		WCN_INFO("can not get wakeup gpio\n");

	marlin_dev->reset = of_get_named_gpio(np,
			"reset-gpios", 0);
	if (!gpio_is_valid(marlin_dev->reset))
		return -EINVAL;

	marlin_dev->chip_en = of_get_named_gpio(np,
			"enable-gpios", 0);
	if (!gpio_is_valid(marlin_dev->chip_en))
		return -EINVAL;

	marlin_dev->int_ap = of_get_named_gpio(np,
			"m2-to-ap-irq-gpios", 0);
	if (!gpio_is_valid(marlin_dev->int_ap)) {
		WCN_ERR("Get int irq error!\n");
		return -EINVAL;
	}

	clk = &marlin_dev->clk_xtal_26m;
	clk->gpio = of_get_named_gpio(np, "xtal-26m-clk-type-gpio", 0);
	if (!gpio_is_valid(clk->gpio))
		WCN_INFO("xtal-26m-clk gpio not config\n");

	/* xtal-26m-clk-type has priority over than xtal-26m-clk-type-gpio */
	ret = of_property_read_string(np, "xtal-26m-clk-type",
				      (const char **)&buf);
	if (!ret) {
		WCN_INFO("force config xtal 26m clk %s\n", buf);
		if (!strncmp(buf, "TCXO", 4))
			clk->type = WCN_CLOCK_TYPE_TCXO;
		else if (!strncmp(buf, "TSX", 3))
			clk->type = WCN_CLOCK_TYPE_TSX;
		else
			WCN_ERR("force config xtal 26m clk %s err!\n", buf);
	} else {
		if (clktype == 0) {
			WCN_INFO("cmd config clk TCXO\n");
			clk->type = WCN_CLOCK_TYPE_TCXO;
		} else if (clktype == 1) {
			WCN_INFO("cmd config clk TSX\n");
			clk->type = WCN_CLOCK_TYPE_TSX;
		} else {
			WCN_INFO("may be not config clktype:%d\n", clktype);
			clk->type = WCN_CLOCK_TYPE_UNKNOWN;
		}
	}

	marlin_dev->dvdd12 = devm_regulator_get(&pdev->dev, "dvdd12");
	if (IS_ERR(marlin_dev->dvdd12)) {
		WCN_ERR("Get regulator of dvdd12 error!\n");
		WCN_ERR("Maybe share the power with mem\n");
	}

	if (of_property_read_bool(np, "bound-avdd12")) {
		WCN_INFO("forbid avdd12 power ctrl\n");
		marlin_dev->bound_avdd12 = true;
	} else {
		WCN_INFO("do avdd12 power ctrl\n");
		marlin_dev->bound_avdd12 = false;
	}

	marlin_dev->avdd12 = devm_regulator_get(&pdev->dev, "avdd12");
	if (IS_ERR(marlin_dev->avdd12)) {
		WCN_ERR("avdd12 err =%ld\n", PTR_ERR(marlin_dev->avdd12));
		if (PTR_ERR(marlin_dev->avdd12) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		WCN_ERR("Get regulator of avdd12 error!\n");
	}

	marlin_dev->avdd33 = devm_regulator_get(&pdev->dev, "avdd33");
	if (IS_ERR(marlin_dev->avdd33)) {
		if (PTR_ERR(marlin_dev->avdd33) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		WCN_ERR("Get regulator of avdd33 error!\n");
	}

	marlin_dev->dcxo18 = devm_regulator_get(&pdev->dev, "dcxo18");
	if (IS_ERR(marlin_dev->dcxo18)) {
		if (PTR_ERR(marlin_dev->dcxo18) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		WCN_ERR("Get regulator of dcxo18 error!\n");
	}

	if (of_property_read_bool(np, "bound-dcxo18")) {
		WCN_INFO("forbid dcxo18 power ctrl\n");
		marlin_dev->bound_dcxo18 = true;
	} else {
		WCN_INFO("do dcxo18 power ctrl\n");
		marlin_dev->bound_dcxo18 = false;
	}

	marlin_dev->clk_32k = devm_clk_get(&pdev->dev, "clk_32k");
	if (IS_ERR(marlin_dev->clk_32k)) {
		WCN_ERR("can't get wcn clock dts config: clk_32k\n");
		return -1;
	}

	marlin_dev->clk_parent = devm_clk_get(&pdev->dev, "source");
	if (IS_ERR(marlin_dev->clk_parent)) {
		WCN_ERR("can't get wcn clock dts config: source\n");
		return -1;
	}
	clk_set_parent(marlin_dev->clk_32k, marlin_dev->clk_parent);

	marlin_dev->clk_enable = devm_clk_get(&pdev->dev, "enable");
	if (IS_ERR(marlin_dev->clk_enable)) {
		WCN_ERR("can't get wcn clock dts config: enable\n");
		return -1;
	}

	ret = gpio_request(marlin_dev->reset, "reset");
	if (ret)
		WCN_ERR("gpio reset request err: %d\n",
				marlin_dev->reset);

	ret = gpio_request(marlin_dev->chip_en, "chip_en");
	if (ret)
		WCN_ERR("gpio_rst request err: %d\n",
				marlin_dev->chip_en);

	ret = gpio_request(marlin_dev->int_ap, "int_ap");
	if (ret)
		WCN_ERR("gpio_rst request err: %d\n",
				marlin_dev->int_ap);

	if (gpio_is_valid(clk->gpio)) {
		ret = gpio_request(clk->gpio, "wcn_xtal_26m_type");
		if (ret)
			WCN_ERR("xtal 26m gpio request err: %d\n", ret);
	}

	WCN_INFO("BTWF_FIRMWARE_PATH len=%ld\n",
		 (long)strlen(BTWF_FIRMWARE_PATH));
	ret = of_property_read_string(np, "sprd,btwf-file-name",
				      (const char **)&marlin_dev->btwf_path);
	if (!ret) {
		WCN_INFO("btwf firmware name:%s\n", marlin_dev->btwf_path);
		strcpy(BTWF_FIRMWARE_PATH, marlin_dev->btwf_path);
		WCN_INFO("BTWG path is %s\n", BTWF_FIRMWARE_PATH);
	}

	WCN_INFO("BTWF_FIRMWARE_PATH2 len=%ld\n",
		 (long)strlen(BTWF_FIRMWARE_PATH));

	ret = of_property_read_string(np, "sprd,gnss-file-name",
				      (const char **)&marlin_dev->gnss_path);
	if (!ret) {
		WCN_INFO("gnss firmware name:%s\n", marlin_dev->gnss_path);
		strcpy(GNSS_FIRMWARE_PATH, marlin_dev->gnss_path);
	}

	if (of_property_read_bool(np, "keep-power-on")) {
		WCN_INFO("wcn config keep power on\n");
		marlin_dev->keep_power_on = true;
	}

	if (of_property_read_bool(np, "wait-ge2")) {
		WCN_INFO("wait-ge2 need wait gps ready\n");
		marlin_dev->wait_ge2 = true;
	}

	pmu_apb_gpr = syscon_regmap_lookup_by_phandle(np,
				"sprd,syscon-pmu-apb");
	if (IS_ERR(pmu_apb_gpr)) {
		WCN_ERR("%s:failed to find pmu_apb_gpr(26M)(ignore)\n",
				__func__);
		return -EINVAL;
	}
	ret = regmap_read(pmu_apb_gpr, REG_PMU_APB_XTL_WAIT_CNT0,
					&clk_wait_val);
	WCN_INFO("marlin2 clk_wait value is 0x%x\n", clk_wait_val);

	ret = of_property_read_u32(np, "sprd,reg-m2-apb-xtl-wait-addr",
			&marlin2_clk_wait_reg);
	if (ret) {
		WCN_ERR("Did not find reg-m2-apb-xtl-wait-addr\n");
		return -EINVAL;
	}
	WCN_INFO("marlin2 clk reg is 0x%x\n", marlin2_clk_wait_reg);

	return 0;
}

static int marlin_gpio_free(struct platform_device *pdev)
{
	if (!marlin_dev)
		return -1;

	gpio_free(marlin_dev->reset);
	gpio_free(marlin_dev->chip_en);
	gpio_free(marlin_dev->int_ap);
	if (!gpio_is_valid(marlin_dev->clk_xtal_26m.gpio))
		gpio_free(marlin_dev->clk_xtal_26m.gpio);

	return 0;
}

static int marlin_clk_enable(bool enable)
{
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(marlin_dev->clk_32k);
		ret = clk_prepare_enable(marlin_dev->clk_enable);
		WCN_INFO("marlin %s successfully!\n", __func__);
	} else {
		clk_disable_unprepare(marlin_dev->clk_enable);
		clk_disable_unprepare(marlin_dev->clk_32k);
	}

	return ret;
}

static int marlin_avdd18_dcxo_enable(bool enable)
{
	int ret = 0;

	if (!marlin_dev->dcxo18)
		return 0;

	if (enable) {
#ifndef CONFIG_WCN_PCIE
		if (!marlin_dev->bound_dcxo18 &&
		    regulator_is_enabled(marlin_dev->dcxo18)) {
			WCN_INFO("avdd18_dcxo 1v8 have enable\n");
			return 0;
		}
#endif
		WCN_INFO("avdd18_dcxo set 1v8\n");
		regulator_set_voltage(marlin_dev->dcxo18, 1800000, 1800000);
		if (!marlin_dev->bound_dcxo18) {
			WCN_INFO("avdd18_dcxo power enable\n");
			ret = regulator_enable(marlin_dev->dcxo18);
			if (ret)
				WCN_ERR("fail to enable avdd18_dcxo\n");
		}
	} else {
		if (!marlin_dev->bound_dcxo18 &&
		    regulator_is_enabled(marlin_dev->dcxo18)) {
			WCN_INFO("avdd18_dcxo power disable\n");
			ret = regulator_disable(marlin_dev->dcxo18);
			if (ret)
				WCN_ERR("fail to disable avdd18_dcxo\n");
		}
	}

	return ret;
}

static int marlin_digital_power_enable(bool enable)
{
	int ret = 0;

	WCN_INFO("%s D1v2 %d\n", __func__, enable);
	if (marlin_dev->dvdd12 == NULL)
		return 0;

	if (enable) {
		regulator_set_voltage(marlin_dev->dvdd12,
					      1200000, 1200000);
		ret = regulator_enable(marlin_dev->dvdd12);
	} else {
		if (regulator_is_enabled(marlin_dev->dvdd12))
			ret = regulator_disable(marlin_dev->dvdd12);
	}

	return ret;
}

static int marlin_analog_power_enable(bool enable)
{
	int ret = 0;

	if (marlin_dev->avdd12 != NULL) {
		usleep_range(4000, 5000);
		if (enable) {
#ifdef CONFIG_WCN_PCIE
			WCN_INFO("%s avdd12 set 1.35v\n", __func__);
			regulator_set_voltage(marlin_dev->avdd12,
					      1350000, 1350000);
#else
			WCN_INFO("%s avdd12 set 1.2v\n", __func__);
			regulator_set_voltage(marlin_dev->avdd12,
					      1200000, 1200000);
#endif
			if (!marlin_dev->bound_avdd12) {
				WCN_INFO("%s avdd12 power enable\n", __func__);
				ret = regulator_enable(marlin_dev->avdd12);
				if (ret)
					WCN_ERR("fail to enalbe avdd12\n");
			}
		} else {
			if (!marlin_dev->bound_avdd12 &&
			    regulator_is_enabled(marlin_dev->avdd12)) {
				WCN_INFO("%s avdd12 power disable\n", __func__);
				ret = regulator_disable(marlin_dev->avdd12);
				if (ret)
					WCN_ERR("fail to disable avdd12\n");
			}
		}
	}

	return ret;
}

/*
 * hold cpu means cpu register is clear
 * different from reset pin gpio
 * reset gpio is all register is clear
 */
void marlin_hold_cpu(void)
{
	int ret = 0;
	unsigned int temp_reg_val;

	ret = sprdwcn_bus_reg_read(CP_RESET_REG, &temp_reg_val, 4);
	if (ret < 0) {
		WCN_ERR("%s read reset reg error:%d\n", __func__, ret);
		return;
	}
	WCN_INFO("%s reset reg val:0x%x\n", __func__, temp_reg_val);
	temp_reg_val |= 1;
	ret = sprdwcn_bus_reg_write(CP_RESET_REG, &temp_reg_val, 4);
	if (ret < 0) {
		WCN_ERR("%s write reset reg error:%d\n", __func__, ret);
		return;
	}
}

void marlin_read_cali_data(void)
{
	int err;

	WCN_INFO("marlin sync entry is_calibrated:%d\n",
		wifi_data.cali_data.cali_config.is_calibrated);

	if (!wifi_data.cali_data.cali_config.is_calibrated) {
		memset(&wifi_data.cali_data, 0x0,
			sizeof(struct wifi_cali_t));
		err = sprdwcn_bus_reg_read(CALI_OFSET_REG,
			&wifi_data.cali_data, sizeof(struct wifi_cali_t));
		if (err < 0) {
			WCN_ERR("marlin read cali data fail:%d\n", err);
			return;
		}
	}

	if ((marlin2_clk_wait_reg > 0) && (clk_wait_val > 0)) {
		sprdwcn_bus_reg_read(marlin2_clk_wait_reg,
					&cp_clk_wait_val, 4);
		WCN_INFO("marlin2 cp_clk_wait_val is 0x%x\n", cp_clk_wait_val);
		clk_wait_val = ((clk_wait_val & 0xFF00) >> 8);
		cp_clk_wait_val =
			((cp_clk_wait_val & 0xFFFFFC00) | clk_wait_val);
		WCN_INFO("marlin2 cp_clk_wait_val is modifyed 0x%x\n",
					cp_clk_wait_val);
		err = sprdwcn_bus_reg_write(marlin2_clk_wait_reg,
					       &cp_clk_wait_val, 4);
		if (err < 0)
			WCN_ERR("marlin2 write 26M error:%d\n", err);
	}

	/* write this flag to notify cp that ap read calibration data */
	reg_val = 0xbbbbbbbb;
	err = sprdwcn_bus_reg_write(CALI_REG, &reg_val, 4);
	if (err < 0) {
		WCN_ERR("marlin write cali finish error:%d\n", err);
		return;
	}

	sprdwcn_bus_runtime_get();

	complete(&marlin_dev->download_done);
}

#ifndef CONFIG_WCN_PCIE
static int marlin_write_cali_data(void)
{
	int i;
	int ret = 0, init_state = 0, cali_data_offset = 0;

	WCN_INFO("tsx_dac_data:%d\n", marlin_dev->tsxcali.tsxdata.dac);
	cali_data_offset = (unsigned long)(&(marlin_dev->sync_f.tsx_dac_data))
		- (unsigned long)(&(marlin_dev->sync_f));
	WCN_INFO("cali_data_offset:0x%x\n", cali_data_offset);

	for (i = 0; i <= 65; i++) {
		ret = sprdwcn_bus_reg_read(SYNC_ADDR, &init_state, 4);
		if (ret < 0) {
			WCN_ERR("%s marlin3 read SYNC_ADDR error:%d\n",
				__func__, ret);
			return ret;
		}

		if (init_state != SYNC_CALI_WAITING)
			usleep_range(3000, 5000);
		/* wait cp in the state of waiting cali data */
		else {
			/* write cali data to cp */
			marlin_dev->sync_f.tsx_dac_data =
					marlin_dev->tsxcali.tsxdata.dac;
			ret = sprdwcn_bus_direct_write(SYNC_ADDR +
					cali_data_offset,
					&(marlin_dev->sync_f.tsx_dac_data), 2);
			if (ret < 0) {
				WCN_ERR("write cali data error:%d\n", ret);
				return ret;
			}

			/* tell cp2 can handle cali data */
			init_state = SYNC_CALI_WRITE_DONE;
			ret = sprdwcn_bus_reg_write(SYNC_ADDR, &init_state, 4);
			if (ret < 0) {
				WCN_ERR("write cali_done flag error:%d\n", ret);
				return ret;
			}

			WCN_INFO("marlin_write_cali_data finish\n");
			return ret;
		}
	}

	WCN_ERR("%s sync init_state:0x%x\n", __func__, init_state);

	return -1;
}
#endif

enum wcn_clock_type wcn_get_xtal_26m_clk_type(void)
{
	return marlin_dev->clk_xtal_26m.type;
}
EXPORT_SYMBOL_GPL(wcn_get_xtal_26m_clk_type);

enum wcn_clock_mode wcn_get_xtal_26m_clk_mode(void)
{
	return marlin_dev->clk_xtal_26m.mode;
}
EXPORT_SYMBOL_GPL(wcn_get_xtal_26m_clk_mode);

#ifndef CONFIG_WCN_PCIE
static int spi_read_rf_reg(unsigned int addr, unsigned int *data)
{
	unsigned int reg_data = 0;
	int ret;

	reg_data = ((addr & 0x7fff) << 16) | SPI_BIT31;
	ret = sprdwcn_bus_reg_write(SPI_BASE_ADDR, &reg_data, 4);
	if (ret < 0) {
		WCN_ERR("write SPI RF reg error:%d\n", ret);
		return ret;
	}

	usleep_range(4000, 6000);

	ret = sprdwcn_bus_reg_read(SPI_BASE_ADDR, &reg_data, 4);
	if (ret < 0) {
		WCN_ERR("read SPI RF reg error:%d\n", ret);
		return ret;
	}
	*data = reg_data & 0xffff;

	return 0;
}

static void wcn_check_xtal_26m_clk(void)
{
	int ret = 0;
	unsigned int temp_val;
	struct wcn_clock_info *clk;

	clk = &marlin_dev->clk_xtal_26m;
	if (likely(clk->type != WCN_CLOCK_TYPE_UNKNOWN) &&
	    likely(clk->mode != WCN_CLOCK_MODE_UNKNOWN)) {
		WCN_INFO("xtal 26m clk type:%s mode:%s\n",
			 (clk->type == WCN_CLOCK_TYPE_TSX) ? "TSX" : "TCXO",
			 (clk->mode == WCN_CLOCK_MODE_XO) ? "XO" : "BUFFER");
		return;
	}

	if (clk->type == WCN_CLOCK_TYPE_UNKNOWN) {
		if (gpio_is_valid(clk->gpio)) {
			gpio_direction_input(clk->gpio);
			ret = gpio_get_value(clk->gpio);
			clk->type = ret ? WCN_CLOCK_TYPE_TSX :
				    WCN_CLOCK_TYPE_TCXO;
			WCN_INFO("xtal gpio clk type:%d %d\n",
				 clk->type, ret);
		} else {
			WCN_ERR("xtal_26m clk type erro!\n");
		}
	}

	if (clk->mode == WCN_CLOCK_MODE_UNKNOWN) {
		ret = spi_read_rf_reg(AD_DCXO_BONDING_OPT, &temp_val);
		if (ret < 0) {
			WCN_ERR("read AD_DCXO_BONDING_OPT error:%d\n", ret);
			return;
		}
		WCN_INFO("read AD_DCXO_BONDING_OPT val:0x%x\n", temp_val);
		if (temp_val & WCN_BOUND_XO_MODE) {
			WCN_INFO("xtal_26m clock XO mode\n");
			clk->mode = WCN_CLOCK_MODE_XO;
		} else {
			WCN_INFO("xtal_26m clock Buffer mode\n");
			clk->mode = WCN_CLOCK_MODE_BUFFER;
		}
	}
}

static int check_cp_clock_mode(void)
{
	struct wcn_clock_info *clk;

	WCN_INFO("%s\n", __func__);

	clk = &marlin_dev->clk_xtal_26m;
	if (clk->mode == WCN_CLOCK_MODE_BUFFER) {
		WCN_INFO("xtal_26m clock use BUFFER mode\n");
		marlin_avdd18_dcxo_enable(false);
		return 0;
	} else if (clk->mode == WCN_CLOCK_MODE_XO) {
		WCN_INFO("xtal_26m clock use XO mode\n");
		return 0;
	}

	return -1;
}
#endif

/* release CPU */
static int marlin_start_run(void)
{
	int ret = 0;
	unsigned int ss_val;

	WCN_INFO("%s\n", __func__);

	marlin_tsx_cali_data_get();
#ifdef CONFIG_WCN_SLP
	sdio_pub_int_btwf_en0();
	/* after chip power on, reset sleep status */
	slp_mgr_reset();
#endif

	ret = sprdwcn_bus_reg_read(CP_RESET_REG, &ss_val, 4);
	if (ret < 0) {
		WCN_ERR("%s read reset reg error:%d\n", __func__, ret);
		return ret;
	}
	WCN_INFO("%s read reset reg val:0x%x\n", __func__, ss_val);
	ss_val &= (~0) - 1;
	WCN_INFO("after do %s reset reg val:0x%x\n", __func__, ss_val);
	ret = sprdwcn_bus_reg_write(CP_RESET_REG, &ss_val, 4);
	if (ret < 0) {
		WCN_ERR("%s write reset reg error:%d\n", __func__, ret);
		return ret;
	}
	/* update the time at once */
	marlin_bootup_time_update();

	ret = sprdwcn_bus_reg_read(CP_RESET_REG, &ss_val, 4);
	if (ret < 0) {
		WCN_ERR("%s read reset reg error:%d\n", __func__, ret);
		return ret;
	}
	WCN_INFO("%s reset reg val:0x%x\n", __func__, ss_val);

	return ret;
}

/* return 0 is ready, other values is error */
static int check_cp_ready(void)
{
	int i, ret = 0;

	for (i = 0; i <= 25; i++) {
		ret = sprdwcn_bus_direct_read(SYNC_ADDR,
			&(marlin_dev->sync_f), sizeof(struct wcn_sync_info_t));
		if (ret < 0) {
			WCN_ERR("%s marlin3 read SYNC_ADDR error:%d\n",
				__func__, ret);
			return ret;
		}
		if (marlin_dev->sync_f.init_status == SYNC_IN_PROGRESS)
			usleep_range(3000, 5000);
		if (marlin_dev->sync_f.init_status == SYNC_ALL_FINISHED)
			return 0;
	}

	WCN_ERR("%s sync val:0x%x, prj_type val:0x%x\n", __func__,
		marlin_dev->sync_f.init_status,
		marlin_dev->sync_f.prj_type);

	return -1;
}

static int gnss_start_run(void)
{
	int ret = 0;
	unsigned int temp;

	WCN_INFO("gnss start run enter ");
#ifdef CONFIG_WCN_SLP
	sdio_pub_int_gnss_en0();
#endif
	ret = sprdwcn_bus_reg_read(GNSS_CP_RESET_REG, &temp, 4);
	if (ret < 0) {
		WCN_ERR("%s marlin3_gnss read reset reg error:%d\n",
			__func__, ret);
		return ret;
	}
	WCN_INFO("%s reset reg val:0x%x\n", __func__, temp);
	temp &= (~0) - 1;
	ret = sprdwcn_bus_reg_write(GNSS_CP_RESET_REG, &temp, 4);
	if (ret < 0) {
		WCN_ERR("%s marlin3_gnss write reset reg error:%d\n",
				__func__, ret);
		return ret;
	}

	return ret;
}

static int marlin_reset(int val)
{
	if (gpio_is_valid(marlin_dev->reset)) {
		gpio_direction_output(marlin_dev->reset, 0);
		mdelay(RESET_DELAY);
		gpio_direction_output(marlin_dev->reset, 1);
	}

	return 0;
}

static int chip_reset_release(int val)
{

	if (!gpio_is_valid(marlin_dev->reset)) {
		WCN_ERR("reset gpio error\n");
		return -1;
	}
	if (val)
		gpio_direction_output(marlin_dev->reset, 1);

	else
		gpio_direction_output(marlin_dev->reset, 0);

	return 0;
}

void marlin_chip_en(bool enable, bool reset)
{

	if (gpio_is_valid(marlin_dev->chip_en)) {
		if (reset) {
			gpio_direction_output(marlin_dev->chip_en, 0);
			WCN_INFO("marlin gnss chip en reset\n");
			msleep(100);
			gpio_direction_output(marlin_dev->chip_en, 1);
		} else if (enable) {
				gpio_direction_output(marlin_dev->chip_en, 0);
				mdelay(1);
				gpio_direction_output(marlin_dev->chip_en, 1);
				mdelay(1);
				WCN_INFO("marlin chip en pull up\n");
		} else {
				gpio_direction_output(marlin_dev->chip_en, 0);
				WCN_INFO("marlin chip en pull down\n");
			}
		}
}
EXPORT_SYMBOL_GPL(marlin_chip_en);

int set_cp_mem_status(int subsys, int val)
{
	int ret;
	unsigned int temp_val;

#if defined(CONFIG_UMW2652) || defined(CONFIG_WCN_PCIE)
	return 0;
#endif
	ret = sprdwcn_bus_reg_read(REG_WIFI_MEM_CFG1, &temp_val, 4);
	if (ret < 0) {
		WCN_ERR("%s read wifimem_cfg1 error:%d\n", __func__, ret);
		return ret;
	}
	WCN_INFO("%s read btram poweron(bit22)val:0x%x\n", __func__, temp_val);

	if ((subsys == MARLIN_BLUETOOTH) && (val == 1)) {
		temp_val = temp_val & (~FORCE_SHUTDOWN_BTRAM);
		WCN_INFO("wr btram poweron(bit22) val:0x%x\n", temp_val);
		ret = sprdwcn_bus_reg_write(REG_WIFI_MEM_CFG1, &temp_val, 4);
		if (ret < 0) {
			WCN_ERR("write wifimem_cfg1 reg error:%d\n", ret);
			return ret;
		}
		return 0;
	} else if (test_bit(MARLIN_BLUETOOTH, &marlin_dev->power_state) &&
		   (subsys != MARLIN_BLUETOOTH))
		return 0;

	temp_val = temp_val | FORCE_SHUTDOWN_BTRAM;
	WCN_INFO(" shut down btram(bit22) val:0x%x\n", temp_val);
	ret = sprdwcn_bus_reg_write(REG_WIFI_MEM_CFG1, &temp_val, 4);
	if (ret < 0) {
		WCN_ERR("write wifimem_cfg1 reg error:%d\n", ret);
		return ret;
	}

	return ret;
}

int enable_spur_remove(void)
{
	int ret;
	unsigned int temp_val;

	temp_val = FM_ENABLE_SPUR_REMOVE_FREQ2_VALUE;
	ret = sprdwcn_bus_reg_write(FM_REG_SPUR_FEQ1_ADDR, &temp_val, 4);
	if (ret < 0) {
		WCN_ERR("write FM_REG_SPUR reg error:%d\n", ret);
		return ret;
	}

	return 0;
}

int disable_spur_remove(void)
{
	int ret;
	unsigned int temp_val;

	temp_val = FM_DISABLE_SPUR_REMOVE_VALUE;
	ret = sprdwcn_bus_reg_write(FM_REG_SPUR_FEQ1_ADDR, &temp_val, 4);
	if (ret < 0) {
		WCN_ERR("write disable FM_REG_SPUR reg error:%d\n", ret);
		return ret;
	}

	return 0;
}

void set_fm_supe_freq(int subsys, int val, unsigned long sub_state)
{
	switch (subsys) {
	case MARLIN_FM:
		if (test_bit(MARLIN_GNSS, &sub_state) && (val == 1))
			enable_spur_remove();
		else
			disable_spur_remove();
		break;
	case MARLIN_GNSS:
		if (test_bit(MARLIN_FM, &sub_state) && (val == 1))
			enable_spur_remove();
		else
			disable_spur_remove();
		break;
	default:
		break;
	}
}

/*
 * MARLIN_GNSS no need loopcheck action
 * MARLIN_AUTO no need loopcheck action
 */
static void power_state_notify_or_not(int subsys, int poweron)
{
	if (poweron == 1) {
		set_cp_mem_status(subsys, poweron);
		set_fm_supe_freq(subsys, poweron, marlin_dev->power_state);
	}

	if ((test_bit(MARLIN_BLUETOOTH, &marlin_dev->power_state) +
		test_bit(MARLIN_FM, &marlin_dev->power_state) +
		test_bit(MARLIN_WIFI, &marlin_dev->power_state) +
		test_bit(MARLIN_MDBG, &marlin_dev->power_state)) == 1) {
		WCN_INFO("only one module open, need to notify loopcheck\n");
		marlin_dev->loopcheck_status_change = 1;
		wakeup_loopcheck_int();
	}

	if (((marlin_dev->power_state) & MARLIN_MASK) == 0) {

		WCN_INFO("marlin close, need to notify loopcheck\n");
		marlin_dev->loopcheck_status_change = 1;
		wakeup_loopcheck_int();
	}
}

static void marlin_scan_finish(void)
{
	WCN_INFO("%s!\n", __func__);
	complete(&marlin_dev->carddetect_done);
}

static int find_firmware_path(void)
{
	int ret;
	int pre_len;

	if (strlen(BTWF_FIRMWARE_PATH) != 0)
		return 0;

	ret = parse_firmware_path(BTWF_FIRMWARE_PATH);
	if (ret != 0) {
		WCN_ERR("can not find wcn partition\n");
		return ret;
	}
	WCN_INFO("BTWF path is %s\n", BTWF_FIRMWARE_PATH);
	pre_len = strlen(BTWF_FIRMWARE_PATH) - strlen("wcnmodem");
	memcpy(GNSS_FIRMWARE_PATH,
		BTWF_FIRMWARE_PATH,
		strlen(BTWF_FIRMWARE_PATH));
	memcpy(&GNSS_FIRMWARE_PATH[pre_len], "gnssmodem",
		strlen("gnssmodem"));
	GNSS_FIRMWARE_PATH[pre_len + strlen("gnssmodem")] = '\0';
	WCN_INFO("GNSS path is %s\n", GNSS_FIRMWARE_PATH);

	return 0;
}

static void pre_gnss_download_firmware(struct work_struct *work)
{
	static int cali_flag;
	int ret;

	/* ./fstab.xxx is prevent for user space progress */
	find_firmware_path();

	if (gnss_download_firmware() != 0) {
		WCN_ERR("gnss download firmware fail\n");
		return;
	}

	if (gnss_ops && (gnss_ops->write_data)) {
		if (gnss_ops->write_data() != 0)
			return;
	} else {
		WCN_ERR("%s gnss_ops write_data error\n", __func__);
	}

	if (gnss_start_run() != 0)
		WCN_ERR("gnss start run fail\n");

	if (cali_flag == 0) {
		WCN_INFO("gnss start to backup calidata\n");
		if (gnss_ops && gnss_ops->backup_data) {
			ret = gnss_ops->backup_data();
			if (ret == 0)
				cali_flag = 1;
		} else {
			WCN_ERR("%s gnss_ops backup_data error\n", __func__);
		}
	} else {
		WCN_INFO("gnss wait boot finish\n");
		if (gnss_ops && gnss_ops->wait_gnss_boot)
			gnss_ops->wait_gnss_boot();
		else
			WCN_ERR("%s gnss_ops wait boot error\n", __func__);
	}
	complete(&marlin_dev->gnss_download_done);
}

static void pre_btwifi_download_sdio(struct work_struct *work)
{
	if (btwifi_download_firmware() == 0 &&
		marlin_start_run() == 0) {
#ifndef CONFIG_WCN_PCIE
		check_cp_clock_mode();
		marlin_write_cali_data();
		mem_pd_save_bin();
#endif
		check_cp_ready();
		complete(&marlin_dev->download_done);
	}
	sprdwcn_bus_runtime_get();
}

static int bus_scan_card(void)
{
	init_completion(&marlin_dev->carddetect_done);
	sprdwcn_bus_rescan(marlin_dev);
	if (wait_for_completion_timeout(&marlin_dev->carddetect_done,
		msecs_to_jiffies(CARD_DETECT_WAIT_MS)) == 0) {
		WCN_ERR("wait bus rescan card time out\n");
		return -1;
	}

	return 0;
}

void wifipa_enable(int enable)
{
	int ret = -1;

	if (marlin_dev->avdd33) {
		WCN_INFO("wifipa 3v3 %d\n", enable);
		usleep_range(4000, 5000);
		if (enable) {

			if (regulator_is_enabled(marlin_dev->avdd33))
				return;

			regulator_set_voltage(marlin_dev->avdd33,
					      3300000, 3300000);
			ret = regulator_enable(marlin_dev->avdd33);
			if (ret)
				WCN_ERR("fail to enable wifipa\n");
		} else {
			if (regulator_is_enabled(marlin_dev->avdd33)) {
				ret =
				regulator_disable(marlin_dev->avdd33);
				if (ret)
					WCN_ERR("fail to disable wifipa\n");
				WCN_INFO(" wifi pa disable\n");
			}
		}
	}
}

void set_wifipa_status(int subsys, int val)
{
	if (val == 1) {
		if (((subsys == MARLIN_BLUETOOTH) || (subsys == MARLIN_WIFI)) &&
		    ((marlin_dev->power_state & 0x5) == 0)) {
			wifipa_enable(1);
			wcn_wifipa_bound_xtl(true);
		}

		if (((subsys != MARLIN_BLUETOOTH) && (subsys != MARLIN_WIFI)) &&
		    ((marlin_dev->power_state & 0x5) == 0)) {
			wcn_wifipa_bound_xtl(false);
			wifipa_enable(0);
		}
	} else {
		if (((subsys == MARLIN_BLUETOOTH) &&
		     ((marlin_dev->power_state & 0x4) == 0)) ||
		    ((subsys == MARLIN_WIFI) &&
		     ((marlin_dev->power_state & 0x1) == 0))) {
			wcn_wifipa_bound_xtl(false);
			wifipa_enable(0);
		}
	}
}

/*
 * RST_N (LOW)
 * VDDIO -> DVDD12/11 ->CHIP_EN ->DVDD_CORE(inner)
 * ->(>=550uS) RST_N (HIGH)
 * ->(>=100uS) ADVV12
 * ->(>=10uS)  AVDD33
 */
static int chip_power_on(int subsys)
{
	wcn_avdd12_parent_bound_chip(false);
	marlin_avdd18_dcxo_enable(true);
	marlin_clk_enable(true);
	marlin_digital_power_enable(true);
	marlin_chip_en(true, false);
	usleep_range(4000, 5000);
	chip_reset_release(1);
	marlin_analog_power_enable(true);
	wcn_avdd12_bound_xtl(true);
	usleep_range(50, 60);
	wifipa_enable(1);
	wcn_wifipa_bound_xtl(true);
	if (bus_scan_card() < 0)
		return -1;
	loopcheck_ready_set();
#ifndef CONFIG_WCN_PCIE
	mem_pd_poweroff_deinit();
	sdio_pub_int_poweron(true);
	wcn_check_xtal_26m_clk();
#endif

	return 0;
}

static int chip_power_off(int subsys)
{
#ifdef CONFIG_WCN_PCIE
	sprdwcn_bus_remove_card(marlin_dev);
#endif
	marlin_dev->power_state = 0;
	wcn_avdd12_bound_xtl(false);
	wcn_wifipa_bound_xtl(false);
	wcn_avdd12_parent_bound_chip(true);
	wifipa_enable(0);
	marlin_avdd18_dcxo_enable(false);
	marlin_clk_enable(false);
	marlin_chip_en(false, false);
	marlin_digital_power_enable(false);
	marlin_analog_power_enable(false);
	chip_reset_release(0);
	marlin_dev->wifi_need_download_ini_flag = 0;
#ifndef CONFIG_WCN_PCIE
	mem_pd_poweroff_deinit();
	sprdwcn_bus_remove_card(marlin_dev);
#endif
	loopcheck_ready_clear();
#ifndef CONFIG_WCN_PCIE
	sdio_pub_int_poweron(false);
#endif

	return 0;
}

static int gnss_powerdomain_open(void)
{
	/* add by this. */
	int ret = 0, retry_cnt = 0;
	unsigned int temp;

	WCN_INFO("%s\n", __func__);
	ret = sprdwcn_bus_reg_read(PD_GNSS_SS_AON_CFG4, &temp, 4);
	if (ret < 0) {
		WCN_ERR("%s read PD_GNSS_SS_AON_CFG4 err:%d\n", __func__, ret);
		return ret;
	}
	WCN_INFO("%s PD_GNSS_SS_AON_CFG4:0x%x\n", __func__, temp);
	temp = temp & (~(FORCE_DEEP_SLEEP));
	WCN_INFO("%s PD_GNSS_SS_AON_CFG4:0x%x\n", __func__, temp);
	ret = sprdwcn_bus_reg_write(PD_GNSS_SS_AON_CFG4, &temp, 4);
	if (ret < 0) {
		WCN_ERR("write PD_GNSS_SS_AON_CFG4 err:%d\n", ret);
		return ret;
	}

	/* wait gnss sys power on finish */
	do {
		usleep_range(3000, 6000);

		ret = sprdwcn_bus_reg_read(CHIP_SLP_REG, &temp, 4);
		if (ret < 0) {
			WCN_ERR("%s read CHIP_SLP_REG err:%d\n", __func__, ret);
			return ret;
		}

		WCN_INFO("%s CHIP_SLP:0x%x,bit12,13 need 1\n", __func__, temp);
		retry_cnt++;
	} while ((!(temp & GNSS_SS_PWRON_FINISH)) &&
		 (!(temp & GNSS_PWR_FINISH)) && (retry_cnt < 3));

	return 0;
}

/*
 * CGM_GNSS_FAKE_CFG : 0x0: for 26M clock; 0x2: for 266M clock
 * gnss should select 26M clock before powerdomain close
 *
 * PD_GNSS_SS_AON_CFG4: 0x4041308->0x4041300 bit3=0 power on
 */
static int gnss_powerdomain_close(void)
{
	/* add by this. */
	int ret;
	int i = 0;
	unsigned int temp;

	WCN_INFO("%s\n", __func__);

	ret = sprdwcn_bus_reg_read(CGM_GNSS_FAKE_CFG, &temp, 4);
	if (ret < 0) {
		WCN_ERR("%s read CGM_GNSS_FAKE_CFG error:%d\n", __func__, ret);
		return ret;
	}
	WCN_INFO("%s R_CGM_GNSS_FAKE_CFG:0x%x\n", __func__, temp);
	temp = temp & (~(CGM_GNSS_FAKE_SEL));
	ret = sprdwcn_bus_reg_write(CGM_GNSS_FAKE_CFG, &temp, 4);
	if (ret < 0) {
		WCN_ERR("write CGM_GNSS_FAKE_CFG err:%d\n", ret);
		return ret;
	}
retry:
	ret = sprdwcn_bus_reg_read(CGM_GNSS_FAKE_CFG, &temp, 4);
	if (ret < 0) {
		WCN_ERR("%s read CGM_GNSS_FAKE_CFG error:%d\n", __func__, ret);
		return ret;
	}
	i++;
	if ((temp & 0x3) && (i < 3)) {
		WCN_ERR("FAKE_CFG:0x%x, GNSS select clk err\n", temp);
		goto retry;
	}

	ret = sprdwcn_bus_reg_read(PD_GNSS_SS_AON_CFG4, &temp, 4);
	if (ret < 0) {
		WCN_ERR("read PD_GNSS_SS_AON_CFG4 err:%d\n", ret);
		return ret;
	}
	WCN_INFO("%s PD_GNSS_SS_AON_CFG4:0x%x\n", __func__, temp);
	temp = (temp | FORCE_DEEP_SLEEP | PD_AUTO_EN) &
		(~(CHIP_DEEP_SLP_EN));
	WCN_INFO("%s PD_GNSS_SS_AON_CFG4:0x%x\n", __func__, temp);
	ret = sprdwcn_bus_reg_write(PD_GNSS_SS_AON_CFG4, &temp, 4);
	if (ret < 0) {
		WCN_ERR("write PD_GNSS_SS_AON_CFG4 err:%d\n", ret);
		return ret;
	}

	return 0;
}

int open_power_ctl(void)
{
	marlin_dev->keep_power_on = false;
	clear_bit(WCN_AUTO, &marlin_dev->power_state);

	return 0;
}
EXPORT_SYMBOL_GPL(open_power_ctl);

static int marlin_set_power(int subsys, int val)
{
	unsigned long timeleft;

	mutex_lock(&marlin_dev->power_lock);

	if (marlin_dev->wait_ge2) {
		if (first_call_flag == 1) {
			WCN_INFO("(marlin2+ge2)waiting ge2 download finish\n");
			timeleft
				= wait_for_completion_timeout(
				&ge2_completion, 12*HZ);
			if (!timeleft)
				WCN_ERR("wait ge2 timeout\n");
			first_call_flag = 2;
		}
	}

	WCN_INFO("marlin power state:%lx, subsys: [%s] power %d\n",
			marlin_dev->power_state, strno(subsys), val);
	init_completion(&marlin_dev->download_done);
	init_completion(&marlin_dev->gnss_download_done);

	/*  power on */
	if (val) {
		/*
		 * 1. when the first open:
		 * `- first download gnss, and then download btwifi
		 */
		if (unlikely(!marlin_dev->first_power_on_ready)) {
			WCN_INFO("the first power on start\n");

			if (chip_power_on(subsys) < 0) {
				mutex_unlock(&marlin_dev->power_lock);
				return -1;
			}

			set_bit(subsys, &marlin_dev->power_state);

			WCN_INFO("GNSS start to auto download\n");
			schedule_work(&marlin_dev->gnss_dl_wq);
			timeleft
				= wait_for_completion_timeout(
				&marlin_dev->gnss_download_done, 10 * HZ);
			if (!timeleft) {
				WCN_ERR("GNSS download timeout\n");
				goto out;
			}
			WCN_INFO("gnss auto download finished and run ok\n");

			if (subsys & MARLIN_MASK)
				gnss_powerdomain_close();
			marlin_dev->first_power_on_ready = 1;

			WCN_INFO("then marlin start to download\n");
			schedule_work(&marlin_dev->download_wq);
			timeleft = wait_for_completion_timeout(
				&marlin_dev->download_done,
				msecs_to_jiffies(POWERUP_WAIT_MS));
			if (!timeleft) {
				WCN_ERR("marlin download timeout\n");
				goto out;
			}
			atomic_set(&marlin_dev->download_finish_flag, 1);
			WCN_INFO("then marlin download finished and run ok\n");

			set_wifipa_status(subsys, val);
			mutex_unlock(&marlin_dev->power_lock);
			power_state_notify_or_not(subsys, val);
			if (subsys == WCN_AUTO) {
				marlin_set_power(WCN_AUTO, false);
				return 0;
			}
			/*
			 * If first power on is GNSS, must power off it
			 * after cali finish, and then re-power on it.
			 * This is gnss requirement.
			 */
			if (subsys == MARLIN_GNSS) {
				marlin_set_power(MARLIN_GNSS, false);
				marlin_set_power(MARLIN_GNSS, true);
				return 0;
			}

			return 0;
		}
		/* 2. the second time, WCN_AUTO coming */
		else if (subsys == WCN_AUTO) {
			if (marlin_dev->keep_power_on) {
				WCN_INFO("have power on, no action\n");
				set_wifipa_status(subsys, val);
				set_bit(subsys, &marlin_dev->power_state);
			} else {
				WCN_INFO("!1st,not to bkup gnss cal, no act\n");
			}
		}

		/*
		 * 3. when GNSS open,
		 *	  |- GNSS and MARLIN have power on and ready
		 */
		else if ((((marlin_dev->power_state) & AUTO_RUN_MASK) != 0)
			|| (((marlin_dev->power_state) & GNSS_MASK) != 0)) {
			WCN_INFO("GNSS and marlin have ready\n");
			if (((marlin_dev->power_state) & MARLIN_MASK) == 0)
				loopcheck_ready_set();
			set_wifipa_status(subsys, val);
			set_bit(subsys, &marlin_dev->power_state);

			goto check_power_state_notify;
		}
		/* 4. when GNSS close, marlin open.
		 *	  ->  subsys=gps,GNSS download
		 */
		else if (((marlin_dev->power_state) & MARLIN_MASK) != 0) {
			if ((subsys == MARLIN_GNSS) || (subsys == WCN_AUTO)) {
				WCN_INFO("BTWF ready, GPS start to download\n");
				set_wifipa_status(subsys, val);
				set_bit(subsys, &marlin_dev->power_state);
				gnss_powerdomain_open();

				schedule_work(&marlin_dev->gnss_dl_wq);
				timeleft = wait_for_completion_timeout(
					&marlin_dev->gnss_download_done, 10*HZ);
				if (!timeleft) {
					WCN_ERR("GNSS download timeout\n");
					goto out;
				}

				WCN_INFO("GNSS download finished and ok\n");

			} else {
				WCN_INFO("marlin have open, GNSS is closed\n");
				set_wifipa_status(subsys, val);
				set_bit(subsys, &marlin_dev->power_state);

				goto check_power_state_notify;
			}
		}
		/* 5. when GNSS close, marlin close.no module to power on */
		else {
			WCN_INFO("no module to power on, start to power on\n");
			if (chip_power_on(subsys) < 0) {
				mutex_unlock(&marlin_dev->power_lock);
				return -1;
			}
			set_bit(subsys, &marlin_dev->power_state);

			/* 5.1 first download marlin, and then download gnss */
			if ((subsys == WCN_AUTO || subsys == MARLIN_GNSS)) {
				WCN_INFO("marlin start to download\n");
				schedule_work(&marlin_dev->download_wq);
				timeleft = wait_for_completion_timeout(
					&marlin_dev->download_done,
					msecs_to_jiffies(POWERUP_WAIT_MS));
				if (!timeleft) {
					WCN_ERR("marlin download timeout\n");
					goto out;
				}
				atomic_set(&marlin_dev->download_finish_flag,
					   1);
				WCN_INFO("marlin dl finished and run ok\n");

				WCN_INFO("GNSS start to download\n");
				schedule_work(&marlin_dev->gnss_dl_wq);
				timeleft = wait_for_completion_timeout(
					&marlin_dev->gnss_download_done, 10*HZ);
				if (!timeleft) {
					WCN_ERR("then GNSS download timeout\n");
					goto out;
				}
				WCN_INFO("then gnss dl finished and ok\n");
			}
			/*
			 * 5.2 only download marlin, and then
			 * close gnss power domain
			 */
			else {
				WCN_INFO("only marlin start to download\n");
				schedule_work(&marlin_dev->download_wq);
				if (wait_for_completion_timeout(
					&marlin_dev->download_done,
					msecs_to_jiffies(POWERUP_WAIT_MS))
					<= 0) {

					WCN_ERR("marlin download timeout\n");
					goto out;
				}
				atomic_set(&marlin_dev->download_finish_flag,
					   1);
				WCN_INFO("BTWF download finished and run ok\n");
				gnss_powerdomain_close();
			}
			set_wifipa_status(subsys, val);
		}
		/* power on together's Action */
		power_state_notify_or_not(subsys, val);

		WCN_INFO("wcn chip power on and run finish: [%s]\n",
				  strno(subsys));
	/* power off */
	} else {
		if (marlin_dev->power_state == 0) {
			if (flag_reset)
				flag_reset = 0;
			goto check_power_state_notify;
		}

		if (marlin_dev->keep_power_on) {
			if (!flag_reset) {
				if (subsys != WCN_AUTO) {
					/* in order to not download again */
					set_bit(WCN_AUTO,
						&marlin_dev->power_state);
					clear_bit(subsys,
						&marlin_dev->power_state);
				}
				WCN_LOG("marlin reset flag_reset:%d\n",
					flag_reset);
				goto check_power_state_notify;
			}
		}

		set_wifipa_status(subsys, val);
		clear_bit(subsys, &marlin_dev->power_state);
		if ((marlin_dev->power_state != 0) && (!flag_reset)) {
			WCN_INFO("can not power off, other module is on\n");
			if (subsys == MARLIN_GNSS)
				gnss_powerdomain_close();
			goto check_power_state_notify;
		}

		set_cp_mem_status(subsys, val);
		set_fm_supe_freq(subsys, val, marlin_dev->power_state);
		power_state_notify_or_not(subsys, val);

		WCN_INFO("wcn chip start power off!\n");
		sprdwcn_bus_runtime_put();
		chip_power_off(subsys);
		WCN_INFO("marlin power off!\n");
		atomic_set(&marlin_dev->download_finish_flag, 0);
		if (flag_reset) {
			flag_reset = FALSE;
			marlin_dev->power_state = 0;
		}
	} /* power off end */

	/* power on off together's Action */
	mutex_unlock(&marlin_dev->power_lock);

	return 0;

out:
	sprdwcn_bus_runtime_put();
#ifndef CONFIG_WCN_PCIE
	mem_pd_poweroff_deinit();
#endif
	marlin_clk_enable(false);
	marlin_digital_power_enable(false);
	marlin_analog_power_enable(false);
	chip_reset_release(0);
	marlin_dev->power_state = 0;
	atomic_set(&marlin_dev->download_finish_flag, 0);
	mutex_unlock(&marlin_dev->power_lock);

	return -1;

check_power_state_notify:
	power_state_notify_or_not(subsys, val);
	WCN_DBG("mutex_unlock\n");
	mutex_unlock(&marlin_dev->power_lock);

	return 0;

}

void marlin_power_off(enum marlin_sub_sys subsys)
{
	WCN_INFO("%s all\n", __func__);

	marlin_dev->keep_power_on = false;
	set_bit(subsys, &marlin_dev->power_state);
	marlin_set_power(subsys, false);
}

int marlin_get_power(void)
{
	return marlin_dev->power_state;
}
EXPORT_SYMBOL_GPL(marlin_get_power);

bool marlin_get_download_status(void)
{
	return atomic_read(&marlin_dev->download_finish_flag);
}
EXPORT_SYMBOL_GPL(marlin_get_download_status);

void marlin_set_download_status(int f)
{
	atomic_set(&marlin_dev->download_finish_flag, f);
}
EXPORT_SYMBOL_GPL(marlin_set_download_status);

int wcn_get_module_status_changed(void)
{
	return marlin_dev->loopcheck_status_change;
}
EXPORT_SYMBOL_GPL(wcn_get_module_status_changed);

void wcn_set_module_status_changed(bool status)
{
	marlin_dev->loopcheck_status_change = status;
}
EXPORT_SYMBOL_GPL(wcn_set_module_status_changed);

int marlin_get_module_status(void)
{
	if (test_bit(MARLIN_BLUETOOTH, &marlin_dev->power_state) ||
	    test_bit(MARLIN_FM, &marlin_dev->power_state) ||
	    test_bit(MARLIN_WIFI, &marlin_dev->power_state) ||
	    test_bit(MARLIN_MDBG, &marlin_dev->power_state))
		/*
		 * Can't send mdbg cmd before download flag ok
		 * If download flag not ready,loopcheck get poweroff
		 */
		return atomic_read(&marlin_dev->download_finish_flag);
	else
		return 0;
}
EXPORT_SYMBOL_GPL(marlin_get_module_status);

int is_first_power_on(enum marlin_sub_sys subsys)
{
	if (marlin_dev->wifi_need_download_ini_flag == 1)
		return 1;	/*the first */
	else
		return 0;	/* not the first */
}
EXPORT_SYMBOL_GPL(is_first_power_on);

int cali_ini_need_download(enum marlin_sub_sys subsys)
{
#ifndef CONFIG_WCN_PCIE
	unsigned int pd_wifi_st = 0;

	pd_wifi_st = mem_pd_wifi_state();
	if ((marlin_dev->wifi_need_download_ini_flag == 1) || pd_wifi_st) {
		WCN_INFO("%s return 1\n", __func__);
		return 1;	/* the first */
	}
#endif
	return 0;	/* not the first */
}
EXPORT_SYMBOL_GPL(cali_ini_need_download);

int marlin_set_wakeup(enum marlin_sub_sys subsys)
{
	int ret = 0;	/* temp */

	return 0;
	if (!atomic_read(&marlin_dev->download_finish_flag))
		return -1;

	return ret;
}
EXPORT_SYMBOL_GPL(marlin_set_wakeup);

int marlin_set_sleep(enum marlin_sub_sys subsys, bool enable)
{
	return 0;	/* temp */

	if (!atomic_read(&marlin_dev->download_finish_flag))
		return -1;

	return 0;
}
EXPORT_SYMBOL_GPL(marlin_set_sleep);

int marlin_reset_reg(void)
{
	init_completion(&marlin_dev->carddetect_done);
	marlin_reset(true);
	mdelay(1);
	sprdwcn_bus_rescan(marlin_dev);
	if (wait_for_completion_timeout(&marlin_dev->carddetect_done,
		msecs_to_jiffies(CARD_DETECT_WAIT_MS))) {
		return 0;
	}
	WCN_ERR("marlin reset reg wait scan error!\n");

	return -1;
}
EXPORT_SYMBOL_GPL(marlin_reset_reg);

int start_marlin(u32 subsys)
{
	WCN_INFO("%s [%s]\n", __func__, strno(subsys));
	if (sprdwcn_bus_get_carddump_status() != 0) {
		WCN_ERR("%s SDIO card dump\n", __func__);
		return -1;
	}

	if (get_loopcheck_status()) {
		WCN_ERR("%s loopcheck status is fail\n", __func__);
		return -1;
	}

	if (subsys == MARLIN_WIFI) {
		/* not need write cali */
		if (marlin_dev->wifi_need_download_ini_flag == 0)
			/* need write cali */
			marlin_dev->wifi_need_download_ini_flag = 1;
		else
			/* not need write cali */
			marlin_dev->wifi_need_download_ini_flag = 2;
	}
	if (marlin_set_power(subsys, true) < 0)
		return -1;
#ifndef CONFIG_WCN_PCIE
	return mem_pd_mgr(subsys, true);
#else
	return 0;
#endif
}
EXPORT_SYMBOL_GPL(start_marlin);

int stop_marlin(u32 subsys)
{
	WCN_INFO("%s [%s]\n", __func__, strno(subsys));
	if (sprdwcn_bus_get_carddump_status() != 0) {
		WCN_ERR("%s SDIO card dump\n", __func__);
		return -1;
	}

	if (get_loopcheck_status()) {
		WCN_ERR("%s loopcheck status is fail\n", __func__);
		return -1;
	}
#ifndef CONFIG_WCN_PCIE
	mem_pd_mgr(subsys, false);
#endif
	return marlin_set_power(subsys, false);
}
EXPORT_SYMBOL_GPL(stop_marlin);



static void marlin_power_wq(struct work_struct *work)
{
	WCN_INFO("%s start\n", __func__);

	/* WCN_AUTO is for auto backup gnss cali data */
	marlin_set_power(WCN_AUTO, true);

}

static int marlin_probe(struct platform_device *pdev)
{
	int err;

	marlin_dev = devm_kzalloc(&pdev->dev,
			sizeof(struct marlin_device), GFP_KERNEL);
	if (!marlin_dev)
		return -ENOMEM;
	marlin_dev->write_buffer = devm_kzalloc(&pdev->dev,
			PACKET_SIZE, GFP_KERNEL);
	if (marlin_dev->write_buffer == NULL) {
		devm_kfree(&pdev->dev, marlin_dev);
		WCN_ERR("%s write buffer low memory\n", __func__);
		return -ENOMEM;
	}

	marlin_dev->np = pdev->dev.of_node;
	WCN_INFO("%s: device node name: %s\n",
		 __func__, marlin_dev->np->name);

	mutex_init(&(marlin_dev->power_lock));
	marlin_dev->power_state = 0;
	err = marlin_parse_dt(pdev);
	if (err < 0) {
		WCN_INFO("marlin2 parse_dt some para not config\n");
		if (err == -EPROBE_DEFER) {
			devm_kfree(&pdev->dev, marlin_dev);
			WCN_ERR("%s: get some resources fail, defer probe it\n",
				__func__);
			return err;
		}
	}
	if (gpio_is_valid(marlin_dev->reset))
		gpio_direction_output(marlin_dev->reset, 0);
	init_completion(&ge2_completion);
	init_completion(&marlin_dev->carddetect_done);

#ifdef CONFIG_WCN_SLP
	slp_mgr_init();
#endif
	/* register ops */
	wcn_bus_init();
	/* sdiom_init or pcie_init */
	err = sprdwcn_bus_preinit();
	if (err) {
		WCN_ERR("sprdwcn_bus_preinit error: %d\n", err);
		goto error3;
	}

	sprdwcn_bus_register_rescan_cb(marlin_scan_finish);
#ifndef CONFIG_WCN_PCIE
	err = sdio_pub_int_init(marlin_dev->int_ap);
	if (err) {
		WCN_ERR("sdio_pub_int_init error: %d\n", err);
		sprdwcn_bus_deinit();
		wcn_bus_deinit();
#ifdef CONFIG_WCN_SLP
		slp_mgr_deinit();
#endif
		devm_kfree(&pdev->dev, marlin_dev);
		return err;
	}

	mem_pd_init();
#endif
	err = proc_fs_init();
	if (err) {
		WCN_ERR("proc_fs_init error: %d\n", err);
		goto error2;
	}

	err = log_dev_init();
	if (err) {
		WCN_ERR("log_dev_init error: %d\n", err);
		goto error1;
	}

	err = wcn_op_init();
	if (err) {
		WCN_ERR("wcn_op_init: %d\n", err);
		goto error0;
	}

	flag_reset = 0;
	INIT_WORK(&marlin_dev->download_wq, pre_btwifi_download_sdio);
	INIT_WORK(&marlin_dev->gnss_dl_wq, pre_gnss_download_firmware);

	INIT_DELAYED_WORK(&marlin_dev->power_wq, marlin_power_wq);
#ifndef CONFIG_WCN_PCIE
	schedule_delayed_work(&marlin_dev->power_wq,
			      msecs_to_jiffies(3500));
#endif

	WCN_INFO("%s ok!\n", __func__);

	return 0;
error0:
	log_dev_exit();
error1:
	proc_fs_exit();
error2:
#ifndef CONFIG_WCN_PCIE
	mem_pd_exit();
	sdio_pub_int_deinit();
#endif
	sprdwcn_bus_deinit();
error3:
	wcn_bus_deinit();
#ifdef CONFIG_WCN_SLP
	slp_mgr_deinit();
#endif
	devm_kfree(&pdev->dev, marlin_dev);
	return err;
}

static int  marlin_remove(struct platform_device *pdev)
{
	cancel_work_sync(&marlin_dev->download_wq);
	cancel_work_sync(&marlin_dev->gnss_dl_wq);
	cancel_delayed_work_sync(&marlin_dev->power_wq);
	wcn_op_exit();
	log_dev_exit();
	proc_fs_exit();
#ifndef CONFIG_WCN_PCIE
	sdio_pub_int_deinit();
	mem_pd_exit();
#endif
	sprdwcn_bus_deinit();
	if (marlin_dev->power_state != 0) {
		WCN_INFO("marlin some subsys power is on, warning!\n");
		wcn_wifipa_bound_xtl(false);
		wifipa_enable(0);
		marlin_chip_en(false, false);
	}
	wcn_bus_deinit();
#ifdef CONFIG_WCN_SLP
	slp_mgr_deinit();
#endif
	marlin_gpio_free(pdev);
	mutex_destroy(&marlin_dev->power_lock);
	devm_kfree(&pdev->dev, marlin_dev->write_buffer);
	devm_kfree(&pdev->dev, marlin_dev);

	WCN_INFO("remove ok!\n");

	return 0;
}

static void marlin_shutdown(struct platform_device *pdev)
{
	if (marlin_dev->power_state != 0) {
		WCN_INFO("marlin some subsys power is on, warning!\n");
		sprdwcn_bus_set_carddump_status(true);
		wcn_avdd12_bound_xtl(false);
		wcn_wifipa_bound_xtl(false);
		wifipa_enable(0);
		marlin_chip_en(false, false);
	}
	WCN_INFO("%s end\n", __func__);
}

static int marlin_suspend(struct device *dev)
{

	WCN_INFO("[%s]enter\n", __func__);

	return 0;
}

int marlin_reset_register_notify(void *callback_func, void *para)
{
	marlin_reset_func = (marlin_reset_callback)callback_func;
	marlin_callback_para = para;

	return 0;
}
EXPORT_SYMBOL_GPL(marlin_reset_register_notify);

int marlin_reset_unregister_notify(void)
{
	marlin_reset_func = NULL;
	marlin_callback_para = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(marlin_reset_unregister_notify);

static int marlin_resume(struct device *dev)
{
	WCN_INFO("[%s]enter\n", __func__);

	return 0;
}

static const struct dev_pm_ops marlin_pm_ops = {
	.suspend = marlin_suspend,
	.resume	= marlin_resume,
};

static const struct of_device_id marlin_match_table[] = {
	{.compatible = "sprd,marlin3",},
	{ },
};

static struct platform_driver marlin_driver = {
	.driver = {
		.name = "marlin",
		.pm = &marlin_pm_ops,
		.of_match_table = marlin_match_table,
	},
	.probe = marlin_probe,
	.remove = marlin_remove,
	.shutdown = marlin_shutdown,
};

static int __init marlin_init(void)
{
	WCN_INFO("%s entry!\n", __func__);

	return platform_driver_register(&marlin_driver);
}

#ifdef CONFIG_WCN_PCIE
device_initcall(marlin_init);
#else
late_initcall(marlin_init);
#endif

static void __exit marlin_exit(void)
{
	platform_driver_unregister(&marlin_driver);
}
module_exit(marlin_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum  WCN Marlin Driver");
MODULE_AUTHOR("Yufeng Yang <yufeng.yang@spreadtrum.com>");
