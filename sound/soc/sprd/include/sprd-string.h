#ifndef __SPRD_MEMCPY_OPS_H
#define __SPRD_MEMCPY_OPS_H

#include <linux/uaccess.h>

#ifdef CONFIG_64BIT
static inline unsigned long unalign_copy_to_user(void __user *to,
						 const void *from,
						 unsigned long n)
{
	unsigned long rval = 0;

	if (((unsigned long)to & 7) == ((unsigned long)from & 7)) {
		while (((unsigned long)from & 7) && n) {
			rval |= copy_to_user(to++, from++, 1);
			n--;
		}
		rval = copy_to_user(to, from, n);
	} else {
		while (n) {
			rval |= copy_to_user(to++, from++, 1);
			n--;
		}
	}

	return rval;
}

static inline unsigned long unalign_copy_from_user(void *to,
						   const void __user *from,
						   unsigned long n)
{
	unsigned long rval = 0;

	if (((unsigned long)to & 7) == ((unsigned long)from & 7)) {
		while (((unsigned long)to & 7) && n) {
			rval |= copy_from_user(to++, from++, 1);
			n--;
		}
		rval = copy_from_user(to, from, n);
	} else {
		while (n) {
			rval |= copy_from_user(to++, from++, 1);
			n--;
		}
	}

	return rval;
}

static inline void *unalign_memcpy(void *to, const void *from, size_t n)
{
	char *dst = (char *)to;
	const char *src = (char *)from;

	if (((unsigned long)dst & 7) == ((unsigned long)src & 7)) {
		while (((unsigned long)src & 7) && n--)
			*dst++ = *src++;
		memcpy(dst, src, n);
	} else {
		while (n--)
			*dst++ = *src++;
	}

	return to;
}

static inline void *unalign_memset(void *s, int c, size_t count)
{
	char *xs = s;

	while (count--)
		*xs++ = c;
	return s;
}
#else
static inline unsigned long unalign_copy_to_user(void __user *to,
						 const void *from,
						 unsigned long n)
{
	return copy_to_user(to, from, n);
}

static inline unsigned long unalign_copy_from_user(void *to,
						   const void __user *from,
						   unsigned long n)
{
	return copy_from_user(to, from, n);
}

static inline void *unalign_memcpy(void *to, const void *from, size_t n)
{
	return memcpy(to, from, n);
}

static inline void *unalign_memset(void *s, int c, size_t count)
{
	return memset(s, c, count);
}
#endif

#endif /* __SPRD_MEMCPY_OPS_H */
