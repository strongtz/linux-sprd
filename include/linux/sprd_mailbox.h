/* sprd_mailbox.h */

#ifndef SPRD_MAILBOX_H
#define SPRD_MAILBOX_H

#include <linux/interrupt.h>

typedef irqreturn_t (*MBOX_FUNCALL)(void *ptr, void *private);
#ifdef CONFIG_SPRD_MAILBOX
int mbox_register_irq_handle(u8 target_id,
			     MBOX_FUNCALL irq_handler,
			     void *priv_data);
int mbox_unregister_irq_handle(u8 target_id);
int mbox_raw_sent(u8 target_id, u64 msg);
void mbox_just_sent(u8 core_id, u64 msg);
u32 mbox_core_fifo_full(int core_id);
#else
static inline int mbox_register_irq_handle(u8 target_id,
			     MBOX_FUNCALL irq_handler,
			     void *priv_data) {return 0; }
static inline int mbox_unregister_irq_handle(u8 target_id) {return 0; }
static inline int mbox_raw_sent(u8 target_id, u64 msg) {return 0; }
static inline void mbox_just_sent(u8 core_id, u64 msg) {return; }
static inline u32 mbox_core_fifo_full(int core_id) {return 0; }
#endif

#endif /* SPRD_MAILBOX_H */
