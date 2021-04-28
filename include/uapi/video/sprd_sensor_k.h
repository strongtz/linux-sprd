/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
#ifndef _SPRD_SENSOR_K_H_
#define _SPRD_SENSOR_K_H_


struct sensor_i2c_tag {
	uint8_t  *i2c_data;
	uint16_t i2c_count;
	uint16_t slave_addr;
	uint16_t read_len;
};

struct sensor_i2c_seq_tag {
	uint8_t  *i2c_data;
	uint16_t i2c_count;
	uint16_t slave_addr;
	uint16_t wr_cnt;
	uint16_t rd_cnt;
};

struct sensor_reg_tag {
	uint16_t reg_addr;
	uint16_t reg_value;
};

struct sensor_reg_bits_tag {
	uint16_t reg_addr;
	uint16_t reg_value;
	uint32_t reg_bits;
};

struct sensor_reg_tab_tag {
	struct sensor_reg_tag *sensor_reg_tab_ptr;
	uint32_t reg_count;
	uint32_t reg_bits;
	uint32_t burst_mode;
};

struct sensor_flash_level {
	uint32_t low_light;
	uint32_t high_light;
};

enum INTERFACE_OP_ID {
	INTERFACE_OPEN = 0,
	INTERFACE_CLOSE
};

enum INTERFACE_TYPE_ID {
	INTERFACE_CCIR = 0,
	INTERFACE_MIPI
};

struct sensor_if_cfg_tag {
	uint32_t is_open;
	uint32_t if_type;
	uint32_t phy_id;
	uint32_t lane_num;
	uint32_t bps_per_lane;
	uint32_t is_cphy;
	uint64_t lane_seq;
};

struct sensor_socid_tag {
	uint32_t d_die;
	uint32_t a_die;
};

struct sensor_power {
	uint32_t af_volt_level;
	uint32_t avdd_volt_level;
	uint32_t dvdd_volt_level;
	uint32_t iovdd_volt_level;
	uint32_t power_down_level;
	uint32_t reset_level;
	uint32_t default_mclk;
};

struct sensor_power_info_tag {
	uint32_t            is_on;
	uint32_t            op_sensor_id;
	struct sensor_power dev0;
	struct sensor_power dev1;
	struct sensor_power dev2;
};

enum sensor_otp_param_type_t {
	SENSOR_OTP_PARAM_NORMAL = 0x00,
	SENSOR_OTP_PARAM_CHECKSUM,
	SENSOR_OTP_PARAM_READBYTE,
	SENSOR_OTP_PARAM_FW_VERSION,
	SENSOR_OTP_PARAM_TYPE_MAX
};

struct _sensor_otp_data_info_tag {
	uint32_t size;
	void    *data_ptr;
};

struct _sensor_otp_param_tag {
	uint32_t type;
	uint32_t start_addr;
	uint32_t len;
	uint8_t *buff;
	struct _sensor_otp_data_info_tag golden;
	struct _sensor_otp_data_info_tag awb;
	struct _sensor_otp_data_info_tag lsc;
};

struct sensor_muti_aec_i2c_tag {
	uint16_t __user *sensor_id;
	uint16_t id_size;
	uint16_t __user *i2c_slave_addr;
	uint16_t i2c_slave_len;
	uint16_t __user *addr_bits_type;
	uint16_t addr_bits_type_len;
	uint16_t __user *data_bits_type;
	uint16_t data_bits_type_len;
	struct sensor_reg_tag __user *master_i2c_tab;
	uint16_t msize;
	struct sensor_reg_tag __user *slave_i2c_tab;
	uint16_t ssize;
	/* TODO optimize this later */
	struct sensor_reg_tag __user *slave_i2c_tab_2;
	uint16_t ssize_2;
};

#define AEC_I2C_SETTINGS_MAX 64
#define AEC_I2C_SENSOR_MAX 3

#define SENSOR_IOC_MAGIC			'R'

#define SENSOR_IO_PD	\
	 _IOW(SENSOR_IOC_MAGIC,  0,  uint8_t)
#define SENSOR_IO_SET_AVDD	\
	 _IOW(SENSOR_IOC_MAGIC,  1,  uint32_t)
#define SENSOR_IO_SET_DVDD	\
	 _IOW(SENSOR_IOC_MAGIC,  2,  uint32_t)
#define SENSOR_IO_SET_IOVDD \
	_IOW(SENSOR_IOC_MAGIC,  3,  uint32_t)
#define SENSOR_IO_SET_MCLK	\
	_IOW(SENSOR_IOC_MAGIC,  4,  uint32_t)
#define SENSOR_IO_RST	\
	_IOW(SENSOR_IOC_MAGIC,  5,  uint32_t)
#define SENSOR_IO_I2C_INIT	\
	_IOW(SENSOR_IOC_MAGIC,  6,  uint32_t)
#define SENSOR_IO_I2C_DEINIT	\
	_IOW(SENSOR_IOC_MAGIC,  7,  uint32_t)
#define SENSOR_IO_SET_ID	\
	_IOW(SENSOR_IOC_MAGIC,  8,  uint32_t)
#define SENSOR_IO_RST_LEVEL	\
	_IOW(SENSOR_IOC_MAGIC,  9,  uint32_t)
#define SENSOR_IO_I2C_ADDR	\
	_IOW(SENSOR_IOC_MAGIC,  10, uint16_t)
#define SENSOR_IO_I2C_READ	\
	_IOWR(SENSOR_IOC_MAGIC, 11, struct sensor_reg_bits_tag)
#define SENSOR_IO_I2C_WRITE	\
	_IOW(SENSOR_IOC_MAGIC,  12, struct sensor_reg_bits_tag)
#define SENSOR_IO_SET_FLASH	\
	_IOW(SENSOR_IOC_MAGIC,  13, uint32_t)
#define SENSOR_IO_I2C_WRITE_REGS	\
	_IOW(SENSOR_IOC_MAGIC,  14, struct sensor_reg_tab_tag)
#define SENSOR_IO_SET_CAMMOT	\
	_IOW(SENSOR_IOC_MAGIC,  15, uint32_t)
#define SENSOR_IO_SET_I2CCLOCK	\
	_IOW(SENSOR_IOC_MAGIC,  16, uint32_t)
#define SENSOR_IO_GET_FLASH_LEVEL	\
	_IOWR(SENSOR_IOC_MAGIC, 17, struct sensor_flash_level)
#define SENSOR_IO_IF_CFG	\
	_IOW(SENSOR_IOC_MAGIC,  18, struct sensor_if_cfg_tag)
#define SENSOR_IO_GRC_I2C_WRITE	\
	_IOW(SENSOR_IOC_MAGIC,  19, struct sensor_i2c_tag)
#define SENSOR_IO_GRC_I2C_READ	\
	_IOWR(SENSOR_IOC_MAGIC, 20, struct sensor_i2c_tag)
#define SENSOR_IO_POWER_CFG	\
	_IOWR(SENSOR_IOC_MAGIC, 21, struct sensor_power_info_tag)
#define SENSOR_IO_SET_MIPI_SWITCH	\
	_IOW(SENSOR_IOC_MAGIC,  22,  uint32_t)
#define SENSOR_IO_MUTI_I2C_WRITE	\
	_IOW(SENSOR_IOC_MAGIC,  23, struct sensor_muti_aec_i2c_tag)
#define SENSOR_IO_IF_SWITCH            \
	_IOW(SENSOR_IOC_MAGIC,  24, struct sensor_if_cfg_tag)
#define SENSOR_IO_READ_OTPDATA	\
	_IOWR(SENSOR_IOC_MAGIC, 254, struct _sensor_otp_param_tag)
#define SENSOR_IO_GET_SOCID	\
	_IOWR(SENSOR_IOC_MAGIC, 255, struct sensor_socid_tag)

#endif

