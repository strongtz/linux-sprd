/*
 * Copyright (C) 2016 Spreadtrum Communications Inc.
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
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/shm.h>
#include <linux/vmalloc.h>
#include <linux/capability.h>
#include <linux/highuid.h>
#include <linux/rcupdate.h>
#include <linux/audit.h>
#include <linux/memory.h>
#include <asm/unistd.h>

#define MAX_SIZE 16
#define MAX_STR_LEN 256

#define RESERVE_SPACE_DIR "reserve_space"
#define BLACK_LIST_PROC_PATH "black_list"
#define WHITE_LIST_PROC_PATH "white_list"
#define BLACK_LIST_COMM_PROC_PATH "black_list_comm"
#define WHITE_LIST_COMM_PROC_PATH "white_list_comm"
#define APP_GUID_PROC_PATH "app_guid"

static char black_list_str[MAX_STR_LEN] = {0};
static char white_list_str[MAX_STR_LEN] = {0};
static char black_list_comm_str[MAX_STR_LEN] = {0};
static char white_list_comm_str[MAX_STR_LEN] = {0};
static char app_guid_str[MAX_SIZE] = {0};

static int atoi(const char *str)
{
	int value = 0;

	while (*str >= '0' && *str <= '9') {
		value *= 10;
		value += *str - '0';
		str++;
	}
	return value;
}

/* parse a string containing a comma-separated */
int analyse_separate(char *str, char str_array[][MAX_SIZE], char *needle)
{
	int i = 0;
	char *buf = NULL;
	char tmp_str[MAX_STR_LEN];
	char *pchar = tmp_str;

	memset(tmp_str, 0, MAX_STR_LEN);

	if (NULL == str || NULL == str_array || NULL == needle)
		return -1;

	strncpy(pchar, str, MAX_STR_LEN - 1);
	buf = strstr(pchar, needle);

	while (NULL != buf) {
		buf[0] = '\0';
		strcpy(str_array[i], pchar);
		i++;
		pchar = buf + strlen(needle);
		/* Get next token: */
		buf = strstr(pchar, needle);
	}

	return 0;
}

/* black_list */
static int black_list[MAX_SIZE] = {0};

static int black_list_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", black_list_str);
	return 0;
}

static ssize_t black_list_proc_write(struct file *file,
const char *buffer, size_t len, loff_t *off)
{
	int i = 0;
	char str[MAX_SIZE][MAX_SIZE];

	if (MAX_STR_LEN <= len)
		return -EFAULT;

	memset(str, 0, MAX_SIZE*MAX_SIZE);
	memset(black_list_str, 0, len + 1);

	if (copy_from_user(black_list_str, buffer, len)) {
		pr_err("black_list_proc_write error\n");
		return -EFAULT;
	}

	if (-1 == analyse_separate(black_list_str, str, ","))
		return -EFAULT;

	for (i = 0; i < MAX_SIZE; i++) {
		if ('\0' == str[i][0])
			break;

		black_list[i] = atoi(str[i]);
		pr_info("%d,", black_list[i]);
	}

	return len;
}

static int black_list_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, black_list_proc_show, NULL);
}

static const struct file_operations black_list_proc_fops = {
	.owner = THIS_MODULE,
	.open = black_list_proc_open,
	.read = seq_read,
	.write = black_list_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};


/* white list */
static int white_list[MAX_SIZE] = {0};

static int white_list_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", white_list_str);
	return 0;
}

static ssize_t white_list_proc_write(struct file *file,
	const char *buffer, size_t len, loff_t *off)
{
	int i = 0;
	char str[MAX_SIZE][MAX_SIZE];

	if (MAX_STR_LEN <= len)
		return -EFAULT;

	memset(str, 0, MAX_SIZE*MAX_SIZE);
	memset(white_list_str, 0, len + 1);

	if (copy_from_user(white_list_str, buffer, len)) {
		pr_err("white_list_proc_write error\n");
		return -EFAULT;
	}

	if (-1 == analyse_separate(white_list_str, str, ","))
		return -EFAULT;

	for (i = 0; i < MAX_SIZE; i++) {
		if ('\0' == str[i][0])
			break;

		white_list[i] = atoi(str[i]);
		pr_info("%d,", white_list[i]);
	}

	return len;
}

static int white_list_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, white_list_proc_show, NULL);
}

static const struct file_operations white_list_proc_fops = {
	.owner = THIS_MODULE,
	.open = white_list_proc_open,
	.read = seq_read,
	.write = white_list_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* black list comm */
static char black_list_comm[MAX_SIZE][MAX_SIZE] = {{0, 0} };

static int black_list_comm_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", black_list_comm_str);
	return 0;
}

static ssize_t black_list_comm_proc_write(struct file *file,
const char __user *buffer, size_t len, loff_t *off)
{
	int i = 0;

	if (MAX_STR_LEN <= len)
		return -EFAULT;

	memset(black_list_comm, 0, MAX_SIZE*MAX_SIZE);
	memset(black_list_comm_str, 0, len + 1);

	if (copy_from_user(black_list_comm_str, buffer, len)) {
		pr_err("black_list_comm_proc_write error\n");
		return -EFAULT;
	}

	if (-1 == analyse_separate(black_list_comm_str, black_list_comm, ","))
		return -EFAULT;

	for (i = 0; i < MAX_SIZE; i++) {
		if ('\0' == black_list_comm[i][0])
			break;
		pr_info("%s,", black_list_comm[i]);
	}

	return len;
}

static int black_list_comm_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, black_list_comm_proc_show, NULL);
}

static const struct file_operations black_list_comm_proc_fops = {
	.owner = THIS_MODULE,
	.open = black_list_comm_proc_open,
	.read = seq_read,
	.write = black_list_comm_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* white list comm */
static char white_list_comm[MAX_SIZE][MAX_SIZE] = {{0, 0} };
static int white_list_comm_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", white_list_comm_str);
	return 0;
}

static ssize_t white_list_comm_proc_write(struct file *file,
const char *buffer, size_t len, loff_t *off)
{
	int i = 0;

	if (len >= MAX_STR_LEN)
		return -EFAULT;

	memset(white_list_comm, 0, MAX_SIZE*MAX_SIZE);
	memset(white_list_comm_str, 0, len + 1);

	if (copy_from_user(white_list_comm_str, buffer, len)) {
		pr_err("white_list_comm_proc_write error\n");
		return -EFAULT;
	}

	if (-1 == analyse_separate(white_list_comm_str, white_list_comm, ","))
		return -EFAULT;

	for (i = 0; i < MAX_SIZE; i++) {
		if ('\0' == white_list_comm[i][0])
			break;
		pr_info("white_list_comm:%s,", white_list_comm[i]);
	}

	return len;
}

static int white_list_comm_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, white_list_comm_proc_show, NULL);
}

static const struct file_operations white_list_comm_proc_fops = {
	.owner = THIS_MODULE,
	.open = white_list_comm_proc_open,
	.read = seq_read,
	.write = white_list_comm_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};


/* app guid */
static int app_guid;

static int app_guid_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", app_guid_str);
	return 0;
}

static ssize_t app_guid_proc_write(struct file *file,
const char *buffer, size_t len, loff_t *off)
{
	if (MAX_SIZE <= len)
		return -EFAULT;

	memset(app_guid_str, 0, len + 1);

	if (copy_from_user(app_guid_str, buffer, len)) {
		pr_err("app_guid_proc_write error\n");
		return -EFAULT;
	}
	app_guid = atoi(app_guid_str);

	pr_info("%d,\n", app_guid);
	return len;
}

static int app_guid_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, app_guid_proc_show, NULL);
}

static const struct file_operations app_guid_proc_fops = {
	.owner = THIS_MODULE,
	.open = app_guid_proc_open,
	.read = seq_read,
	.write = app_guid_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init reserve_space_proc_init(void)
{
	struct proc_dir_entry *parent_dir = proc_mkdir(RESERVE_SPACE_DIR, NULL);

	proc_create(BLACK_LIST_PROC_PATH, 0666, parent_dir,
		&black_list_proc_fops);
	proc_create(WHITE_LIST_PROC_PATH, 0666, parent_dir,
		&white_list_proc_fops);
	proc_create(BLACK_LIST_COMM_PROC_PATH, 0666, parent_dir,
		&black_list_comm_proc_fops);
	proc_create(WHITE_LIST_COMM_PROC_PATH, 0660, parent_dir,
		&white_list_comm_proc_fops);
	proc_create(APP_GUID_PROC_PATH, 0666, parent_dir,
	&app_guid_proc_fops);

	return 0;
}

static void __exit reserve_space_proc_exit(void)
{
}

bool is_in_black_list(uid_t euid, gid_t egid)
{
	bool ret = false;
	int i = 0, j = 0;
	uid_t id[2] = {euid, egid};

	if ('\0' != black_list_comm[0][0]) {
		for (i = 0; i < MAX_SIZE; i++) {
			if (0 == black_list_comm[i][0])
				break;

		if (!strncmp(current->comm, black_list_comm[i],
			strlen(current->comm)))
				return true;
		}
	}

	if (0 == black_list[0])
		return false;

	for (i = 0; i < sizeof(id)/sizeof(uid_t); i++) {
		for (j = 0; j < sizeof(black_list)/sizeof(int); j++) {
			if (0 == black_list[j])
				break;

			if (id[i] == black_list[j])
				return true;
		}
	}

	return ret;
}

int is_in_white_list(void)
{
	int i = 0;
	kgid_t cur_mgid;
	bool ret = false;

	if (white_list_comm[0][0] != '\0') {
		for (i = 0; i < MAX_SIZE; i++) {
			if (white_list_comm[i][0] == 0)
				break;
			if (strstr(current->comm, white_list_comm[i]))
				return true;
		}
	}

	if (0 == white_list[0])
		return false;

	for (i = 0; i < sizeof(white_list)/sizeof(int); i++) {
		if (0 == white_list[i])
			break;

		cur_mgid.val = white_list[i];
		if (1 == in_egroup_p(cur_mgid)) {
			ret = true;
			break;
		}
	}

	return ret;
}

bool check_have_permission(int log_print)
{
	bool ret = false;
	kuid_t cur_euid;
	kgid_t cur_egid;

	current_euid_egid(&cur_euid, &cur_egid);

	if (is_in_white_list()) {
		pr_info("whitelist: egid is in egid groups.\n");
		return ret = true;
	}

	if (0 != app_guid) {
		ret = (cur_euid.val < app_guid || cur_egid.val < app_guid) ?
			(!is_in_black_list(cur_euid.val, cur_egid.val)):false;
	} else {
		ret = !is_in_black_list(cur_euid.val, cur_egid.val);
	}

	if (!ret) {
		if (log_print)
			pr_err("[check_have_permission] euid:%u, egid:%u, pid:%u, comm:%s.\n",
				cur_euid.val, cur_egid.val,
				current->pid, current->comm);
	}

	return ret;
}
EXPORT_SYMBOL(check_have_permission);

MODULE_LICENSE("GPL");
module_init(reserve_space_proc_init);
module_exit(reserve_space_proc_exit);
