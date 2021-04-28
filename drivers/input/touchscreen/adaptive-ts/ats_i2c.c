#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include "ats_core.h"

#define TS_I2C_MAX_RETRY 3
#define TS_I2C_RETRY_MSLEEP 10
#define TS_DEFAULT_SLAVE_REG_WIDTH 1
#define TS_MAX_SLAVE_REG_WIDTH 2
#define TS_I2C_MAX_BUF_LEN 250

static struct i2c_client *g_client;
static DEFINE_MUTEX(i2c_mutex);

/*
 * send msgs to adapter, retry must not be negative
 */
static inline int ts_send_retry(
	struct i2c_adapter *adapter, struct i2c_msg *msgs, int num, int retry)
{
	int tx_len, re = retry;

	if (!adapter || !msgs || num < 0 || retry < 0)
		return 0;

	while (re--) {
		mutex_lock(&i2c_mutex);
		tx_len = i2c_transfer(adapter, msgs, num);
		mutex_unlock(&i2c_mutex);

		if (tx_len == num)
			return tx_len;

		pr_warn("I2C failed %d times\n", retry - re);
		msleep(TS_I2C_RETRY_MSLEEP);
	}

	pr_err("I2C failed over limits\n");
	return tx_len;
}

static inline int ts_assign_reg(
	unsigned char *buf, unsigned short reg, int width)
{
	if (width == 1) {
		buf[0] = reg & 0xFF;
	} else {
		buf[0] = (reg >> 8) & 0xFF;
		buf[1] = reg & 0xFF;
	}

	return width;
}

static int ts_i2c_simple_read(unsigned char *data, unsigned short length)
{
	int rx_len;
	struct i2c_client *i2c = g_client;
	struct i2c_msg msgs[] = {
		{
			.flags = I2C_M_RD,
			.buf = data,
			.len = length,
		},
	};

	if (!i2c) {
		pr_err("I2C dev not ready\n");
		return -EIO;
	}

	msgs[0].addr = i2c->addr;

	mutex_lock(&i2c_mutex);
	rx_len = i2c_transfer(i2c->adapter, msgs, ARRAY_SIZE(msgs));
	mutex_unlock(&i2c_mutex);

	return rx_len == ARRAY_SIZE(msgs) ? length : -EIO;
}

static int ts_i2c_simple_write(unsigned char *data, unsigned short length)
{
	int tx_len;
	struct i2c_client *i2c = g_client;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.buf = data,
			.len = length,
		},
	};

	if (!i2c) {
		pr_err("I2C dev not ready\n");
		return -EIO;
	}

	msgs[0].addr = i2c->addr;

	mutex_lock(&i2c_mutex);
	tx_len = i2c_transfer(i2c->adapter, msgs, ARRAY_SIZE(msgs));
	mutex_unlock(&i2c_mutex);

	return tx_len == ARRAY_SIZE(msgs) ? length : -EIO;
}

static int ts_i2c_simple_read_reg(
	unsigned short reg, unsigned char *data, unsigned short length)
{
	int tx;
	unsigned char addr_buf[TS_MAX_SLAVE_REG_WIDTH];
	struct i2c_client *i2c = g_client;
	struct ts_bus_access *bus;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.buf = addr_buf,
		},
		{
			.flags = I2C_M_RD,
			.buf = data,
			.len = length,
		},
	};

	if (!i2c) {
		pr_err("I2C dev not ready\n");
		return -EIO;
	}

	bus = (struct ts_bus_access *)i2c_get_clientdata(i2c);
	if (bus->reg_width != 1 && bus->reg_width != 2) {
		pr_err("bad reg width: %u\n", bus->reg_width);
		return -EIO;
	}

	msgs[0].addr = i2c->addr;
	msgs[0].len = ts_assign_reg(addr_buf, reg, bus->reg_width);
	msgs[1].addr = i2c->addr;

	tx = ts_send_retry(i2c->adapter, msgs, ARRAY_SIZE(msgs), 1);

	return tx == ARRAY_SIZE(msgs) ? length : tx;
}

static int ts_i2c_simple_write_reg(
	unsigned short reg,	unsigned char *data, unsigned short length)
{
	int tx;
	unsigned char *tx_buf;
	struct i2c_client *i2c;
	struct ts_bus_access *bus;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
		},
	};

	i2c = g_client;
	if (!i2c) {
		pr_err("I2C dev not ready\n");
		return -EIO;
	}

	bus = (struct ts_bus_access *)i2c_get_clientdata(i2c);
	if (bus->reg_width != 1 && bus->reg_width != 2) {
		pr_err("bad reg width: %u\n", bus->reg_width);
		return -EIO;
	}

	tx_buf = devm_kzalloc(&i2c->dev, length + bus->reg_width, GFP_KERNEL);
	if (IS_ERR_OR_NULL(tx_buf)) {
		pr_err("failed to allocate memory");
		return -ENOMEM;
	}

	msgs[0].addr = i2c->addr;
	msgs[0].buf = tx_buf;
	msgs[0].len = length + ts_assign_reg(tx_buf, reg, bus->reg_width);
	memcpy(tx_buf + bus->reg_width, data, length);

	tx = ts_send_retry(i2c->adapter, msgs, ARRAY_SIZE(msgs), 1);

	return tx == ARRAY_SIZE(msgs) ? length : tx;
}

static int ts_i2c_read(
	unsigned short reg, unsigned char *data, unsigned short length)
{
	int tx;
	unsigned short index = 0, tx_length, max_length;
	unsigned char addr_buf[TS_I2C_MAX_BUF_LEN];
	struct ts_bus_access *bus;
	struct i2c_client *i2c = g_client;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.buf = addr_buf,
		},
		{
			.flags = I2C_M_RD,
		}
	};

	if (!i2c) {
		pr_err("I2C dev not ready\n");
		return -EIO;
	}

	bus = (struct ts_bus_access *)i2c_get_clientdata(i2c);
	if (bus->reg_width != 1 && bus->reg_width != 2) {
		pr_err("bad reg width: %u\n", bus->reg_width);
		return -EIO;
	}

	max_length = TS_I2C_MAX_BUF_LEN - bus->reg_width;
	msgs[0].addr = i2c->addr;
	msgs[0].len = bus->reg_width;
	msgs[1].addr = i2c->addr;

	while (index < length) {
		tx_length = length - index;
		if (tx_length > max_length)
			tx_length = max_length;

		ts_assign_reg(addr_buf, reg, bus->reg_width);
		msgs[1].len = tx_length;
		msgs[1].buf = data;

		tx = ts_send_retry(i2c->adapter,
			msgs, ARRAY_SIZE(msgs), TS_I2C_MAX_RETRY);
		if (tx != ARRAY_SIZE(msgs)) {
			pr_err("I2C read error: %d\n", tx);
			return tx;
		}

		index += tx_length;
		reg += tx_length;
		data += tx_length;
	}

	return length;
}

static int ts_i2c_write(
	unsigned short reg,	unsigned char *data, unsigned short length)
{
	int tx;
	unsigned short index = 0, tx_length, max_length;
	unsigned char buf[TS_I2C_MAX_BUF_LEN];
	struct ts_bus_access *bus;
	struct i2c_client *i2c = g_client;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.buf = buf,
		},
	};

	if (!i2c) {
		pr_err("I2C is not ready, please wait a moment.\n");
		return -EIO;
	}

	bus = (struct ts_bus_access *)i2c_get_clientdata(i2c);
	if (bus->reg_width != 1 && bus->reg_width != 2) {
		pr_err("bad reg width: %u\n", bus->reg_width);
		return -EIO;
	}

	max_length = TS_I2C_MAX_BUF_LEN - bus->reg_width;
	msgs[0].addr = i2c->addr;

	while (index < length) {
		tx_length = length - index;
		if (tx_length > max_length)
			tx_length = max_length;

		ts_assign_reg(buf, reg, bus->reg_width);
		memcpy(buf + bus->reg_width, data + index, tx_length);
		msgs[0].len = tx_length + bus->reg_width;

		tx = ts_send_retry(i2c->adapter,
			msgs, ARRAY_SIZE(msgs), TS_I2C_MAX_RETRY);
		if (tx != ARRAY_SIZE(msgs)) {
			pr_err("I2C read error: %d\n", tx);
			return tx;
		}

		index += tx_length;
		reg += tx_length;
	}
	return length;
}

static struct ts_bus_access ts_i2c_bus_access = {
	.simple_read = ts_i2c_simple_read,
	.simple_write = ts_i2c_simple_write,
	.read = ts_i2c_read,
	.write = ts_i2c_write,
	.simple_read_reg = ts_i2c_simple_read_reg,
	.simple_write_reg = ts_i2c_simple_write_reg,
	.reg_width = TS_DEFAULT_SLAVE_REG_WIDTH,
	.bus_type = TSBUS_I2C,
};

static int ts_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C check functionality fail!\n");
		return -ENODEV;
	}

	ts_i2c_bus_access.client_addr = client->addr;
	i2c_set_clientdata(client, &ts_i2c_bus_access);
	ts_register_bus_dev(&client->dev);
	g_client = client;

	dev_info(&client->dev, "I2C device probe OK\n");
	return 0;
}

static int ts_i2c_remove(struct i2c_client *client)
{
	ts_unregister_bus_dev();
	i2c_set_clientdata(client, NULL);
	g_client = NULL;

	return 0;
}

static const struct i2c_device_id ts_i2c_ids[] = {
	{ ATS_I2C_DEV, 0 },
	{ }
};

static const struct of_device_id ts_i2c_matchs[] = {
	{ .compatible = ATS_COMPATIBLE, },
	{ }
};

static struct i2c_driver ts_i2c_driver = {
	.probe		= ts_i2c_probe,
	.remove		= ts_i2c_remove,
	.id_table	= ts_i2c_ids,
	.driver	= {
		.name	= ATS_I2C_DEV,
		.owner	= THIS_MODULE,
		.of_match_table = ts_i2c_matchs,
	},
};

int ts_i2c_init(struct device_node *pn, unsigned short *addrs)
{
	struct i2c_adapter *adap = NULL;
	struct i2c_board_info i2c_info;
	struct i2c_client *client = NULL;

	pr_debug("init i2c in %sadaptive mode", addrs ? "" : "non-");

	if (addrs != NULL) {
		/* works in adaptive mode */
		adap = of_find_i2c_adapter_by_node(pn);
		if (adap == NULL) {
			pr_err("no adapter found on this i2c!\n");
			return -ENODEV;
		}

		memset(&i2c_info, 0, sizeof(struct i2c_board_info));
		strlcpy(i2c_info.type, ATS_I2C_DEV, I2C_NAME_SIZE);
		client = i2c_new_probed_device(adap, &i2c_info, addrs, NULL);
		if (client == NULL) {
			pr_err("i2c instantiating device failed\n");
			put_device(&adap->dev);
			return -ENODEV;
		}

		put_device(&adap->dev);
	}

	return i2c_add_driver(&ts_i2c_driver);
}

void ts_i2c_exit(void)
{
	i2c_del_driver(&ts_i2c_driver);
}

MODULE_AUTHOR("joseph.cai@spreadtrum.com");
MODULE_DESCRIPTION("Spreadtrum touchscreen i2c driver");
MODULE_LICENSE("GPL");
