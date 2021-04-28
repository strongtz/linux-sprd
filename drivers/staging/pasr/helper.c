/*
 * Copyright (C) ST-Ericsson SA 2011
 * Author: Maxime Coquelin <maxime.coquelin at stericsson.com> for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/pasr.h>

struct pasr_die *pasr_addr2die(struct pasr_map *map, phys_addr_t addr)
{
	unsigned int left, right, mid;

	if (!map)
		return NULL;

	left = 0;
	right = map->nr_dies;

	while (left != right) {
		struct pasr_die *d;

		mid = (left + right) >> 1;
		d = &map->die[mid];

		if (addr >= d->start && addr < d->end)
			return d;

		if (left == mid || right == mid)
			break;

		if (addr >= d->end)
			left = mid;
		else
			right = mid;
	}

	pr_err("%s: No die found for address %#llx"
		, __func__, addr);
	return NULL;
}

struct pasr_section *pasr_addr2section(struct pasr_map *map
				, phys_addr_t addr)
{
	unsigned int left, right, mid;
	struct pasr_die *die;

	/* Find the die the address it is located in */
	die = pasr_addr2die(map, addr);
	if (!die)
		goto err;

	left = 0;
	right = die->nr_sections;

	addr &= ~(section_size - 1);

	while (left != right) {
		struct pasr_section *s;

		mid = (left + right) >> 1;
		s = &die->section[mid];

		if (addr == s->start)
			return s;

		if (left == mid || right == mid)
			break;

		if (addr > s->start)
			left = mid;
		else
			right = mid;
	}

err:
	/* Provided address isn't in any declared section */
	pr_err("%s: No section found for address %#llx"
		, __func__, addr);

	return NULL;
}

phys_addr_t pasr_section2addr(struct pasr_section *s)
{
	return s->start;
}
