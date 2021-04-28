#include <linux/debugfs.h>
#include <linux/file.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <misc/wcn_bus.h>

#include "sdiohal.h"
#include "sdiohal_dbg.h"

/*
 * TCP_TEST_RX clear 0: TX send packets ceaselessly; set to 1:
 * RX received one or two big packets (depend on TCP_TEST_1VS2)
 * then TX send one short packet (1*100bytes)
 */
#define TCP_TEST_RX 0
#define TCP_TEST_1VS2 0

#define SDIOHAL_WRITE_SIZE 64
#define SDIOHAL_DIR_TX 1
#define SDIOHAL_DIR_RX 0

#define TX_MULTI_BUF_SIZE 200

#define TP_TX_BUF_CNT 520
#define TP_TX_BUF_LEN 2044

#define FIRMWARE_PATH "/dev/block/platform/sdio_emmc/by-name/wcnmodem"
#define FIRMWARE_MAX_SIZE 0x90c00
#define PACKET_SIZE		(32*1024)
#define CP_START_ADDR		0

#define AT_TX_CHANNEL CHANNEL_2
#define AT_RX_CHANNEL CHANNEL_16

/* for sdio int test */
#define REG_TO_CP0_REQ0	0x1b0
#define REG_TO_CP0_REQ1	0x1b1

#define REG_TO_AP_ENABLE_0	0x1c0
#define REG_TO_AP_ENABLE_1	0x1c1
#define REG_TO_AP_INT_CLR0	0x1d0
#define REG_TO_AP_INT_CLR1	0x1d1
#define REG_TO_AP_PUB_STS0	0x1f0
#define REG_TO_AP_PUB_STS1	0x1f1

#define SDIOHAL_INT_PWR_FUNC	0
#define SDIOHAL_GNSS_DUMP_FUNC	1
#define GNSS_DUMP_WIFI_RAM_ADDR	0x40580000
#define GNSS_DUMP_DATA_SIZE	0x38000

enum {
	/* SDIO TX */
	CHANNEL_TX_BASE = 0,
	CHANNEL_0 = CHANNEL_TX_BASE,
	CHANNEL_1,
	CHANNEL_2,
	CHANNEL_3,
	CHANNEL_4,
	CHANNEL_5,
	CHANNEL_6,
	CHANNEL_7,
	CHANNEL_8,
	CHANNEL_9,
	CHANNEL_10,
	CHANNEL_11,

	/* SDIO RX */
	CHANNEL_RX_BASE = 12,
	CHANNEL_12 = CHANNEL_RX_BASE,
	CHANNEL_13,
	CHANNEL_14,
	CHANNEL_15,
	CHANNEL_16,
	CHANNEL_17,
	CHANNEL_18,
	CHANNEL_19,
	CHANNEL_20,
	CHANNEL_21,
	CHANNEL_22,
	CHANNEL_23,
	CHANNEL_24,
	CHANNEL_25,
	CHANNEL_26,
	CHANNEL_27,
};

static char test_buf[1024];
static char buf[SDIOHAL_WRITE_SIZE + PUB_HEAD_RSV];
static char buf_list[CHANNEL_RX_BASE][SDIOHAL_WRITE_SIZE + PUB_HEAD_RSV];
static char tp_tx_buf[TP_TX_BUF_CNT][TP_TX_BUF_LEN + PUB_HEAD_RSV];

static struct mchn_ops_t at_tx_ops;
static struct mchn_ops_t at_rx_ops;
static struct timeval tp_tx_start_time;
static struct timeval tp_tx_stop_time;
static int tp_tx_cnt;
static int tp_tx_flag;
static int tp_tx_buf_cnt = TP_TX_BUF_CNT;
static int tp_tx_buf_len = TP_TX_BUF_LEN;
long int sdiohal_log_level;

#if TCP_TEST_RX
struct completion tp_rx_completed;
int rx_pop_cnt;

static void sdiohal_tp_rx_up(void)
{
	complete(&tp_rx_completed);
}

static void sdiohal_tp_rx_down(void)
{
	wait_for_completion(&tp_rx_completed);
}
#endif

static int sdiohal_extract_num(char *p, int cur)
{
	int i, num = 0;

	for (i = cur + 1; i < SDIOHAL_WRITE_SIZE; i++) {
		if ((p[i] >= '0') && (p[i] <= '9')) {
			num *= 10;
			num += p[i] - '0';
		} else
			break;
	}

	return num;
}

/*
 * comma_cnt: buf_cnt
 * star_len: buf_len
 * eg.
 * echo "tp,10*52424\r" >/d/sdiohal_debug/at_cmd
 * comma_cnt: buf_cnt = 10
 * star_len: buf_len = 52424
 */
static void sdiohal_find_num(char *p, int *comma_cnt, int *star_len)
{
	int i;

	for (i = 0; i < SDIOHAL_WRITE_SIZE; i++) {
		if (p[i] == '*') {
			/* '*' with buf len */
			*star_len = sdiohal_extract_num(p, i);
		} else if (p[i] == ',') {
			/* ',' with buf cnt */
			*comma_cnt = sdiohal_extract_num(p, i);
		} else if (p[i] == '\0')
			break;
	}

}

static int sdiohal_simple_test_tx(size_t count)
{
	struct mbuf_t *head, *tail, *temp;
	int tx_debug_num = 4;
	int i;

	memset(test_buf, 0x30, 256);
	memset(test_buf + 256, 0x31, 256);
	memset(test_buf + 512, 0x32, 256);
	memset(test_buf + 768, 0x33, 256);

	if (!sprdwcn_bus_list_alloc(at_tx_ops.channel,
		&head, &tail, &tx_debug_num)) {
		if (tx_debug_num >= 4) {
			WCN_INFO("%s tx_debug_num=%d head:%p\n",
				 __func__, tx_debug_num, head);
			head->buf = test_buf;
			head->len = count;
			temp = head->next;

			for (i = 1; i < 4; i++) {
				if (!temp)
					break;
				temp->buf = &test_buf[256*i];
				temp->len = count;
				if (temp)
					temp = temp->next;
			}
			sprdwcn_bus_push_list(at_tx_ops.channel,
				head, tail, tx_debug_num);
		} else
			WCN_INFO("%s tx_debug_num=%d < 5\n",
				 __func__, tx_debug_num);
	}

	return 0;
}

static int sdiohal_simple_test_rx(void)
{
	return 0;
}

static int sdiohal_throughput_tx(void)
{
	struct mbuf_t *head, *tail, *temp;
	int tx_debug_num = tp_tx_buf_cnt;
	int i = 0;
	int buf_len = tp_tx_buf_len;
	int ret = 0;

	if (!sprdwcn_bus_list_alloc(AT_TX_CHANNEL,
				    &head, &tail, &tx_debug_num)) {
		if (tx_debug_num >= tp_tx_buf_cnt) {
			/* linked list */
			temp = head;
			for (i = 0; i < tp_tx_buf_cnt; i++) {
				temp->buf = tp_tx_buf[i];
				temp->len = buf_len;
				if ((i + 1) < tp_tx_buf_cnt)
					temp = temp->next;
				else
					temp->next = NULL;
			}
			ret = sprdwcn_bus_push_list(AT_TX_CHANNEL,
				head, tail, tx_debug_num);
			if (ret)
				WCN_INFO("send_data_func failed!!!\n");
			return 0;
		}

		sprdwcn_bus_list_free(AT_TX_CHANNEL, head, tail,
				      tx_debug_num);
		WCN_INFO("%s tx_debug_num=%d < %d\n",
			 __func__, tx_debug_num, tp_tx_buf_cnt);

		return -ENOMEM;
	}

	return -ENOMEM;
}

static void sdiohal_throughput_tx_compute_time(void)
{
	static signed long long times_count;

	if (tp_tx_flag != 1)
		return;

	/* throughput test */
	tp_tx_cnt++;
	if (tp_tx_cnt % 500 == 0) {
		do_gettimeofday(&tp_tx_stop_time);
		tp_tx_cnt = 0;
		times_count = timeval_to_ns(&tp_tx_stop_time)
			- timeval_to_ns(&tp_tx_start_time);
		WCN_INFO("tx -> times(500c) is %lld\n",
			 times_count);
		do_gettimeofday(&tp_tx_start_time);
	}
	sdiohal_throughput_tx();
}

static int sdiohal_throughput_rx(void)
{
	return 0;
}

#if TCP_TEST_RX
static int sdiohal_throughput_tx_thread(void *data)
{
	int i = 0, rx_pop_cnt_old = 0;

	while (1) {
		sdiohal_tp_rx_down();
		rx_pop_cnt_old = rx_pop_cnt - rx_pop_cnt_old;
		tp_tx_buf_cnt = 1;
		tp_tx_buf_len = 100;
		for (i = 0; i < rx_pop_cnt_old; i = i + (TCP_TEST_1VS2 ? 2 : 1))
			sdiohal_throughput_tx_compute_time();
	}

	return 0;
}

static void sdiohal_launch_tp_tx_thread(void)
{
	struct task_struct *tx_thread = NULL;

	init_completion(&tp_rx_completed);

	tx_thread =
		kthread_create(sdiohal_throughput_tx_thread,
		NULL, "sdiohal_tp_tx_thread");
	if (tx_thread)
		wake_up_process(tx_thread);
	else
		WCN_ERR("create sdiohal_tp_tx_thread fail\n");
}
#endif

static void sdiohal_tx_send(int chn);

static int sdiohal_tx_thread_chn6(void *data)
{
	do {
		sdiohal_tx_send(6);
	} while (1);

	return 0;
}

static int sdiohal_tx_thread_chn7(void *data)
{
	do {
		sdiohal_tx_send(7);
	} while (1);

	return 0;
}

static int sdiohal_tx_thread_chn8(void *data)
{
	do {
		sdiohal_tx_send(8);
	} while (1);

	return 0;
}

static int sdiohal_tx_thread_chn9(void *data)
{
	do {
		sdiohal_tx_send(9);
	} while (1);

	return 0;
}

static int sdiohal_tx_thread_chn10(void *data)
{
	do {
		sdiohal_tx_send(10);
	} while (1);

	return 0;
}

static int sdiohal_tx_thread_chn11(void *data)
{
	do {
		sdiohal_tx_send(11);
	} while (1);

	return 0;
}

struct sdiohal_test_thread_info_t {
	char *thread_name;
	int (*thread_func)(void *data);
	struct completion tx_completed;
};

static struct sdiohal_test_thread_info_t sdiohal_thread_info[] = {
	{
		.thread_name = "sdiohal_tx_thread_chn6",
		.thread_func = sdiohal_tx_thread_chn6,
	},
	{
		.thread_name = "sdiohal_tx_thread_chn7",
		.thread_func = sdiohal_tx_thread_chn7,
	},
	{
		.thread_name = "sdiohal_tx_thread_chn8",
		.thread_func = sdiohal_tx_thread_chn8,
	},
	{
		.thread_name = "sdiohal_tx_thread_chn9",
		.thread_func = sdiohal_tx_thread_chn9,
	},
	{
		.thread_name = "sdiohal_tx_thread_chn10",
		.thread_func = sdiohal_tx_thread_chn10,
	},
	{
		.thread_name = "sdiohal_tx_thread_chn11",
		.thread_func = sdiohal_tx_thread_chn11,
	},

};

static void sdiohal_tx_send(int chn)
{
	struct mbuf_t *head, *tail, *mbuf_node;
	int num = 5;
	int ret, i;

	wait_for_completion(&sdiohal_thread_info[chn - 6].tx_completed);

	if (!sprdwcn_bus_list_alloc(chn, &head, &tail, &num)) {
		if (num >= 5) {
			mbuf_node = head;
			for (i = 0; i < num; i++) {
				mbuf_node->buf = kzalloc(TX_MULTI_BUF_SIZE +
							 PUB_HEAD_RSV,
							 GFP_KERNEL);
				if (!mbuf_node->buf) {
					sdiohal_err(" mbuf_node->buf kzalloc memory fail");
					return;
					}
				mbuf_node->len = TX_MULTI_BUF_SIZE;
				if ((i + 1) < num)
					mbuf_node = mbuf_node->next;
				else
					mbuf_node->next = NULL;
			}

			WCN_INFO("%s channel:%d num:%d\n",
				 __func__, chn, num);

			ret = sprdwcn_bus_push_list(chn, head, tail, num);
			if (ret)
				WCN_ERR("send_data_func failed, num:%d\n",
					num);
		} else
			WCN_INFO("%s alloced mbuf num=%d < 8\n",
				 __func__,	num);
	}

}

static int sdiohal_tx_muti_channel_pop(int channel, struct mbuf_t *head,
		   struct mbuf_t *tail, int num)
{
	struct mbuf_t *mbuf_node;
	int i;

	sdiohal_debug("%s channel:%d head:%p tail:%p num:%d\n",
		      __func__, channel, head, tail, num);

	if (channel < 12) {
		for (mbuf_node = head, i = 0; i < num; i++,
			mbuf_node = mbuf_node->next) {
			kfree(mbuf_node->buf);
			mbuf_node->buf = NULL;
		}
		sprdwcn_bus_list_free(channel, head, tail, num);
		complete(&sdiohal_thread_info[channel - 6].tx_completed);
	} else
		WCN_ERR("channel err:%d\n", channel);

	return 0;
}

static void sdiohal_tx_test_init(void)
{
	struct task_struct *tx_thread = NULL;
	struct mchn_ops_t *tx_test_ops;
	int chn_num = 6, chn;


	for (chn = 0; chn < chn_num; chn++) {
		tx_test_ops = kzalloc(sizeof(struct mchn_ops_t), GFP_KERNEL);
		if (!tx_test_ops) {
			WCN_ERR("sdio tx test,alloc mem fail\n");
			return;
		}

		tx_test_ops->channel = chn + chn_num;
		tx_test_ops->hif_type = HW_TYPE_SDIO;
		tx_test_ops->inout = SDIOHAL_DIR_TX;
		tx_test_ops->pool_size = 5;
		tx_test_ops->pop_link = sdiohal_tx_muti_channel_pop;

		if (sprdwcn_bus_chn_init(tx_test_ops)) {
			sprdwcn_bus_chn_deinit(tx_test_ops);
			sprdwcn_bus_chn_init(tx_test_ops);
		}

		init_completion(&sdiohal_thread_info[chn].tx_completed);
		tx_thread =
			kthread_create(sdiohal_thread_info[chn].thread_func,
					NULL,
					sdiohal_thread_info[chn].thread_name);
		if (tx_thread)
			wake_up_process(tx_thread);
		else {
			WCN_ERR("create sdiohal_tx_thread fail\n");
			return;
		}
		complete(&sdiohal_thread_info[chn].tx_completed);
	}
}

static int sdiohal_rx_muti_channel_pop(int channel, struct mbuf_t *head,
		   struct mbuf_t *tail, int num)
{
	int i;

	WCN_INFO("%s channel:%d head:%p tail:%p num:%d\n",
		 __func__, channel, head, tail, num);

	for (i = 0; i < (head->len < 80 ? head->len:80); i++)
		WCN_INFO("%s i%d 0x%x\n", __func__, i, head->buf[i]);

	sprdwcn_bus_push_list(channel, head, tail, num);

	return 0;
}

static void sdiohal_rx_test_init(void)
{
	struct mchn_ops_t *rx_test_ops;
	int chn_num = 6, chn;


	for (chn = 0; chn < chn_num; chn++) {
		rx_test_ops = kzalloc(sizeof(struct mchn_ops_t), GFP_KERNEL);
		if (!rx_test_ops)
			return;

		rx_test_ops->channel = chn + 8 + 12;
		rx_test_ops->hif_type = HW_TYPE_SDIO;
		rx_test_ops->inout = SDIOHAL_DIR_RX;
		rx_test_ops->pool_size = 1;
		rx_test_ops->pop_link = sdiohal_rx_muti_channel_pop;

		if (sprdwcn_bus_chn_init(rx_test_ops)) {
			sprdwcn_bus_chn_deinit(rx_test_ops);
			sprdwcn_bus_chn_init(rx_test_ops);
		}
	}
}

static int at_list_tx_pop(int channel, struct mbuf_t *head,
		   struct mbuf_t *tail, int num)
{
	int i;

	sdiohal_debug("%s channel:%d head:%p tail:%p num:%d\n",
		      __func__, channel, head, tail, num);

	for (i = 0; i < (head->len < 80 ? head->len:80); i++)
		sdiohal_debug("%s i%d 0x%x\n", __func__, i, head->buf[i]);

	sdiohal_debug("%s len:%d buf:%s\n",
		      __func__, head->len, head->buf + 4);

	sprdwcn_bus_list_free(channel, head, tail, num);

#if (!TCP_TEST_RX)
	sdiohal_throughput_tx_compute_time();
#endif

	return 0;
}

static int at_list_rx_pop(int channel, struct mbuf_t *head,
		   struct mbuf_t *tail, int num)
{
	int i;

	WCN_INFO("%s channel:%d head:%p tail:%p num:%d\n",
		 __func__, channel, head, tail, num);

	for (i = 0; i < (head->len < 80 ? head->len:80); i++)
		WCN_INFO("%s i%d 0x%x\n", __func__, i, head->buf[i]);

	WCN_INFO("%s len:%d buf:%s\n",
		 __func__, head->len, head->buf + 4);

	sprdwcn_bus_push_list(at_rx_ops.channel, head, tail, num);
#if TCP_TEST_RX
	rx_pop_cnt += num;
	sdiohal_tp_rx_up();
#endif

	return 0;
}

static struct mchn_ops_t at_tx_ops = {
	.channel = AT_TX_CHANNEL,
	.hif_type = HW_TYPE_SDIO,
	.inout = SDIOHAL_DIR_TX,
	.pool_size = 13,
	.pop_link = at_list_tx_pop,
};

static struct mchn_ops_t at_rx_ops = {
	.channel = AT_RX_CHANNEL,
	.hif_type = HW_TYPE_SDIO,
	.inout = SDIOHAL_DIR_RX,
	.pool_size = 1,
	.pop_link = at_list_rx_pop,
};

static int at_cmd_init(void)
{
	sprdwcn_bus_chn_init(&at_tx_ops);
	sprdwcn_bus_chn_init(&at_rx_ops);

	return 0;
}

static int at_cmd_deinit(void)
{
	sprdwcn_bus_chn_deinit(&at_tx_ops);
	sprdwcn_bus_chn_deinit(&at_rx_ops);

	return 0;
}

static char *sdiohal_firmware_data(unsigned long int imag_size)
{
	int read_len, size;
	char *buffer = NULL;
	char *data = NULL;
	struct file *file;
	loff_t pos = 0;

	WCN_INFO("%s entry\n", __func__);
	file = filp_open(FIRMWARE_PATH, O_RDONLY, 0);
	if (IS_ERR(file)) {
		WCN_ERR("%s open file %s error\n",
			FIRMWARE_PATH, __func__);
		return NULL;
	}
	WCN_INFO("marlin %s open image file  successfully\n", __func__);
	size = imag_size;
	buffer = vmalloc(size);
	if (!buffer) {
		fput(file);
		WCN_ERR("%s no memory\n", __func__);
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

	return data;
}

static int sdiohal_download_firmware(void)
{
	int err, len, trans_size;
	unsigned long int img_size;
	char *buffer = NULL;
	char *temp_buf;

	img_size = FIRMWARE_MAX_SIZE;

	WCN_INFO("%s entry\n", __func__);
	buffer = sdiohal_firmware_data(img_size);
	if (!buffer) {
		WCN_ERR("%s buff is NULL\n", __func__);
		return -1;
	}

	len = 0;
	temp_buf = kzalloc(PACKET_SIZE, GFP_KERNEL);
	while (len < img_size) {
		trans_size = (img_size - len) > PACKET_SIZE ?
				PACKET_SIZE : (img_size - len);
		memcpy(temp_buf, buffer + len, trans_size);
		err = sprdwcn_bus_direct_write(CP_START_ADDR + len,
			temp_buf, trans_size);
		if (err < 0) {
			WCN_ERR("marlin %s error:%d\n", __func__, err);
			vfree(buffer);
			kfree(temp_buf);
			return -1;
		}
		len += PACKET_SIZE;
	}
	vfree(buffer);
	kfree(temp_buf);
	WCN_INFO("%s finish\n", __func__);

	return 0;
}

static int sdiohal_public_irq;
struct work_struct sdiohal_int_wq;

static void sdiohal_int_power_wq(struct work_struct *work)
{
	unsigned char reg_pub_int_sts0 = 0;
	unsigned char reg_pub_int_sts1 = 0;

	WCN_INFO("%s entry\n", __func__);
	/* read public interrupt status register */
	sprdwcn_bus_aon_readb(REG_TO_AP_PUB_STS0, &reg_pub_int_sts0);
	sprdwcn_bus_aon_readb(REG_TO_AP_PUB_STS1, &reg_pub_int_sts1);

	sprdwcn_bus_aon_writeb(REG_TO_AP_INT_CLR0, 0xff);
	sprdwcn_bus_aon_writeb(REG_TO_AP_INT_CLR1, 0xff);

	WCN_INFO("PUB INT_STS0-0x%x\n", reg_pub_int_sts0);
	WCN_INFO("PUB INT_STS1-0x%x\n", reg_pub_int_sts1);

	enable_irq(sdiohal_public_irq);
}

/* below is used for gnss data capture function */
static void sdiohal_gnss_dump_wq(struct work_struct *work)
{
	int i = 0, ret = 0;
	unsigned int *reg_val;

	reg_val = kzalloc(GNSS_DUMP_DATA_SIZE, GFP_KERNEL);
	ret = sprdwcn_bus_direct_read(GNSS_DUMP_WIFI_RAM_ADDR,
		reg_val, GNSS_DUMP_DATA_SIZE);
	if (ret < 0) {
		WCN_ERR("%s read reg error:%d\n", __func__, ret);
		return;
	}
	for (i = 0; i < 2000; i++)
		WCN_INFO("%d 0x%x\n", i, reg_val[i]);
}

static irqreturn_t sdiohal_public_isr(int irq, void *para)
{
	disable_irq_nosync(irq);
	schedule_work(&sdiohal_int_wq);

	return IRQ_HANDLED;
}

static int sdiohal_test_int_init(unsigned char func_tag)
{
	struct device_node *np;
	unsigned int pub_gpio_num;
	unsigned char reg_int_en = 0;
	int ret;

	if (func_tag == SDIOHAL_INT_PWR_FUNC)
		INIT_WORK(&sdiohal_int_wq, sdiohal_int_power_wq);
	else if (func_tag == SDIOHAL_GNSS_DUMP_FUNC)
		INIT_WORK(&sdiohal_int_wq, sdiohal_gnss_dump_wq);

	np = of_find_node_by_name(NULL, "sprd-marlin3");
	if (!np) {
		WCN_ERR("dts node not found\n");
		return -1;
	}
	pub_gpio_num = of_get_named_gpio(np, "m2-to-ap-irq-gpios", 0);
	WCN_INFO("pub_gpio_num:%d\n", pub_gpio_num);
	ret = gpio_request(pub_gpio_num, "sdiohal_int_gpio");
	if (ret < 0) {
		WCN_ERR("req gpio irq = %d fail!!!\n", pub_gpio_num);
		return ret;
	}

	ret = gpio_direction_input(pub_gpio_num);
	if (ret < 0) {
		WCN_ERR("public_int, gpio-%d input set fail!!!\n",
			pub_gpio_num);
		return ret;
	}

	sdiohal_public_irq = gpio_to_irq(pub_gpio_num);

	ret = request_irq(sdiohal_public_irq,
			sdiohal_public_isr,
			IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND,
			"sdiohal_test_irq",
			NULL);

	/* enable sdio cp to ap int */
	sprdwcn_bus_aon_writeb(REG_TO_AP_ENABLE_0, 0xff);
	sprdwcn_bus_aon_readb(REG_TO_AP_ENABLE_0, &reg_int_en);
	WCN_INFO("REG_TO_AP_ENABLE_0-0x%x\n", reg_int_en);

	sprdwcn_bus_aon_writeb(REG_TO_AP_ENABLE_1, 0xff);
	sprdwcn_bus_aon_readb(REG_TO_AP_ENABLE_1, &reg_int_en);
	WCN_INFO("REG_TO_AP_ENABLE_1-0x%x\n", reg_int_en);

	return 0;
}

static int at_cmd_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t at_cmd_read(struct file *filp,
	char __user *user_buf, size_t count, loff_t *pos)
{
	return count;
}

static ssize_t at_cmd_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *pos)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct mbuf_t *head, *tail, *mbuf_node;
	int num = 1, i;
	long int channel;
	int ret;

	if (count > SDIOHAL_WRITE_SIZE) {
		WCN_ERR("%s write size > %d\n",
			__func__, SDIOHAL_WRITE_SIZE);
		return -ENOMEM;
	}

	memset(buf, 0, SDIOHAL_WRITE_SIZE);
	if (copy_from_user(buf + PUB_HEAD_RSV, user_buf, count))
		return -EFAULT;

	WCN_INFO("%s write :%s\n", __func__, buf + PUB_HEAD_RSV);

	if (strncmp(buf + PUB_HEAD_RSV, "download", 8) == 0) {
		sdiohal_download_firmware();
		return count;
	}

	/* dedicated int1 is reusable with wifi analog iq monitor */
	if (strncmp(buf + PUB_HEAD_RSV, "switch_irq", 10) == 0) {
		p_data->debug_iq = true;
		WCN_INFO("%s switch irq to [%d][%s]\n",
			 __func__, p_data->debug_iq,
			 (p_data->debug_iq ? "wifi analog IQ" :
			 "sdio dedicated int"));
		sdiohal_rx_up();

		return count;
	}

	if (strncmp(buf + PUB_HEAD_RSV, "tx_multi_task", 13) == 0) {
		sdiohal_tx_test_init();
		return count;
	}

	if (strncmp(buf + PUB_HEAD_RSV, "rx_multi_task", 13) == 0) {
		sdiohal_rx_test_init();
		return count;
	}

	if (strncmp(buf + PUB_HEAD_RSV, "log_level=", 10) == 0) {
		buf[SDIOHAL_WRITE_SIZE + PUB_HEAD_RSV - 1] = 0;
		ret = kstrtol(&buf[PUB_HEAD_RSV + sizeof("log_level=") - 1],
			10, &sdiohal_log_level);
		WCN_INFO("%s sdiohal_log_level:%ld\n",
			 __func__, sdiohal_log_level);
		return count;
	}

	if (strncmp(buf + PUB_HEAD_RSV, "sdio_int", 8) == 0) {
		unsigned long int int_bitmap;
		unsigned int addr;

		if (strncmp(buf + PUB_HEAD_RSV, "sdio_int_rx", 11) == 0)
			sdiohal_test_int_init(SDIOHAL_INT_PWR_FUNC);
		else if (strncmp(buf + PUB_HEAD_RSV,
				"sdio_int_tx", 11) == 0) {
			buf[SDIOHAL_WRITE_SIZE + PUB_HEAD_RSV - 1] = 0;
			ret = kstrtol(&buf[PUB_HEAD_RSV +
				sizeof("sdio_int_tx=") - 1], 10, &int_bitmap);

			WCN_INFO("%s int_bitmap:%ld\n",
				 __func__, int_bitmap);

			if (int_bitmap & 0xff) {
				addr = REG_TO_CP0_REQ0;
				sprdwcn_bus_aon_writeb(addr, int_bitmap & 0xff);
			}

			if (int_bitmap & 0xff00) {
				addr = REG_TO_CP0_REQ1;
				sprdwcn_bus_aon_writeb(addr, int_bitmap >> 8);
			}

		}

		return count;
	}

	if (strncmp(buf + PUB_HEAD_RSV, "at_init tx chn=", 15) == 0) {
		buf[SDIOHAL_WRITE_SIZE + PUB_HEAD_RSV - 1] = 0;
		ret = kstrtol(&buf[PUB_HEAD_RSV
			+ sizeof("at_init tx chn=") - 1], 10, &channel);
		WCN_INFO("%s tx channel:%ld\n", __func__, channel);
		at_tx_ops.channel = channel;
		sprdwcn_bus_chn_init(&at_tx_ops);
		return count;
	}

	if (strncmp(buf + PUB_HEAD_RSV, "at_init rx chn=", 15) == 0) {
		buf[SDIOHAL_WRITE_SIZE + PUB_HEAD_RSV - 1] = 0;
		ret = kstrtol(&buf[PUB_HEAD_RSV
			+ sizeof("at_init rx chn=") - 1], 10, &channel);
		WCN_INFO("%s rx channel:%ld\n", __func__, channel);
		at_rx_ops.channel = channel;
		sprdwcn_bus_chn_init(&at_rx_ops);
		return count;
	}

	if (strncmp(buf + PUB_HEAD_RSV, "at_deinit", 8) == 0) {
		sprdwcn_bus_chn_deinit(&at_tx_ops);
		sprdwcn_bus_chn_deinit(&at_rx_ops);
		return count;
	}

	/* below is used for gnss data capture function, irq TBD */
	if (strncmp(buf + PUB_HEAD_RSV, "gnss_data_cap", 13) == 0) {
		sdiohal_test_int_init(SDIOHAL_GNSS_DUMP_FUNC);
		return count;
	}

	/* sdio basic function test */
	if (strstr((buf + PUB_HEAD_RSV), "simple_test_tx")) {
		sdiohal_simple_test_tx(240);
		return count;
	} else if (strstr((buf + PUB_HEAD_RSV), "simple_test_rx")) {
		sdiohal_simple_test_rx();
		return count;
	}

	/* sdio throughput test */
	if (strstr((buf + PUB_HEAD_RSV), "tp")) {
		sdiohal_find_num(buf + PUB_HEAD_RSV,
			&tp_tx_buf_cnt, &tp_tx_buf_len);
		WCN_INFO("%s buf_cnt=%d buf_len=%d\n",
			 __func__, tp_tx_buf_cnt, tp_tx_buf_len);
		tp_tx_flag = 1;
		tp_tx_cnt = 0;
		do_gettimeofday(&tp_tx_start_time);
		if ((tp_tx_buf_cnt <= TP_TX_BUF_CNT) &&
			(tp_tx_buf_len <= TP_TX_BUF_LEN)) {
#if TCP_TEST_RX
			sdiohal_launch_tp_tx_thread();
#endif
			sdiohal_log_level = 0;
			sdiohal_throughput_tx();
		} else
			WCN_INFO("%s buf_cnt or buf_len false!!\n",
				 __func__);
		return count;
	} else if (strstr((buf + PUB_HEAD_RSV), "tp_test_rx")) {
		sdiohal_throughput_rx();
		return count;
	}

	if (!sprdwcn_bus_list_alloc(at_tx_ops.channel, &head, &tail, &num)) {
		mbuf_node = head;
		for (i = 0; i < num; i++) {
			memcpy(buf_list[i], buf, count + PUB_HEAD_RSV);
			mbuf_node->buf = buf_list[i];
			mbuf_node->len = count;
			if ((i+1) < num)
				mbuf_node = mbuf_node->next;
			else
				mbuf_node->next = NULL;
		}

		sprdwcn_bus_push_list(at_tx_ops.channel, head, tail, num);
	}

	return count;
}

static int at_cmd_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations at_cmd_fops = {
	.open = at_cmd_open,
	.read = at_cmd_read,
	.write = at_cmd_write,
	.release = at_cmd_release,
};

static int debug_help_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t debug_help_read(struct file *filp,
	char __user *user_buf, size_t count, loff_t *pos)
{
	return count;
}

static int debug_help_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations debug_help = {
	.open = debug_help_open,
	.read = debug_help_read,
	.release = debug_help_release,
};

struct entry_file {
	const char *name;
	const struct file_operations *file_ops;
};

static struct entry_file entry_table[] = {
	{
		.name = "help",
		.file_ops = &debug_help,
	},
	{
		.name = "at_cmd",
		.file_ops = &at_cmd_fops,
	},
};

static struct dentry *debug_root;
void sdiohal_debug_init(void)
{
	int i;

	/* create debugfs */
	debug_root = debugfs_create_dir("sdiohal_debug", NULL);
	for (i = 0; i < ARRAY_SIZE(entry_table); i++) {
		if (!debugfs_create_file(entry_table[i].name, 0444,
					 debug_root, NULL,
					 entry_table[i].file_ops)) {
			WCN_ERR("%s debugfs_create_file[%d] fail!!\n",
				__func__, i);
			debugfs_remove_recursive(debug_root);
			return;
		}
	}

	at_cmd_init();
}

void sdiohal_debug_deinit(void)
{
	debugfs_remove_recursive(debug_root);
	at_cmd_deinit();
}

