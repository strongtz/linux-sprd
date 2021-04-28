#ifndef _SPRD_NAND_H_
#define _SPRD_NAND_H_

#include <linux/io.h>
#include <linux/kernel.h>

/* reg memory map --checked */
#define NFC_START_REG 0x00
#define NFC_CFG0_REG 0x04
#define NFC_CFG1_REG 0x08
#define NFC_CFG2_REG 0x0C
#define NFC_INT_REG 0x10
#define NFC_TIMING0_REG 0x14
#define NFC_TIMING1_REG 0x18
#define NFC_TIMING2_REG 0x1C
#define NFC_STAT_STSMCH_REG 0x30
#define NFC_TIMEOUT_REG 0x34
#define NFC_CFG3_REG 0x38
#define NFC_STATUS0_REG 0x40
#define NFC_STATUS1_REG 0x44
#define NFC_STATUS2_REG 0x48
#define NFC_STATUS3_REG 0x4C
#define NFC_STATUS4_REG 0x50
#define NFC_STATUS5_REG 0x54
#define NFC_STATUS6_REG 0x58
#define NFC_STATUS7_REG 0x5C
#define NFC_STATUS8_REG 0xA4

#define NFC_POLY0_REG 0xB0
#define NFC_POLY1_REG 0xB4
#define NFC_POLY2_REG 0xB8
#define NFC_POLY3_REG 0xBC
#define NFC_POLY4_REG 0x60
#define NFC_POLY5_REG 0x64
#define NFC_POLY6_REG 0x68
#define NFC_POLY7_REG 0x6C

#define NFC_DLL0_CFG 0xE0
#define NFC_DLL1_CFG 0xE4
#define NFC_DLL2_CFG 0xE8
#define NFC_DLL_REG 0xEC
#define NFC_CFG4_REG 0xF8

#define NFC_FREE_COUNT0_REG 0x160
#define NFC_FREE_COUNT1_REG 0x164
#define NFC_FREE_COUNT2_REG 0x168
#define NFC_FREE_COUNT3_REG 0x16C
#define NFC_FREE_COUNT4_REG 0x170

#define NFC_MAIN_ADDRH_REG 0x200
#define NFC_MAIN_ADDRL_REG 0x204
#define NFC_SPAR_ADDRH_REG 0x208
#define NFC_SPAR_ADDRL_REG 0x20C
#define NFC_STAT_ADDRH_REG 0x210
#define NFC_STAT_ADDRL_REG 0x214
#define NFC_SEED_ADDRH_REG 0x218
#define NFC_SEED_ADDRL_REG 0x21C
#define NFC_INST00_REG 0x220
#define NFC_INST14_REG 0x258
#define NFC_INST15_REG 0x25C
#define NFC_INST23_REG 0x270

#define NFC_MAX_MC_INST_NUM 24

/* NFC_START bit define --checked */
#define CTRL_NFC_VALID BIT(31)
#define CTRL_DEF1_ECC_MODE(mode) (((mode) & 0xF) << 11)
#define CTRL_NFC_CMD_CLR BIT(1)
#define CTRL_NFC_CMD_START BIT(0)

/* NFC_CFG0 bit define --checked */
#define CFG0_DEF0_MAST_ENDIAN (0)
#define CFG0_DEF1_SECT_NUM(num) (((num - 1) & 0x1F) << 24)
#define CFG0_SET_SECT_NUM_MSK (0x1F << 24)
#define CFG0_SET_REPEAT_NUM(num) (((num - 1) & 0xFF) << 16)
#define CFG0_SET_WPN BIT(15)
#define CFG0_DEF1_BUS_WIDTH(width) (((!!(width != BW_08)) & 0x1) << 14)
#define CFG0_SET_SPARE_ONLY_INFO_PROD_EN BIT(13)
#define CFG0_DEF0_SECT_NUM_IN_INST (0 << 12)
#define CFG0_DEF0_DETECT_ALL_FF BIT(11)
#define CFG0_SET_CS_SEL(cs) (((cs) & 0x3) << 9)
#define CFG0_CS_MAX 4
#define CFG0_CS_MSKCLR (~(GENMASK(10, 9)))
#define CFG0_SET_NFC_RW BIT(8)
#define CFG0_DEF1_MAIN_SPAR_APT(sectperpage) ((sectperpage == 1) ? 0 : BIT(6))
#define CFG0_SET_SPAR_USE BIT(5)
#define CFG0_SET_MAIN_USE BIT(4)
#define CFG0_SET_ECC_EN BIT(2)
#define CFG0_SET_NFC_MODE(mode) ((mode) & 0x3)

/* NFC_CFG1 bit define --checked */
#define CFG1_DEF1_SPAR_INFO_SIZE(size) (((size) & 0x7F) << 24)
#define CFG1_DEF1_SPAR_SIZE(size) ((((size) - 1) & 0x7F) << 16)
#define CFG1_INTF_TYPE(type) (((type) & 0x07) << 12)
#define CFG1_DEF_MAIN_SIZE(size) ((((size) - 1)) & 0x7FF)
#define CFG1_temp_MIAN_SIZE_MSK (0x7FF)

/* NFC_CFG2 bit define --checked */
#define CFG2_DEF1_SPAR_SECTOR_NUM(num) (((num - 1) & 0x1F) << 24)
#define CFG2_DEF1_SPAR_INFO_POS(pos) (((pos) & 0x7F) << 16)
#define CFG2_DEF1_ECC_POSITION(pos) ((pos) & 0x7F)

/* NFC_CFG3 --checked may be unused  */
#define CFG3_SEED_LOOP_CNT(cnt) (((cnt) & 0x3FF) << 16)
#define CFG3_SEED_LOOP_EN BIT(2)
#define CFG3_DETECT_ALL_FF BIT(3)
#define CFG3_POLY_4R1_EN BIT(1)
#define CFG3_RANDOM_EN BIT(0)

/* NFC_CFG4 --checked may be unused  */
#define CFG4_ONLY_SEL_MODE BIT(14)
#define CFG4_PHY_DLL_CLK_2X_EN BIT(2)
#define CFG4_SLICE_CLK_EN BIT(1)

/* NFC_POLYNOMIALS --checked may be unused  */
#define NFC_POLYNOMIALS0 0x100d
#define NFC_POLYNOMIALS1 0x10004
#define NFC_POLYNOMIALS2 0x40013
#define NFC_POLYNOMIALS3 0x400010

/* NFC register --checked */
#define INT_DONE_RAW BIT(24)
#define INT_STSMCH_CLR BIT(11)
#define INT_WP_CLR BIT(10)
#define INT_TO_CLR BIT(9)
#define INT_DONE_CLR BIT(8)
#define INT_STSMCH BIT(3)
#define INT_WP BIT(2)
#define INT_TO BIT(1)
#define INT_DONE BIT(0)

/* NFC_TIMING0 --checked */
#define NFC_ACS_OFFSET (27)
#define NFC_ACS_MASK (GENMASK((NFC_ACS_OFFSET + 4), NFC_ACS_OFFSET))
#define NFC_ACE_OFFSET (22)
#define NFC_ACE_MASK (GENMASK((NFC_ACE_OFFSET + 4), NFC_ACE_OFFSET))
#define NFC_RWS_OFFSET (16)
#define NFC_RWS_MASK (GENMASK((NFC_RWS_OFFSET + 5), NFC_RWS_OFFSET))
#define NFC_RWE_OFFSET (11)
#define NFC_RWE_MASK (GENMASK((NFC_RWE_OFFSET + 5), NFC_RWE_OFFSET))
#define NFC_RWH_OFFSET (6)
#define NFC_RWH_MASK (GENMASK((NFC_RWH_OFFSET + 4), NFC_RWH_OFFSET))
#define NFC_RWL_MASK (GENMASK(5, 0))

#define TIME0_ACS(acs) (((acs - 1) & 0x1F) << 27)
#define TIME0_ACE(ace) (((ace - 1) & 0x1F) << 22)
#define TIME0_RDS(rds) (((rds - 1) & 0x3F) << 16)
#define TIME0_RDE(rde) (((rde - 1) & 0x1F) << 11)
#define TIME0_RWH(rwh) (((rwh - 1) & 0x1F) << 6)
#define TIME0_RWL(rwl) ((rwl - 1) & 0x3F) /* must >= 2  */

/* NFC_TIMING1 --checked may be unused */
#define TIME1_WTE(wte) (((wte - 1) & 0x1F) << 26)
#define TIME1_WTS(wts) (((wts - 1) & 0x1F) << 21)
#define TIME1_WTI(wti) (((wti - 1) & 0x1F) << 16)
#define TIME1_CL0(cl0) (((cl0 - 1) & 0x1F) << 10)
#define TIME1_CL1(cl1) (((cl1 - 1) & 0x1F) << 5)
#define TIME1_RDI(rdi) (((rdi - 1) & 0x1F) << 0)

/* NFC_TIMEOUT bit define --checked */
#define TIMEOUT_REPT_EN BIT(31)
#define TIMEOUT(val) (val & 0x7FFFFFFF)
/* NFC Ram address --checked */
/* 0xFFFF'FFFF means not to move data to read buf, when read. */
#define RAM_MAIN_ADDR(addr) ((unsigned long)(addr))
/* 0xFFFF'FFFF means not to move data to read buf, when read. */
#define RAM_SPAR_ADDR(addr) ((unsigned long)(addr))
/* 0xFFFF'FFFF means not to move data to read buf, when read. */
#define RAM_STAT_ADDR(addr) ((unsigned long)(addr))
#define RAM_SEED_ADDR(addr) ((unsigned long)(addr))
/* NFC status mach --checked */
/* check IO 0_bit, stop if error */
#define MACH_ERASE (BIT(0) | BIT(11) | BIT(22))
/* check IO 0_bit, stop if error */
#define MACH_WRITE (BIT(0) | BIT(11) | BIT(22))
#define MACH_READ (0)
#define DEF0_MATCH (0)
/* NFC_STATUS0_REG bit define --checked */
#define ECC_STAT(status) (((status) >> 9) & 0x3)
#define ECC_NUM(status) ((status) & 0x1FF)
/* NFC Micro-Instrction Register --checked */
#define CMD_TYPE1(cmdid, param)                                                \
	((u16)(((param & 0xff) << 8) | (cmdid & 0xff)))
#define CMD_TYPE2(cmdid) ((u16)(cmdid & 0xff))
#define CMD_TYPE3(cmdid, param1, param0)                                       \
	((u16)(((param1 & 0xff) << 8) | ((cmdid & 0xf) << 4) | (param0 & 0xf)))
#define INST_CMD(cmd) CMD_TYPE1(0xCD, (cmd))
#define INST_ADDR(addr, step) CMD_TYPE3(0x0A, (addr), (step))
#define INST_WRB0() CMD_TYPE2(0xB0)
#define INST_WRB1(cycle) CMD_TYPE1(0xB1, (cycle))
#define INST_MRDT() CMD_TYPE2(0xD0)
#define INST_MWDT() CMD_TYPE2(0xD1)
#define INST_SRDT() CMD_TYPE2(0xD2)
#define INST_SWDT() CMD_TYPE2(0xD3)
#define INST_IDST(num) CMD_TYPE1(0xDD, (num - 1))
/* 0 or 1, priority > _CFG0_CS_SEL */
#define INST_CSEN(en) CMD_TYPE1(0xCE, (en))
#define INST_INOP(num) CMD_TYPE1(0xF0, (num - 1))
#define INST_DONE() CMD_TYPE2(0xFF)
/* Other define */
#define NFC_MAX_CHIP 4
#define NFC_TIMEOUT_VAL 3000000      /* usecs */
#define NFC_RESET_TIMEOUT_VAL 500000 /* usecs */

#define NFC_READID_TIMING                                                     \
	((0x1f) | (7 << NFC_RWH_OFFSET) | (0x1f << NFC_RWE_OFFSET) |          \
	 (0x1f << NFC_RWS_OFFSET) | (0x1f << NFC_ACE_OFFSET) |		\
	 (0x1f << NFC_ACS_OFFSET))
#define NFC_DEFAULT_TIMING                                                     \
	((7) | (6 << NFC_RWH_OFFSET) | (7 << NFC_RWE_OFFSET) |                 \
	 (7 << NFC_RWS_OFFSET) | (7 << NFC_ACE_OFFSET) | (7 << NFC_ACS_OFFSET))
#define GET_CYCLE(ns)                                                          \
	((u32)(((u32)(((host->frequence / 1000000) * ns) / 1000)) + 1))
#define NFC_MBUF_SIZE 4096
#define SEED_TBL_SIZE 64
#define SEED_BUF_SIZE 68
#define PAGE2SEED_ADDR_OFFSET(pg) ((pg & GENMASK(3, 0)) << 2)
#define SPL_MAX_SIZE BIT(16)
#define SPRD_NAND_PAGE_MASK GENMASK(7, 0)
#define SPRD_NAND_PAGE_SHIFT_0(pg) ((pg) & SPRD_NAND_PAGE_MASK)
#define SPRD_NAND_PAGE_SHIFT_8(pg) (((pg) >> 8) & SPRD_NAND_PAGE_MASK)
#define SPRD_NAND_PAGE_SHIFT_16(pg) (((pg) >> 16) & SPRD_NAND_PAGE_MASK)
#define SPRD_NAND_GET_INT_VAL(x) ((x >> 24) & 0xF)

#endif
