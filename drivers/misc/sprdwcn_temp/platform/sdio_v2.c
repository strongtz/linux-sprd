#include <linux/slab.h>
#include <linux/spinlock.h>
#include <misc/wcn_bus.h>

#include "bus_common.h"
#include "../sdiom/sdiom_depend.h"
#include "../sdiom/sdiom_rx_recvbuf.h"

#define SDIO_CHN_TX_NUM 16
#define SDIO_CHN_RX_NUM 16

struct sdio_tx_info_t {
	struct list_head node;
	struct mbuf_t *mbuf_node;
};

struct sdio_rx_info_t {
	struct list_head node;
	unsigned char *addr;
	unsigned int len;
	unsigned int fifo_id;
};

struct sdio_chn_info_t {
	struct list_head chn_head[CHN_MAX_NUM];
	spinlock_t tx_spinlock;
	spinlock_t rx_spinlock;
};

static struct sdio_chn_info_t *sdio_chn_info;

static void sdio_channel_to_hwtype(int inout, int channel,
	unsigned int *type, unsigned int *subtype)
{
	if (inout) {
		*type = channel / 4;
		*subtype = channel % 4;
	} else {
		channel -= SDIO_CHN_TX_NUM;
		*type = channel / 4;
		*subtype = channel % 4;
	}

	sdiom_print("%s type:%d, subtype:%d\n", __func__, *type, *subtype);
}

static int sdio_hwtype_to_channel(int inout, unsigned int type,
	unsigned int subtype)
{
	int channel = -1;

	if (inout)
		channel = type * 4 + subtype;
	else
		channel = type * 4 + subtype + SDIO_CHN_TX_NUM;

	sdiom_print("%s channel:%d\n", __func__, channel);

	return channel;
}

static struct sdio_chn_info_t *sdio_get_chn_info(void)
{
	return sdio_chn_info;
}

static int sdio_module_init(void)
{
	int i;
	struct sdio_chn_info_t *chn_info;

	chn_info = kzalloc(sizeof(struct sdio_chn_info_t), GFP_KERNEL);
	if (!chn_info)
		return -ENOMEM;

	sdio_chn_info = chn_info;
	for (i = 0; i < CHN_MAX_NUM; i++)
		INIT_LIST_HEAD(&chn_info->chn_head[i]);

	spin_lock_init(&chn_info->tx_spinlock);
	spin_lock_init(&chn_info->rx_spinlock);

	return 0;
}

static void sdio_module_deinit(void)
{
	kfree(sdio_chn_info);
}

static unsigned int sdio_tx_cb(void *addr)
{
	int chn;
	struct mchn_ops_t *sdiom_ops = NULL;
	struct sdio_tx_info_t *tx_tmp1, *tx_tmp2;
	struct sdio_tx_info_t *tx_info = NULL;
	struct sdio_chn_info_t *chn_info = sdio_get_chn_info();

	spin_lock_bh(&chn_info->tx_spinlock);
	for (chn = 0; chn < SDIO_CHN_TX_NUM; chn++) {
		if (list_empty(&chn_info->chn_head[chn]))
			continue;

		list_for_each_entry_safe(tx_tmp1, tx_tmp2,
					 &chn_info->chn_head[chn], node) {
			if ((tx_tmp1->mbuf_node->buf +
				PUB_HEAD_RSV) == addr) {
				list_del(&tx_tmp1->node);
				tx_info = tx_tmp1;
				goto out;
			}
		}
	}
out:
	spin_unlock_bh(&chn_info->tx_spinlock);

	if ((chn < SDIO_CHN_TX_NUM) && (tx_info != NULL)) {
		sdiom_ops = chn_ops(chn);
		if (sdiom_ops)
			sdiom_ops->pop_link(chn, tx_info->mbuf_node,
					    tx_info->mbuf_node, 1);
		kfree(tx_info);
	}

	if (chn >= SDIO_CHN_TX_NUM || !sdiom_ops) {
		sdiom_info("tx cb chn:%d sdiom_ops:%p addr:%p\n",
			   chn, sdiom_ops, addr);
		kfree(addr);
	}

	return 0;
}

static int sdio_rx_cb(void *addr,
			       unsigned int len, unsigned int fifo_id)

{
	int ret = -1, chn;
	struct mbuf_t *mbuf_node;
	struct mchn_ops_t *sdiom_ops = NULL;
	struct sdio_puh_t *puh;
	struct sdio_rx_info_t *rx_info;
	struct sdio_chn_info_t *chn_info = sdio_get_chn_info();

	mbuf_node = kmalloc(sizeof(struct mbuf_t), GFP_KERNEL);
	if (!mbuf_node) {
		ret =  -ENOMEM;
		goto err1;
	}

	rx_info = kmalloc(sizeof(struct sdio_rx_info_t), GFP_KERNEL);
	if (!rx_info) {
		ret = -ENOMEM;
		goto err2;
	}

	spin_lock_bh(&chn_info->rx_spinlock);
	mbuf_node->buf = (char *)((char *)addr - 4);
	mbuf_node->len = len;
	puh = (struct sdio_puh_t *)(mbuf_node->buf);
	chn = sdio_hwtype_to_channel(0, puh->type, puh->subtype);
	if (chn < SDIO_CHN_TX_NUM) {
		ret = -EBADF;
		goto err3;
	}

	rx_info->addr = (unsigned char *)addr;
	rx_info->len = len;
	rx_info->fifo_id = fifo_id;
	list_add_tail(&rx_info->node, &chn_info->chn_head[chn]);
	spin_unlock_bh(&chn_info->rx_spinlock);

	sdiom_ops = chn_ops(chn);
	if (sdiom_ops)
		sdiom_ops->pop_link(chn, mbuf_node, mbuf_node, 1);
	else {
		sdiom_info("sdiom no rx ops,chn:%d\n", chn);
		sdiom_pt_read_release(fifo_id);
	}

	return ret;
err3:
	spin_unlock_bh(&chn_info->rx_spinlock);
	kfree(rx_info);
err2:
	kfree(mbuf_node);
err1:
	sdiom_pt_read_release(fifo_id);
	sdiom_err("%s, rx error chn:%d, ret:%d\n", __func__, chn, ret);
	WARN_ON(1);

	return ret;
}

static int sdio_preinit(void)
{
	return sdiom_init();
}

static void sdio_preexit(void)
{
	sdiom_exit();
}

static int sdio_chn_init(struct mchn_ops_t *ops)
{
	int ret = 0;
	unsigned int type = 0, subtype = 0;

	if (ops->channel >= CHN_MAX_NUM)
		return -EINVAL;

	ret = bus_chn_init(ops, HW_TYPE_SDIO);
	if (ret)
		return ret;

	if (ops->channel < SDIO_CHN_TX_NUM) {
		sdio_channel_to_hwtype(1, ops->channel, &type, &subtype);
		sdiom_register_pt_tx_release(type, subtype, sdio_tx_cb);
	} else {
		sdio_channel_to_hwtype(0, ops->channel, &type, &subtype);
		sdiom_register_pt_rx_process(type, subtype, sdio_rx_cb);
	}

	return 0;
}

static int sdio_chn_deinit(struct mchn_ops_t *ops)
{
	return bus_chn_deinit(ops);
}

static int sdio_buf_list_alloc(int chn, struct mbuf_t **head,
			       struct mbuf_t **tail, int *num)
{
	return buf_list_alloc(chn, head, tail, num);
}

static int sdio_buf_list_free(int chn, struct mbuf_t *head,
			      struct mbuf_t *tail, int num)
{
	return buf_list_free(chn, head, tail, num);
}

static int sdio_list_push(int chn, struct mbuf_t *head,
			  struct mbuf_t *tail, int num)
{
	unsigned int type = 0, subtype = 0;
	struct sdio_rx_info_t *rx_info, *rx_tmp;
	struct sdio_tx_info_t *tx_info;
	struct sdio_chn_info_t *chn_info = sdio_get_chn_info();

	if (chn >= CHN_MAX_NUM)
		return -EINVAL;

	if (chn < SDIO_CHN_TX_NUM) {
		tx_info = kmalloc(sizeof(struct sdio_tx_info_t), GFP_KERNEL);
		if (!tx_info)
			return -ENOMEM;

		spin_lock_bh(&chn_info->tx_spinlock);
		tx_info->mbuf_node = head;
		list_add_tail(&tx_info->node, &chn_info->chn_head[chn]);
		spin_unlock_bh(&chn_info->tx_spinlock);

		sdio_channel_to_hwtype(1, chn, &type, &subtype);
		sdiom_pt_write((void *)(head->buf + PUB_HEAD_RSV),
			head->len, type, subtype);
	} else {
		spin_lock_bh(&chn_info->rx_spinlock);
		list_for_each_entry_safe(rx_info, rx_tmp,
					 &chn_info->chn_head[chn], node) {
			if (rx_info->addr == (head->buf + PUB_HEAD_RSV)) {
				list_del(&rx_info->node);
				sdiom_pt_read_release(rx_info->fifo_id);
				kfree(rx_info);
				break;
			}
		}
		kfree(head);
		spin_unlock_bh(&chn_info->rx_spinlock);
	}

	return 0;
}

static int sdio_direct_read(unsigned int addr,
				void *buf, unsigned int len)
{
	return sdiom_dt_read(addr, buf, len);
}

static int sdio_direct_write(unsigned int addr,
				void *buf, unsigned int len)
{
	return sdiom_dt_write(addr, buf, len);
}

static int sdio_readbyte(unsigned int addr, unsigned char *val)
{
	return sdiom_aon_readb(addr, val);
}

static int sdio_writebyte(unsigned int addr, unsigned char val)
{
	return sdiom_aon_writeb(addr, val);
}

static unsigned int sdio_get_carddump_status(void)
{
	return sdiom_get_carddump_status();
}

static void sdio_set_carddump_status(unsigned int flag)
{
	return sdiom_set_carddump_status(flag);
}

static unsigned long long sdio_get_rx_total_cnt(void)
{
	return sdiom_get_rx_total_cnt();
}

static int sdio_rescan(void)
{
	return sdiom_sdio_rescan();
}

static void sdio_register_rescan_cb(void *func)
{
	return sdiom_register_rescan_cb(func);
}

static void sdio_remove_card(void)
{
	return sdiom_remove_card();
}

static int sdio_driver_register(void)
{
	return sdiom_driver_register();
}

static void sdio_driver_unregister(void)
{
	sdiom_driver_unregister();
}

static struct sprdwcn_bus_ops sdiom_bus_ops = {
	.preinit = sdio_preinit,
	.deinit = sdio_preexit,

	.chn_init = sdio_chn_init,
	.chn_deinit = sdio_chn_deinit,
	.list_alloc = sdio_buf_list_alloc,
	.list_free = sdio_buf_list_free,
	.push_list = sdio_list_push,

	.direct_read = sdio_direct_read,
	.direct_write = sdio_direct_write,
	.readbyte = sdio_readbyte,
	.writebyte = sdio_writebyte,

	.get_carddump_status = sdio_get_carddump_status,
	.set_carddump_status = sdio_set_carddump_status,
	.get_rx_total_cnt = sdio_get_rx_total_cnt,

	.register_rescan_cb = sdio_register_rescan_cb,
	.rescan = sdio_rescan,
	.remove_card = sdio_remove_card,

	/* v2 */
	.register_pt_rx_process = sdiom_register_pt_rx_process,
	.register_pt_tx_release = sdiom_register_pt_tx_release,
	.pt_write = sdiom_pt_write,
	.pt_read_release = sdiom_pt_read_release,

	.driver_register = sdio_driver_register,
	.driver_unregister = sdio_driver_unregister,
};


void module_bus_init(void)
{
	sdio_module_init();

	module_ops_register(&sdiom_bus_ops);
}
EXPORT_SYMBOL(module_bus_init);

void module_bus_deinit(void)
{
	module_ops_unregister();
	sdio_module_deinit();
}
EXPORT_SYMBOL(module_bus_deinit);

