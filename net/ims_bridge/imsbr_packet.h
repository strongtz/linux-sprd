#ifndef _IMSBR_PACKET_H
#define _IMSBR_PACKET_H

#define IMSBR_PACKET_VER	0

struct imsbr_packet {
	__u8	version;
	__u8	resv1;
	__u16	resv2;
	__u16	reasm_tlen;
	__u16	frag_off;
	/**
	 * Current packet length can be caculated by
	 * blk.length - sizeof(imsbr_packet)
	 */
	char	packet[0];
};

#define IMSBR_PACKET_MAXSZ \
	(IMSBR_DATA_BLKSZ - sizeof(struct imsbr_packet))

#define INIT_IMSBR_PACKET(p, reasm_tl) do { \
	typeof(p) _p = (p); \
	_p->version = IMSBR_PACKET_VER; \
	_p->resv1 = 0; \
	_p->resv2 = 0; \
	_p->reasm_tlen = reasm_tl; \
	_p->frag_off = 0; \
} while (0)

extern uint imsbr_frag_size;

void imsbr_packet_relay2cp(struct sk_buff *skb);

void imsbr_process_packet(struct imsbr_sipc *sipc, struct sblock *blk,
			  bool freeit);

#endif
