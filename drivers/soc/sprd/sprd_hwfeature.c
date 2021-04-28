// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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

#define pr_fmt(fmt)	"hwfeature: " fmt

#include <linux/hash.h>
#include <linux/libfdt.h>
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/soc/sprd/hwfeature.h>
#include <linux/stat.h>
#include <linux/string.h>

#define HWFEATURE_STR_SIZE_LIMIT_KEY         1024
#define HWFEATURE_HASH_BITS                  4
#define HWFEATURE_TABLE_SIZE                 (1 << HWFEATURE_HASH_BITS)
#define HWFEATURE_TABLE_MASK                 (HWFEATURE_TABLE_SIZE - 1)
#define HWFEATURE_ROOT_NAME                  "/hwfeature"

static struct hlist_head hwf_table[HWFEATURE_TABLE_SIZE];
struct hwf_property {
	struct hlist_node hlist;
	const char *key;
	const char *value;
};
static void __init sprd_kproperty_append(const char *key, const char *value);

static unsigned int hash_str(const char *str)
{
	const unsigned int hash_mult = 2654435387U;
	unsigned int h = 0;

	while (*str)
		h = (h + (unsigned int) *str++) * hash_mult;

	return h & HWFEATURE_TABLE_MASK;
}

static void __init process_hwfeatures_node(struct device_node *np)
{
	char *value, *prop_name;
	struct property *pp;
	const char *full_name;
	char key_buf[HWFEATURE_STR_SIZE_LIMIT_KEY];
	int len = strlen(HWFEATURE_ROOT_NAME) + 1;

	for_each_property_of_node(np, pp) {
		if (!pp->next)
			continue;
		value = pp->value;
		full_name = of_node_full_name(np);
		prop_name = pp->name;
		pr_info("%s/%s=%s\n", full_name, prop_name, value);
		snprintf(key_buf, sizeof(key_buf), "%s/%s", full_name, prop_name);
		sprd_kproperty_append(kstrdup_const(key_buf + len, GFP_KERNEL),
			kstrdup_const(value, GFP_KERNEL));
	}
}

static void __init early_init_fdt_hwfeature(void)
{
	struct device_node *np, *hwf;

	hwf = of_find_node_by_path(HWFEATURE_ROOT_NAME);
	if (!hwf)
		return;

	for_each_of_allnodes_from(hwf, np) {
		if (hwf->sibling == np)
			break;
		process_hwfeatures_node(np);
	}
	process_hwfeatures_node(hwf);
}

static void __init sprd_kproperty_append(const char *key, const char *value)
{
	struct hlist_head *head;
	struct hwf_property *hp;

	if (!key || !value)
		return;

	hp = kzalloc(sizeof(*hp), GFP_ATOMIC);
	if (!hp)
		return;

	INIT_HLIST_NODE(&hp->hlist);
	hp->key = key;
	hp->value = value;

	head = &hwf_table[hash_str(key)];
	hlist_add_head(&hp->hlist, head);
}

static int __init hwfeature_init(void)
{
	int i;

	for (i = 0; i < HWFEATURE_TABLE_SIZE; i++)
		INIT_HLIST_HEAD(&hwf_table[i]);

	early_init_fdt_hwfeature();

	return 0;
}
early_initcall(hwfeature_init);

int sprd_kproperty_eq(const char *key, const char *value)
{
	struct hlist_head *head;
	struct hwf_property *hp;

	head = &hwf_table[hash_str(key)];
	hlist_for_each_entry(hp, head, hlist) {
		if (!strcmp(hp->key, key) && !strcmp(value, hp->value))
			return 1;
	}
	return 0;
}
EXPORT_SYMBOL(sprd_kproperty_eq);

void sprd_kproperty_get(const char *key, char *value, const char *default_value)
{
	struct hlist_head *head;
	struct hwf_property *hp;

	head = &hwf_table[hash_str(key)];
	hlist_for_each_entry(hp, head, hlist) {
		if (!strcmp(hp->key, key)) {
			strlcpy(value, hp->value, HWFEATURE_STR_SIZE_LIMIT);
			return;
		}
	}

	if (default_value == NULL)
		default_value = HWFEATURE_KPROPERTY_DEFAULT_VALUE;
	strlcpy(value, default_value, HWFEATURE_STR_SIZE_LIMIT);
}
EXPORT_SYMBOL(sprd_kproperty_get);
