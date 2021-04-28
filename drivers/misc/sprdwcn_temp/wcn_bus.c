/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * Authors	: jinglong.chen
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <misc/wcn_bus.h>

#include "include/wcn_dbg.h"

struct buffer_pool_t {
	int size;
	int free;
	int payload;
	void *head;
	char *mem;
	spinlock_t lock;
};

struct chn_info_t {
	struct mchn_ops_t *ops[CHN_MAX_NUM];
	struct mutex callback_lock[CHN_MAX_NUM];
	struct buffer_pool_t pool[CHN_MAX_NUM];
};

static struct sprdwcn_bus_ops *wcn_bus_ops;

static struct chn_info_t g_chn_info;
static struct chn_info_t *chn_info(void)
{
	return &g_chn_info;
}

static int buf_list_check(struct buffer_pool_t *pool,
			  struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	int i;
	struct mbuf_t *mbuf;

	if (num == 0)
		return 0;
	for (i = 0, mbuf = head; i < num; i++) {
		if ((i == (num - 1)) && (mbuf != tail)) {
			WCN_ERR("%s(0x%lx, 0x%lx, %d), err 1\n", __func__,
				(unsigned long)virt_to_phys(head),
				(unsigned long)virt_to_phys(tail), num);
			WARN_ON(1);
		}
		WARN_ON(!mbuf);
		WARN_ON((char *)mbuf < pool->mem ||
			(char *)mbuf > pool->mem + ((sizeof(struct mbuf_t)
			+ pool->payload) * pool->size));
		mbuf = mbuf->next;
	}

	if (tail->next != NULL) {
		WCN_ERR("%s(0x%lx, 0x%lx, %d), err 2\n", __func__,
			(unsigned long)virt_to_phys(head),
			(unsigned long)virt_to_phys(tail), num);
		WARN_ON(1);
	}

	return 0;
}

static int buf_pool_check(struct buffer_pool_t *pool)
{
	int i;
	struct mbuf_t *mbuf;

	for (i = 0, mbuf = pool->head;
	     i < pool->free; i++, mbuf = mbuf->next) {
		WARN_ON(!mbuf);
		WARN_ON((char *)mbuf < pool->mem ||
			(char *)mbuf > pool->mem + ((sizeof(struct mbuf_t)
			+ pool->payload) * pool->size));
	}

	if (mbuf != NULL) {
		WCN_ERR("%s(0x%p) err\n", __func__, pool);
		WARN_ON(1);
	}

	return 0;
}

/* mbuf init and list, current payload is zero */
static int buf_pool_init(struct buffer_pool_t *pool, int size, int payload)
{
	int i;
	struct mbuf_t *mbuf, *next;

	pool->size = size;
	pool->payload = payload;
	spin_lock_init(&(pool->lock));
	pool->mem = kzalloc((sizeof(struct mbuf_t) + payload) * size,
			    GFP_KERNEL);
	if (!pool->mem)
		return -ENOMEM;

	WCN_INFO("mbuf_pool->mem:0x%lx\n",
		 (unsigned long)virt_to_phys(pool->mem));
	pool->head = (struct mbuf_t *) (pool->mem);
	for (i = 0, mbuf = (struct mbuf_t *)(pool->head);
	     i < (size - 1); i++) {
		mbuf->seq = i;
		WCN_INFO("%s mbuf[%d]:{0x%lx, 0x%lx}\n", __func__, i,
			 (unsigned long)mbuf,
			 (unsigned long)virt_to_phys(mbuf));
		next = (struct mbuf_t *)((char *)mbuf +
			sizeof(struct mbuf_t) + payload);
		mbuf->buf = (char *)mbuf + sizeof(struct mbuf_t);
		mbuf->len = payload;
		mbuf->next = next;
		mbuf = next;
	}
	WCN_INFO("%s mbuf[%d]:{0x%lx, 0x%lx}\n", __func__, i,
		 (unsigned long)mbuf,
		 (unsigned long)virt_to_phys(mbuf));
	mbuf->seq = i;
	mbuf->buf = (char *)mbuf + sizeof(struct mbuf_t);
	mbuf->len = payload;
	mbuf->next = NULL;
	pool->free = size;

	return 0;
}

static int buf_pool_deinit(struct buffer_pool_t *pool)
{
	memset(pool->mem, 0x00,
	       (sizeof(struct mbuf_t) + pool->payload) * pool->size);
	kfree(pool->mem);
	pool->mem = NULL;

	return 0;
}

/* take mbuf from pool list */
int buf_list_alloc(int chn, struct mbuf_t **head,
		   struct mbuf_t **tail, int *num)
{
	int i;
	struct buffer_pool_t *pool;
	struct mbuf_t *cur, *temp_head, *temp_tail = NULL;
	struct chn_info_t *chn_inf = chn_info();

	pool = &(chn_inf->pool[chn]);

	if ((*num <= 0) || (pool->free <= 0)) {
		WCN_ERR("[+]%s err, num %d, free %d)\n",
			__func__, *num, pool->free);
		*num = 0;
		*head = *tail = NULL;
		return -1;
	}

	spin_lock_bh(&(pool->lock));
	buf_pool_check(pool);
	if (*num > pool->free)
		*num = pool->free;

	for (i = 0, cur = temp_head = pool->head; i < *num; i++) {
		if (i == (*num - 1))
			temp_tail = cur;
		cur = cur->next;
	}
	*head = temp_head;
	if (temp_tail)
		temp_tail->next = NULL;
	*tail = temp_tail;
	pool->free -= *num;
	pool->head = cur;
	buf_list_check(pool, *head, *tail, *num);
	spin_unlock_bh(&(pool->lock));

	return 0;
}
EXPORT_SYMBOL(buf_list_alloc);

int buf_list_free(int chn, struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	struct buffer_pool_t *pool;
	struct chn_info_t *chn_inf = chn_info();

	if ((head == NULL) || (tail == NULL) || (num == 0)) {
		WCN_ERR("%s(%d, 0x%lx, 0x%lx, %d)\n", __func__, chn,
			(unsigned long)virt_to_phys(head),
			(unsigned long)virt_to_phys(tail), num);
		return -1;
	}

	pool = &(chn_inf->pool[chn]);
	spin_lock_bh(&(pool->lock));
	buf_list_check(pool, head, tail, num);
	tail->next = pool->head;
	pool->head = head;
	pool->free += num;
	buf_pool_check(pool);
	spin_unlock_bh(&(pool->lock));

	return 0;
}
EXPORT_SYMBOL(buf_list_free);

int bus_chn_init(struct mchn_ops_t *ops, int hif_type)
{
	int ret = 0;
	struct chn_info_t *chn_inf = chn_info();

	WCN_INFO("[+]%s(%d, %d)\n", __func__, ops->channel, ops->hif_type);
	if (chn_inf->ops[ops->channel] != NULL) {
		WCN_ERR("%s err, hif_type %d\n", __func__, ops->hif_type);
		WARN_ON(1);
		return -1;
	}

	mutex_init(&chn_inf->callback_lock[ops->channel]);
	mutex_lock(&chn_inf->callback_lock[ops->channel]);
	ops->hif_type = hif_type;
	chn_inf->ops[ops->channel] = ops;
	if (ops->pool_size > 0)
		ret = buf_pool_init(&(chn_inf->pool[ops->channel]),
				    ops->pool_size, 0);
	mutex_unlock(&chn_inf->callback_lock[ops->channel]);

	WCN_INFO("[-]%s(%d)\n", __func__, ops->channel);

	return ret;
}
EXPORT_SYMBOL(bus_chn_init);

int bus_chn_deinit(struct mchn_ops_t *ops)
{
	int ret = 0;
	struct chn_info_t *chn_inf = chn_info();

	WCN_INFO("[+]%s(%d, %d)\n", __func__, ops->channel, ops->hif_type);
	if (chn_inf->ops[ops->channel] == NULL) {
		WCN_ERR("%s err\n", __func__);
		return -1;
	}

	mutex_lock(&chn_inf->callback_lock[ops->channel]);
	if (ops->pool_size > 0)
		ret = buf_pool_deinit(&(chn_inf->pool[ops->channel]));
	chn_inf->ops[ops->channel] = NULL;
	mutex_unlock(&chn_inf->callback_lock[ops->channel]);
	mutex_destroy(&chn_inf->callback_lock[ops->channel]);

	WCN_INFO("[-]%s(%d)\n", __func__, ops->channel);

	return ret;
}
EXPORT_SYMBOL(bus_chn_deinit);

struct mchn_ops_t *chn_ops(int channel)
{
	if (channel >= CHN_MAX_NUM || channel < 0)
		return NULL;

	return g_chn_info.ops[channel];
}
EXPORT_SYMBOL(chn_ops);

int module_ops_register(struct sprdwcn_bus_ops *ops)
{
	if (wcn_bus_ops) {
		WARN_ON(1);
		return -EBUSY;
	}

	wcn_bus_ops = ops;

	return 0;
}
EXPORT_SYMBOL(module_ops_register);

void module_ops_unregister(void)
{
	wcn_bus_ops = NULL;
}
EXPORT_SYMBOL(module_ops_unregister);

struct sprdwcn_bus_ops *get_wcn_bus_ops(void)
{
	return wcn_bus_ops;
}
EXPORT_SYMBOL(get_wcn_bus_ops);

