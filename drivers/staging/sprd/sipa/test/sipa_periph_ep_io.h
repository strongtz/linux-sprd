#include "../sipa_priv.h"

struct sipa_periph_sender {
	struct sipa_context *ctx;
	struct sipa_endpoint *ep;
	enum sipa_xfer_pkt_type type;
	u32 left_cnt;
	spinlock_t lock;
	struct list_head sending_list;
	struct kmem_cache *sending_pair_cache;
};


struct sipa_periph_receiver;

typedef int (*sipa_periph_dispatch_func)(struct sipa_periph_receiver *receiver,
		struct sipa_hal_fifo_item *item,
		struct sk_buff *skb);


struct sipa_periph_receiver {
	struct sipa_context *ctx;
	struct sipa_endpoint *ep;
	struct sipa_skb_array recv_array;
	wait_queue_head_t recv_waitq;
	struct workqueue_struct *recv_wq;
	struct work_struct recv_work;
	spinlock_t lock;
	sipa_periph_dispatch_func dispatch_func;

	struct sipa_periph_sender *senders[SIPA_EP_MAX];
};


int create_sipa_periph_sender(struct sipa_context *ipa,
							  struct sipa_endpoint *ep,
							  enum sipa_xfer_pkt_type type,
							  struct sipa_periph_sender **sender_pp);

void destroy_sipa_periph_sender(struct sipa_periph_sender *sender);

int sipa_periph_send_data(struct sipa_periph_sender *sender,
						  struct sk_buff *skb,
						  enum sipa_term_type dst,
						  u8 netid);

int sipa_periph_search_item(struct sipa_periph_sender *sender,
							struct sipa_hal_fifo_item *item,
							struct sk_buff **skb);


int create_sipa_periph_receiver(struct sipa_context *ipa,
								struct sipa_endpoint *ep,
								sipa_periph_dispatch_func dispatch,
								struct sipa_periph_receiver **receiver_pp);

void destroy_sipa_periph_receiver(struct sipa_periph_receiver *receiver);

void sipa_receiver_add_sender(struct sipa_periph_receiver *receiver,
							  struct sipa_periph_sender *sender);


