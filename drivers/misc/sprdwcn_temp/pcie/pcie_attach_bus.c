/*
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <misc/wcn_bus.h>

#include "bus_common.h"
#include "edma_engine.h"
#include "mchn.h"
#include "pcie.h"

static int pcie_preinit(void)
{
	return 0;
}

static void pcie_preexit(void)
{
}

static int pcie_buf_list_alloc(int chn, struct mbuf_t **head,
			       struct mbuf_t **tail, int *num)
{
	return mbuf_link_alloc(chn, head, tail, num);
}

static int pcie_buf_list_free(int chn, struct mbuf_t *head,
			      struct mbuf_t *tail, int num)
{
	return mbuf_link_free(chn, head, tail, num);
}

static int pcie_list_push(int chn, struct mbuf_t *head,
			  struct mbuf_t *tail, int num)
{
	return mchn_push_link(chn, head, tail, num);
}

static int pcie_chn_init(struct mchn_ops_t *ops)
{
	return mchn_init(ops);
}

static int pcie_chn_deinit(struct mchn_ops_t *ops)
{
	return mchn_deinit(ops);
}

static int pcie_direct_read(unsigned int addr, void *buf, unsigned int len)
{
	return mchn_wcn_mem_read(addr, buf, len);
}

static int pcie_direct_write(unsigned int addr, void *buf, unsigned int len)
{
	return mchn_wcn_mem_write(addr, buf, len);
}

static int pcie_readbyte(unsigned int addr, unsigned char *val)
{
	return mchn_wcn_mem_read(addr, val, 1);
}

static int pcie_writebyte(unsigned int addr, unsigned char val)
{
	return mchn_wcn_mem_write(addr, &val, 1);
}

int pcie_read32(unsigned int system_addr, void *buf)
{
	return mchn_wcn_mem_read(system_addr, buf, 4);
}

int pcie_write32(unsigned int system_addr, void *buf)
{
	return mchn_wcn_mem_write(system_addr, buf, 4);
}

int pcie_update_bits(unsigned int reg, unsigned int mask, unsigned int val)
{
	return mchn_wcn_update_bits(reg, mask, val);
}

int pcie_get_bus_status(void)
{
	return wcn_pcie_get_bus_status();
}

static unsigned int pcie_get_carddump_status(void)
{
	return sprd_pcie_get_carddump_status();
}

static void pcie_set_carddump_status(unsigned int flag)
{
	return sprd_pcie_set_carddump_status(flag);
}

static int pcie_rescan(void *wcn_dev)
{
	return sprd_pcie_scan_card(wcn_dev);
}

static void pcie_register_rescan_cb(void *func)
{
	return sprd_pcie_register_scan_notify(func);
}

static void pcie_remove_card(void *wcn_dev)
{
	return sprd_pcie_remove_card(wcn_dev);
}

static struct sprdwcn_bus_ops pcie_bus_ops = {
	.preinit = pcie_preinit,
	.deinit = pcie_preexit,
	.chn_init = pcie_chn_init,
	.chn_deinit = pcie_chn_deinit,
	.list_alloc = pcie_buf_list_alloc,
	.list_free = pcie_buf_list_free,
	.push_list = pcie_list_push,
	.direct_read = pcie_direct_read,
	.direct_write = pcie_direct_write,
	.readbyte = pcie_readbyte,
	.writebyte = pcie_writebyte,
	.read_l = pcie_read32,
	.write_l = pcie_write32,
	.update_bits = pcie_update_bits,
	.get_bus_status = pcie_get_bus_status,
	.get_carddump_status = pcie_get_carddump_status,
	.set_carddump_status = pcie_set_carddump_status,
	.register_rescan_cb = pcie_register_rescan_cb,
	.rescan = pcie_rescan,
	.remove_card = pcie_remove_card,
};

void module_bus_init(void)
{
	module_ops_register(&pcie_bus_ops);
}
EXPORT_SYMBOL(module_bus_init);

void module_bus_deinit(void)
{
	module_ops_unregister();
}
EXPORT_SYMBOL(module_bus_deinit);
