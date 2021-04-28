/*
 * linux/drivers/mmc/host/sprd-sdhcr10.c - Secure Digital Host Controller
 * Interface driver
 *
 * Copyright (C) 2019 Spreadtrum corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

#include "sprd-sdhcr10.h"

#define DRIVER_NAME "sprd-sdhcr10"

#ifdef CONFIG_PM
static int sprd_sdhc_runtime_pm_get(struct sprd_sdhc_host *host)
{
	return pm_runtime_get_sync(host->mmc->parent);
}

static int sprd_sdhc_runtime_pm_put(struct sprd_sdhc_host *host)
{
	pm_runtime_mark_last_busy(host->mmc->parent);
	return pm_runtime_put_autosuspend(host->mmc->parent);
}
#else
static inline int sprd_sdhc_runtime_pm_get(struct sprd_sdhc_host *host)
{
	return 0;
}

static inline int sprd_sdhc_runtime_pm_put(struct sprd_sdhc_host *host)
{
	return 0;
}
#endif

static void dump_adma_info(struct sprd_sdhc_host *host)
{
	void *desc_header = host->adma_desc;
	u64 desc_ptr;
	unsigned long start = jiffies;
	struct device *dev = &host->pdev->dev;

	if (host->flags & SPRD_USE_64_BIT_DMA) {
		desc_ptr = (u64)sprd_sdhc_readl(host,
				SPRD_SDHC_REG_32_ADMA2_ADDR_H) << 32;
		desc_ptr |= sprd_sdhc_readl(host,
				SPRD_SDHC_REG_32_ADMA2_ADDR_L);
	} else {
		desc_ptr = sprd_sdhc_readl(host, SPRD_SDHC_REG_32_ADMA2_ADDR_L);
	}

	dev_info(dev, "ADMA desc_ptr: 0x%llx ERROR: 0x%08x\n",
		 desc_ptr, sprd_sdhc_readl(host, SPRD_SDHC_REG_ADMA_ERROR));
	dev_info(dev, "ADMA DEBUG0:0x%08x, DEBUG1:0x%08x DEBUG2:0x%08x\n",
		 sprd_sdhc_readl(host, SPRD_SDHC_REG_EMMC_DEBUG0),
		 sprd_sdhc_readl(host, SPRD_SDHC_REG_EMMC_DEBUG1),
		 sprd_sdhc_readl(host, SPRD_SDHC_REG_EMMC_DEBUG2));


	while (desc_ptr) {
		if (host->flags & SPRD_USE_64_BIT_DMA) {
			struct sprd_adma2_128_desc *dma_desc = desc_header;

			dev_info(dev, "ADMA desc 0x%p: addr: 0x%08x%08x\n",
				 desc_header,
				 le32_to_cpu(dma_desc->addr_hi),
				 le32_to_cpu(dma_desc->addr_lo));
			dev_info(dev, "ADMA len 0x%x,  attr=0x%x\n",
				 le32_to_cpu(dma_desc->len_cmd) >> 16,
				 le32_to_cpu(dma_desc->len_cmd) & 0xFFFF);

			desc_header += host->adma_desc_sz;
			if (dma_desc->len_cmd & cpu_to_le16(ADMA2_END))
				break;
		} else {
			struct sdhci_adma2_32_desc *dma_desc = desc_header;

			dev_info(dev, "ADMA desc 0x%p: addr: 0x%08x\n",
				 desc_header, le32_to_cpu(dma_desc->addr));
			dev_info(dev, "ADMA len 0x%x,  attr=0x%x\n",
				 le16_to_cpu(dma_desc->len),
				 le16_to_cpu(dma_desc->cmd));


			desc_header += host->adma_desc_sz;
			if (dma_desc->cmd & cpu_to_le16(ADMA2_END))
				break;
		}

		if (time_after(jiffies, start + HZ)) {
			dev_err(dev, "ADMA error have no end desc\n");
			break;
		}
	}

	desc_ptr = (u64)sprd_sdhc_readl(host,
				SPRD_SDHC_REG_ADMA_PROCESS_H) << 32;
	desc_ptr |= sprd_sdhc_readl(host, SPRD_SDHC_REG_ADMA_PROCESS_L);
	dev_info(dev, "Current ADMA  desc_ptr: 0x%llx\n", desc_ptr);
}

static void dump_sdio_reg(struct sprd_sdhc_host *host)
{
	if (!host->mmc->card)
		return;

	print_hex_dump(KERN_INFO, "sprd-sdhcr10", DUMP_PREFIX_OFFSET,
			16, 4, host->ioaddr, 64, 0);

	if ((host->flags & SPRD_USE_ADMA) &&
			(strcmp(host->device_name, "sdio_wifi") != 0))
		dump_adma_info(host);
}

static int sprd_get_delay_value(struct platform_device *pdev)
{
	int ret = 0;
	u32 dly_vl[4];
	struct device_node *np = pdev->dev.of_node;
	struct sprd_sdhc_host *host = platform_get_drvdata(pdev);
	struct timing_delay_value *timing_dly;

	timing_dly = devm_kzalloc(&pdev->dev, sizeof(*timing_dly), GFP_KERNEL);
	if (!timing_dly)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "sprd,mmchs-dly", dly_vl, 4);
	if (!ret)
		timing_dly->mmchs_dly = SPRD_SDHC_DLY_TIMING(dly_vl[0],
					dly_vl[1], dly_vl[2], dly_vl[3]);

	ret = of_property_read_u32_array(np, "sprd,sdr50-dly", dly_vl, 4);
	if (!ret)
		timing_dly->sdr50_dly = SPRD_SDHC_DLY_TIMING(dly_vl[0],
					dly_vl[1], dly_vl[2], dly_vl[3]);

	ret = of_property_read_u32_array(np, "sprd,sdr104-dly", dly_vl, 4);
	if (!ret)
		timing_dly->sdr104_dly = SPRD_SDHC_DLY_TIMING(dly_vl[0],
					dly_vl[1], dly_vl[2], dly_vl[3]);

	ret = of_property_read_u32_array(np, "sprd,ddr52-dly", dly_vl, 4);
	if (!ret)
		timing_dly->ddr52_dly = SPRD_SDHC_DLY_TIMING(dly_vl[0],
					dly_vl[1], dly_vl[2], dly_vl[3]);

	ret = of_property_read_u32_array(np, "sprd,hs200-dly", dly_vl, 4);
	if (!ret)
		timing_dly->hs200_dly = SPRD_SDHC_DLY_TIMING(dly_vl[0],
					dly_vl[1], dly_vl[2], dly_vl[3]);

	ret = of_property_read_u32_array(np, "sprd,hs400-dly", dly_vl, 4);
	if (!ret)
		timing_dly->hs400_dly = SPRD_SDHC_DLY_TIMING(dly_vl[0],
					dly_vl[1], dly_vl[2], dly_vl[3]);

	ret = of_property_read_u32_array(np, "sprd,hs400es-dly", dly_vl, 4);
	if (!ret)
		timing_dly->hs400es_dly = SPRD_SDHC_DLY_TIMING(dly_vl[0],
					dly_vl[1], dly_vl[2], dly_vl[3]);

	host->timing_dly = timing_dly;

	return 0;
}

static void sprd_set_delay_value(struct sprd_sdhc_host *host)
{
	struct device *dev = &host->pdev->dev;

	switch (host->ios.timing) {
	case MMC_TIMING_MMC_HS:
		if (host->ios.clock == 52000000) {
			host->dll_dly = host->timing_dly->mmchs_dly;
			sprd_sdhc_writel(host, host->dll_dly,
					SPRD_SDHC_REG_32_DLL_DLY);
			dev_info(dev,
				 "mmchs default timing delay value 0x%08x\n",
				 host->dll_dly);
		}
		break;
	case MMC_TIMING_UHS_SDR50:
		if (host->ios.clock == 100000000) {
			host->dll_dly = host->timing_dly->sdr50_dly;
			sprd_sdhc_writel(host, host->dll_dly,
					SPRD_SDHC_REG_32_DLL_DLY);
			dev_info(dev,
				 "sdr50 default timing delay value 0x%08x\n",
				 host->dll_dly);
		}
		break;
	case MMC_TIMING_UHS_SDR104:
		if (host->ios.clock == 208000000) {
			host->dll_dly = host->timing_dly->sdr104_dly;
			sprd_sdhc_writel(host, host->dll_dly,
					SPRD_SDHC_REG_32_DLL_DLY);
			dev_info(dev,
				 "sdr104 default timing delay value 0x%08x\n",
				 host->dll_dly);
		}
		break;
	case MMC_TIMING_MMC_DDR52:
		if (host->ios.clock == 52000000) {
			host->dll_dly = host->timing_dly->ddr52_dly;
			sprd_sdhc_writel(host, host->dll_dly,
					SPRD_SDHC_REG_32_DLL_DLY);
			dev_info(dev,
				 "ddr52 default timing delay value: 0x%08x\n",
				 host->dll_dly);
		}
		break;
	case MMC_TIMING_MMC_HS200:
		if (host->ios.clock == 200000000) {
			host->dll_dly = host->timing_dly->hs200_dly;
			sprd_sdhc_writel(host, host->dll_dly,
					SPRD_SDHC_REG_32_DLL_DLY);
			dev_info(dev,
				 "hs200 default timing delay value: 0x%08x\n",
				 host->dll_dly);
		}
		break;
	case MMC_TIMING_MMC_HS400:
		if (host->ios.clock == 200000000) {
			host->dll_dly = host->timing_dly->hs400_dly;
			sprd_sdhc_writel(host, host->dll_dly,
					SPRD_SDHC_REG_32_DLL_DLY);
			dev_info(dev,
				 "hs400 final timing delay value: 0x%08x\n",
				 host->dll_dly);
		}
		break;
	default:
		break;
	}
}

static void sprd_reset_ios(struct sprd_sdhc_host *host)
{
	sprd_sdhc_disable_all_int(host);
	host->ios.clock = 0;
	host->ios.vdd = 0;
	host->ios.power_mode = MMC_POWER_OFF;
	host->ios.bus_width = MMC_BUS_WIDTH_1;
	host->ios.timing = MMC_TIMING_LEGACY;
	host->ios.signal_voltage = MMC_SIGNAL_VOLTAGE_330;

	sprd_sdhc_reset(host, SPRD_SDHC_BIT_RST_ALL);
	if (!strcmp("sdio_wifi", host->device_name))
		sprd_sdhc_writel(host, host->dll_dly, SPRD_SDHC_REG_32_DLL_DLY);
	#ifdef CONFIG_64BIT
	sprd_sdhc_set_64bit_addr(host, 1);
	#endif
	sprd_sdhc_set_dll_backup(host, SPRD_SDHC_BIT_DLL_BAK |
							SPRD_SDHC_BIT_DLL_VAL);
}

static void sprd_get_rsp(struct sprd_sdhc_host *host)
{
	u32 i, offset;
	unsigned int flags = host->cmd->flags;
	u32 *resp = host->cmd->resp;

	if (!(flags & MMC_RSP_PRESENT))
		return;

	if (flags & MMC_RSP_136) {
		/* CRC is stripped so we need to do some shifting. */
		for (i = 0, offset = 12; i < 3; i++, offset -= 4) {
			resp[i] = sprd_sdhc_readl(host,
					SPRD_SDHC_REG_32_RESP + offset) << 8;
			resp[i] |= sprd_sdhc_readb(host,
					SPRD_SDHC_REG_32_RESP + offset - 1);
		}
		resp[3] = sprd_sdhc_readl(host, SPRD_SDHC_REG_32_RESP) << 8;
	} else
		resp[0] = sprd_sdhc_readl(host, SPRD_SDHC_REG_32_RESP);
}

static int sdhci_pre_dma_transfer(struct sprd_sdhc_host *host,
				       struct mmc_data *data)
{
	int sg_count;

	if (data->host_cookie == COOKIE_MAPPED) {
		data->host_cookie = COOKIE_GIVEN;
		return data->sg_count;
	}
	WARN_ON(data->host_cookie == COOKIE_GIVEN);
	sg_count = dma_map_sg(mmc_dev(host->mmc), data->sg, data->sg_len,
				data->flags & MMC_DATA_WRITE ?
				DMA_TO_DEVICE : DMA_FROM_DEVICE);
	if (sg_count == 0)
		return -ENOSPC;
	data->sg_count = sg_count;
	data->host_cookie = COOKIE_MAPPED;
	return sg_count;
}

static char *sprd_sdhc_kmap_atomic(struct scatterlist *sg, unsigned long *flags)
{
	local_irq_save(*flags);
	return kmap_atomic(sg_page(sg)) + sg->offset;
}

static void sprd_sdhc_kunmap_atomic(void *buffer, unsigned long *flags)
{
	kunmap_atomic(buffer);
	local_irq_restore(*flags);
}

static void sprd_adma_write_desc(struct sprd_sdhc_host *host,
				void *desc, dma_addr_t addr,
				int len, unsigned cmd)
{
	if (host->flags & SPRD_USE_64_BIT_DMA) {
		struct sprd_adma2_128_desc *dma_desc = desc;

		dma_desc->len_cmd = cpu_to_le16(cmd);
		dma_desc->len_cmd |= cpu_to_le32(len) << 16;
		dma_desc->addr_lo = cpu_to_le32((u32)addr);
		dma_desc->addr_hi = cpu_to_le32((u64)addr >> 32);
		dma_desc->reserve = cpu_to_le32(0);
	} else {
		struct sdhci_adma2_32_desc *dma_desc = desc;

		dma_desc->cmd = cpu_to_le16(cmd);
		dma_desc->len = cpu_to_le16(len);
		dma_desc->addr = cpu_to_le32((u32)addr);
	}
}

static void sprd_adma_mark_end(struct sprd_sdhc_host *host, void *desc)
{
	if (host->flags & SPRD_USE_64_BIT_DMA) {
		struct sprd_adma2_128_desc *dma_desc = desc;

		dma_desc->len_cmd |= cpu_to_le16(ADMA2_END);
	} else {
		struct sdhci_adma2_32_desc *dma_desc = desc;

		dma_desc->cmd |= cpu_to_le16(ADMA2_END);
	}
}

static int sprd_sdhc_adma_table_pre(struct sprd_sdhc_host *host,
	struct mmc_data *data)
{
	int direction;

	u8 *desc;
	u8 *align;

	dma_addr_t addr;
	dma_addr_t align_addr;
	int len, offset, align_len;

	struct scatterlist *sg;
	int i;
	char *buffer;
	unsigned long flags;

	/*
	 * The spec does not specify endianness of descriptor table.
	 * We currently guess that it is LE.
	 */
	if (data->flags & MMC_DATA_READ)
		direction = DMA_FROM_DEVICE;
	else
		direction = DMA_TO_DEVICE;

	host->sg_count = sdhci_pre_dma_transfer(host, data);

	if (host->sg_count <= 0)
		return -EINVAL;

	desc = host->adma_desc;
	align = host->align_buffer;
	align_addr = host->align_addr;

	for_each_sg(data->sg, sg, host->sg_count, i) {
		addr = sg_dma_address(sg);
		len = sg_dma_len(sg);
		offset = (SPRD_ADMA2_64_ALIGN -
			(addr & (SPRD_ADMA2_64_ALIGN - 1))) &
			(SPRD_ADMA2_64_ALIGN - 1);
		align_len = min(len, offset);

		if (align_len) {
			if (data->flags & MMC_DATA_WRITE) {
				buffer = sprd_sdhc_kmap_atomic(sg, &flags);
				WARN_ON(((long)buffer & (PAGE_SIZE - 1)) >
					(PAGE_SIZE - align_len));
				memcpy(align, buffer, align_len);
				sprd_sdhc_kunmap_atomic(buffer, &flags);
			}
			/* tran, valid */
			sprd_adma_write_desc(host, desc, align_addr,
					align_len, ADMA2_TRAN_VALID);

			WARN_ON(offset > 65536);

			align += SPRD_ADMA2_64_ALIGN;
			align_addr += SPRD_ADMA2_64_ALIGN;

			desc += host->adma_desc_sz;

			addr += align_len;
			len -= align_len;
		}

		WARN_ON(len > 65536);

		/* tran, valid */
		if (len > 0) {
			sprd_adma_write_desc(host, desc,
				addr, len, ADMA2_TRAN_VALID);
			desc += host->adma_desc_sz;
		}

		/*
		 * If this triggers then we have a calculation bug
		 * somewhere.
		 */
		WARN_ON((desc - host->adma_desc) >= host->adma_table_sz);
	}

		/*
		* Mark the last descriptor as the terminating descriptor
		*/

		if (desc != host->adma_desc) {
			desc -= host->adma_desc_sz;
			sprd_adma_mark_end(host, desc);
		}

	return 0;
}

static void sprd_sdhc_adma_table_post(struct sprd_sdhc_host *host,
				  struct mmc_data *data)
{
	int direction;

	struct scatterlist *sg;
	int i, size;
	u8 *align;
	char *buffer;
	unsigned long flags;
	bool has_unaligned;
	int align_len, len;

	if (data->flags & MMC_DATA_READ)
		direction = DMA_FROM_DEVICE;
	else
		direction = DMA_TO_DEVICE;

	has_unaligned = false;
	for_each_sg(data->sg, sg, host->sg_count, i)
		if (sg_dma_address(sg) & (SPRD_ADMA2_64_ALIGN - 1)) {
			has_unaligned = true;
			break;
		}

	if (has_unaligned && (data->flags & MMC_DATA_READ)) {
		dma_sync_sg_for_cpu(mmc_dev(host->mmc),
			data->sg, data->sg_len, direction);

		align = host->align_buffer;

		for_each_sg(data->sg, sg, host->sg_count, i) {
			if (sg_dma_address(sg) & (SPRD_ADMA2_64_ALIGN - 1)) {
				size = SPRD_ADMA2_64_ALIGN -
					(sg_dma_address(sg) &
					(SPRD_ADMA2_64_ALIGN - 1));
				len = sg_dma_len(sg);
				align_len = min(len, size);
				buffer = sprd_sdhc_kmap_atomic(sg, &flags);
				memcpy(buffer, align, align_len);
				sprd_sdhc_kunmap_atomic(buffer, &flags);

				align += SPRD_ADMA2_64_ALIGN;
			}
		}
	}

	if (data->host_cookie == COOKIE_MAPPED) {
		dma_unmap_sg(mmc_dev(host->mmc), data->sg,
			data->sg_len, direction);
		data->host_cookie = COOKIE_UNMAPPED;
	}
}

static void sprd_sdhc_dma_unmap(struct sprd_sdhc_host *host)
{
	struct mmc_data *data = host->cmd->data;

	if (host->flags & SPRD_USE_ADMA)
		sprd_sdhc_adma_table_post(host, data);
	else {
		if (data->host_cookie == COOKIE_MAPPED) {
			dma_unmap_sg(mmc_dev(host->mmc),
				data->sg, data->sg_len,
				(data->flags & MMC_DATA_READ) ?
				DMA_FROM_DEVICE : DMA_TO_DEVICE);
			data->host_cookie = COOKIE_UNMAPPED;
		}
	}
}

static void sprd_admd_mode(struct sprd_sdhc_host *host, struct mmc_data *data)
{

	if (host->flags & SPRD_USE_64_BIT_DMA)
		sprd_sdhc_set_64bit_addr(host, 1);
	sprd_sdhc_set_adma2_len(host);
	sprd_sdhc_set_dma(host, SPRD_SDHC_BIT_32ADMA_MOD);
	sprd_sdhc_adma_table_pre(host, data);
	sprd_sdhc_writel(host,
		(u32)host->adma_addr,
		SPRD_SDHC_REG_32_ADMA2_ADDR_L);
	 if (host->flags & SPRD_USE_64_BIT_DMA)
			sprd_sdhc_writel(host,
				(u32)((u64)host->adma_addr >> 32),
				SPRD_SDHC_REG_32_ADMA2_ADDR_H);
}

static void sprd_sdmd_mode(struct sprd_sdhc_host *host, struct mmc_data *data)
{
	int sg_cnt;

	sprd_sdhc_set_dma(host, SPRD_SDHC_BIT_SDMA_MOD);
	sg_cnt = sdhci_pre_dma_transfer(host, data);

	if (sg_cnt == 1) {
			u32 addr;

			addr = (u32)(sg_dma_address(data->sg));
			sprd_sdhc_writel(host, addr,
				SPRD_SDHC_REG_32_SDMA_ADDR);
		#ifdef CONFIG_64BIT
			addr = (u32)((u64)(sg_dma_address(data->sg)) >> 32);
			sprd_sdhc_writel(host, addr,
				SPRD_SDHC_REG_32_ADMA2_ADDR_H);
		#endif
	}
}


static void sprd_send_cmd(struct sprd_sdhc_host *host, struct mmc_command *cmd)
{
	struct mmc_data *data = cmd->data;
	struct device *dev = &host->pdev->dev;
	u32 flag = 0;
	u16 rsp_type = 0;
	int if_has_data = 0;
	int if_mult = 0;
	int if_read = 0;
	int if_dma = 0;
	u16 auto_cmd = SPRD_SDHC_BIT_ACMD_DIS;

	dev_dbg(dev, "CMD%d, arg 0x%x, flag 0x%x\n",
		cmd->opcode, cmd->arg, cmd->flags);
	if (cmd->data)
		dev_dbg(dev, "block size %d, cnt %d\n",
			cmd->data->blksz, cmd->data->blocks);
	sprd_sdhc_disable_all_int(host);

	if (cmd->opcode == MMC_ERASE) {
		/*
		 * if it is erase command , it's busy time will long,
		 * so we set long timeout value here.
		 */
		mod_timer(&host->timer, jiffies +
			msecs_to_jiffies(host->mmc->max_busy_timeout + 1000));
		sprd_sdhc_writeb(host,
				SPRD_SDHC_DATA_TIMEOUT_MAX_VAL,
				SPRD_SDHC_REG_8_TIMEOUT);
	} else {
		mod_timer(&host->timer,
			jiffies + (SPRD_SDHC_MAX_TIMEOUT + 1) * HZ);
		sprd_sdhc_writeb(host, host->data_timeout_val,
				SPRD_SDHC_REG_8_TIMEOUT);
	}

	host->cmd = cmd;
	if (data) {
		/* set data param */
		WARN_ON((data->blksz * data->blocks > 524288) ||
			(data->blksz > host->mmc->max_blk_size) ||
			(data->blocks > 65535));

		data->bytes_xfered = 0;

		if_has_data = 1;
		if_read = (data->flags & MMC_DATA_READ);
		if_mult = (mmc_op_multi(cmd->opcode) || data->blocks > 1);
		if (if_read && !if_mult)
			flag = SPRD_SDHC_DAT_FILTER_RD_SIGLE;
		else if (if_read && if_mult)
			flag = SPRD_SDHC_DAT_FILTER_RD_MULTI;
		else if (!if_read && !if_mult)
			flag = SPRD_SDHC_DAT_FILTER_WR_SIGLE;
		else
			flag = SPRD_SDHC_DAT_FILTER_WR_MULTI;

		if (host->auto_cmd_mode)
			flag |= SPRD_SDHC_BIT_INT_ERR_ACMD;

		if_dma = 1;

		sprd_sdhc_set_blk_size(host, data->blksz);
		if (host->version == SPRD_SDHC_BIT_SPEC_300)
			sprd_sdhc_set_16_blk_cnt(host, data->blocks);
		else if (host->version >= SPRD_SDHC_BIT_SPEC_400)
			sprd_sdhc_set_32_blk_cnt(host, data->blocks);
		if (if_mult) {
			if (!host->mrq->sbc && SPRD_SDHC_FLAG_ENABLE_ACMD12)
				auto_cmd = host->auto_cmd_mode;
			else if (host->mrq->sbc &&
				(host->flags & SPRD_AUTO_CMD23)) {
				auto_cmd = host->auto_cmd_mode;
				sprd_sdhc_set_autocmd23(host);
			}
		}
		if (host->flags & SPRD_USE_ADMA) {
			sprd_admd_mode(host, data);
			flag |= SPRD_SDHC_BIT_INT_ERR_ADMA;
		} else
			sprd_sdmd_mode(host, data);
	}

	sprd_sdhc_writel(host, cmd->arg, SPRD_SDHC_REG_32_ARG);
	switch (mmc_resp_type(cmd)) {
	case MMC_RSP_R1B:
		rsp_type = SPRD_SDHC_RSP1B_5B;
		flag |= SPRD_SDHC_CMD_FILTER_R1B;
		break;
	case MMC_RSP_NONE:
		rsp_type = SPRD_SDHC_RSP0;
		flag |= SPRD_SDHC_CMD_FILTER_R0;
		break;
	case MMC_RSP_R2:
		rsp_type = SPRD_SDHC_RSP2;
		flag |= SPRD_SDHC_CMD_FILTER_R2;
		break;
	case MMC_RSP_R4:
		rsp_type = SPRD_SDHC_RSP3_4;
		flag |= SPRD_SDHC_CMD_FILTER_R1_R4_R6_R7;
		break;
	case MMC_RSP_R1:
	case MMC_RSP_R1 & ~MMC_RSP_CRC:
		rsp_type = SPRD_SDHC_RSP1_5_6_7;
		flag |= SPRD_SDHC_CMD_FILTER_R1_R4_R6_R7;
		break;
	default:
		WARN_ON(1);
		break;
	}

	host->int_filter = flag;
	host->int_come = 0;
	sprd_sdhc_enable_int(host, flag);
	dev_dbg(dev, "CMD%d rsp:0x%x intflag:0x%x\n"
		"if_mult:0x%x if_read:0x%x auto_cmd:0x%x if_dma:0x%x\n",
		cmd->opcode, mmc_resp_type(cmd), flag, if_mult, if_read,
		auto_cmd, if_dma);

	/* adma need stric squence*/
	wmb();
	sprd_sdhc_set_trans_and_cmd(host, if_mult, if_read, auto_cmd,
		if_mult, if_dma, cmd->opcode, if_has_data, rsp_type);

}

static int irq_err_handle(struct sprd_sdhc_host *host, u32 intmask)
{
	struct device *dev = &host->pdev->dev;

	/* some error happened in command */
	if (SPRD_SDHC_INT_FILTER_ERR_CMD & intmask) {
		if (SPRD_SDHC_BIT_INT_ERR_CMD_TIMEOUT & intmask)
			host->cmd->error = -ETIMEDOUT;
		else
			host->cmd->error = -EILSEQ;
	}

	/* some error happened in data token or command  with R1B */
	if (SPRD_SDHC_INT_FILTER_ERR_DAT & intmask) {
		if (host->cmd->data) {
			/*
			 * We need ignore data error in hs400 tunning.
			 * Data timeout and data crc error should be considered
			 * as correct.
			 */
			if (!(host->flags & SPRD_HS400_TUNING)) {
				/* current error is happened in data token */
				if (SPRD_SDHC_BIT_INT_ERR_DATA_TIMEOUT &
				    intmask)
					host->cmd->data->error = -ETIMEDOUT;
				else
					host->cmd->data->error = -EILSEQ;
			}
		} else {
			/* current error is happend in response with busy */
			if (SPRD_SDHC_BIT_INT_ERR_DATA_TIMEOUT & intmask)
				host->cmd->error = -ETIMEDOUT;
			else
				host->cmd->error = -EILSEQ;
		}
	}

	/*
	 * We need ignore data error in hs400 tunning. Used -EIO to judge if it
	 * is a data compare failed error, so in hs400 tunning, the data error
	 * should be  -EIO
	 */
	if (SPRD_SDHC_BIT_INT_ERR_ACMD & intmask) {
		/* Auto cmd12 and cmd23 error is belong to data token error */
		if (!(host->flags & SPRD_HS400_TUNING))
			host->cmd->data->error = -EILSEQ;
		else
			host->cmd->data->error = -EIO;
	}

	if (SPRD_SDHC_BIT_INT_ERR_ADMA & intmask)
		host->cmd->data->error = -EIO;

	if ((host->cmd->opcode != MMC_SEND_TUNING_BLOCK) &&
		(host->cmd->opcode != MMC_SEND_TUNING_BLOCK_HS200)) {
		dev_info(dev, "CMD%d int 0x%x\n", host->cmd->opcode, intmask);
		dump_sdio_reg(host);
	}

	if (host->cmd->data) {
		uint32_t i = 0;

		sprd_sdhc_dma_unmap(host);
		/*
		 * if send cmd with data err happened, sw reset cannot stop
		 * controller.It must check following bit before sw reset,
		 * in SPRD SDIO IP
		 */
		while (!(host->int_come &
			(SPRD_SDHC_BIT_INT_ERR_CMD_TIMEOUT |
			SPRD_SDHC_BIT_INT_ERR_DATA_TIMEOUT |
			SPRD_SDHC_BIT_INT_ERR_DATA_END |
			SPRD_SDHC_BIT_INT_TRAN_END
		))){
			host->int_come |= sprd_sdhc_readl(host,
				SPRD_SDHC_REG_32_INT_STATE);
			if (i++ > 10000000)
				break;
			cpu_relax();
		}
	}
	sprd_sdhc_disable_all_int(host);

	sprd_sdhc_reset(host, SPRD_SDHC_BIT_RST_CMD|SPRD_SDHC_BIT_RST_DAT);

	/* if current error happened in data token, we send cmd12 to stop it */
	if ((host->mrq->cmd == host->cmd) && (host->mrq->stop)) {
		sprd_send_cmd(host, host->mrq->stop);
	} else {
		/* request finish with error, so reset and stop it */
		tasklet_schedule(&host->finish_tasklet);
	}

	return IRQ_HANDLED;
}

static int irq_normal_handle(struct sprd_sdhc_host *host, u32 intmask,
			u32 *ret_intmask)
{
	struct mmc_request *mrq = host->mrq;
	struct mmc_command *cmd = host->cmd;
	struct mmc_data *data = host->cmd->data;
	u32 sys_addr;

	/* delete irq that wanted in filter */
	host->int_filter &= ~(SPRD_SDHC_FILTER_NORMAL & intmask);

	if (SPRD_SDHC_BIT_INT_DMA_END & intmask) {
			sys_addr = sprd_sdhc_readl(host,
					SPRD_SDHC_REG_32_SDMA_ADDR);
			sprd_sdhc_writel(host, sys_addr,
					SPRD_SDHC_REG_32_SDMA_ADDR);
			#ifdef CONFIG_64BIT
			sys_addr = sprd_sdhc_readl(host,
					SPRD_SDHC_REG_32_ADMA2_ADDR_H);
			sprd_sdhc_writel(host, sys_addr,
					SPRD_SDHC_REG_32_ADMA2_ADDR_H);
			#endif
	}

	if (SPRD_SDHC_BIT_INT_CMD_END & intmask) {
		cmd->error = 0;
		sprd_get_rsp(host);
	}

	if (SPRD_SDHC_BIT_INT_TRAN_END & intmask) {
		if (cmd->data) {
			sprd_sdhc_dma_unmap(host);
			data->error = 0;
			data->bytes_xfered = data->blksz * data->blocks;
		} else {
			/* R1B also can produce transfer complete interrupt */
			cmd->error = 0;
		}
	}

	if (!(SPRD_SDHC_FILTER_NORMAL & host->int_filter)) {
		/* current cmd finished */
		sprd_sdhc_disable_all_int(host);
		if (mrq->sbc == cmd)
			sprd_send_cmd(host, mrq->cmd);
		else if ((mrq->cmd == host->cmd) && (mrq->stop))
			sprd_send_cmd(host, mrq->stop);
		else {
			/* finish with success and stop the request */
			tasklet_schedule(&host->finish_tasklet);
			return IRQ_HANDLED;
		}
	}

	intmask = sprd_sdhc_readl(host, SPRD_SDHC_REG_32_INT_STATE);
	host->int_come |= intmask;
	sprd_sdhc_clear_int(host, intmask);
	intmask &= host->int_filter;
	*ret_intmask = intmask;
	return IRQ_NONE;
}

static irqreturn_t sprd_sdhc_irq(int irq, void *param)
{
	u32 intmask;
	struct sprd_sdhc_host *host = (struct sprd_sdhc_host *)param;
	struct device *dev = &host->pdev->dev;
	struct mmc_request *mrq = host->mrq;
	struct mmc_command *cmd = host->cmd;
	u32 ret_intmask = 0;

	spin_lock(&host->lock);
	/*
	 * maybe sprd_sdhc_timeout() run in one core and _irq() run in
	 * another core, this will panic if access cmd->data
	 */
	if ((!mrq) || (!cmd)) {
		spin_unlock(&host->lock);
		return IRQ_NONE;
	}

	intmask = sprd_sdhc_readl(host, SPRD_SDHC_REG_32_INT_STATE);
	host->int_come |= intmask;
	if (!intmask) {
		spin_unlock(&host->lock);
		return IRQ_NONE;
	} else if (((intmask & SPRD_SDHC_BIT_INT_ERR_CMD_CRC) |
		(intmask & SPRD_SDHC_BIT_INT_ERR_DATA_CRC)) &&
		(cmd->opcode != MMC_SEND_TUNING_BLOCK) &&
		(cmd->opcode != MMC_SEND_TUNING_BLOCK_HS200))
		dev_info(dev, "CMD%d, dll delay reg: 0x%x\n", cmd->opcode,
			 sprd_sdhc_readl(host, SPRD_SDHC_REG_32_DLL_DLY));

	dev_dbg(dev, "CMD%d, intmask 0x%x, filter = 0x%x\n",
		cmd->opcode, intmask, host->int_filter);

	/*
	 * sometimes an undesired interrupt will happen, so we must clear
	 * this unused interrupt.
	 */
	sprd_sdhc_clear_int(host, intmask);
	/* just care about the interrupt that we want */
	intmask &= host->int_filter;

	while (intmask) {
		int ret = 0;

		if (SPRD_SDHC_INT_FILTER_ERR & intmask) {
			ret = irq_err_handle(host, intmask);
			if (ret == IRQ_HANDLED)
				goto out_to_irq;
		} else {
			ret = irq_normal_handle(host, intmask, &ret_intmask);
			if (ret == IRQ_HANDLED)
				goto out_to_irq;
			else
				intmask = ret_intmask;
		}
	};

out_to_irq:
	spin_unlock(&host->lock);
	return IRQ_HANDLED;
}

static void sprd_sdhc_finish_tasklet(unsigned long param)
{
	struct sprd_sdhc_host *host = (struct sprd_sdhc_host *)param;
	unsigned long flags;
	struct mmc_request *mrq;

	del_timer(&host->timer);

	spin_lock_irqsave(&host->lock, flags);
	if (!host->mrq) {
		spin_unlock_irqrestore(&host->lock, flags);
		return;
	}
	mrq = host->mrq;

	host->mrq = NULL;
	host->cmd = NULL;
	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);

	mmc_request_done(host->mmc, mrq);
	sprd_sdhc_runtime_pm_put(host);


}

static void sprd_sdhc_timeout(unsigned long data)
{
	struct sprd_sdhc_host *host = (struct sprd_sdhc_host *)data;
	unsigned long flags;
	struct device *dev = &host->pdev->dev;

	spin_lock_irqsave(&host->lock, flags);
	if (host->mrq) {
		dev_info(dev, "Timeout waiting for hardware interrupt!\n");
		dump_sdio_reg(host);
		if (host->cmd->data)
			host->cmd->data->error = -ETIMEDOUT;
		else if (host->cmd)
			host->cmd->error = -ETIMEDOUT;
		else
			host->mrq->cmd->error = -ETIMEDOUT;

		if (host->cmd->data)
			sprd_sdhc_dma_unmap(host);

		sprd_sdhc_disable_all_int(host);
		sprd_sdhc_reset(host,
			SPRD_SDHC_BIT_RST_CMD | SPRD_SDHC_BIT_RST_DAT);
		tasklet_schedule(&host->finish_tasklet);
	}
	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
}

static void sprd_sdhc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct sprd_sdhc_host *host = mmc_priv(mmc);
	struct device *dev = &host->pdev->dev;
	unsigned long flags;
	int present;

	sprd_sdhc_runtime_pm_get(host);
	present = mmc_gpio_get_cd(host->mmc);
	spin_lock_irqsave(&host->lock, flags);

	host->mrq = mrq;
	/* 1 find whether card is still in slot */
	if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE)) {
		if (!present) {
			mrq->cmd->error = -ENOMEDIUM;
			tasklet_schedule(&host->finish_tasklet);
			mmiowb();
			spin_unlock_irqrestore(&host->lock, flags);
			return;
		}
	}

	/*
	 * in our control we can not use auto cmd12 and auto cmd23 together
	 * so in following program we use auto cmd23 prior to auto cmd12
	 */
	dev_dbg(dev, "CMD%d request %d %d %d\n",
		mrq->cmd->opcode, !!mrq->sbc, !!mrq->cmd, !!mrq->stop);
	host->auto_cmd_mode = SPRD_SDHC_BIT_ACMD_DIS;
	if (!mrq->sbc && mrq->stop && SPRD_SDHC_FLAG_ENABLE_ACMD12) {
		host->auto_cmd_mode = SPRD_SDHC_BIT_ACMD12;
		mrq->data->stop = NULL;
		mrq->stop = NULL;
	}

	/* 3 send cmd list */
	if ((mrq->sbc) && (host->flags & SPRD_AUTO_CMD23)) {
		host->auto_cmd_mode = SPRD_SDHC_BIT_ACMD23;
		mrq->stop = NULL;
		sprd_send_cmd(host, mrq->cmd);
	} else if (mrq->sbc) {
		mrq->stop = NULL;
		sprd_send_cmd(host, mrq->sbc);
	} else {
		sprd_send_cmd(host, mrq->cmd);
	}

	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
}

static void sprd_signal_voltage_on_off(struct sprd_sdhc_host *host, u32 on_off)
{
	struct device *dev = &host->pdev->dev;

	if (IS_ERR(host->mmc->supply.vqmmc)) {
		dev_err(dev, "there is no signal voltage!\n");
		return;
	}

	if (on_off) {
		if (host->flags & SPRD_PINCTRL_AVOID_LEAK_VOLTAGE) {
			if (pinctrl_select_state(
				host->pinctrl, host->pins_sl_off))
				dev_err(dev, "switch vddsdio sl off failed\n");
		}

		if (!regulator_is_enabled(host->mmc->supply.vqmmc)) {
			if (regulator_enable(host->mmc->supply.vqmmc))
				dev_info(dev, "signal voltage enable fail!\n");
			else if (regulator_is_enabled(host->mmc->supply.vqmmc))
				dev_info(dev,
					 "signal voltage enable success!\n");
			else
				dev_info(dev,
					 "signal voltage enable hw fail!\n");

		}
	} else {
		if (host->flags & SPRD_PINCTRL_SWITCH_VOLTAGE) {
			if (pinctrl_select_state(
				host->pinctrl, host->pins_default))
				dev_err(dev,
					"switch vddsdio ms pinctrl failed\n");
		}

		if (regulator_is_enabled(host->mmc->supply.vqmmc)) {
			if (regulator_disable(host->mmc->supply.vqmmc))
				dev_info(dev, "signal voltage disable fail\n");
			else if (!regulator_is_enabled(
					host->mmc->supply.vqmmc))
				dev_info(dev,
					 "signal voltage disable success!\n");
			else
				dev_info(dev,
					 "signal voltage disable hw fail\n");
		}

		if (host->flags & SPRD_PINCTRL_AVOID_LEAK_VOLTAGE) {
			if (pinctrl_select_state(
				host->pinctrl, host->pins_sl_on))
				dev_err(dev, "switch vddsdio sl on failed\n");
		}
	}
}

/*
 * 1 This votage is always poweron
 * 2 initial votage is 2.7v~3.6v
 * 3 It can be reconfig to 1.7v~1.95v
 */
static int sprd_sdhc_set_vqmmc(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct sprd_sdhc_host *host = mmc_priv(mmc);
	struct device *dev = &host->pdev->dev;
	unsigned long flags;
	int err;

	sprd_sdhc_runtime_pm_get(host);
	spin_lock_irqsave(&host->lock, flags);

	if (IS_ERR(mmc->supply.vqmmc)) {
		/* there are no 1.8v signal votage. */
		err = 0;
		dev_err(dev, "There is no signalling voltage\n");
		goto out;
	}

	switch (ios->signal_voltage) {
	case MMC_SIGNAL_VOLTAGE_330:
		/*
		 * SPRD LDO can't support 3.0v signal voltage for emmc,
		 * so we must return
		 */
		if (strcmp(host->device_name, "sdio_emmc") == 0) {
			err = -EINVAL;
			goto out;
		}
		mmiowb();
		spin_unlock_irqrestore(&host->lock, flags);
		err = regulator_set_voltage(mmc->supply.vqmmc,
					    3000000, 3000000);
		spin_lock_irqsave(&host->lock, flags);
		break;
	case MMC_SIGNAL_VOLTAGE_180:
		mmiowb();
		spin_unlock_irqrestore(&host->lock, flags);
		err = regulator_set_voltage(mmc->supply.vqmmc,
					1800000, 1800000);
		spin_lock_irqsave(&host->lock, flags);
		if ((strcmp(host->device_name, "sdio_sd") == 0)) {
			if (host->flags & SPRD_PINCTRL_SWITCH_VOLTAGE)  {
				err = pinctrl_select_state(
					host->pinctrl, host->pins_uhs);
				if (err)
					dev_err(dev,
						"switch vddsdio mspinctrl failed\n");
				else
					dev_info(dev, "set vddsdio ms bit as 1 ok\n");

				udelay(300);
			}
			sprd_sdhc_reset(host, SPRD_SDHC_BIT_RST_CMD |
					SPRD_SDHC_BIT_RST_DAT);
		}
		break;
	case MMC_SIGNAL_VOLTAGE_120:
		mmiowb();
		spin_unlock_irqrestore(&host->lock, flags);
		err = regulator_set_voltage(mmc->supply.vqmmc,
					1100000, 1300000);
		spin_lock_irqsave(&host->lock, flags);
		break;
	default:
		/* No signal voltage switch required */
		dev_err(dev, "0x%x is an unsupportted signal voltage\n",
			ios->signal_voltage);
		err = EIO;
		break;
	}

	if (likely(!err))
		host->ios.signal_voltage = ios->signal_voltage;
	else
		dev_warn(dev, "switching to signalling voltage failed\n");

out:
	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);

	sprd_sdhc_runtime_pm_put(host);
	return err;
}

static void sprd_sdhc_enable_dpll(struct sprd_sdhc_host *host)
{
	u32 tmp = 0;
	struct device *dev = &host->pdev->dev;

	tmp = sprd_sdhc_readl(host, SPRD_SDHC_REG_32_DLL_CFG);
	tmp &= ~(SPRD_SDHC_DLL_EN | SPRD_SDHC_DLL_ALL_CPST_EN);
	sprd_sdhc_writel(host, tmp, SPRD_SDHC_REG_32_DLL_CFG);
	mdelay(1);

	tmp = sprd_sdhc_readl(host, SPRD_SDHC_REG_32_DLL_CFG);
	tmp |= SPRD_SDHC_DLL_ALL_CPST_EN | SPRD_SDHC_DLL_SEARCH_MODE |
		SPRD_SDHC_DLL_INIT_COUNT | SPRD_SDHC_DLL_PHA_INTERNAL;
	sprd_sdhc_writel(host, tmp, SPRD_SDHC_REG_32_DLL_CFG);
	mdelay(1);

	tmp = sprd_sdhc_readl(host, SPRD_SDHC_REG_32_DLL_CFG);
	tmp |= SPRD_SDHC_DLL_EN;
	sprd_sdhc_writel(host, tmp, SPRD_SDHC_REG_32_DLL_CFG);
	mdelay(1);

	while ((sprd_sdhc_readl(host, SPRD_SDHC_REG_32_DLL_STS0) &
		SPRD_SDHC_DLL_LOCKED) != SPRD_SDHC_DLL_LOCKED) {
		dev_warn(dev, "dpll locked faile!\n");
		dev_info(dev, "DLL_STS0 : 0x%x, DLL_CFG : 0x%x\n",
			 sprd_sdhc_readl(host, SPRD_SDHC_REG_32_DLL_STS0),
			 sprd_sdhc_readl(host, SPRD_SDHC_REG_32_DLL_CFG));
		dev_info(dev, "DLL_DLY : 0x%x, DLL_STS1 : 0x%x\n",
			 sprd_sdhc_readl(host, SPRD_SDHC_REG_32_DLL_DLY),
			 sprd_sdhc_readl(host, SPRD_SDHC_REG_32_DLL_STS1));
		mdelay(1);
	}

	dev_info(dev, "dpll locked done\n");
}

static void sprd_sdhc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct sprd_sdhc_host *host = mmc_priv(mmc);
	struct device *dev = &host->pdev->dev;
	unsigned long flags;
	u8 ctrl = 0;
	u8 clkchg_flag = 0;
	u32 div;

	sprd_sdhc_runtime_pm_get(host);
	spin_lock_irqsave(&host->lock, flags);

	if (ios->clock == 0) {
		sprd_sdhc_all_clk_off(host);
		host->ios.clock = 0;
	} else if (ios->clock != host->ios.clock) {
		div = sprd_sdhc_calc_div(host->base_clk, ios->clock);
		sprd_sdhc_sd_clk_off(host);
		clkchg_flag = 1;
		mmiowb();
		sprd_sdhcr10_clk_set_and_on(host, div);
		sprd_sdhc_sd_clk_on(host);
		host->ios.clock = ios->clock;
		host->data_timeout_val = sprd_sdhc_calc_timeout(ios->clock,
						SPRD_SDHC_MAX_TIMEOUT);
		mmc->max_busy_timeout = (1 << 31) / (ios->clock / 1000);
		if (ios->clock <= 400000) {
			sdhc_set_dll_invert(host, SPRD_SDHC_BIT_CMD_DLY_INV |
					SPRD_SDHC_BIT_POSRD_DLY_INV, 1);
			host->dll_dly = 0;
			sprd_sdhc_writel(host, host->dll_dly,
					SPRD_SDHC_REG_32_DLL_DLY);
		} else
			sdhc_set_dll_invert(host, SPRD_SDHC_BIT_CMD_DLY_INV |
					SPRD_SDHC_BIT_POSRD_DLY_INV, 0);
	}

	if (ios->power_mode != host->ios.power_mode) {
		switch (ios->power_mode) {
		case MMC_POWER_OFF:
			mmiowb();
			spin_unlock_irqrestore(&host->lock, flags);
			sprd_signal_voltage_on_off(host, 0);
			if (!IS_ERR(mmc->supply.vmmc))
				mmc_regulator_set_ocr(host->mmc,
						mmc->supply.vmmc, 0);
			spin_lock_irqsave(&host->lock, flags);
			sprd_reset_ios(host);
			host->ios.power_mode = ios->power_mode;
			break;
		case MMC_POWER_ON:
		case MMC_POWER_UP:
			mmiowb();
			spin_unlock_irqrestore(&host->lock, flags);
			if (!IS_ERR(mmc->supply.vmmc))
				mmc_regulator_set_ocr(host->mmc,
					mmc->supply.vmmc, ios->vdd);
			sprd_signal_voltage_on_off(host, 1);
			spin_lock_irqsave(&host->lock, flags);
			host->ios.power_mode = ios->power_mode;
			host->ios.vdd = ios->vdd;
			break;
		}
	}

	/* flash power voltage select */
	if (ios->vdd != host->ios.vdd) {
		mmiowb();
		spin_unlock_irqrestore(&host->lock, flags);
		if (!IS_ERR(mmc->supply.vmmc)) {
			dev_info(dev, "3.0 %d!\n", ios->vdd);
			mmc_regulator_set_ocr(host->mmc,
				mmc->supply.vmmc, ios->vdd);
		}
		spin_lock_irqsave(&host->lock, flags);
		host->ios.vdd = ios->vdd;
	}

	if (ios->bus_width != host->ios.bus_width) {
		sprd_sdhc_set_buswidth(host, ios->bus_width);
		host->ios.bus_width = ios->bus_width;
	}

	if (ios->timing != host->ios.timing) {
		/* 1 first close SD clock */
		sprd_sdhc_sd_clk_off(host);
		/* 2 set timing mode, timing specification used*/
		switch (ios->timing) {
		case MMC_TIMING_MMC_HS:
		case MMC_TIMING_SD_HS:
			ctrl = SPRD_SDHC_BIT_TIMING_MODE_SDR25;
			break;
		case MMC_TIMING_UHS_SDR12:
			ctrl = SPRD_SDHC_BIT_TIMING_MODE_SDR12;
			break;
		case MMC_TIMING_UHS_SDR25:
			ctrl = SPRD_SDHC_BIT_TIMING_MODE_SDR25;
			break;
		case MMC_TIMING_UHS_SDR50:
			ctrl = SPRD_SDHC_BIT_TIMING_MODE_SDR50;
			break;
		case MMC_TIMING_UHS_SDR104:
			ctrl = SPRD_SDHC_BIT_TIMING_MODE_SDR104;
			break;
		case MMC_TIMING_UHS_DDR50:
			ctrl = SPRD_SDHC_BIT_TIMING_MODE_DDR50;
			break;
		case MMC_TIMING_MMC_DDR52:
			ctrl = SPRD_SDHC_BIT_TIMING_MODE_DDR50;
			break;
		case MMC_TIMING_MMC_HS200:
			ctrl = SPRD_SDHC_BIT_TIMING_MODE_HS200;
			break;
		case MMC_TIMING_MMC_HS400:
			ctrl = SPRD_SDHC_BIT_TIMING_MODE_HS400;
			break;
		default:
			break;
		}
		sprd_sdhc_set_uhs_mode(host, ctrl);

		/* 3 open SD clock */
		sprd_sdhc_sd_clk_on(host);
		host->ios.timing = ios->timing;
	}

    /*
     * Under enhanced strobe mode, emmc has alreadly set delay timing in
     * ops->hs400_enhanced_strobe().
     * After ops->hs400_enhanced_strobe(), mmc_set_bus_speed which set clock
     * to 200MHz will be called. In this case sprd_set_delay_value() may think
     * emmc is running in hs400 mode, and it will set hs400 delay timing not
     * enhanced strobe.
     */
	if (!ios->enhanced_strobe)
		sprd_set_delay_value(host);

	if ((ios->clock > 52000000) && (clkchg_flag == 1)) {
		clkchg_flag = 0;
		sprd_sdhc_enable_dpll(host);
	}

	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
	sprd_sdhc_runtime_pm_put(host);
}

static int sprd_sdhc_get_cd(struct mmc_host *mmc)
{
	struct sprd_sdhc_host *host = mmc_priv(mmc);
	unsigned long flags;
	int gpio_cd;

	sprd_sdhc_runtime_pm_get(host);
	gpio_cd = mmc_gpio_get_cd(host->mmc);
	spin_lock_irqsave(&host->lock, flags);

	if (host->mmc->caps & MMC_CAP_NONREMOVABLE) {
		mmiowb();
		spin_unlock_irqrestore(&host->lock, flags);
		sprd_sdhc_runtime_pm_put(host);
		return 1;
	}

	if (IS_ERR_VALUE((unsigned long)gpio_cd))
		gpio_cd = 1;
	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
	sprd_sdhc_runtime_pm_put(host);
	return !!gpio_cd;
}

static int sprd_sdhc_card_busy(struct mmc_host *mmc)
{
	struct sprd_sdhc_host *host = mmc_priv(mmc);
	unsigned long flags;
	u32 present_state;

	sprd_sdhc_runtime_pm_get(host);
	spin_lock_irqsave(&host->lock, flags);

	/* Check whether DAT[3:0] is 0000 */
	present_state = sprd_sdhc_readl(host, SPRD_SDHC_REG_32_PRES_STATE);

	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
	sprd_sdhc_runtime_pm_put(host);
	return !(present_state & SPRD_SDHC_DATA_LVL_MASK);
}

static void sprd_sdhc_emmc_hw_reset(struct mmc_host *mmc)
{
	struct sprd_sdhc_host *host = mmc_priv(mmc);
	unsigned long flags;
	int val;

	sprd_sdhc_runtime_pm_get(host);
	spin_lock_irqsave(&host->lock, flags);

	val = sprd_sdhc_readb(host, SPRD_SDHC_REG_8_RST);
	val &= ~SPRD_SDHC_BIT_RST_EMMC;
	sprd_sdhc_writeb(host, val, SPRD_SDHC_REG_8_RST);
	udelay(10);

	val |= SPRD_SDHC_BIT_RST_EMMC;
	sprd_sdhc_writeb(host, val, SPRD_SDHC_REG_8_RST);
	udelay(300);

	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
	sprd_sdhc_runtime_pm_put(host);
}

static int sprd_sdhc_prepare_hs400_tuning(struct mmc_host *mmc,
	struct mmc_ios *ios)
{
	struct sprd_sdhc_host *host = mmc_priv(mmc);
	struct device *dev = &host->pdev->dev;
	unsigned long flags;

	sprd_sdhc_runtime_pm_get(host);
	spin_lock_irqsave(&host->lock, flags);

	host->flags |= SPRD_HS400_TUNING;
	dev_info(dev, "hs400 default timing delay value: 0x%08x\n",
		 host->dll_dly);

	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
	sprd_sdhc_runtime_pm_put(host);

	return 0;
}

static int sprd_sdhc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct sprd_sdhc_host *host = mmc_priv(mmc);
	struct device *dev = &host->pdev->dev;

	unsigned long flags;
	int err = 0;
	int i = 0;
	bool value, first_vl, prev_vl = 0;
	struct ranges_t {
		int start;
		int end;
	};
	struct ranges_t *ranges;

	int length = 0;
	unsigned int range_count = 0;
	int longest_range_len = 0;
	int longest_range = 0;
	int mid_step;
	int final_phase = 0;
	u32 dll_cfg = 0;
	u32 dll_cnt = 0;
	u32 dll_dly = 0;
	int ret = 0;

	sprd_sdhc_runtime_pm_get(host);
	spin_lock_irqsave(&host->lock, flags);

	sprd_sdhc_reset(host, SPRD_SDHC_BIT_RST_CMD | SPRD_SDHC_BIT_RST_DAT);


	dll_cfg = sprd_sdhc_readl(host, SPRD_SDHC_REG_32_DLL_CFG);
	dll_cfg &= ~(0xf << 24);
	sprd_sdhc_writel(host, dll_cfg, SPRD_SDHC_REG_32_DLL_CFG);
	dll_cnt = sprd_sdhc_readl(host, SPRD_SDHC_REG_32_DLL_STS0) & 0xff;
	length = (dll_cnt * 105) / 100;
	dev_info(dev, "dll config 0x%08x, dll count %d, tuning length: %d\n",
		 dll_cfg, dll_cnt, length);

	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);

	ranges = kmalloc_array(length + 1, sizeof(*ranges), GFP_KERNEL);
	if (!ranges)
		return -ENOMEM;
	spin_lock_irqsave(&host->lock, flags);
	dll_dly = host->dll_dly;
	do {
		if (host->flags & SPRD_HS400_TUNING) {
			dll_dly &= ~SPRD_CMD_DLY_MASK;
			dll_dly |= (i << 8);
		} else {
			dll_dly &= ~(SPRD_CMD_DLY_MASK | SPRD_POSRD_DLY_MASK);
			dll_dly |= (((i << 8) & SPRD_CMD_DLY_MASK) |
				((i << 16) & SPRD_POSRD_DLY_MASK));
		}
		sprd_sdhc_writel(host, dll_dly, SPRD_SDHC_REG_32_DLL_DLY);

		mmiowb();
		spin_unlock_irqrestore(&host->lock, flags);
		ret = mmc_send_tuning(mmc, opcode, NULL);
		/*
		 * We need ignore data error in hs400 tunning. In function
		 * mmc_send_tuning, the data timeout & data crc error will
		 * ignored at irq_err_handle. But -EIO error will come because
		 * of data compare failed. So we used -EIO to filt the
		 * data error
		 */
		if (ret == -EIO && (host->flags & SPRD_HS400_TUNING))
			ret = !ret;
		value = !ret;
		spin_lock_irqsave(&host->lock, flags);
		if (i == 0)
			first_vl = value;

		if ((!prev_vl) && value) {
			range_count++;
			ranges[range_count - 1].start = i;
		}
		if (value) {
			dev_dbg(dev, "tuning ok: %d\n", i);
			ranges[range_count - 1].end = i;
		} else
			dev_dbg(dev, "tuning fail: %d\n", i);

		prev_vl = value;
	} while (++i <= length);

	if (range_count == 0) {
		dev_warn(dev, "all tuning phases fail!\n");
		err = -EIO;
		goto out;
	}

	if ((range_count > 1) && first_vl && value) {
		ranges[0].start = ranges[range_count - 1].start;
		range_count--;
	}

	for (i = 0; i < range_count; i++) {
		int len = (ranges[i].end - ranges[i].start + 1);

		if (len < 0)
			len += length;

		dev_info(dev, "good tuning phase range %d ~ %d\n",
			 ranges[i].start, ranges[i].end);

		if (longest_range_len < len) {
			longest_range_len = len;
			longest_range = i;
		}

	}
	dev_info(dev, "the best tuning step range %d-%d(the length is %d)\n",
		 ranges[longest_range].start, ranges[longest_range].end,
		 longest_range_len);

	mid_step = ranges[longest_range].start + longest_range_len / 2;
	mid_step %= (length+1);

	dll_cfg |= 0xf << 24;
	sprd_sdhc_writel(host, dll_cfg, SPRD_SDHC_REG_32_DLL_CFG);

	final_phase = (mid_step * 256) / dll_cnt;

	if (host->flags & SPRD_HS400_TUNING) {
		host->timing_dly->hs400_dly &= ~SPRD_CMD_DLY_MASK;
		host->timing_dly->hs400_dly |= (final_phase << 8);
		host->dll_dly = host->timing_dly->hs400_dly;
	} else {
		host->dll_dly &= ~(SPRD_CMD_DLY_MASK | SPRD_POSRD_DLY_MASK);
		host->dll_dly |= (((final_phase << 8) & SPRD_CMD_DLY_MASK) |
				((final_phase << 16) & SPRD_POSRD_DLY_MASK));
	}

	dev_info(dev, "the best step %d, phase 0x%02x, delay value 0x%08x\n",
		 mid_step, final_phase, host->dll_dly);
	sprd_sdhc_writel(host, host->dll_dly, SPRD_SDHC_REG_32_DLL_DLY);
	err = 0;

out:
	host->flags &= ~SPRD_HS400_TUNING;
	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
	kfree(ranges);
	sprd_sdhc_runtime_pm_put(host);

	return err;
}


static void sprd_sdhc_hs400_enhanced_strobe(struct mmc_host *mmc,
	struct mmc_ios *ios)
{
	struct sprd_sdhc_host *host = mmc_priv(mmc);
	struct device *dev = &host->pdev->dev;
	unsigned long flags;

	sprd_sdhc_runtime_pm_get(host);
	spin_lock_irqsave(&host->lock, flags);

	if (ios->enhanced_strobe) {
		sprd_sdhc_sd_clk_off(host);
		sprd_sdhc_set_uhs_mode(host, SPRD_SDHC_BIT_TIMING_MODE_HS400ES);
		sprd_sdhc_sd_clk_on(host);
		host->dll_dly = host->timing_dly->hs400es_dly;
		sprd_sdhc_writel(host, host->dll_dly, SPRD_SDHC_REG_32_DLL_DLY);
		dev_info(dev, "hs400es final timing delay value: 0x%08x\n",
			 host->dll_dly);
	}

	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
	sprd_sdhc_runtime_pm_put(host);
}

static const struct mmc_host_ops sprd_sdhc_ops = {
	.request = sprd_sdhc_request,
	.set_ios = sprd_sdhc_set_ios,
	.get_cd = sprd_sdhc_get_cd,
	.start_signal_voltage_switch = sprd_sdhc_set_vqmmc,
	.card_busy = sprd_sdhc_card_busy,
	.hw_reset = sprd_sdhc_emmc_hw_reset,
	.prepare_hs400_tuning = sprd_sdhc_prepare_hs400_tuning,
	.execute_tuning = sprd_sdhc_execute_tuning,
	.hs400_enhanced_strobe = sprd_sdhc_hs400_enhanced_strobe,
};

static int sprd_get_dt_resource(struct platform_device *pdev,
		struct sprd_sdhc_host *host)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	int ret = 0;

	host->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!host->res)
		return -ENOENT;

	host->ioaddr = devm_ioremap_resource(dev, host->res);
	if (IS_ERR(host->ioaddr)) {
		ret = PTR_ERR(host->ioaddr);
		dev_err(dev, "can not map iomem: %d\n", ret);
		goto err;
	}

	host->irq = platform_get_irq(pdev, 0);
	if (host->irq < 0) {
		ret = host->irq;
		goto err;
	}

	host->clk = devm_clk_get(dev, "sdio_clk");
	if (IS_ERR(host->clk)) {
		dev_warn(dev, "Can't get clock property: sdio_clk\n");
		host->clk = NULL;
	}
	/* r10p0 need only one clock source */
	host->clk_source = devm_clk_get(&pdev->dev, "sdio_clk_source");
	if (IS_ERR(host->clk_source)) {
		dev_warn(dev, "Can't get clock property: sdio_clk_source\n");
		host->clk_source = NULL;
	}

	clk_set_parent(host->clk, host->clk_source);
	host->base_clk = clk_get_rate(host->clk_source);
	if (!host->base_clk) {
		host->base_clk = 26000000;
		 dev_warn(dev, "Can't find clk source, switch to FPGA mode!\n");
	}
	host->sdio_ahb = devm_clk_get(&pdev->dev, "sdio_ahb_enable");

	if (IS_ERR(host->sdio_ahb)) {
		dev_warn(dev, "Can't get clock property: sdio_ahb_enable\n");
		host->sdio_ahb = NULL;
	}

	host->sdio_ckg = devm_clk_get(&pdev->dev, "sdio_2x_eb");
	if (IS_ERR(host->sdio_ckg)) {
		dev_err(dev, "Can't get clock property: sdio_2x_eb\n");
		host->sdio_ckg = NULL;
	}

	host->sdio_1x_ckg = devm_clk_get(&pdev->dev, "sdio_1x_eb");
	if (IS_ERR(host->sdio_1x_ckg)) {
		dev_warn(dev, "Can't get sdio_1x eb in dts maybe default on\n");
		host->sdio_1x_ckg = NULL;
	}

	ret = of_property_read_string(np, "sprd,name", &host->device_name);
	if (ret) {
		dev_err(dev, "Can't read the property of sprd name\n");
		goto err;
	}

	if (of_property_read_bool(np, "sprd,sdio-adma")) {
		host->flags |= SPRD_USE_ADMA;
		host->flags |= SPRD_AUTO_CMD23;
#ifdef CONFIG_64BIT
		host->flags |= SPRD_USE_64_BIT_DMA;
#endif
		dev_info(dev, "find sdio-adma\n");
	}

	host->detect_gpio = of_get_named_gpio(np, "cd-gpios", 0);
	if (!gpio_is_valid(host->detect_gpio))
		host->detect_gpio = -1;

	if (sprd_get_delay_value(pdev))
		goto out;

	if ((strcmp(host->device_name, "sdio_sd") == 0)) {
		host->pinctrl = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(host->pinctrl)) {
			dev_err(dev, "Can't find pinctrl\n");
			goto out;
		}

		host->pins_sl_off =
			pinctrl_lookup_state(host->pinctrl, "sd0_sl_0");
		host->pins_sl_on =
			pinctrl_lookup_state(host->pinctrl, "sd0_sl_1");

		if (!IS_ERR_OR_NULL(host->pins_sl_off) &&
			!IS_ERR_OR_NULL(host->pins_sl_on))
			host->flags |= SPRD_PINCTRL_AVOID_LEAK_VOLTAGE;

		host->pins_uhs =
			pinctrl_lookup_state(host->pinctrl, "sd0_ms_1");
		host->pins_default =
			pinctrl_lookup_state(host->pinctrl, "sd0_ms_0");

		if (!IS_ERR_OR_NULL(host->pins_uhs) &&
			!IS_ERR_OR_NULL(host->pins_default))
			host->flags |= SPRD_PINCTRL_SWITCH_VOLTAGE;
	}

out:
	return 0;

err:
	dev_err(dev, "get basic resource fail\n");
	return ret;
}

static int sprd_get_ext_resource(struct sprd_sdhc_host *host)
{
	int err;
	struct mmc_host *mmc = host->mmc;
	struct device *dev = &host->pdev->dev;

	host->data_timeout_val = 0;

	err = mmc_regulator_get_supply(mmc);
	if (err) {
		if (err != -EPROBE_DEFER)
			dev_err(dev, "Could not get vmmc supply\n");
		return err;
	}
	host->mmc = mmc;
	err = request_irq(host->irq, sprd_sdhc_irq,
			IRQF_SHARED, mmc_hostname(host->mmc), host);

	if (err) {
		dev_err(dev, "Can't request irq\n");
		return err;
	}
#if defined(CONFIG_X86)
	else {
		int res;

		res = irq_set_affinity(host->irq, cpumask_of(0));
		dev_err(dev, "%s: affinity irq to cpu 0 result=%d\n",
			host->device_name, res);
	}
#endif

	tasklet_init(&host->finish_tasklet,
		sprd_sdhc_finish_tasklet, (unsigned long)host);
	/* init timer */
	setup_timer(&host->timer, sprd_sdhc_timeout, (unsigned long)host);

	return 0;
}

static void sprd_set_mmc_struct(struct sprd_sdhc_host *host,
				struct mmc_host *mmc)
{
	struct device_node *np = host->pdev->dev.of_node;
	struct device *dev = &host->pdev->dev;
	u32 host_caps;

	mmc = host->mmc;
	mmc->ops = &sprd_sdhc_ops;
	mmc->f_max = host->base_clk;
	mmc->f_min = 400000;

	mmc->caps = MMC_CAP_SD_HIGHSPEED | MMC_CAP_MMC_HIGHSPEED |
		MMC_CAP_ERASE | MMC_CAP_CMD23;

	mmc_of_parse(mmc);
	mmc_of_parse_voltage(np, &host->ocr_mask);
    /*
     * if there is not regulator device in some of the SPRD platform, the
     * regulator driver will not be used and the REGULATOR will not be configed
     * in defconfig, in this case the mmc->supply.vmmc will be NULL.
     */

	host_caps = sprd_sdhc_readw(host, SPRD_SDHC_REG_16_HOST_CAP);

	mmc->ocr_avail = 0x40000;
	mmc->ocr_avail_sdio = mmc->ocr_avail;
	mmc->ocr_avail_sd = mmc->ocr_avail;
	mmc->ocr_avail_mmc = mmc->ocr_avail;

	mmc->max_current_330 = SPRD_SDHC_MAX_CUR;
	mmc->max_current_300 = SPRD_SDHC_MAX_CUR;
	mmc->max_current_180 = SPRD_SDHC_MAX_CUR;

	mmc->max_req_size = 524288;	/* 512k */
	mmc->max_blk_count = 65535;
	mmc->max_blk_size =  512 << (host_caps & SPRD_SDHC_BIT_BLOCK_SIZE_MASK);
	if (host->flags & SPRD_USE_ADMA) {
		mmc->max_segs = 128;
		mmc->max_seg_size = 65536;
		if (host->flags & SPRD_USE_64_BIT_DMA) {
			host->adma_table_sz =
				((SPRD_MAX_SEGS * 2 + 1) *
				SPRD_ADMA2_64_DESC_SZ);
			host->adma_desc_sz = SPRD_ADMA2_64_DESC_SZ;
		} else {
			host->adma_table_sz =
				((SPRD_MAX_SEGS * 2 + 1) *
				SPRD_ADMA2_32_DESC_SZ);
			host->adma_desc_sz = SPRD_ADMA2_32_DESC_SZ;
		}
		host->adma_desc = dma_alloc_coherent(mmc_dev(mmc),
							host->adma_table_sz,
							&host->adma_addr,
							GFP_KERNEL);
		host->align_buffer = dma_alloc_coherent(mmc_dev(mmc),
						      SPRD_ALIGN_BUFFER_SZ,
						      &host->align_addr,
						      GFP_KERNEL);

		if (!host->adma_desc || !host->align_buffer) {
			if (host->adma_desc)
				dma_free_coherent(mmc_dev(mmc),
					host->adma_table_sz, host->adma_desc,
					host->adma_addr);
			if (host->align_buffer)
				dma_free_coherent(mmc_dev(mmc),
				SPRD_ALIGN_BUFFER_SZ,
				host->align_buffer, host->align_addr);

			host->flags &= ~SPRD_USE_ADMA;
			mmc->max_segs = 1;
			mmc->max_seg_size = mmc->max_req_size;
			host->adma_desc = NULL;
			host->align_buffer = NULL;
			host->adma_table_sz = 0;
			host->adma_desc_sz = 0;
			dev_err(dev, "Unable to allocate ADMA buffers.\n");
		} else
			dev_info(dev, "using ADMA2\n");
	} else {
		mmc->max_segs = 1;
		mmc->max_seg_size = mmc->max_req_size;
		host->adma_desc = NULL;
		host->align_buffer = NULL;
		host->adma_table_sz = 0;
		host->adma_desc_sz = 0;
	}

	host->dma_mask = DMA_BIT_MASK(64);
	mmc_dev(host->mmc)->dma_mask = &host->dma_mask;

	mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;

	dev_info(dev, "ocr avail = 0x%x, base clock = %u\n"
		 "pm_caps = 0x%x, caps: 0x%x, caps2: 0x%x,\n"
		 "host->flags:0x%x, host_caps:0x%x\n",
		 mmc->ocr_avail, host->base_clk, mmc->pm_caps,
		 mmc->caps, mmc->caps2, host->flags, host_caps);
}

#ifdef CONFIG_PM_SLEEP
static int sprd_sdhc_suspend(struct device *dev)
{
	struct platform_device *pdev =
		container_of(dev, struct platform_device, dev);
	struct sprd_sdhc_host *host = platform_get_drvdata(pdev);

	sprd_sdhc_runtime_pm_get(host);
	disable_irq(host->irq);
	clk_disable_unprepare(host->clk);
	clk_disable_unprepare(host->sdio_ahb);
	if (host->sdio_ckg)
		clk_disable_unprepare(host->sdio_ckg);
	if (host->sdio_1x_ckg)
		clk_disable_unprepare(host->sdio_1x_ckg);

	return 0;
}

static int sprd_sdhc_resume(struct device *dev)
{
	struct platform_device *pdev =
		container_of(dev, struct platform_device, dev);
	struct sprd_sdhc_host *host = platform_get_drvdata(pdev);
	struct mmc_ios ios;

	if (host->sdio_1x_ckg)
		clk_prepare_enable(host->sdio_1x_ckg);
	if (host->sdio_ckg)
		clk_prepare_enable(host->sdio_ckg);
	clk_prepare_enable(host->sdio_ahb);
	clk_prepare_enable(host->clk);
	enable_irq(host->irq);

	ios = host->mmc->ios;
	sprd_reset_ios(host);
	host->mmc->ops->set_ios(host->mmc, &ios);

	sprd_sdhc_runtime_pm_put(host);

	return 0;
}
#endif

#ifdef CONFIG_PM
static int sprd_sdhc_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev =
		container_of(dev, struct platform_device, dev);
	struct sprd_sdhc_host *host = platform_get_drvdata(pdev);
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	sprd_sdhc_disable_all_int(host);
	sprd_sdhc_all_clk_off(host);
	spin_unlock_irqrestore(&host->lock, flags);

	clk_disable_unprepare(host->clk);
	clk_disable_unprepare(host->sdio_ahb);
	if (host->sdio_ckg)
		clk_disable_unprepare(host->sdio_ckg);
	if (host->sdio_1x_ckg)
		clk_disable_unprepare(host->sdio_1x_ckg);
	synchronize_irq(host->irq);

	return 0;
}

static int sprd_sdhc_runtime_resume(struct device *dev)
{
	struct platform_device *pdev =
		container_of(dev, struct platform_device, dev);
	struct sprd_sdhc_host *host = platform_get_drvdata(pdev);
	unsigned long flags;

	if (host->sdio_1x_ckg)
		clk_prepare_enable(host->sdio_1x_ckg);
	if (host->sdio_ckg)
		clk_prepare_enable(host->sdio_ckg);
	clk_prepare_enable(host->sdio_ahb);
	clk_prepare_enable(host->clk);
	spin_lock_irqsave(&host->lock, flags);
	if (host->ios.clock) {
		sprd_sdhc_sd_clk_off(host);
		sprd_sdhcr10_clk_set_and_on(host,
			sprd_sdhc_calc_div(host->base_clk, host->ios.clock));
		sprd_sdhc_sd_clk_on(host);
	}
	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);

	return 0;
}
#endif

static const struct dev_pm_ops sprd_sdhc_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(sprd_sdhc_suspend, sprd_sdhc_resume)
	SET_RUNTIME_PM_OPS(sprd_sdhc_runtime_suspend,
		sprd_sdhc_runtime_resume, NULL)
};

static const struct of_device_id sprd_sdhc_of_match[] = {
	{.compatible = "sprd,sdhc-r10"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sprd_sdhc_of_match);

static int sprd_sdhc_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct sprd_sdhc_host *host;
	struct device *dev = &pdev->dev;
	int ret;

	/* globe resource */
	mmc = mmc_alloc_host(sizeof(struct sprd_sdhc_host), &pdev->dev);
	if (!mmc) {
		dev_err(dev, "no memory for mmc host\n");
		return -ENOMEM;
	}

	host = mmc_priv(mmc);
	host->mmc = mmc;
	host->pdev = pdev;
	host->flags = 0;
	spin_lock_init(&host->lock);
	platform_set_drvdata(pdev, host);

	/* get basic resource from device tree */
	ret = sprd_get_dt_resource(pdev, host);
	if (ret) {
		dev_err(dev, "fail to get basic resource: %d\n", ret);
		goto err_free_host;
	}

	ret = sprd_get_ext_resource(host);
	if (ret) {
		dev_err(dev, "fail to get external resource: %d\n", ret);
		goto err_free_host;
	}

	clk_prepare_enable(host->clk);
	clk_prepare_enable(host->sdio_ahb);
	if (host->sdio_ckg)
		clk_prepare_enable(host->sdio_ckg);
	if (host->sdio_1x_ckg)
		clk_prepare_enable(host->sdio_1x_ckg);
	sprd_reset_ios(host);

	sprd_set_mmc_struct(host, mmc);
	host->version = sprd_sdhc_readw(host, SPRD_SDHC_REG_16_HOST_VER);
	#ifdef CONFIG_64BIT
	sprd_sdhc_set_64bit_addr(host, 1);
	#else
	sprd_sdhc_set_64bit_addr(host, 0);
	#endif
	sprd_sdhc_set_dll_backup(host, SPRD_SDHC_BIT_DLL_BAK |
							SPRD_SDHC_BIT_DLL_VAL);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 50);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, 1);

	/* add host */
	ret = mmc_add_host(mmc);
	if (ret) {
		dev_err(dev, "failed to add mmc host: %d\n", ret);
		goto err_free_host;
	}
	dev_info(dev, "%s[%s] host controller, irq %d\n",
		 host->device_name, mmc_hostname(mmc), host->irq);

	return 0;

err_free_host:
	mmc_free_host(mmc);
	return ret;
}

static int sprd_sdhc_remove(struct platform_device *pdev)
{
	struct sprd_sdhc_host *host = platform_get_drvdata(pdev);
	struct mmc_host *mmc = host->mmc;

	mmc_remove_host(mmc);
	clk_disable_unprepare(host->clk);
	clk_disable_unprepare(host->sdio_ahb);
	if (host->adma_desc)
		dma_free_coherent(mmc_dev(mmc),
			host->adma_table_sz, host->adma_desc,
			host->adma_addr);
	if (host->align_buffer)
		dma_free_coherent(mmc_dev(mmc), SPRD_ALIGN_BUFFER_SZ,
			host->align_buffer, host->align_addr);
	host->adma_desc = NULL;
	host->align_buffer = NULL;

	mmc_free_host(mmc);

	return 0;
}

static struct platform_driver sprd_sdhc_driver = {
	.probe = sprd_sdhc_probe,
	.remove = sprd_sdhc_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(sprd_sdhc_of_match),
		.pm = &sprd_sdhc_pmops,
	},
};

module_platform_driver(sprd_sdhc_driver);

MODULE_DESCRIPTION("Spreadtrum sdio host controller driver");
MODULE_LICENSE("GPL");
