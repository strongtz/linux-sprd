/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt)  "sprd-sysdump: " fmt

#include <asm/cacheflush.h>
#include <linux/delay.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/highuid.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/reboot.h>
#include <linux/rtc.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/sysrq.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/regmap.h>
#ifdef CONFIG_SPRD_SIPC
#include <linux/sipc.h>
#endif
#include "sysdump.h"
#include "sysdumpdb.h"
#include <linux/kallsyms.h>
#include <asm/stacktrace.h>
#include <asm-generic/kdebug.h>
#include <linux/kdebug.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif

#include <crypto/hash.h>
#include <linux/crypto.h>
#include <crypto/sha.h>
#include <asm/sections.h>

#define CORE_STR	"CORE"
#ifndef ELF_CORE_EFLAGS
#define ELF_CORE_EFLAGS	0
#endif


#define SYSDUMP_MAGIC	"SPRD_SYSDUMP_119"
#define SYSDUMP_MAGIC_VOLUP  (0x766f7570) // v-l-u-p
#define SYSDUMP_MAGIC_VOLDN  (0X766f646e) // v-l-d-n

#define SYSDUMP_NOTE_BYTES (ALIGN(sizeof(struct elf_note), 4) +   \
			    ALIGN(sizeof(CORE_STR), 4) + \
			    ALIGN(sizeof(struct elf_prstatus), 4))

#define DUMP_REGS_SIZE min(sizeof(elf_gregset_t), sizeof(struct pt_regs))

#define ANA_RST_STATUS_OFFSET_2730 (0x1bac) /* pmic 2730 rst status register offset */
#define HWRST_STATUS_SYSDUMP  (0x200)
#define ANA_RST_STATUS_OFFSET_2721 (0xed8)  /* pmic 2721 rst status register offset */
static unsigned int pmic_reg;

#ifdef CONFIG_SPRD_MINI_SYSDUMP /*	minidump code start	*/
#define REG_SP_INDEX	31
#define REG_PC_INDEX	32
extern void stext(void);
struct pt_regs pregs_die_g;
int  die_notify_flag;
struct pt_regs minidump_regs_g;
static int prepare_minidump_info(struct pt_regs *regs);
struct info_desc minidump_info_desc_g;
unsigned int pt_data_len;

struct minidump_info  minidump_info_g =	{
	.kernel_magic			=	KERNEL_MAGIC,
	.regs_info			=	{
#ifdef CONFIG_ARM
		.arch			=	ARM,
		.num			=	16,
		.size			=	sizeof(struct pt_regs)
#endif
#ifdef CONFIG_ARM64
		.arch			=	ARM64,
		.num			=	33, /*x0~x30 and SP(x31) ,PC  x30 = lr */
		.size			=	sizeof(struct pt_regs)
#endif
#ifdef CONFIG_X86_64
		.arch			=	X86_64,
		.num			=	32,
		.size			=	sizeof(struct pt_regs)
#endif
	},
	.regs_memory_info		=	{
		.per_reg_memory_size	=	256,
		.valid_reg_num	=	0,
	},
	.section_info_total		=	{
		.section_info		=	{
			{"text", (unsigned long)_stext, (unsigned long)_etext, 0, 0, 0},
			{"data", (unsigned long)_sdata, (unsigned long)_edata, 0, 0, 0},
			{"bss", (unsigned long)__bss_start, (unsigned long)__bss_stop, 0, 0, 0},
			{"init", (unsigned long)__init_begin, (unsigned long)__init_end, 0, 0, 0},
			{"inittext", (unsigned long)_sinittext, (unsigned long)_einittext, 0, 0, 0},
			{"rodata", (unsigned long)__start_rodata, (unsigned long)__end_rodata, 0, 0, 0},
			{"per_cpu", (unsigned long)__per_cpu_start, (unsigned long)__per_cpu_end, 0, 0, 0},
			{"log_buf", 0, 0, 0, 0, 0},
			{"ylog_buf", 0, 0, 0, 0, 0},
			{"kernel_pt", 0, 0, 0, 0, 0},
			{"", 0, 0, 0, 0, 0},

		},
		.total_size	=	0,
		.total_num	=	0,
	},
	.compressed			=	1,
};
static int prepare_exception_info(struct pt_regs *regs,
			struct task_struct *tsk, const char *reason);
static char *ylog_buffer;
#endif /*	minidump code end	*/
typedef char note_buf_t[SYSDUMP_NOTE_BYTES];

static DEFINE_PER_CPU(note_buf_t, crash_notes_temp);
note_buf_t __percpu *crash_notes;

/* An ELF note in memory */
struct memelfnote {
	const char *name;
	int type;
	unsigned int datasz;
	void *data;
};

struct sysdump_info {
	char magic[16];
	char time[32];
	char reason[32];
	char dump_path[128];
	int elfhdr_size;
	int mem_num;
	unsigned long dump_mem_paddr;
	int crash_key;
};

struct sysdump_extra {
	int enter_id;
	int enter_cpu;
	char reason[256];
	struct pt_regs cpu_context[CONFIG_NR_CPUS];
};

struct sysdump_config {
	int enable;
	int crashkey_only;
	int dump_modem;
	int reboot;
	char dump_path[128];
};

static struct sysdump_info *sprd_sysdump_info;
static unsigned long sysdump_magic_paddr;

/* global var for memory hash */
static u8 g_ktxt_hash_data[SHA1_DIGEST_SIZE];
static struct shash_desc *desc;

/* must be global to let gdb know */
struct sysdump_extra sprd_sysdump_extra = {
	.enter_id = -1,
	.enter_cpu = -1,
	.reason = {0},
};

static struct sysdump_config sysdump_conf = {
	.enable = 1,
	.crashkey_only = 0,
	.dump_modem = 1,
	.reboot = 1,
	.dump_path = "",
};

static int sprd_sysdump_init;

int sysdump_status;
struct regmap *regmap;
#ifdef CONFIG_SPRD_WATCHDOG_SYS
extern void sysdump_enable_watchdog(int on);
#else
#define sysdump_enable_watchdog(on) do { } while (0)
#endif
static int set_sysdump_enable(int on);


void sprd_debug_check_crash_key(unsigned int code, int value)
{
	static unsigned int volup_p;
	static unsigned int voldown_p;
	static unsigned int loopcount;
	static unsigned long vol_pressed;

#if 0
	/* Must be deleted later */
	pr_info("Test %s:key code(%d) value(%d),(up:%d,down:%d),lpct(%d),vop(%ld)\n", __func__,
		code, value, volup_p, voldown_p, loopcount, vol_pressed);
#endif

	/*  Enter Force Upload
	 *  hold the volume down and volume up
	 *  and then press power key twice
	 */
	if (value) {
		if (code == KEY_VOLUMEUP)
			volup_p = SYSDUMP_MAGIC_VOLUP;
		if (code == KEY_VOLUMEDOWN)
			voldown_p = SYSDUMP_MAGIC_VOLDN;

		if ((volup_p == SYSDUMP_MAGIC_VOLUP) && (voldown_p == SYSDUMP_MAGIC_VOLDN)) {
			if (!vol_pressed)
				vol_pressed = jiffies;

			if (code == KEY_POWER) {
				pr_info("%s: Crash key count : %d,vol_pressed:%ld\n", __func__,
					++loopcount, vol_pressed);
				if (time_before(jiffies, vol_pressed + 5 * HZ)) {
					if (loopcount == 2)
						panic("Crash Key");
				} else {
					pr_info("%s: exceed 5s(%u) between power key and volup/voldn key\n",
						__func__, jiffies_to_msecs(jiffies - vol_pressed));
					volup_p = 0;
					voldown_p = 0;
					loopcount = 0;
					vol_pressed = 0;
				}
			}
		}
	} else {
		if (code == KEY_VOLUMEUP) {
			volup_p = 0;
			loopcount = 0;
			vol_pressed = 0;
		}
		if (code == KEY_VOLUMEDOWN) {
			voldown_p = 0;
			loopcount = 0;
			vol_pressed = 0;
		}
	}
}

static char *storenote(struct memelfnote *men, char *bufp)
{
	struct elf_note en;

#define DUMP_WRITE(addr, nr) do {memcpy(bufp, addr, nr); bufp += nr; } while (0)

	en.n_namesz = strlen(men->name) + 1;
	en.n_descsz = men->datasz;
	en.n_type = men->type;

	DUMP_WRITE(&en, sizeof(en));
	DUMP_WRITE(men->name, en.n_namesz);

	/* XXX - cast from long long to long to avoid need for libgcc.a */
	bufp = (char *)roundup((unsigned long)bufp, 4);
	DUMP_WRITE(men->data, men->datasz);
	bufp = (char *)roundup((unsigned long)bufp, 4);

#undef DUMP_WRITE

	return bufp;
}

/*
 * fill up all the fields in prstatus from the given task struct, except
 * registers which need to be filled up separately.
 */
static void fill_prstatus(struct elf_prstatus *prstatus,
			  struct task_struct *p, long signr)
{
	prstatus->pr_info.si_signo = prstatus->pr_cursig = signr;
	prstatus->pr_sigpend = p->pending.signal.sig[0];
	prstatus->pr_sighold = p->blocked.sig[0];
	rcu_read_lock();
	prstatus->pr_ppid = task_pid_vnr(rcu_dereference(p->real_parent));
	rcu_read_unlock();
	prstatus->pr_pid = task_pid_vnr(p);
	prstatus->pr_pgrp = task_pgrp_vnr(p);
	prstatus->pr_sid = task_session_vnr(p);
	if (0 /* thread_group_leader(p) */) {
		struct task_cputime cputime;

		/*
		 * This is the record for the group leader.  It shows the
		 * group-wide total, not its individual thread total.
		 */
		/* thread_group_cputime(p, &cputime); */
		prstatus->pr_utime = ns_to_timeval(cputime.utime);
		prstatus->pr_stime = ns_to_timeval(cputime.stime);
	} else {
		prstatus->pr_utime = ns_to_timeval(p->utime);
		prstatus->pr_stime = ns_to_timeval(p->stime);
	}
	prstatus->pr_cutime = ns_to_timeval(p->signal->cutime);
	prstatus->pr_cstime = ns_to_timeval(p->signal->cstime);

}

void crash_note_save_cpu(struct pt_regs *regs, int cpu)
{
	struct elf_prstatus prstatus;
	struct memelfnote notes;

	notes.name = CORE_STR;
	notes.type = NT_PRSTATUS;
	notes.datasz = sizeof(struct elf_prstatus);
	notes.data = &prstatus;

	memset(&prstatus, 0, sizeof(struct elf_prstatus));
	fill_prstatus(&prstatus, current, 0);
	memcpy(&prstatus.pr_reg, regs, DUMP_REGS_SIZE);
	/* memcpy(&prstatus.pr_reg, regs, sizeof(struct pt_regs)); */
	storenote(&notes, (char *)per_cpu_ptr(crash_notes, cpu));
}

static void sysdump_fill_core_hdr(struct pt_regs *regs, char *bufp)
{
	struct elf_phdr *nhdr;

	/* setup ELF header */
	bufp += sizeof(struct elfhdr);

	/* setup ELF PT_NOTE program header */
	nhdr = (struct elf_phdr *)bufp;
	memset(nhdr, 0, sizeof(struct elf_phdr));
	nhdr->p_memsz = SYSDUMP_NOTE_BYTES * NR_CPUS;

	return;
}

static int __init sysdump_magic_setup(char *str)
{
	if (str != NULL)
		sscanf(&str[0], "%lx", &sysdump_magic_paddr);

	pr_info("[%s]SYSDUMP paddr from uboot: 0x%lx\n",
		 __func__, sysdump_magic_paddr);
	return 1;
}

__setup("sysdump_magic=", sysdump_magic_setup);

static unsigned long get_sprd_sysdump_info_paddr(void)
{
	struct device_node *node;
	unsigned long *magic_addr;
	unsigned long reg_phy = 0;
	int aw = 0, len = 0;

	if (sysdump_magic_paddr)
		reg_phy = sysdump_magic_paddr;
	else {
		pr_err
		    ("Not find sysdump_magic_paddr from bootargs,use sysdump node from dts\n");
		node = of_find_node_by_name(NULL, "sprd-sysdump");

		if (!node) {
			pr_err
			    ("Not find sprd-sysdump node from dts,use SPRD_SYSDUMP_MAGIC\n");
			reg_phy = SPRD_SYSDUMP_MAGIC;
		} else {
			magic_addr =
			    (unsigned long *)of_get_property(node, "magic-addr",
							     &len);
			if (!magic_addr) {
				pr_err
				    ("Not find magic-addr property from sprd-sysdump node\n");
				reg_phy = SPRD_SYSDUMP_MAGIC;
			} else {
				aw = of_n_addr_cells(node);
				reg_phy =
				    of_read_ulong((const __be32 *)magic_addr,
						  aw);
			}
		}
	}
	return reg_phy;
}

static void sysdump_prepare_info(int enter_id, const char *reason,
				 struct pt_regs *regs)
{
	struct timex txc;
	struct rtc_time tm;

	strncpy(sprd_sysdump_extra.reason,
		reason, sizeof(sprd_sysdump_extra.reason)-1);
	sprd_sysdump_extra.enter_id = enter_id;
	memcpy(sprd_sysdump_info->magic, SYSDUMP_MAGIC,
	       sizeof(sprd_sysdump_info->magic));

	if (reason != NULL && !strcmp(reason, "Crash Key"))
		sprd_sysdump_info->crash_key = 1;
	else
		sprd_sysdump_info->crash_key = 0;

	pr_info("reason: %s, sprd_sysdump_info->crash_key: %d\n",
		 reason, sprd_sysdump_info->crash_key);
	do_gettimeofday(&(txc.time));
	txc.time.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time_to_tm(txc.time.tv_sec, &tm);
	sprintf(sprd_sysdump_info->time, "%04d-%02d-%02d_%02d:%02d:%02d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
		tm.tm_min, tm.tm_sec);

	memcpy(sprd_sysdump_info->dump_path, sysdump_conf.dump_path,
	       sizeof(sprd_sysdump_info->dump_path));

	sysdump_fill_core_hdr(regs,
			      (char *)sprd_sysdump_info +
			      sizeof(*sprd_sysdump_info));
	return;
}

DEFINE_PER_CPU(struct sprd_debug_core_t, sprd_debug_core_reg);
DEFINE_PER_CPU(struct sprd_debug_mmu_reg_t, sprd_debug_mmu_reg);

static inline void sprd_debug_save_context(void)
{
	unsigned long flags;
	local_irq_save(flags);
	sprd_debug_save_mmu_reg(&per_cpu
				(sprd_debug_mmu_reg, smp_processor_id()));
	sprd_debug_save_core_reg(&per_cpu
				 (sprd_debug_core_reg, smp_processor_id()));

	pr_emerg("(%s) context saved(CPU:%d)\n", __func__, smp_processor_id());
	local_irq_restore(flags);

	flush_cache_all();
}


void sysdump_enter(int enter_id, const char *reason, struct pt_regs *regs)
{
	struct pt_regs *pregs;

	bust_spinlocks(1);
	if (sprd_sysdump_init == 0) {
		unsigned long sprd_sysdump_info_paddr;
		sprd_sysdump_info_paddr = get_sprd_sysdump_info_paddr();
		if (!sprd_sysdump_info_paddr) {
			pr_emerg("get sprd_sysdump_info_paddr failed2.\n");
			while (1) {
				pr_emerg("sprd_sysdump_info_paddr failed...\n");
				mdelay(3000);
			}
		}

		sprd_sysdump_info = (struct sysdump_info *)phys_to_virt(sprd_sysdump_info_paddr);
		pr_emerg("vaddr is %p, paddr is %p.\n", sprd_sysdump_info, (void *)sprd_sysdump_info_paddr);

		crash_notes = &crash_notes_temp;
	}

	/* this should before smp_send_stop() to make sysdump_ipi enable */
	sprd_sysdump_extra.enter_cpu = smp_processor_id();

	pregs = &sprd_sysdump_extra.cpu_context[sprd_sysdump_extra.enter_cpu];
	if (regs)
		memcpy(pregs, regs, sizeof(*regs));
	else
		crash_setup_regs((struct pt_regs *)pregs, NULL);

	crash_note_save_cpu(pregs, sprd_sysdump_extra.enter_cpu);
	sprd_debug_save_context();

#ifdef CONFIG_SPRD_SIPC
	if (!(reason != NULL && strstr(reason, "cpcrash")))
		smsg_senddie(SIPC_ID_LTE);
#endif

	smp_send_stop();
	mdelay(1000);

	pr_emerg("\n");
	pr_emerg("*****************************************************\n");
	pr_emerg("*                                                   *\n");
	pr_emerg("*  Sysdump enter, preparing debug info to dump ...  *\n");
	pr_emerg("*                                                   *\n");
	pr_emerg("*****************************************************\n");
	pr_emerg("\n");

	if (reason != NULL)
		sysdump_prepare_info(enter_id, reason, regs);

#ifdef CONFIG_SPRD_MINI_SYSDUMP
	/* when track regs use pregs_die,  others use now regs*/
	if (die_notify_flag) {
		if (!user_mode(&pregs_die_g)) {
			prepare_minidump_info(&pregs_die_g);
			prepare_exception_info(&pregs_die_g, NULL, reason);
		} else {
			prepare_minidump_info(pregs);
			prepare_exception_info(pregs, NULL, reason);
		}
	} else {
		prepare_minidump_info(pregs);
		prepare_exception_info(pregs, NULL, reason);
	}
#endif
	if (sprd_sysdump_init) {
		pr_emerg("KTXT VERIFY...\n");
		crypto_shash_update(desc, (u8 *)_stext, _etext-_stext);
		crypto_shash_final(desc, g_ktxt_hash_data);

		pr_emerg("KTXT [0x%lx--0x%lx]\n",
			(unsigned long)_stext, (unsigned long)_etext);
		pr_emerg("SHA1:\n");
		pr_emerg("%x %x %x %x %x\n",
			*((unsigned int *)g_ktxt_hash_data + 0),
			*((unsigned int *)g_ktxt_hash_data + 1),
			*((unsigned int *)g_ktxt_hash_data + 2),
			*((unsigned int *)g_ktxt_hash_data + 3),
			*((unsigned int *)g_ktxt_hash_data + 4));
	}

	pr_emerg("\n");
	pr_emerg("*****************************************************\n");
	pr_emerg("*                                                   *\n");
	pr_emerg("*  Try to reboot system ...                         *\n");
	pr_emerg("*                                                   *\n");
	pr_emerg("*****************************************************\n");
	pr_emerg("\n");

	flush_cache_all();
	mdelay(1000);

	bust_spinlocks(0);

#ifdef CONFIG_SPRD_DEBUG
	if (reason != NULL && strstr(reason, "Watchdog detected hard LOCKUP"))
		while (1)
			;
#endif

	if (reason != NULL && strstr(reason, "tospanic")) {
		machine_restart("tospanic");
		return;
	}
#ifdef CONFIG_X86_64
	if (!is_x86_mobilevisor())
#endif
	{
		machine_restart("panic");
	}
	return;
}

void sysdump_ipi(struct pt_regs *regs)
{
	int cpu = smp_processor_id();

	if (sprd_sysdump_extra.enter_cpu != -1) {
		memcpy((void *)&(sprd_sysdump_extra.cpu_context[cpu]),
		       (void *)regs, sizeof(struct pt_regs));
		crash_note_save_cpu(regs, cpu);
		sprd_debug_save_context();
	}
	return;
}

static void sysdump_event(struct input_handle *handle,
	unsigned int type, unsigned int code, int value)
{
	if (type == EV_KEY && code != BTN_TOUCH)
		sprd_debug_check_crash_key(code, value);
}

static const struct input_device_id sysdump_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{},
};

static int sysdump_connect(struct input_handler *handler,
			 struct input_dev *dev,
			 const struct input_device_id *id)
{
	struct input_handle *sysdump_handle;
	int error;

	sysdump_handle = (struct input_handle *)kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!sysdump_handle)
		return -ENOMEM;

	sysdump_handle->dev = dev;
	sysdump_handle->handler = handler;
	sysdump_handle->name = "sysdump";

	error = input_register_handle(sysdump_handle);
	if (error) {
		pr_err("Failed to register input sysrq handler, error %d\n",
			error);
		goto err_free;
	}

	error = input_open_device(sysdump_handle);
	if (error) {
		pr_err("Failed to open input device, error %d\n", error);
		goto err_unregister;
	}

	return 0;

 err_unregister:
	input_unregister_handle(sysdump_handle);
 err_free:
	kfree(sysdump_handle);
	return error;
}

static void sysdump_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
}

static int sprd_sysdump_read(struct seq_file *s, void *v)
{
	seq_printf(s, "sysdump_status = %d\n", sysdump_status);
	return 0;
}

static int sprd_sysdump_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_sysdump_read, NULL);
}


static ssize_t sprd_sysdump_write(struct file *file, const char __user *buf,
				size_t count, loff_t *data)
{
	char sysdump_buf[5] = {0};
	int *test = NULL;

	pr_info("%s: start!!!\n", __func__);
	if (count) {
		if (copy_from_user(sysdump_buf, buf, count)) {
			pr_err("%s: copy_from_user failed!!!\n", __func__);
			return -1;
		}
		sysdump_buf[count] = '\0';

		if (!strncmp(sysdump_buf, "on", 2)) {
			pr_info("%s: enable user version sysdump!!!\n",
				__func__);
			set_sysdump_enable(1);
			sysdump_enable_watchdog(0);
		} else if (!strncmp(sysdump_buf, "off", 3)) {
			pr_info("%s: disable user version sysdump!!!\n",
				__func__);
			set_sysdump_enable(0);
			sysdump_enable_watchdog(1);
		} else if (!strncmp(sysdump_buf, "bug", 3)) {
			pr_info("%s  bug-on !!\n", __func__);
			BUG_ON(1);
		} else if (!strncmp(sysdump_buf, "null", 4)) {
			pr_info("%s  null pointer !!\n", __func__);
			count = *test;
		}
	}

	pr_info("%s: End!!!\n", __func__);
	return count;
}


static struct ctl_table sysdump_sysctl_table[] = {
	{
	 .procname = "sysdump_enable",
	 .data = &sysdump_conf.enable,
	 .maxlen = sizeof(int),
	 .mode = 0644,
	 .proc_handler = proc_dointvec,
	 },
	{
	 .procname = "sysdump_crashkey_only",
	 .data = &sysdump_conf.crashkey_only,
	 .maxlen = sizeof(int),
	 .mode = 0644,
	 .proc_handler = proc_dointvec,
	 },
	{
	 .procname = "sysdump_dump_modem",
	 .data = &sysdump_conf.dump_modem,
	 .maxlen = sizeof(int),
	 .mode = 0644,
	 .proc_handler = proc_dointvec,
	 },
	{
	 .procname = "sysdump_reboot",
	 .data = &sysdump_conf.reboot,
	 .maxlen = sizeof(int),
	 .mode = 0644,
	 .proc_handler = proc_dointvec,
	 },
	{
	 .procname = "sysdump_dump_path",
	 .data = sysdump_conf.dump_path,
	 .maxlen = sizeof(sysdump_conf.dump_path),
	 .mode = 0644,
	 .proc_handler = proc_dostring,
	 },
	{}
};

static struct ctl_table sysdump_sysctl_root[] = {
	{
	 .procname = "kernel",
	 .mode = 0555,
	 .child = sysdump_sysctl_table,
	 },
	{}
};

static struct ctl_table_header *sysdump_sysctl_hdr;

static struct input_handler sysdump_handler = {
	.event = sysdump_event,
	.connect	= sysdump_connect,
	.disconnect	= sysdump_disconnect,
	.name = "sysdump_crashkey",
	.id_table	= sysdump_ids,
};

static const struct file_operations sysdump_proc_fops = {
	.owner = THIS_MODULE,
	.open = sprd_sysdump_open,
	.read = seq_read,
	.write = sprd_sysdump_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sprd_sysdump_enable_prepare(void)
{
	struct platform_device *pdev_regmap;
	struct device_node *regmap_np;

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		pr_err("of_find_compatible_node failed!!!\n");
		goto error_pmic_node;
	}

	if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721")) {
		pmic_reg = ANA_RST_STATUS_OFFSET_2721;
		pr_info(" detect pmic is sc2721 ,offset = 0x%x !!!\n",
			 pmic_reg);
	} else {
		pmic_reg = ANA_RST_STATUS_OFFSET_2730;
		pr_info(" detect pmic is sc2730 ,offset = 0x%x !!!\n",
			pmic_reg);
	}

	pdev_regmap = of_find_device_by_node(regmap_np);
	if (!pdev_regmap) {
		pr_err("of_find_device_by_node failed!!!\n");
		goto error_find_device;
	}

	regmap = dev_get_regmap(pdev_regmap->dev.parent, NULL);
	if (!regmap) {
		pr_err("dev_get_regmap failed!!!\n");
		goto error_find_device;
	}

	of_node_put(regmap_np);
	pr_info("%s ok\n", __func__);
	return 0;

error_find_device:
	of_node_put(regmap_np);
error_pmic_node:
	return -ENODEV;
}

static int set_sysdump_enable(int on)
{
	unsigned int val = 0;


	if (!regmap) {
		pr_err("can not %s sysdump because of regmap is NULL\n",
			on ? "enable" : "disable");
		return -1;
	}

	regmap_read(regmap, pmic_reg, &val);
	pr_info("%s: get rst mode  value is = %x\n", __func__, val);

	if (on) {
		pr_info("%s: enable sysdump!\n", __func__);
		val |= HWRST_STATUS_SYSDUMP;
		regmap_write(regmap, pmic_reg, val);
		sysdump_status = 1;
	} else {
		pr_info("%s: disable sysdump!\n", __func__);
		val &= ~(HWRST_STATUS_SYSDUMP);
		regmap_write(regmap, pmic_reg, val);
		sysdump_status = 0;
	}

	return 0;
}

static int sysdump_shash_init(void)
{
	struct crypto_shash *tfm;
	size_t desc_size;
	int ret;

	tfm = crypto_alloc_shash("sha1", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm))
		return (PTR_ERR(tfm) == -ENOENT) ? -ENOPKG : PTR_ERR(tfm);

	desc_size = crypto_shash_descsize(tfm) + sizeof(*desc);

	desc = kzalloc(desc_size, GFP_KERNEL);
	if (!desc)
		goto error_no_desc;

	desc->tfm = tfm;
	desc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	ret = crypto_shash_init(desc);
	if (ret < 0) {
		pr_err("crypto_shash_init fail(%d)!\n", ret);
		return ret;
	}

	return 0;
error_no_desc:
	crypto_free_shash(tfm);
	return -ENOMEM;
}

#ifdef CONFIG_SPRD_MINI_SYSDUMP /*	minidump code start	*/
static int minidump_info_read(struct seq_file *s, void *v)
{
	seq_printf(s,
		    "%s:0x%lx\n"
		    "%s:0x%x\n"
		    , GET_MINIDUMP_INFO_NAME(MINIDUMP_INFO_PADDR), minidump_info_desc_g.paddr
		    , GET_MINIDUMP_INFO_NAME(MINIDUMP_INFO_SIZE), minidump_info_desc_g.size
		   );
	return 0;
}

static int minidump_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, minidump_info_read, NULL);
}
static const struct file_operations minidump_proc_fops = {
		.owner = THIS_MODULE,
		.open = minidump_info_open,
		.read = seq_read,
		.llseek = seq_lseek,
		.release = single_release,
};
void prepare_minidump_reg_memory(struct pt_regs *regs)
{
	int i;
	unsigned long addr;
	mm_segment_t fs;
	if (user_mode(regs))
		for (i = 0; i < minidump_info_g.regs_info.num; i++)
			minidump_info_g.regs_memory_info.reg_paddr[i] = 0;
	fs = get_fs();
	set_fs(KERNEL_DS);
	/*	get all valid paddr every  reg refers to  */
	for (i = 0; i < minidump_info_g.regs_info.num; i++) {
#ifdef CONFIG_ARM
		addr = regs->uregs[i] - minidump_info_g.regs_memory_info.per_reg_memory_size / 2;
		pr_emerg("R%d: %08lx\n", i, regs->uregs[i]);
		pr_emerg("addr: %08lx\n", addr);
		if (addr < PAGE_OFFSET || addr > -256UL) {
#endif
#ifdef CONFIG_ARM64
		if (REG_SP_INDEX == i) {
			addr = regs->sp - minidump_info_g.regs_memory_info.per_reg_memory_size / 2;
			pr_emerg("sp: %llx\n", regs->sp);
			pr_emerg("addr: %lx\n", addr);

		} else if (REG_PC_INDEX == i) {
			addr = regs->pc - minidump_info_g.regs_memory_info.per_reg_memory_size / 2;
			pr_emerg("pc: %llx\n", regs->pc);
			pr_emerg("addr: %lx\n", addr);

		} else {
			addr = regs->regs[i] - minidump_info_g.regs_memory_info.per_reg_memory_size / 2;
			pr_emerg("R%d: %llx\n", i, regs->regs[i]);
			pr_emerg("addr: %lx\n", addr);
		}
		if (addr < KIMAGE_VADDR || addr > -256UL) {

#endif
			minidump_info_g.regs_memory_info.reg_paddr[i] = 0;
			pr_emerg("reg value invalid !!!\n");
		} else {
			minidump_info_g.regs_memory_info.reg_paddr[i] = __pa(addr);
			minidump_info_g.regs_memory_info.valid_reg_num++;
		}
		pr_emerg("reg[%d] paddr: %lx\n",
			 i, minidump_info_g.regs_memory_info.reg_paddr[i]);
	}
	minidump_info_g.regs_memory_info.size = minidump_info_g.regs_memory_info.valid_reg_num * minidump_info_g.regs_memory_info.per_reg_memory_size;
	pr_emerg("size : %d\n", minidump_info_g.regs_memory_info.size);
	set_fs(fs);
	return;
}
void show_minidump_info(struct minidump_info *minidump_infop)
{
	int i;

	pr_emerg("kernel_magic: %s\n ", minidump_infop->kernel_magic);
	pr_emerg("---     regs_info       ---\n ");
	pr_emerg("arch:              %d\n ", minidump_infop->regs_info.arch);
	pr_emerg("num:               %d\n ", minidump_infop->regs_info.num);
	pr_emerg("paddr:         %lx\n ", minidump_infop->regs_info.paddr);
	pr_emerg("size:          %d\n ", minidump_infop->regs_info.size);
	pr_emerg("---     regs_memory_info        ---\n ");
	for (i = 0; i < minidump_infop->regs_info.num; i++) {
		pr_emerg("reg[%d] paddr:          %lx\n ",
			i, minidump_infop->regs_memory_info.reg_paddr[i]);
	}
	pr_emerg("per_reg_memory_size:    %d\n ",
		minidump_infop->regs_memory_info.per_reg_memory_size);
	pr_emerg("valid_reg_num:          %d\n ",
		minidump_infop->regs_memory_info.valid_reg_num);
	pr_emerg("reg_memory_all_size:    %d\n ",
		minidump_infop->regs_memory_info.size);
	pr_emerg("---     section_info_total        ---\n ");
	pr_emerg("Here are %d sections, Total size : %d\n",
		minidump_infop->section_info_total.total_num,
		minidump_infop->section_info_total.total_size);
	pr_emerg("total_num:        %x\n ",
		minidump_infop->section_info_total.total_num);
	pr_emerg("total_size        %x\n ",
		minidump_infop->section_info_total.total_size);
	for (i = 0; i < minidump_infop->section_info_total.total_num; i++) {
		pr_emerg("section_name:           %s\n ",
		minidump_infop->section_info_total.section_info[i].section_name);
		pr_emerg("section_start_vaddr:    %lx\n ",
		minidump_infop->section_info_total.section_info[i].section_start_vaddr);
		pr_emerg("section_end_vaddr:      %lx\n ",
		minidump_infop->section_info_total.section_info[i].section_end_vaddr);
		pr_emerg("section_start_paddr:    %lx\n ",
		minidump_infop->section_info_total.section_info[i].section_start_paddr);
		pr_emerg("section_end_paddr:      %lx\n ",
		minidump_infop->section_info_total.section_info[i].section_end_paddr);
		pr_emerg("section_size:           %x n ",
		minidump_infop->section_info_total.section_info[i].section_size);
	}
	pr_emerg("minidump_data_size:     %x\n ",
		minidump_infop->minidump_data_size);
	return;
}

/*	Here we prepare minidump all info
	| minidump_info | struct pt_regs | memory amount regs | sections | others(just like kernel logbuf ) |
*/
static int prepare_minidump_info(struct pt_regs *regs)
{

	if (regs != NULL) {
		/*	struct pt_regs part: save minidump_regs_g contents */
		memcpy(&minidump_regs_g, regs, sizeof(struct pt_regs));
		/*      memory amount regs part: save minidump_regs_g contents */
		prepare_minidump_reg_memory(regs);

	} else {
		pr_err("%s regs NULL .\n", __func__);
	}

	minidump_info_g.minidump_data_size =  minidump_info_g.regs_info.size + minidump_info_g.regs_memory_info.size + minidump_info_g.section_info_total.total_size;

	/*	sections part: we have got all info when init, here do nothing */
	show_minidump_info(&minidump_info_g);
	return 0;
}

static int dump_die_cb(struct notifier_block *nb, unsigned long reason, void *arg)
{
	struct die_args *die_args = arg;
	if (reason == DIE_OOPS) {
		memcpy(&pregs_die_g, die_args->regs, sizeof(pregs_die_g));
		die_notify_flag = 1;
		pr_emerg("%s save pregs_die_g ok .\n", __func__);
	}
	return NOTIFY_DONE;
}

static struct notifier_block dump_die_notifier = {
	.notifier_call = dump_die_cb
};

void section_info_log_buf(int section_index)
{
	int i = section_index;
	unsigned long vaddr = (unsigned long)(log_buf_addr_get());
	int len = log_buf_len_get();

	pr_info("%s in. vaddr : 0x%lx  len :0x%x  section_index: %d\n",
		__func__, vaddr, len, i);
	minidump_info_g.section_info_total.section_info[i].section_start_vaddr = vaddr;
	minidump_info_g.section_info_total.section_info[i].section_end_vaddr = vaddr + len;
	minidump_info_g.section_info_total.section_info[i].section_start_paddr = __pa(minidump_info_g.section_info_total.section_info[i].section_start_vaddr);
	minidump_info_g.section_info_total.section_info[i].section_end_paddr = __pa(minidump_info_g.section_info_total.section_info[i].section_end_vaddr);
	minidump_info_g.section_info_total.section_info[i].section_size = len;
}

void section_info_ylog_buf(int section_index)
{
	int i = section_index;
	long vaddr = (long)(ylog_buffer);;

	pr_info("%s in. vaddr : 0x%lx  len :0x%x  section_index: %d\n",
		 __func__, vaddr, YLOG_BUF_SIZE, i);
	minidump_info_g.section_info_total.section_info[i].section_start_vaddr = vaddr;
	minidump_info_g.section_info_total.section_info[i].section_end_vaddr = vaddr + YLOG_BUF_SIZE;
	minidump_info_g.section_info_total.section_info[i].section_start_paddr = __pa(minidump_info_g.section_info_total.section_info[i].section_start_vaddr);
	minidump_info_g.section_info_total.section_info[i].section_end_paddr = __pa(minidump_info_g.section_info_total.section_info[i].section_end_vaddr);
	minidump_info_g.section_info_total.section_info[i].section_size = YLOG_BUF_SIZE;
}

void section_info_pt(int section_index)
{
	int i = section_index;
	unsigned long vaddr;
	int len;
	vaddr  = (unsigned long)swapper_pg_dir;

#ifdef CONFIG_ARM
		len = (unsigned long)stext - vaddr;
#endif
#ifdef CONFIG_ARM64
		len = SWAPPER_DIR_SIZE;
#endif
	minidump_info_g.section_info_total.section_info[i].section_start_vaddr = vaddr;
	minidump_info_g.section_info_total.section_info[i].section_end_vaddr = vaddr + len;
	minidump_info_g.section_info_total.section_info[i].section_start_paddr = __pa(vaddr);
	minidump_info_g.section_info_total.section_info[i].section_end_paddr = __pa(vaddr + len);
	minidump_info_g.section_info_total.section_info[i].section_size = len;
#ifdef CONFIG_ARM
	pr_info("pgd vaddr start: 0x%lx  paddr start: 0x%x "
		" len :0x%x  section_index: %d\n",
		vaddr, __pa(vaddr), len, i);
#endif
#ifdef CONFIG_ARM64
	pr_info("pgd vaddr start: 0x%lx  paddr start: 0x%llx "
		" len :0x%x  section_index: %d\n",
		vaddr, __pa(vaddr), len, i);
#endif
	return;
}
int add_extend_section(const char *name, unsigned long paddr_start, unsigned long paddr_end, int index)
{
	struct section_info *extend_section = &minidump_info_g.section_info_total.section_info[index];

	if (SECTION_NUM_MAX <= index) {
		pr_err("No space for new section \n");
		return -1;
	}
	sprintf(extend_section->section_name, "%s", name);
	extend_section->section_start_paddr = paddr_start;
	extend_section->section_end_paddr = paddr_end;
	extend_section->section_size = extend_section->section_end_paddr - extend_section->section_start_paddr;

	minidump_info_g.section_info_total.total_size += extend_section->section_size;
	minidump_info_g.section_info_total.total_num++;
	minidump_info_g.minidump_data_size += extend_section->section_size  ;
	return 0;
}

int extend_section_cm4dump(int index)
{
#define CM4_DUMP_IRAM "scproc"
	struct device_node *node;
	unsigned long cm4_dump_start, cm4_dump_end;
	struct resource res;
	int ret;

	node = of_find_node_by_name(NULL, CM4_DUMP_IRAM);
	if (!node) {
		pr_err("Not find %s from dts \n", CM4_DUMP_IRAM);
		return -1;
	} else {
		ret = of_address_to_resource(node, 0, &res);
		if (!ret) {
			cm4_dump_start = res.start;
			cm4_dump_end = res.end;
			pr_err("cm4_dump_start : 0x%lx cm4_dump_end :0x%lx , size : 0x%lx \n", cm4_dump_start, cm4_dump_end, cm4_dump_end - cm4_dump_start + 1);
		} else {
			pr_err("Not find cm4_reg property from %s node\n", CM4_DUMP_IRAM);
			return -1;
		}
	}
	add_extend_section(CM4_DUMP_IRAM, cm4_dump_start, cm4_dump_end, index);
	return 0;
}
void section_info_per_cpu(int section_index)
{
	int i = section_index;
	long vaddr = (long)(__per_cpu_start)+(long)(__per_cpu_offset[0]);
	int len = (__per_cpu_offset[1] - __per_cpu_offset[0])*CONFIG_NR_CPUS;
	minidump_info_g.section_info_total.section_info[i].section_start_vaddr = vaddr;
	minidump_info_g.section_info_total.section_info[i].section_end_vaddr = vaddr + len;
	minidump_info_g.section_info_total.section_info[i].section_start_paddr = __pa(minidump_info_g.section_info_total.section_info[i].section_start_vaddr);
	minidump_info_g.section_info_total.section_info[i].section_end_paddr = __pa(minidump_info_g.section_info_total.section_info[i].section_end_vaddr);
	minidump_info_g.section_info_total.section_info[i].section_size = len;
	return;
}
/*	init section_info_all_item : name,paddr,size 	return: total_size */
void minidump_info_init(void)
{
	int i;

	minidump_info_g.regs_info.paddr = __pa(&minidump_regs_g);
	/*	regs_memory_info init*/
	minidump_info_g.regs_memory_info.size = REGS_NUM_MAX * minidump_info_g.regs_memory_info.per_reg_memory_size;

	/*	section info init*/
	for (i = 0; i < SECTION_NUM_MAX; i++) {
		/*	when section name is null, break*/
		if (!strlen(minidump_info_g.section_info_total.section_info[i].section_name))
			break;
		/*      when section name is log_buf */
		if (!memcmp(minidump_info_g.section_info_total.section_info[i].section_name, "log_buf", strlen("log_buf"))) {
			section_info_log_buf(i);
		} else if (!memcmp(minidump_info_g.section_info_total.section_info[i].section_name, "ylog_buf", strlen("ylog_buf"))) {
			section_info_ylog_buf(i);
		} else if (!memcmp(minidump_info_g.section_info_total.section_info[i].section_name, "kernel_pt", strlen("kernel_pt"))) {
			section_info_pt(i);
		} else if (!memcmp(minidump_info_g.section_info_total.section_info[i].section_name, "per_cpu", strlen("per_cpu"))) {
			section_info_per_cpu(i);
		} else {
			minidump_info_g.section_info_total.section_info[i].section_start_paddr = __pa(minidump_info_g.section_info_total.section_info[i].section_start_vaddr);
			minidump_info_g.section_info_total.section_info[i].section_end_paddr = __pa(minidump_info_g.section_info_total.section_info[i].section_end_vaddr);
			minidump_info_g.section_info_total.section_info[i].section_size = minidump_info_g.section_info_total.section_info[i].section_end_paddr - minidump_info_g.section_info_total.section_info[i].section_start_paddr;
		}
		minidump_info_g.section_info_total.total_size += minidump_info_g.section_info_total.section_info[i].section_size;
	}
	minidump_info_g.section_info_total.total_num = i;


	minidump_info_g.minidump_data_size =  minidump_info_g.regs_info.size + minidump_info_g.regs_memory_info.size + minidump_info_g.section_info_total.total_size;

	extend_section_cm4dump(minidump_info_g.section_info_total.total_num);

	return;
}
static int ylog_buffer_open(struct inode *inode, struct file *file)
{
	pr_info("open ylog_buffer ok !\n");
	return 0;
}

static int ylog_buffer_map(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long ylog_buffer_paddr;

	if (vma->vm_end - vma->vm_start > YLOG_BUF_SIZE)
		return -EINVAL;

	ylog_buffer_paddr = virt_to_phys(ylog_buffer);
	if (remap_pfn_range(vma,
			vma->vm_start,
			ylog_buffer_paddr >> PAGE_SHIFT, /*	get pfn */
			vma->vm_end - vma->vm_start,
			vma->vm_page_prot))
		return -1;

	pr_info("mmap ylog_buffer ok !\n");
	return 0;
}

static const struct file_operations ylog_buffer_fops = {
	.owner = THIS_MODULE,
	.open = ylog_buffer_open,
	.mmap = ylog_buffer_map,
};

static struct miscdevice misc_dev_ylog = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME_YLOG,
	.fops = &ylog_buffer_fops,
};
static int ylog_buffer_init(void)
{
	int ret;

	ylog_buffer = kzalloc(YLOG_BUF_SIZE, GFP_KERNEL);
	if (ylog_buffer == NULL) {
		return -1;
	}
	pr_info("%s: ylog_buffer vaddr is %p\n", __func__, ylog_buffer);
	sprintf(ylog_buffer, "%s", "This is ylog buffer. Now , it is nothing . ");
	/*here, we can add something to head to check if data is ok */
	SetPageReserved(virt_to_page(ylog_buffer));
	ret = misc_register(&misc_dev_ylog);
	return ret;
}
static void ylog_buffer_exit(void)
{
	misc_deregister(&misc_dev_ylog);
	ClearPageReserved(virt_to_page(ylog_buffer));
	kfree(ylog_buffer);
}
int minidump_init(void)
{
	struct proc_dir_entry *minidump_info_dir;
	struct proc_dir_entry *minidump_info;

	minidump_info_dir = proc_mkdir(MINIDUMP_INFO_DIR, NULL);
	if (!minidump_info_dir)
		return -ENOMEM;
	minidump_info = proc_create(MINIDUMP_INFO_PROC, S_IRUGO | S_IWUGO, minidump_info_dir, &minidump_proc_fops);
	if (!minidump_info)
		return -ENOMEM;
	ylog_buffer_init();
	/*	dump_die_notifier for get infomation when die */
	if (register_die_notifier(&dump_die_notifier) != 0) {
		pr_err("register dump_die_notifyier failed.\n");
		return -1;
	}
	minidump_info_desc_g.paddr = __pa(&minidump_info_g);
	minidump_info_desc_g.size = sizeof(minidump_info_g);
	minidump_info_init();
	pr_info("%s out.\n", __func__);
	return 0;
}
void show_exception_info(void)
{
	pr_emerg("kernel_magic:             %s\n ",
		minidump_info_g.exception_info.kernel_magic);
	pr_emerg("exception_serialno:  %s\n ",
		minidump_info_g.exception_info.exception_serialno);
	pr_emerg("exception_kernel_version: %s\n ",
		minidump_info_g.exception_info.exception_kernel_version);
	pr_emerg("exception_reboot_reason:  %s\n ",
		minidump_info_g.exception_info.exception_reboot_reason);
	pr_emerg("exception_panic_reason:   %s\n ",
		minidump_info_g.exception_info.exception_panic_reason);
	pr_emerg("exception_time:           %s\n ",
		minidump_info_g.exception_info.exception_time);
	pr_emerg("exception_file_info:      %s\n ",
		minidump_info_g.exception_info.exception_file_info);
	pr_emerg("exception_task_id:        %d\n ",
		minidump_info_g.exception_info.exception_task_id);
	pr_emerg("exception_task_family:      %s\n ",
		minidump_info_g.exception_info.exception_task_family);
	pr_emerg("exception_pc_symbol:      %s\n ",
		minidump_info_g.exception_info.exception_pc_symbol);
	pr_emerg("exception_stack_info:     %s\n ",
		minidump_info_g.exception_info.exception_stack_info);
}
void get_file_line_info(struct pt_regs *regs)
{
	struct bug_entry *bug;
	const char *file = NULL;
	unsigned int line = 0;

	if (!regs || !is_valid_bugaddr(regs->reg_pc)) {
		snprintf(minidump_info_g.exception_info.exception_file_info,
			EXCEPTION_INFO_SIZE_SHORT, "not-bugon");
		pr_err("no regs  or not a bugon,do nothing\n");
		return;
	}

	bug = find_bug(regs->reg_pc);
	if (!bug) {
		pr_err("not a bugon, no  bug info ,do nothing\n");
		snprintf(minidump_info_g.exception_info.exception_file_info,
			EXCEPTION_INFO_SIZE_SHORT, "not-bugon");
		return;
	}
#ifdef CONFIG_DEBUG_BUGVERBOSE
#ifndef CONFIG_GENERIC_BUG_RELATIVE_POINTERS
	file = bug->file;
#else
	 file = (const char *)bug + bug->file_disp;
#endif
	line = bug->line;
#endif
	if (file) {
		snprintf(minidump_info_g.exception_info.exception_file_info,
			EXCEPTION_INFO_SIZE_SHORT, "[%s:%d]", file, line);
	} else {
		snprintf(minidump_info_g.exception_info.exception_file_info,
			EXCEPTION_INFO_SIZE_SHORT, "not-bugon");
		pr_err("no file info ,do nothing\n");
	}
}
void get_exception_stack_info(struct pt_regs *regs)
{
	unsigned long stack_entries[MAX_STACK_TRACE_DEPTH];
	char symbol[96];
	int sz;
	int off, plen;
	struct stack_trace trace;
	int i;
	struct task_struct *tsk, *cur;

	cur = current;
	tsk = cur;
	if (!virt_addr_valid(tsk))
		return;

	/* Current panic user tasks */
	sz = 0;
	do {
		if (!tsk) {
			pr_err("No tsk info\n");
			break;
		}
		sz += snprintf(
			minidump_info_g.exception_info.exception_task_family + sz,
			EXCEPTION_INFO_SIZE_SHORT - sz,
			"[%s, %d]", tsk->comm, tsk->pid);
		tsk = tsk->real_parent;
	} while (tsk && (tsk->pid != 0) && (tsk->pid != 1));

	/* Grab kernel task stack trace */
	trace.nr_entries = 0;
	trace.max_entries = MAX_STACK_TRACE_DEPTH;
	trace.entries = stack_entries;
	trace.skip = 0;
	save_stack_trace_tsk(cur, &trace);
	for (i = 0; i < trace.nr_entries; i++) {
		off = strlen(minidump_info_g.exception_info.exception_stack_info);
		plen = EXCEPTION_INFO_SIZE_LONG - ALIGN(off, 8);
		if (plen > 16) {
			sz = snprintf(symbol, 96, "[<%p>] %pS\n",
				      (void *)stack_entries[i],
				      (void *)stack_entries[i]);
			if (ALIGN(sz, 8) - sz) {
				memset_io(symbol + sz - 1, ' ', ALIGN(sz, 8) - sz);
				memset_io(symbol + ALIGN(sz, 8) - 1, '\n', 1);
			}
			if (ALIGN(sz, 8) <= plen)
				memcpy(
				minidump_info_g.exception_info.exception_stack_info + ALIGN(off, 8),
				symbol, ALIGN(sz, 8));
		}
	}
	if (regs) {
		snprintf(minidump_info_g.exception_info.exception_pc_symbol,
			EXCEPTION_INFO_SIZE_SHORT, "[<%p>] %pS",
			(void *)(unsigned long)regs->reg_pc,
			(void *)(unsigned long)regs->reg_pc);
	} else {
		snprintf(minidump_info_g.exception_info.exception_pc_symbol,
			EXCEPTION_INFO_SIZE_SHORT, "[<%p>] %pS",
			(void *)(unsigned long)stack_entries[0],
			(void *)(unsigned long)stack_entries[0]);
	}
}
static int prepare_exception_info(struct pt_regs *regs,
				struct task_struct *tsk, const char *reason)
{

	struct timex txc;
	struct rtc_time tm;

	if (!tsk)
		tsk = current;
	memset(&minidump_info_g.exception_info, 0,
		sizeof(minidump_info_g.exception_info));
	memcpy(minidump_info_g.exception_info.kernel_magic, KERNEL_MAGIC, 4);

	/*	exception_kernel_version	*/
	memcpy(minidump_info_g.exception_info.exception_kernel_version,
		linux_banner,
		strlen(linux_banner));

	if (reason != NULL)
		memcpy(minidump_info_g.exception_info.exception_panic_reason,
			reason,
			strlen(reason));
	/*	exception_reboot_reason	 update in uboot */

	/*	exception_time		*/
	do_gettimeofday(&(txc.time));
	txc.time.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time_to_tm(txc.time.tv_sec, &tm);
	snprintf(minidump_info_g.exception_info.exception_time,
		EXCEPTION_INFO_SIZE_SHORT,
		"%04d-%02d-%02d:%02d:%02d:%02d",
		tm.tm_year + 1900,
		tm.tm_mon + 1,
		tm.tm_mday,
		tm.tm_hour,
		tm.tm_min,
		tm.tm_sec);

	/*	exception_file & exception_line		*/
	get_file_line_info(regs);
	/*	exception_task_id	*/
	minidump_info_g.exception_info.exception_task_id = tsk->pid;
	/*	exception_stack		*/
	get_exception_stack_info(regs);
	show_exception_info();
	return 0;
}
#endif  /*	minidump code end	*/
int sysdump_sysctl_init(void)
{
	/*get_sprd_sysdump_info_paddr(); */
	unsigned long sprd_sysdump_info_paddr;
	struct proc_dir_entry *sysdump_proc;

	sprd_sysdump_info_paddr = get_sprd_sysdump_info_paddr();
	if (!sprd_sysdump_info_paddr)
		pr_err("get sprd_sysdump_info_paddr failed.\n");
	sprd_sysdump_info = (struct sysdump_info *)
	    phys_to_virt(sprd_sysdump_info_paddr);

	sysdump_sysctl_hdr =
	    register_sysctl_table((struct ctl_table *)sysdump_sysctl_root);
	if (!sysdump_sysctl_hdr)
		return -ENOMEM;

	crash_notes = &crash_notes_temp;

	if (input_register_handler(&sysdump_handler))
		pr_err("regist sysdump_handler failed.\n");

	sysdump_proc = proc_create("sprd_sysdump", S_IWUSR | S_IRUSR, NULL, &sysdump_proc_fops);
	if (!sysdump_proc)
		return -ENOMEM;

	memset(g_ktxt_hash_data, 0x55, SHA1_DIGEST_SIZE);
	if (sysdump_shash_init())
		return -ENOMEM;

	sprd_sysdump_init = 1;

	sprd_sysdump_enable_prepare();
#if defined(CONFIG_SPRD_DEBUG)
	pr_info("userdebug enable sysdump in default !!!\n");
	set_sysdump_enable(1);
#endif
#ifdef CONFIG_SPRD_MINI_SYSDUMP
	minidump_init();
#endif
	return 0;
}

void sysdump_sysctl_exit(void)
{
	if (sysdump_sysctl_hdr)
		unregister_sysctl_table(sysdump_sysctl_hdr);
	input_unregister_handler(&sysdump_handler);
	remove_proc_entry("sprd_sysdump", NULL);
	if (desc) {
		if (desc->tfm)
			crypto_free_shash(desc->tfm);
		kfree(desc);
	}
#ifdef CONFIG_SPRD_MINI_SYSDUMP
	ylog_buffer_exit();
#endif
}

late_initcall_sync(sysdump_sysctl_init);
module_exit(sysdump_sysctl_exit);

MODULE_AUTHOR("Jianjun.He <jianjun.he@spreadtrum.com>");
MODULE_DESCRIPTION("kernel core dump for Spreadtrum");
MODULE_LICENSE("GPL");
