#ifndef __SWCN_BLK_H__
#define __SWCN_BLK_H__

#define SPRD_SWCN_MEM_CACHE_EN 1
#ifdef SPRD_SWCN_MEM_CACHE_EN
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>

void __inval_cache_range(const void *, const void *);
void __dma_flush_range(const void *, const void *);

void v7_dma_unmap_area(const void *, size_t, int);
void v7_dma_flush_range(const void *, const void *);

/* Temporary workaround  */
#ifdef ioremap_cached
#define ioremap_cache	ioremap_cached
#endif

#ifdef CONFIG_64BIT
#ifdef CONFIG_ARM64
#define SWCN_DATA_TO_SKB_CACHE_INV(start, end) \
	__inval_dcache_area(start,  (end - start))
#define SKB_DATA_TO_SWCN_CACHE_FLUSH(start, end) \
	__dma_flush_area(start, (end - start))
#else /* x86 */
#define SWCN_DATA_TO_SKB_CACHE_INV(start, end) \
	clflush_cache_range(start, (end - start))
#define SKB_DATA_TO_SWCN_CACHE_FLUSH(start, end) \
	clflush_cache_range(start, (end - start))
#endif /* CONFIG_ARM64 */
#else /* CONFIG_64BIT */
#define SWCN_DATA_TO_SKB_CACHE_INV(start, end) \
	v7_dma_unmap_area(start, (end - start), DMA_FROM_DEVICE)
#define SKB_DATA_TO_SWCN_CACHE_FLUSH(start, end) \
	v7_dma_flush_range(start, end)
#endif /* CONFIG_64BIT */
#endif /* SPRD_SWCN_MEM_CACHE_EN */

/* flag for CMD/DONE msg type */
#define SMSG_CMD_SWCNBLK_INIT		0x0001
#define SMSG_DONE_SWCNBLK_INIT		0x0002

/* flag for EVENT msg type */
#define SMSG_EVENT_SWCNBLK_SEND		0x0001
#define SMSG_EVENT_SWCNBLK_RELEASE	0x0002

#define SWCNBLK_STATE_IDLE		0
#define SWCNBLK_STATE_READY		1

#define SWCNBLK_BLK_STATE_DONE		0
#define SWCNBLK_BLK_STATE_PENDING	1
struct swcnblk_blks {
	u32		addr;
	u32		length;
};

/* ring block header */
struct swcnblk_ring_header {
	/* get|send-block info */
	u32 txblk_addr;
	u32 txblk_count;
	u32 txblk_size;
	u32 txblk_blks;
	u32 txblk_rdptr;
	u32 txblk_wrptr;

	/* release|recv-block info */
	u32 rxblk_addr;
	u32 rxblk_count;
	u32 rxblk_size;
	u32 rxblk_blks;
	u32 rxblk_rdptr;
	u32 rxblk_wrptr;
};

struct swcnblk_header {
	struct swcnblk_ring_header ring;
	struct swcnblk_ring_header pool;
};

struct swcnblk_ring {
	struct swcnblk_header	*header;
	/* virt of header->txblk_addr */
	void			*txblk_virt;
	/* virt of header->rxblk_addr */
	void			*rxblk_virt;

	/* virt of header->ring->txblk_blks */
	struct swcnblk_blks	*r_txblks;
	/* virt of header->ring->rxblk_blks */
	struct swcnblk_blks	*r_rxblks;
	/* virt of header->pool->txblk_blks */
	struct swcnblk_blks	*p_txblks;
	/* virt of header->pool->rxblk_blks */
	struct swcnblk_blks	*p_rxblks;

	/* record the state of every txblk */
	int			*txrecord;
	/* record the state of every rxblk */
	int			*rxrecord;
	/* send */
	spinlock_t		r_txlock;
	/* recv */
	spinlock_t		r_rxlock;
	/* get */
	spinlock_t		p_txlock;
	/* release */
	spinlock_t		p_rxlock;

	wait_queue_head_t	getwait;
	wait_queue_head_t	recvwait;
};

struct swcnblk_mgr {
	u8		dst;
	u8		channel;
	u32		state;

	void		*smem_virt;
	void		*smem_cached_virt;
	void		*smem_blk_virt;
	u32		smem_addr;
	u32		smem_size;
	u32		mapped_smem_addr;
	u32		mapped_cache_addr;
	u32		mapped_cache_size;

	u32		txblksz;
	u32		rxblksz;
	u32		txblknum;
	u32		rxblknum;

	struct swcnblk_ring	*ring;
	struct task_struct	*thread;

	void			(*handler)(int event, void *data);
	void			*data;
};

#define SWCNBLKSZ_ALIGN(blksz, size) \
	(((blksz) + ((size) - 1)) & (~((size) - 1)))

#ifdef CONFIG_64BIT
#define SWCNBLK_ALIGN_BYTES (8)
#else
#define SWCNBLK_ALIGN_BYTES (4)
#endif

static inline u32 swcnblk_get_index(u32 x, u32 y)
{
	return (x / y);
}

static inline u32 swcnblk_get_ringpos(u32 x, u32 y)
{
	return is_power_of_2(y) ? (x & (y - 1)) : (x % y);
}
#endif
