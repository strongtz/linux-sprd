/*
 * f_mbim.c - generic USB MBIM function driver
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/usb/cdc.h>
#include <linux/usb/composite.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/unaligned/access_ok.h>
#include <linux/configfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/netdevice.h>
#include <linux/of_gpio.h>
#include <linux/sipc.h>


#undef pr_fmt
#define pr_fmt(fmt) "[MBIM]%s: " fmt, __func__

/*
 * This function is a "Mobile Broadband Interface Model" (MBIM) link.
 * MBIM is intended to be used with high-speed network attachments.
 *
 * Note that MBIM requires the use of "alternate settings" for its data
 * interface.  This means that the set_alt() method has real work to do,
 * and also means that a get_alt() method is required.
 */

#define MBIM_BULK_BUFFER_SIZE		4096

#define MBIM_IOCTL_MAGIC		'o'
#define MBIM_GET_NTB_SIZE		_IOR(MBIM_IOCTL_MAGIC, 2, u32)
#define MBIM_GET_DATAGRAM_COUNT		_IOR(MBIM_IOCTL_MAGIC, 3, u16)

#define NR_MBIM_PORTS			1

static struct workqueue_struct	*mbim_rx_wq;
static struct workqueue_struct	*mbim_tx_wq;

static int mbim_init(int instances);
static void fmbim_cleanup(void);

struct data_port {
	struct usb_ep	*in;
	struct usb_ep	*out;
};

struct mbim_device_stats {
	u32 in_discards;
	u32 in_errors;
	u64 in_octets;
	u64 in_packets;
	u64 out_octets;
	u64 out_packets;
	u32 out_errors;
	u32 out_discards;
};

struct ctrl_pkt {
	void			*buf;
	int			len;
	struct list_head	list;
};

struct data_pkt {
	void *buf;
	int len;
	int actual; /*only for RX*/
	struct sblock *sblk; /*only for TX*/
	struct list_head list;
};

struct mbim_ep_descs {
	struct usb_endpoint_descriptor	*in;
	struct usb_endpoint_descriptor	*out;
	struct usb_endpoint_descriptor	*notify;
};

struct mbim_notify_port {
	struct usb_ep			*notify;
	struct usb_request		*notify_req;
	u8				notify_state;
	atomic_t			notify_count;
};

enum mbim_notify_state {
	NCM_NOTIFY_NONE,
	NCM_NOTIFY_CONNECT,
	NCM_NOTIFY_SPEED,
	NCM_NOTIFY_RESPONSE_AVAILABLE,
};

struct f_mbim {
	struct usb_function		function;
	struct usb_composite_dev	*cdev;

	atomic_t	online;
	bool		is_open;

	atomic_t	open_excl;
	atomic_t	ioctl_excl;
	atomic_t	read_excl;
	atomic_t	write_excl;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;

	u8				port_num;
	struct data_port		mbim_data_port;
	struct mbim_notify_port		not_port;

	struct mbim_ep_descs		fs;
	struct mbim_ep_descs		hs;

	u8				ctrl_id, data_id;
	u8				data_alt_int;

	struct ndp_parser_opts		*parser_opts;

	spinlock_t			lock;
	spinlock_t			req_lock;	/* guard {rx,tx}_reqs */

	struct list_head	cpkt_req_q;
	struct list_head	cpkt_resp_q;
	struct list_head	tx_reqs, rx_reqs;
	struct list_head	tx_data_raw, rx_data_raw;

	u32			ntb_input_size;
	u16			ntb_max_datagrams;

	unsigned long todo;
#define WORK_RX_MEMORY 0

	struct work_struct	work;
	struct work_struct	rx_work;
	struct work_struct	tx_work;
	struct mbim_device_stats stats;

	bool sg_enabled;
	bool fix_header;

	/* For multi-frame NDP TX */
	atomic_t		error;
};

struct mbim_instance {
	struct usb_function_instance func_inst;
	const char *name;
	struct f_mbim *dev;
};

struct mbim_ntb_input_size {
	u32	ntb_input_size;
	u16	ntb_max_datagrams;
	u16	reserved;
};

/* temporary variable used between mbim_open() and mbim_gadget_bind() */
static struct f_mbim *_mbim_dev;

static unsigned int nr_mbim_ports;

static struct mbim_ports {
	struct f_mbim	*port;
	unsigned int	port_num;
} mbim_ports[NR_MBIM_PORTS];

static void rx_fill(struct f_mbim *dev, gfp_t gfp_flags);
static void mbim_disconnect(struct f_mbim *dev);
static inline struct f_mbim *func_to_mbim(struct usb_function *f)
{
	return container_of(f, struct f_mbim, function);
}

/* peak (theoretical) bulk transfer rate in bits-per-second */
static inline unsigned  int mbim_bitrate(struct usb_gadget *g)
{
	if (gadget_is_dualspeed(g) && g->speed == USB_SPEED_HIGH)
		return 13 * 512 * 8 * 1000 * 8;
	else
		return 19 *  64 * 1 * 1000 * 8;
}

/*-------------------------------------------------------------------------*/

#define NTB_DEFAULT_IN_SIZE	0x4000
#define NTB_OUT_SIZE		0x1000
#define NDP_IN_DIVISOR		0x4

static struct usb_cdc_ncm_ntb_parameters ntb_parameters = {
	.wLength = sizeof(ntb_parameters),
	.bmNtbFormatsSupported = cpu_to_le16(USB_CDC_NCM_NTB16_SUPPORTED),
	.dwNtbInMaxSize = cpu_to_le32(NTB_DEFAULT_IN_SIZE),
	.wNdpInDivisor = cpu_to_le16(NDP_IN_DIVISOR),
	.wNdpInPayloadRemainder = cpu_to_le16(0),
	.wNdpInAlignment = cpu_to_le16(4),

	.dwNtbOutMaxSize = cpu_to_le32(NTB_OUT_SIZE),
	.wNdpOutDivisor = cpu_to_le16(4),
	.wNdpOutPayloadRemainder = cpu_to_le16(0),
	.wNdpOutAlignment = cpu_to_le16(4),
	.wNtbOutMaxDatagrams = 16,
};

/*
 * Use wMaxPacketSize big enough to fit CDC_NOTIFY_SPEED_CHANGE in one
 * packet, to simplify cancellation; and a big transfer interval, to
 * waste less bandwidth.
 */

#define LOG2_STATUS_INTERVAL_MSEC	5	/* 1 << 5 == 32 msec */
#define NCM_STATUS_BYTECOUNT		16	/* 8 byte header + data */

static struct usb_interface_assoc_descriptor mbim_iad_desc = {
	.bLength =		sizeof(mbim_iad_desc),
	.bDescriptorType =	USB_DT_INTERFACE_ASSOCIATION,

	/* .bFirstInterface =	DYNAMIC, */
	.bInterfaceCount =	2,	/* control + data */
	.bFunctionClass =	2,
	.bFunctionSubClass =	0x0e,
	.bFunctionProtocol =	0,
	/* .iFunction =		DYNAMIC */
};

/* interface descriptor: */
static struct usb_interface_descriptor mbim_control_intf = {
	.bLength =		sizeof(mbim_control_intf),
	.bDescriptorType =	USB_DT_INTERFACE,

	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	1,
	.bInterfaceClass =	0x02,
	.bInterfaceSubClass =	0x0e,
	.bInterfaceProtocol =	0,
	/* .iInterface = DYNAMIC */
};

static struct usb_cdc_header_desc mbim_header_desc = {
	.bLength =		sizeof(mbim_header_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,

	.bcdCDC =		cpu_to_le16(0x0110),
};

static struct usb_cdc_union_desc mbim_union_desc = {
	.bLength =		sizeof(mbim_union_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_UNION_TYPE,
	/* .bMasterInterface0 =	DYNAMIC */
	/* .bSlaveInterface0 =	DYNAMIC */
};

static struct usb_cdc_mbim_desc mbim_desc = {
	.bLength =		sizeof(mbim_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_MBIM_TYPE,

	.bcdMBIMVersion =	cpu_to_le16(0x0100),

	.wMaxControlMessage =	cpu_to_le16(0x1000),
	.bNumberFilters =	0x20,
	.bMaxFilterSize =	0x80,
	.wMaxSegmentSize =	cpu_to_le16(0xfe0),
	.bmNetworkCapabilities = 0x20,
};

static struct usb_cdc_mbim_extended_desc ext_mbb_desc = {
	.bLength =	sizeof(ext_mbb_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_MBIM_EXTENDED_TYPE,

	.bcdMBIMExtendedVersion =	cpu_to_le16(0x0100),
	.bMaxOutstandingCommandMessages =	64,
	.wMTU =	1500,
};

/* the default data interface has no endpoints ... */
static struct usb_interface_descriptor mbim_data_nop_intf = {
	.bLength =		sizeof(mbim_data_nop_intf),
	.bDescriptorType =	USB_DT_INTERFACE,

	/* .bInterfaceNumber = DYNAMIC */
	.bAlternateSetting =	0,
	.bNumEndpoints =	0,
	.bInterfaceClass =	0x0a,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0x02,
	/* .iInterface = DYNAMIC */
};

/* ... but the "real" data interface has two bulk endpoints */
static struct usb_interface_descriptor mbim_data_intf = {
	.bLength =		sizeof(mbim_data_intf),
	.bDescriptorType =	USB_DT_INTERFACE,

	/* .bInterfaceNumber = DYNAMIC */
	.bAlternateSetting =	1,
	.bNumEndpoints =	2,
	.bInterfaceClass =	0x0a,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0x02,
	/* .iInterface = DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor fs_mbim_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	4*cpu_to_le16(NCM_STATUS_BYTECOUNT),
	.bInterval =		1 << LOG2_STATUS_INTERVAL_MSEC,
};

static struct usb_endpoint_descriptor fs_mbim_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor fs_mbim_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *mbim_fs_function[] = {
	(struct usb_descriptor_header *) &mbim_iad_desc,
	/* MBIM control descriptors */
	(struct usb_descriptor_header *) &mbim_control_intf,
	(struct usb_descriptor_header *) &mbim_header_desc,
	(struct usb_descriptor_header *) &mbim_union_desc,
	(struct usb_descriptor_header *) &mbim_desc,
	(struct usb_descriptor_header *) &ext_mbb_desc,
	(struct usb_descriptor_header *) &fs_mbim_notify_desc,
	/* data interface, altsettings 0 and 1 */
	(struct usb_descriptor_header *) &mbim_data_nop_intf,
	(struct usb_descriptor_header *) &mbim_data_intf,
	(struct usb_descriptor_header *) &fs_mbim_in_desc,
	(struct usb_descriptor_header *) &fs_mbim_out_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor hs_mbim_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	4*cpu_to_le16(NCM_STATUS_BYTECOUNT),
	.bInterval =		LOG2_STATUS_INTERVAL_MSEC + 4,
};
static struct usb_endpoint_descriptor hs_mbim_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor hs_mbim_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *mbim_hs_function[] = {
	(struct usb_descriptor_header *) &mbim_iad_desc,
	/* MBIM control descriptors */
	(struct usb_descriptor_header *) &mbim_control_intf,
	(struct usb_descriptor_header *) &mbim_header_desc,
	(struct usb_descriptor_header *) &mbim_union_desc,
	(struct usb_descriptor_header *) &mbim_desc,
	(struct usb_descriptor_header *) &ext_mbb_desc,
	(struct usb_descriptor_header *) &hs_mbim_notify_desc,
	/* data interface, altsettings 0 and 1 */
	(struct usb_descriptor_header *) &mbim_data_nop_intf,
	(struct usb_descriptor_header *) &mbim_data_intf,
	(struct usb_descriptor_header *) &hs_mbim_in_desc,
	(struct usb_descriptor_header *) &hs_mbim_out_desc,
	NULL,
};

/* string descriptors: */

#define STRING_CTRL_IDX	0
#define STRING_DATA_IDX	1

static struct usb_string mbim_string_defs[] = {
	[STRING_CTRL_IDX].s = "MBIM Control",
	[STRING_DATA_IDX].s = "MBIM Data",
	{  } /* end of list */
};

static struct usb_gadget_strings mbim_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		mbim_string_defs,
};

static struct usb_gadget_strings *mbim_strings[] = {
	&mbim_string_table,
	NULL,
};

/* Microsoft OS Descriptors */

/*
 * We specify our own bMS_VendorCode byte which Windows will use
 * as the bRequest value in subsequent device get requests.
 */
#define MBIM_VENDOR_CODE	0xA5

/* Microsoft Extended Configuration Descriptor Header Section */
struct mbim_ext_config_desc_header {
	__le32	dwLength;
	__u16	bcdVersion;
	__le16	wIndex;
	__u8	bCount;
	__u8	reserved[7];
};

/* Microsoft Extended Configuration Descriptor Function Section */
struct mbim_ext_config_desc_function {
	__u8	bFirstInterfaceNumber;
	__u8	bInterfaceCount;
	__u8	compatibleID[8];
	__u8	subCompatibleID[8];
	__u8	reserved[6];
};

/* Microsoft Extended Configuration Descriptor */
static struct {
	struct mbim_ext_config_desc_header	header;
	struct mbim_ext_config_desc_function    function;
} mbim_ext_config_desc = {
	.header = {
		.dwLength = cpu_to_le32(sizeof(mbim_ext_config_desc)),
		.bcdVersion = cpu_to_le16(0x0100),
		.wIndex = cpu_to_le16(4),
		.bCount = 1,
	},
	.function = {
		.bFirstInterfaceNumber = 0,
		.bInterfaceCount = 1,
		.compatibleID = { 'A', 'L', 'T', 'R', 'C', 'F', 'G' },
		/* .subCompatibleID = DYNAMIC */
	},
};

/*
 * Here are options for the Datagram Pointer table (NDP) parser.
 * There are 2 different formats: NDP16 and NDP32 in the spec (ch. 3),
 * in NDP16 offsets and sizes fields are 1 16bit word wide,
 * in NDP32 -- 2 16bit words wide. Also signatures are different.
 * To make the parser code the same, put the differences in the structure,
 * and switch pointers to the structures when the format is changed.
 */

struct ndp_parser_opts {
	unsigned int	nth_sign;
	unsigned int	ndp_sign;
	unsigned int	nth_size;
	unsigned int	ndp_size;
	unsigned int 	dpe_size;
	unsigned int	ndplen_align;
	/* sizes in u16 units */
	unsigned int	dgram_item_len; /* index or length */
	unsigned int	block_length;
	unsigned int	ndp_index;
	unsigned int	reserved1;
	unsigned int	reserved2;
	unsigned int	next_ndp_index;
};

#define INIT_NDP16_OPTS {				\
	.nth_sign = USB_CDC_NCM_NTH16_SIGN,		\
	.ndp_sign = USB_CDC_MBIM_NDP16_IPS_SIGN,	\
	.nth_size = sizeof(struct usb_cdc_ncm_nth16),	\
	.ndp_size = sizeof(struct usb_cdc_ncm_ndp16),	\
	.dpe_size = sizeof(struct usb_cdc_ncm_dpe16),   \
	.ndplen_align = 4,				\
	.dgram_item_len = 1,				\
	.block_length = 1,				\
	.ndp_index = 1,					\
	.reserved1 = 0,					\
	.reserved2 = 0,					\
	.next_ndp_index = 1,				\
}

#define INIT_NDP32_OPTS {				\
	.nth_sign = USB_CDC_NCM_NTH32_SIGN,		\
	.ndp_sign = USB_CDC_MBIM_NDP32_IPS_SIGN,	\
	.nth_size = sizeof(struct usb_cdc_ncm_nth32),	\
	.ndp_size = sizeof(struct usb_cdc_ncm_ndp32),	\
	.dpe_size = sizeof(struct usb_cdc_ncm_dpe32),   \
	.ndplen_align = 8,				\
	.dgram_item_len = 2,				\
	.block_length = 2,				\
	.ndp_index = 2,					\
	.reserved1 = 1,					\
	.reserved2 = 2,					\
	.next_ndp_index = 2,				\
}

static struct ndp_parser_opts ndp16_opts = INIT_NDP16_OPTS;
static struct ndp_parser_opts ndp32_opts = INIT_NDP32_OPTS;

static inline void put_ncm(__le16 **p, unsigned int size, unsigned int val)
{
	switch (size) {
	case 1:
		put_unaligned_le16((u16)val, *p);
		break;
	case 2:
		put_unaligned_le32((u32)val, *p);

		break;
	default:
		pr_err("invalid parameters!!!");
	}

	*p += size;
}

static inline unsigned int get_ncm(__le16 **p, unsigned int size)
{
	unsigned int tmp = 0;

	switch (size) {
	case 1:
		tmp = get_unaligned_le16(*p);
		break;
	case 2:
		tmp = get_unaligned_le32(*p);
		break;
	default:
		pr_err("invalid parameters!!!");
	}

	*p += size;
	return tmp;
}

/*-------------------------txrx----------------------------*/
/* bytes guarding against rx overflows */
#define RX_EXTRA	20
#define QMULT_DEFAULT	5
#define DEFAULT_QLEN	2	/* double buffering by default */


/* Allocation for storing the NDP, 32 should suffice for a
 * 16k packet. This allows a maximum of 32 * 507 Byte packets to
 * be transmitted in a single 16kB skb, though when sending full size
 * packets this limit will be plenty.
 * Smaller packets are not likely to be trying to maximize the
 * throughput and will be mstly sending smaller infrequent frames.
 */
#define TX_MAX_NUM_DPE 11

/* this refers to max number sgs per transfer
 * which includes headers/data packets
 */
#define DL_MAX_PKTS_PER_XFER (TX_MAX_NUM_DPE + 2)

static struct data_pkt *mbim_alloc_data_pkt(unsigned int len, gfp_t flags)
{
	struct data_pkt *pkt;

	pkt = kzalloc(sizeof(struct data_pkt), flags);
	if (!pkt)
		return ERR_PTR(-ENOMEM);

	pkt->buf = kmalloc(len, flags);
	if (!pkt->buf) {
		kfree(pkt);
		return ERR_PTR(-ENOMEM);
	}
	pkt->len = len;

	return pkt;
}

static void mbim_free_data_pkt(struct data_pkt *pkt)
{
	if (pkt) {
		kfree(pkt->buf);
		kfree(pkt);
	}
}

/*rx unwrap*/
static int mbim_unwrap_ntb(struct f_mbim *dev, unsigned int length, u8 *data)
{
	__le16 *tmp = (void *)data;
	const struct ndp_parser_opts *opts = dev->parser_opts;
	unsigned int max_size = le32_to_cpu(ntb_parameters.dwNtbOutMaxSize);
	int ndp_index;
	unsigned int dg_len, dg_len2;
	unsigned int ndp_len;
	unsigned int index = 0;
	unsigned int index2 = 0;

	/* dwSignature */
	if (get_unaligned_le32(tmp) != opts->nth_sign) {
		pr_err("Wrong NTH SIGN, skblen %d\n", length);
		print_hex_dump(KERN_INFO, "HEAD:",
			 DUMP_PREFIX_ADDRESS, 32, 1, data, 32, false);
		return -EINVAL;
	}
	tmp += 2;

	if (get_unaligned_le16(tmp++) != opts->nth_size) {
		pr_err("Wrong NTB headersize\n");
		return -EINVAL;
	}

	tmp++; /* skip wSequence */

	/* (d)wBlockLength */
	if (get_ncm(&tmp, opts->block_length) > max_size) {
		pr_err("OUT size exceeded\n");
		return -EINVAL;
	}

	ndp_index = get_ncm(&tmp, opts->ndp_index);

	/* Run through all the NDP's in the NTB */
	do {
		/* NCM 3.2 */
		if (((ndp_index % 4) != 0) && (ndp_index < opts->nth_size)) {
			pr_err("Bad index: %#X\n", ndp_index);
			return -EINVAL;
		}

		/* walk through NDP */
		tmp = (void *)(data + ndp_index);
		if (get_unaligned_le32(tmp) != opts->ndp_sign) {
			pr_err("Wrong NDP SIGN\n");
			return -EINVAL;
		}
		tmp += 2;

		ndp_len = get_unaligned_le16(tmp++);
		/*
		 * NCM 3.3.1
		 * entry is 2 items
		 * item size is 16/32 bits, opts->dgram_item_len * 2 bytes
		 * minimal: struct usb_cdc_ncm_ndpX + normal entry + zero entry
		 * Each entry is a dgram index and a dgram length.
		 */
		if ((ndp_len < opts->ndp_size
				+ 2 * 2 * (opts->dgram_item_len * 2))
				|| (ndp_len % opts->ndplen_align != 0)) {
			pr_err("Bad NDP length: %#X\n", ndp_len);
			return -EINVAL;
		}
		tmp += opts->reserved1;
		/* Check for another NDP (d)wNextNdpIndex */
		ndp_index = get_ncm(&tmp, opts->next_ndp_index);
		tmp += opts->reserved2;

		ndp_len -= opts->ndp_size;
		index2 = get_ncm(&tmp, opts->dgram_item_len);
		dg_len2 = get_ncm(&tmp, opts->dgram_item_len);

		do {
			index = index2;
			dg_len = dg_len2;

			index2 = get_ncm(&tmp, opts->dgram_item_len);
			dg_len2 = get_ncm(&tmp, opts->dgram_item_len);

			ndp_len -= 2 * (opts->dgram_item_len * 2);

			if (index2 == 0 || dg_len2 == 0)
				break;
		} while (ndp_len > 2 * (opts->dgram_item_len * 2));

	} while (ndp_index);

	return 0;
}

static void rx_complete(struct usb_ep *ep, struct usb_request *req);
static void tx_complete(struct usb_ep *ep, struct usb_request *req);
static netdev_tx_t mbim_start_xmit(struct sblock *blk, struct f_mbim *mbim_dev);

static void defer_work(struct work_struct *work)
{
	struct f_mbim *dev = container_of(work, struct f_mbim, work);

	if (test_and_clear_bit(WORK_RX_MEMORY, &dev->todo))
		rx_fill(dev, GFP_KERNEL);

}

static void defer_kevent(struct f_mbim *dev, int flag)
{
	if (test_and_set_bit(flag, &dev->todo))
		return;
	if (!schedule_work(&dev->work))
		pr_err("kevent %d may have been dropped\n", flag);
}

static int prealloc_sg(struct list_head *list,
	 struct usb_ep *ep, unsigned int n, bool sg_supported)
{
	unsigned int		i;
	struct usb_request	*req;
	struct data_pkt *rpkt;
	int	size;

	if (!n)
		return -EINVAL;

	/* queue/recycle up to N requests */
	i = n;
	list_for_each_entry(req, list, list) {
		if (i-- == 0)
			goto extra;
	}
	while (i--) {
		req = usb_ep_alloc_request(ep, GFP_ATOMIC);
		if (!req)
			return list_empty(list) ? -ENOMEM : 0;

		req->complete = tx_complete;
		if (!sg_supported)
			goto add_list;

		size = DL_MAX_PKTS_PER_XFER * sizeof(struct scatterlist);
		req->sg = kmalloc(size,	GFP_ATOMIC);
		if (!req->sg)
			goto extra;

		rpkt = kmalloc(sizeof(struct data_pkt), GFP_ATOMIC);
		if (!rpkt)
			goto extra;
		req->context = rpkt;

		req->buf = kzalloc(sizeof(struct usb_cdc_ncm_nth32) +
			 sizeof(struct usb_cdc_ncm_ndp32) +
			 8 * (TX_MAX_NUM_DPE + 1), GFP_ATOMIC);
		if (!req->buf)
			goto extra;
add_list:
		list_add(&req->list, list);
	}
	return 0;

extra:
	/* free extras */
	for (;;) {
		struct list_head	*next;

		next = req->list.next;
		list_del(&req->list);

		if (req->buf)
			kfree(req->buf);

		if (req->sg)
			kfree(req->sg);

		if (req->context)
			kfree(req->context);

		usb_ep_free_request(ep, req);

		if (next == list)
			break;
		req = container_of(next, struct usb_request, list);
	}
	return -ENOMEM;
}

static int prealloc(struct list_head *list, struct usb_ep *ep, unsigned int n)
{
	unsigned int		i;
	struct usb_request	*req;

	if (!n)
		return -EINVAL;

	/* queue/recycle up to N requests */
	i = n;
	list_for_each_entry(req, list, list) {
		if (i-- == 0)
			goto extra;
	}
	while (i--) {
		req = usb_ep_alloc_request(ep, GFP_ATOMIC);
		if (!req)
			return list_empty(list) ? -ENOMEM : 0;
		list_add(&req->list, list);
	}
	return 0;

extra:
	/* free extras */
	for (;;) {
		struct list_head	*next;

		next = req->list.next;
		list_del(&req->list);
		usb_ep_free_request(ep, req);

		if (next == list)
			break;

		req = container_of(next, struct usb_request, list);
	}
	return -ENOMEM;
}

static int alloc_requests(struct f_mbim *dev, unsigned int n)
{
	int	status;

	spin_lock(&dev->req_lock);
	if (dev->sg_enabled)
		status = prealloc_sg(&dev->tx_reqs, dev->mbim_data_port.in,
				 n, dev->sg_enabled);
	else
		status = prealloc(&dev->tx_reqs, dev->mbim_data_port.in, n);

	if (status >= 0)
		status = prealloc(&dev->rx_reqs, dev->mbim_data_port.out, n);
	spin_unlock(&dev->req_lock);
	return status;
}

static int
rx_submit(struct f_mbim *dev, struct usb_request *req, gfp_t gfp_flags)
{
	int	retval = -ENOMEM;
	struct usb_ep	*out;
	size_t size = le32_to_cpu(NTB_OUT_SIZE);
	unsigned long flags;
	struct data_pkt *rx_data = NULL;

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->mbim_data_port.out)
		out = dev->mbim_data_port.out;
	else
		out = NULL;
	spin_unlock_irqrestore(&dev->lock, flags);

	if (!out)
		return -ENOTCONN;

	rx_data = mbim_alloc_data_pkt(size, GFP_ATOMIC);
	if (rx_data) {
		req->buf = rx_data->buf;
		req->length = size;
		req->complete = rx_complete;
		req->context = rx_data;

		retval = usb_ep_queue(out, req, gfp_flags);
	} else {
		pr_err("Unable to allocate RX data pkt\n");
	}

	if (retval == -ENOMEM)
		defer_kevent(dev, WORK_RX_MEMORY);
	if (retval) {
		pr_err("rx submit --> %d\n", retval);
		if (rx_data)
			mbim_free_data_pkt(rx_data);
	}
	return retval;
}

static void rx_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct data_pkt *rx_data = req->context;
	struct f_mbim *dev = ep->driver_data;
	int	status = req->status;
	bool queue = 0;
	unsigned long flags;

	switch (status) {
	/* normal completion */
	case 0:
		if (atomic_read(&dev->online)) {
			rx_data->actual = req->actual;
			spin_lock_irqsave(&dev->req_lock, flags);
			list_add_tail(&rx_data->list, &dev->rx_data_raw);
			spin_unlock_irqrestore(&dev->req_lock, flags);
			queue = 1;
		} else {
			pr_err("USB cable not connected\n");
			mbim_free_data_pkt(rx_data);
			status = -ENOTCONN;
		}

		break;

	/* software-driven interface shutdown */
	case -ECONNRESET:		/* unlink */
	case -ESHUTDOWN:		/* disconnect etc */
		pr_err("rx connreset/shutdown, code %d\n", status);
		mbim_free_data_pkt(rx_data);
		break;

	/* for hardware automagic (such as pxa) */
	case -ECONNABORTED:		/* endpoint reset */
		pr_err("rx %s reset\n", ep->name);
		defer_kevent(dev, WORK_RX_MEMORY);
		mbim_free_data_pkt(rx_data);
		break;

	/* data overrun */
	case -EOVERFLOW:
		/* FALLTHROUGH */

	default:
		queue = 1;
		mbim_free_data_pkt(rx_data);
		dev->stats.out_errors++;
		pr_err("rx status %d\n", status);
		break;
	}

	spin_lock(&dev->req_lock);
	list_add_tail(&req->list, &dev->rx_reqs);
	spin_unlock(&dev->req_lock);

	if (queue)
		queue_work(mbim_rx_wq, &dev->rx_work);
}

static void rx_fill(struct f_mbim *dev, gfp_t gfp_flags)
{
	struct usb_request	*req;
	unsigned long		flags;
	int			req_cnt = 0;

	/* fill unused rxq slots with some skb */
	spin_lock_irqsave(&dev->req_lock, flags);
	while (!list_empty(&dev->rx_reqs)) {
		/* break the nexus of continuous completion and re-submission*/
		if (++req_cnt > DEFAULT_QLEN)
			break;

		req = container_of(dev->rx_reqs.next, struct usb_request, list);
		list_del_init(&req->list);
		spin_unlock_irqrestore(&dev->req_lock, flags);

		if (rx_submit(dev, req, gfp_flags) < 0) {
			pr_err("rx_submit failed\n");
			spin_lock_irqsave(&dev->req_lock, flags);
			list_add(&req->list, &dev->rx_reqs);
			spin_unlock_irqrestore(&dev->req_lock, flags);
			defer_kevent(dev, WORK_RX_MEMORY);
			return;
		}
		spin_lock_irqsave(&dev->req_lock, flags);
	}
	spin_unlock_irqrestore(&dev->req_lock, flags);
}
static void process_rx_w(struct work_struct *work)
{
	struct f_mbim *dev = container_of(work, struct f_mbim, rx_work);
	struct data_pkt *rx_data = NULL;
	int	status = 0;
	unsigned long flags;

	if (!dev) {
		pr_err("NULL mbim pointer\n");
		return;
	}

	spin_lock_irqsave(&dev->req_lock, flags);
	while (!list_empty(&dev->rx_data_raw)) {
		rx_data = list_first_entry(&dev->rx_data_raw,
			struct data_pkt, list);
		list_del(&rx_data->list);
		spin_unlock_irqrestore(&dev->req_lock, flags);

		status = mbim_unwrap_ntb(dev, rx_data->actual, rx_data->buf);
		if (status < 0) {
			dev->stats.out_errors++;
		} else {
			dev->stats.out_packets++;
			dev->stats.out_octets += rx_data->actual;
		}

		mbim_free_data_pkt(rx_data);
		spin_lock_irqsave(&dev->req_lock, flags);
	}
	spin_unlock_irqrestore(&dev->req_lock, flags);

	if (atomic_read(&dev->online))
		rx_fill(dev, GFP_KERNEL);
}

static void tx_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_mbim *dev = ep->driver_data;
	struct data_pkt *rd = NULL;
	unsigned long flags;

	switch (req->status) {
	default:
		dev->stats.in_errors++;
		pr_err("tx err %d\n", req->status);
	case -ECONNRESET:       /* unlink */
	case -ESHUTDOWN:        /* disconnect etc */
		break;
	case 0:
		if (!req->zero)
			dev->stats.in_octets += req->length - 1;
		else
			dev->stats.in_octets += req->length;
	}

	if (req->num_sgs) {
		struct data_pkt *rpkt = req->context;

		spin_lock_irqsave(&dev->req_lock, flags);
		while (!list_empty(&rpkt->list)) {
		    rd = list_first_entry(&rpkt->list, struct data_pkt, list);
		    list_del(&rd->list);
			spin_unlock_irqrestore(&dev->req_lock, flags);

			kfree(rd->sblk);
			kfree(rd);
			spin_lock_irqsave(&dev->req_lock, flags);
		}
		spin_unlock_irqrestore(&dev->req_lock, flags);
	}

	dev->stats.in_packets++;

	spin_lock(&dev->req_lock);
	list_add_tail(&req->list, &dev->tx_reqs);

	if (req->num_sgs) {
		if (!req->status)
			queue_work(mbim_tx_wq, &dev->tx_work);

		spin_unlock(&dev->req_lock);
		return;
	}
	spin_unlock(&dev->req_lock);
}

static int ncm_header_wrap(struct f_mbim *dev, void *req_buf,
	 struct data_pkt *dpkt, int count, int blk_len, int pad_len)
{
	const struct ndp_parser_opts *opts = dev->parser_opts;
	unsigned int blk_len_new = 0;
	unsigned int ndp_len = 0;
	__le16 *ntb_data;
	__le16 *ndp_data;
	unsigned int fix_header_length = 0;

	if (dev->fix_header) {
		fix_header_length = sizeof(struct usb_cdc_ncm_nth32) +
		sizeof(struct usb_cdc_ncm_ndp32) + 8 * (TX_MAX_NUM_DPE + 1);
	}
	ndp_len = opts->ndp_size + ((count + 1) * 4 * opts->dgram_item_len);

	/*NTH dwSignature */
	ntb_data = (__le16 *)req_buf;
	put_unaligned_le32(opts->nth_sign, ntb_data);
	/* wHeaderLength */
	ntb_data += 2;
	put_unaligned_le16(opts->nth_size, ntb_data);
	/* dwBlockLength */
	ntb_data += 2;
	if (dev->fix_header) {
		if (count == 1)
			blk_len_new = fix_header_length
					 + pad_len + dpkt->len;
		else
			blk_len_new = pad_len + dpkt->len + blk_len;
	} else {
		if (count == 1)
			blk_len_new = opts->nth_size +
				 (dpkt->len + pad_len) + ndp_len;
		else
			blk_len_new = (dpkt->len + pad_len)
				 + blk_len + (4 * opts->dgram_item_len);
	}
	put_ncm(&ntb_data, opts->block_length, blk_len_new);
	/* dwNdpIndex */
	if (dev->fix_header)
		put_ncm(&ntb_data, opts->ndp_index, opts->nth_size);
	else
		put_ncm(&ntb_data, opts->ndp_index, blk_len_new - ndp_len);
	/*NDP dwSignature */
	ndp_data = (__le16 *)req_buf + (opts->nth_size >> 1);
	put_unaligned_le32(opts->ndp_sign, ndp_data);
	/* NDP wLength */
	ndp_data += 2;
	put_unaligned_le16(ndp_len, ndp_data);

	ndp_data += 1 + opts->reserved1 + opts->next_ndp_index +
		 opts->reserved2 + (count - 1) * opts->dgram_item_len * 2;

	if (dev->fix_header) {
		/* (d)wDatagramIndex */
		put_ncm(&ndp_data, opts->dgram_item_len,
				 blk_len_new - pad_len - dpkt->len);
	} else {
		/* (d)wDatagramIndex */
		put_ncm(&ndp_data, opts->dgram_item_len,
			 blk_len_new - pad_len - dpkt->len - ndp_len);
	}
	/* (d)wDatagramLength */
	put_ncm(&ndp_data, opts->dgram_item_len, dpkt->len);
	/* (d)wDatagramIndex last one */
	put_ncm(&ndp_data, opts->dgram_item_len, 0);
	/* (d)wDatagramLength last one */
	put_ncm(&ndp_data, opts->dgram_item_len, 0);

	return blk_len_new;
}

static void process_tx_w(struct work_struct *work)
{
	struct f_mbim *dev = container_of(work, struct f_mbim, tx_work);
	const struct ndp_parser_opts *opts = dev->parser_opts;
	struct usb_ep *in = NULL;
	unsigned long flags;
	struct usb_request	*req;
	struct data_pkt *dpkt = NULL;
	struct data_pkt *rpkt = NULL;
	int ret, count, blk_len = 0;
	int pad_len;

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->mbim_data_port.in)
		in = dev->mbim_data_port.in;
	else {
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	spin_lock_irqsave(&dev->req_lock, flags);
	while (in && !list_empty(&dev->tx_reqs)
		 && !list_empty(&dev->tx_data_raw)) {
		req = list_first_entry(&dev->tx_reqs,
			 struct usb_request, list);
		list_del(&req->list);

		dpkt = list_first_entry(&dev->tx_data_raw,
			 struct data_pkt, list);
		list_del(&dpkt->list);

		rpkt = req->context;
		INIT_LIST_HEAD(&rpkt->list);
		spin_unlock_irqrestore(&dev->req_lock, flags);

		req->num_sgs = 0;
		req->zero = 1;
		req->length = 0;
		sg_init_table(req->sg, DL_MAX_PKTS_PER_XFER);

		count = 1;

		do {
			pad_len = dpkt->len % 4;
			if (pad_len)
				pad_len = 4 - pad_len;

			blk_len = ncm_header_wrap(dev, req->buf,
				 dpkt, count, blk_len, pad_len);

			if (count == 1) {
				if (dev->fix_header)
					sg_set_buf(&req->sg[req->num_sgs],
						 req->buf, 128);
				else
					sg_set_buf(&req->sg[req->num_sgs],
						 req->buf, opts->nth_size);
				req->num_sgs++;
			}

			sg_set_buf(&req->sg[req->num_sgs],
				 dpkt->sblk->addr, dpkt->len + pad_len);
			req->num_sgs++;
			req->length = blk_len;

			spin_lock_irqsave(&dev->req_lock, flags);
			list_add_tail(&dpkt->list, &rpkt->list);

			if (list_empty(&dev->tx_data_raw)) {
				spin_unlock_irqrestore(&dev->req_lock, flags);
				break;
			}
			dpkt = list_first_entry(&dev->tx_data_raw,
				 struct data_pkt, list);

			if ((req->length + dpkt->len) >= NTB_DEFAULT_IN_SIZE ||
					count >= TX_MAX_NUM_DPE) {
				spin_unlock_irqrestore(&dev->req_lock, flags);
				break;
			}
			list_del(&dpkt->list);
			spin_unlock_irqrestore(&dev->req_lock, flags);
			count++;
		} while (true);

		if (!dev->fix_header) {
			sg_set_buf(&req->sg[req->num_sgs],
				 req->buf + opts->nth_size, opts->ndp_size
				 + ((count + 1) * 4 * opts->dgram_item_len));
			req->num_sgs++;
		}

		sg_mark_end(&req->sg[req->num_sgs - 1]);

		ret = usb_ep_queue(in, req, GFP_KERNEL);
		spin_lock_irqsave(&dev->req_lock, flags);
		if (ret) {
			dev->stats.in_discards++;
			pr_err("tx usb_ep_queue ERROR!!!\n");
			list_add_tail(&req->list, &dev->tx_reqs);
			break;
		}
	}
	spin_unlock_irqrestore(&dev->req_lock, flags);
}

static int mbim_start_xmit(struct sblock *blk, struct f_mbim *mbim_dev)
{
	struct f_mbim *dev = mbim_dev;
	unsigned long flags;
	struct data_pkt *dpkt = NULL;

	dpkt = kzalloc(sizeof(struct data_pkt), GFP_ATOMIC);
	if (!dpkt)
		return -ENOMEM;
	dpkt->len = blk->length;
	dpkt->sblk = kzalloc(sizeof(struct sblock), GFP_ATOMIC);
	if (!dpkt->sblk) {
		kfree(dpkt);
		return -ENOMEM;
	}
	memcpy(dpkt->sblk, blk, sizeof(struct sblock));
	spin_lock_irqsave(&dev->req_lock, flags);
	list_add_tail(&dpkt->list, &dev->tx_data_raw);
	spin_unlock_irqrestore(&dev->req_lock, flags);

	queue_work(mbim_tx_wq, &dev->tx_work);

	return 0;
}

int eth_start_fast_xmit(struct sblock *blk)
{
	return mbim_start_xmit(blk, _mbim_dev);
}

/*---------------------------------------------------------*/

static inline int mbim_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1)
		return 0;

	atomic_dec(excl);
	return -EBUSY;
}

static inline void mbim_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

static struct ctrl_pkt *mbim_alloc_ctrl_pkt(unsigned int len, gfp_t flags)
{
	struct ctrl_pkt *pkt;

	pkt = kzalloc(sizeof(struct ctrl_pkt), flags);
	if (!pkt)
		return ERR_PTR(-ENOMEM);

	pkt->buf = kmalloc(len, flags);
	if (!pkt->buf) {
		kfree(pkt);
		return ERR_PTR(-ENOMEM);
	}
	pkt->len = len;

	return pkt;
}

static void mbim_free_ctrl_pkt(struct ctrl_pkt *pkt)
{
	if (pkt) {
		kfree(pkt->buf);
		kfree(pkt);
	}
}

static struct usb_request *mbim_alloc_req(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);

	if (!req)
		return NULL;

	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}
	req->length = buffer_size;
	return req;
}

void fmbim_free_req(struct usb_ep *ep, struct usb_request *req)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static void fmbim_ctrl_response_available(struct f_mbim *dev)
{
	struct usb_request		*req = dev->not_port.notify_req;
	struct usb_cdc_notification	*event = NULL;
	unsigned long			flags;
	int				ret;

	pr_debug("dev:%p portno#%d\n", dev, dev->port_num);

	spin_lock_irqsave(&dev->lock, flags);

	if (!atomic_read(&dev->online)) {
		pr_err("dev:%p is not online\n", dev);
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}

	if (!req) {
		pr_err("dev:%p req is NULL\n", dev);
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}

	if (!req->buf) {
		pr_err("dev:%p req->buf is NULL\n", dev);
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}

	if (atomic_inc_return(&dev->not_port.notify_count) != 1) {
		pr_debug("delay ep_queue: notifications queue is busy[%d]",
			atomic_read(&dev->not_port.notify_count));
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}

	req->length = sizeof(*event);
	event = req->buf;
	event->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
			| USB_RECIP_INTERFACE;
	event->bNotificationType = USB_CDC_NOTIFY_RESPONSE_AVAILABLE;
	event->wValue = cpu_to_le16(0);
	event->wIndex = cpu_to_le16(dev->ctrl_id);
	event->wLength = cpu_to_le16(0);
	spin_unlock_irqrestore(&dev->lock, flags);

	ret = usb_ep_queue(dev->not_port.notify,
			   req, GFP_ATOMIC);
	if (ret) {
		atomic_dec(&dev->not_port.notify_count);
		pr_err("ep enqueue error %d\n", ret);
	}

	pr_debug("Successful Exit");
}

static int
fmbim_send_cpkt_response(struct f_mbim *gr, struct ctrl_pkt *cpkt)
{
	struct f_mbim	*dev = gr;
	unsigned long	flags;

	if (!gr || !cpkt) {
		pr_err("Invalid cpkt, dev:%p cpkt:%p\n",
				gr, cpkt);
		return -ENODEV;
	}

	pr_debug("dev:%p port_num#%d\n", dev, dev->port_num);

	if (!atomic_read(&dev->online)) {
		pr_err("dev:%p is not connected\n", dev);
		mbim_free_ctrl_pkt(cpkt);
		return -ENODEV;
	}

	if (dev->not_port.notify_state != NCM_NOTIFY_RESPONSE_AVAILABLE) {
		pr_err("dev:%p state=%d, recover!!\n", dev,
			dev->not_port.notify_state);
		mbim_free_ctrl_pkt(cpkt);
		return -ENODEV;
	}

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&cpkt->list, &dev->cpkt_resp_q);
	spin_unlock_irqrestore(&dev->lock, flags);

	fmbim_ctrl_response_available(dev);

	return 0;
}

/* -------------------------------------------------------------------------*/

static inline void mbim_reset_values(struct f_mbim *mbim)
{
	mbim->parser_opts = &ndp16_opts;

	mbim->ntb_input_size = NTB_DEFAULT_IN_SIZE;

	atomic_set(&mbim->online, 0);
}

static void mbim_reset_function_queue(struct f_mbim *dev)
{
	struct ctrl_pkt	*cpkt = NULL;

	pr_debug("Queue empty packet for QBI");

	spin_lock(&dev->lock);
	if (!dev->is_open) {
		pr_err("%s: mbim file handler %p is not open", __func__, dev);
		spin_unlock(&dev->lock);
		return;
	}

	cpkt = mbim_alloc_ctrl_pkt(0, GFP_ATOMIC);
	if (!cpkt) {
		pr_err("%s: Unable to alloc reset function pkt\n", __func__);
		spin_unlock(&dev->lock);
		return;
	}

	list_add_tail(&cpkt->list, &dev->cpkt_req_q);
	spin_unlock(&dev->lock);

	pr_debug("%s: Wake up read queue", __func__);
	wake_up(&dev->read_wq);
}

static void fmbim_reset_cmd_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_mbim		*dev = req->context;

	mbim_reset_function_queue(dev);
}

static void mbim_clear_queues(struct f_mbim *mbim)
{
	struct ctrl_pkt	*cpkt = NULL;
	struct list_head *act, *tmp;

	spin_lock(&mbim->lock);
	list_for_each_safe(act, tmp, &mbim->cpkt_req_q) {
		cpkt = list_entry(act, struct ctrl_pkt, list);
		list_del(&cpkt->list);
		mbim_free_ctrl_pkt(cpkt);
	}
	list_for_each_safe(act, tmp, &mbim->cpkt_resp_q) {
		cpkt = list_entry(act, struct ctrl_pkt, list);
		list_del(&cpkt->list);
		mbim_free_ctrl_pkt(cpkt);
	}
	spin_unlock(&mbim->lock);
}

/*
 * Context: mbim->lock held
 */
static void mbim_do_notify(struct f_mbim *mbim)
{
	struct usb_request		*req = mbim->not_port.notify_req;
	struct usb_cdc_notification	*event;
	struct usb_composite_dev	*cdev = mbim->cdev;
	__le32				*data;
	int				status;

	pr_debug("notify_state: %d", mbim->not_port.notify_state);

	if (!req)
		return;

	event = req->buf;

	switch (mbim->not_port.notify_state) {

	case NCM_NOTIFY_NONE:
		if (atomic_read(&mbim->not_port.notify_count) > 0)
			pr_err("Pending notifications in NCM_NOTIFY_NONE\n");
		else
			pr_debug("No pending notifications\n");

		return;

	case NCM_NOTIFY_RESPONSE_AVAILABLE:
		pr_debug("Notification %02x sent\n", event->bNotificationType);

		if (atomic_read(&mbim->not_port.notify_count) <= 0)
			return;

		spin_unlock(&mbim->lock);
		status = usb_ep_queue(mbim->not_port.notify, req, GFP_ATOMIC);
		spin_lock(&mbim->lock);
		if (status) {
			atomic_dec(&mbim->not_port.notify_count);
			pr_err("Queue notify request failed, err: %d", status);
		}

		return;

	case NCM_NOTIFY_CONNECT:
		event->bNotificationType = USB_CDC_NOTIFY_NETWORK_CONNECTION;
		if (mbim->is_open)
			event->wValue = cpu_to_le16(1);
		else
			event->wValue = cpu_to_le16(0);
		event->wLength = 0;
		req->length = sizeof(*event);

		pr_debug("notify connect %s\n",
			mbim->is_open ? "true" : "false");
		mbim->not_port.notify_state = NCM_NOTIFY_RESPONSE_AVAILABLE;
		break;

	case NCM_NOTIFY_SPEED:
		event->bNotificationType = USB_CDC_NOTIFY_SPEED_CHANGE;
		event->wValue = cpu_to_le16(0);
		event->wLength = cpu_to_le16(8);
		req->length = NCM_STATUS_BYTECOUNT;

		/* SPEED_CHANGE data is up/down speeds in bits/sec */
		data = req->buf + sizeof(*event);
		data[0] = cpu_to_le32(mbim_bitrate(cdev->gadget));
		data[1] = data[0];

		pr_debug("notify speed %d\n",
			mbim_bitrate(cdev->gadget));
		mbim->not_port.notify_state = NCM_NOTIFY_CONNECT;
		break;
	}

	event->bmRequestType = 0xA1;
	event->wIndex = cpu_to_le16(mbim->ctrl_id);

	/*
	 * In double buffering if there is a space in FIFO,
	 * completion callback can be called right after the call,
	 * so unlocking
	 */
	atomic_inc(&mbim->not_port.notify_count);
	pr_debug("queue request: notify_count = %d",
		atomic_read(&mbim->not_port.notify_count));
	spin_unlock(&mbim->lock);
	status = usb_ep_queue(mbim->not_port.notify, req, GFP_ATOMIC);
	spin_lock(&mbim->lock);
	if (status) {
		atomic_dec(&mbim->not_port.notify_count);
		pr_err("usb_ep_queue failed, err: %d", status);
	}
}

/*
 * Context: mbim->lock held
 */
static void mbim_notify(struct f_mbim *mbim)
{
	/*
	 * If mbim_notify() is called before the second (CONNECT)
	 * notification is sent, then it will reset to send the SPEED
	 * notificaion again (and again, and again), but it's not a problem
	 */

	mbim->not_port.notify_state = NCM_NOTIFY_RESPONSE_AVAILABLE;
	mbim_do_notify(mbim);
}

static void mbim_notify_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_mbim			*mbim = req->context;
	struct usb_cdc_notification	*event = req->buf;

	pr_debug("req->status:%x\n", req->status);

	spin_lock(&mbim->lock);
	switch (req->status) {
	case 0:
		atomic_dec(&mbim->not_port.notify_count);
		pr_debug("notify_count = %d",
			atomic_read(&mbim->not_port.notify_count));
		break;

	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		mbim->not_port.notify_state = NCM_NOTIFY_NONE;
		atomic_set(&mbim->not_port.notify_count, 0);
		pr_err("ESHUTDOWN/ECONNRESET, connection gone");
		spin_unlock(&mbim->lock);
		mbim_clear_queues(mbim);
		mbim_reset_function_queue(mbim);
		spin_lock(&mbim->lock);
		break;
	default:
		pr_err("Unknown event %02x --> %d\n",
			event->bNotificationType, req->status);
		break;
	}

	mbim_do_notify(mbim);
	spin_unlock(&mbim->lock);
}

static void mbim_ep0out_complete(struct usb_ep *ep, struct usb_request *req)
{
	/* now for SET_NTB_INPUT_SIZE only */
	unsigned int		in_size = 0;
	struct usb_function	*f = req->context;
	struct f_mbim		*mbim = func_to_mbim(f);
	struct mbim_ntb_input_size *ntb = NULL;

	pr_debug("dev:%p\n", mbim);

	req->context = NULL;
	if (req->status || req->actual != req->length) {
		pr_err("Bad control-OUT transfer\n");
		goto invalid;
	}

	if (req->length == 4) {
		in_size = get_unaligned_le32(req->buf);
		if (in_size < USB_CDC_NCM_NTB_MIN_IN_SIZE ||
		    in_size > le32_to_cpu(ntb_parameters.dwNtbInMaxSize)) {
			pr_err("Illegal INPUT SIZE (%d) from host\n", in_size);
			goto invalid;
		}
	} else if (req->length == 8) {
		ntb = (struct mbim_ntb_input_size *)req->buf;
		in_size = get_unaligned_le32(&(ntb->ntb_input_size));
		if (in_size < USB_CDC_NCM_NTB_MIN_IN_SIZE ||
		    in_size > le32_to_cpu(ntb_parameters.dwNtbInMaxSize)) {
			pr_err("Illegal INPUT SIZE (%d) from host\n", in_size);
			goto invalid;
		}
		mbim->ntb_max_datagrams =
			get_unaligned_le16(&(ntb->ntb_max_datagrams));
	} else {
		pr_err("Illegal NTB length %d\n", in_size);
		goto invalid;
	}

	pr_debug("Set NTB INPUT SIZE %d\n", in_size);

	mbim->ntb_input_size = in_size;
	return;

invalid:
	usb_ep_set_halt(ep);
}

static void
fmbim_cmd_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_mbim		*dev = req->context;
	struct ctrl_pkt		*cpkt = NULL;
	int			len = req->actual;

	if (!dev) {
		pr_err("mbim dev is null\n");
		return;
	}

	if (req->status < 0) {
		pr_err("mbim command error %d\n", req->status);
		return;
	}

	pr_debug("dev:%p port#%d\n", dev, dev->port_num);

	cpkt = mbim_alloc_ctrl_pkt(len, GFP_ATOMIC);
	if (!cpkt) {
		pr_err("Unable to allocate ctrl pkt\n");
		return;
	}

	pr_debug("Add to cpkt_req_q packet with len = %d\n", len);
	memcpy(cpkt->buf, req->buf, len);

	spin_lock(&dev->lock);
	if (!dev->is_open) {
		pr_err("mbim file handler %p is not open", dev);
		spin_unlock(&dev->lock);
		mbim_free_ctrl_pkt(cpkt);
		return;
	}

	list_add_tail(&cpkt->list, &dev->cpkt_req_q);
	spin_unlock(&dev->lock);

	/* wakeup read thread */
	pr_debug("Wake up read queue");
	wake_up(&dev->read_wq);
}

static int
mbim_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct f_mbim			*mbim = func_to_mbim(f);
	struct usb_composite_dev	*cdev = mbim->cdev;
	struct usb_request		*req = cdev->req;
	struct ctrl_pkt		*cpkt = NULL;
	int	value = -EOPNOTSUPP;
	u16	w_index = le16_to_cpu(ctrl->wIndex);
	u16	w_value = le16_to_cpu(ctrl->wValue);
	u16	w_length = le16_to_cpu(ctrl->wLength);

	/*
	 * composite driver infrastructure handles everything except
	 * CDC class messages; interface activation uses set_alt().
	 */

	if (!atomic_read(&mbim->online)) {
		pr_debug("usb cable is not connected\n");
		return -ENOTCONN;
	}

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_RESET_FUNCTION:

		pr_debug("USB_CDC_RESET_FUNCTION");
		value = 0;
		req->complete = fmbim_reset_cmd_complete;
		req->context = mbim;
		break;

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_SEND_ENCAPSULATED_COMMAND:

		pr_debug("USB_CDC_SEND_ENCAPSULATED_COMMAND");

		if (w_length > req->length) {
			pr_debug("w_length > req->length: %d > %d",
			w_length, req->length);
		}
		value = w_length;
		req->complete = fmbim_cmd_complete;
		req->context = mbim;
		break;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_GET_ENCAPSULATED_RESPONSE:

		pr_debug("USB_CDC_GET_ENCAPSULATED_RESPONSE");

		if (w_value) {
			pr_err("w_length > 0: %d", w_length);
			break;
		}

		pr_debug("req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);

		spin_lock(&mbim->lock);
		if (list_empty(&mbim->cpkt_resp_q)) {
			pr_err("ctrl resp queue empty\n");
			spin_unlock(&mbim->lock);
			break;
		}

		cpkt = list_first_entry(&mbim->cpkt_resp_q,
					struct ctrl_pkt, list);
		list_del(&cpkt->list);
		spin_unlock(&mbim->lock);

		value = min_t(unsigned int, w_length, cpkt->len);
		memcpy(req->buf, cpkt->buf, value);
		mbim_free_ctrl_pkt(cpkt);

		pr_debug("copied encapsulated_response %d bytes",
			value);

		break;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_GET_NTB_PARAMETERS:

		pr_debug("USB_CDC_GET_NTB_PARAMETERS");

		if (w_length == 0 || w_value != 0 || w_index != mbim->ctrl_id)
			break;

		value = w_length > sizeof(ntb_parameters) ?
			sizeof(ntb_parameters) : w_length;
		memcpy(req->buf, &ntb_parameters, value);
		break;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_GET_NTB_INPUT_SIZE:

		pr_debug("USB_CDC_GET_NTB_INPUT_SIZE");

		if (w_length < 4 || w_value != 0 || w_index != mbim->ctrl_id)
			break;

		put_unaligned_le32(mbim->ntb_input_size, req->buf);
		value = 4;
		pr_debug("Reply to host INPUT SIZE %d\n",
		     mbim->ntb_input_size);
		break;

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_SET_NTB_INPUT_SIZE:

		pr_debug("USB_CDC_SET_NTB_INPUT_SIZE");

		if (w_length != 4 && w_length != 8) {
			pr_err("wrong NTB length %d", w_length);
			break;
		}

		if (w_value != 0 || w_index != mbim->ctrl_id)
			break;

		req->complete = mbim_ep0out_complete;
		req->length = w_length;
		req->context = f;

		value = req->length;
		break;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_GET_NTB_FORMAT:
	{
		u16 format;

		pr_debug("USB_CDC_GET_NTB_FORMAT");

		if (w_length < 2 || w_value != 0 || w_index != mbim->ctrl_id)
			break;

		format = (mbim->parser_opts == &ndp16_opts) ? 0x0000 : 0x0001;
		put_unaligned_le16(format, req->buf);
		value = 2;
		pr_debug("NTB FORMAT: sending %d\n", format);
		break;
	}

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_SET_NTB_FORMAT:
	{
		pr_debug("USB_CDC_SET_NTB_FORMAT");

		if (w_length != 0 || w_index != mbim->ctrl_id)
			break;
		switch (w_value) {
		case 0x0000:
			mbim->parser_opts = &ndp16_opts;
			pr_debug("NCM16 selected\n");
			break;
		case 0x0001:
			mbim->parser_opts = &ndp32_opts;
			pr_debug("NCM32 selected\n");
			break;
		default:
			break;
		}
		value = 0;
		break;
	}

	/* optional in mbim descriptor: */
	/* case USB_CDC_GET_MAX_DATAGRAM_SIZE: */
	/* case USB_CDC_SET_MAX_DATAGRAM_SIZE: */

	default:
		pr_err("invalid control req: %02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	 /* respond with data transfer or status phase? */
	if (value >= 0) {
		pr_debug("control request: %02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = (value < w_length);
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0) {
			pr_err("queueing req failed: %02x.%02x, err %d\n",
				ctrl->bRequestType,
			       ctrl->bRequest, value);
		}
	} else {
		pr_err("ctrl req err %d: %02x.%02x v%04x i%04x l%d\n",
			value, ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

static int mbim_set_alt(struct usb_function *f, unsigned int intf,
	 unsigned int alt)
{
	struct f_mbim		*mbim = func_to_mbim(f);
	struct usb_composite_dev *cdev = mbim->cdev;
	int ret = 0;

	/* Control interface has only altsetting 0 */
	if (intf == mbim->ctrl_id) {

		pr_debug("CONTROL_INTERFACE m->ctrl_id:%x\n", mbim->ctrl_id);

		if (alt != 0)
			return -EINVAL;

		if (mbim->not_port.notify->driver_data) {
			pr_debug("reset mbim control %d\n", intf);
			usb_ep_disable(mbim->not_port.notify);
		}

		ret = config_ep_by_speed(cdev->gadget, f,
					mbim->not_port.notify);
		if (ret) {
			mbim->not_port.notify->desc = NULL;
			pr_err("Failed configuring notify ep %s: err %d\n",
				mbim->not_port.notify->name, ret);
			return -EINVAL;
		}

		ret = usb_ep_enable(mbim->not_port.notify);
		if (ret) {
			pr_err("usb ep#%s enable failed, err#%d\n",
				mbim->not_port.notify->name, ret);
			return ret;
		}
		mbim->not_port.notify->driver_data = mbim;

	/* Data interface has two altsettings, 0 and 1 */
	} else if (intf == mbim->data_id) {

		pr_info("DATA_INTERFACE m->data_id:%x\n", mbim->data_id);

		if (alt > 1)
			return -EINVAL;

		if (mbim->mbim_data_port.in->enabled) {
			pr_debug("reset mbim value\n");
			mbim_disconnect(mbim);
			mbim_reset_values(mbim);
		}

		/*
		 * CDC Network only sends data in non-default altsettings.
		 * Changing altsettings resets filters, statistics, etc.
		 */
		if (alt == 1) {
			pr_debug("Alt set 1, initialize ports");

			ret = config_ep_by_speed(cdev->gadget, f,
						mbim->mbim_data_port.in);
			if (ret) {
				mbim->mbim_data_port.in->desc = NULL;
				pr_err("IN ep %s failed: %d\n",
				mbim->mbim_data_port.in->name, ret);
				return ret;
			}

			pr_debug("in_desc = 0x%p, driver_data= 0x%p\n",
				 mbim->mbim_data_port.in->desc, mbim);
			mbim->mbim_data_port.in->driver_data = mbim;
			ret = usb_ep_enable(mbim->mbim_data_port.in);
			if (ret != 0) {
				pr_err("enable %s --> %d\n",
					 mbim->mbim_data_port.in->name, ret);
				return ret;
			}

			ret = config_ep_by_speed(cdev->gadget, f,
						mbim->mbim_data_port.out);
			if (ret) {
				mbim->mbim_data_port.out->desc = NULL;
				pr_err("OUT ep %s failed\n",
				mbim->mbim_data_port.out->name);
				return ret;
			}

			pr_debug("out_desc = 0x%p, driver_data= 0x%p\n",
				 mbim->mbim_data_port.out->desc, mbim);
			mbim->mbim_data_port.out->driver_data = mbim;
			ret = usb_ep_enable(mbim->mbim_data_port.out);
			if (ret != 0) {
				usb_ep_disable(mbim->mbim_data_port.in);
				pr_err("enable %s --> %d\n",
					 mbim->mbim_data_port.out->name, ret);
				return ret;
			}

			ret = alloc_requests(mbim, QMULT_DEFAULT);
			if (ret) {
				usb_ep_disable(mbim->mbim_data_port.out);
				usb_ep_disable(mbim->mbim_data_port.in);
				pr_err("Alloc fail\n");
				return ret;
			}
			rx_fill(mbim, GFP_KERNEL);
		}
		mbim->data_alt_int = alt;
		spin_lock(&mbim->lock);
		mbim_notify(mbim);
		spin_unlock(&mbim->lock);
	} else {
		return -EINVAL;
	}

	pr_info("SET DEVICE ONLINE\n");
	atomic_set(&mbim->online, 1);

	/* wakeup file threads */
	wake_up(&mbim->read_wq);
	wake_up(&mbim->write_wq);

	return 0;
}

/*
 * Because the data interface supports multiple altsettings,
 * this MBIM function *MUST* implement a get_alt() method.
 */
static int mbim_get_alt(struct usb_function *f, unsigned int intf)
{
	struct f_mbim	*mbim = func_to_mbim(f);

	if (intf == mbim->ctrl_id)
		return 0;
	else if (intf == mbim->data_id)
		return mbim->data_alt_int;

	return -EINVAL;
}

static void mbim_disconnect(struct f_mbim *dev)
{
	struct usb_request *req;

	if (!dev)
		return;

	/* disable endpoints, forcing (synchronous) completion
	 * of all pending i/o.  then free the request objects
	 * and forget about the endpoints.
	 */
	usb_ep_disable(dev->mbim_data_port.in);

	spin_lock(&dev->req_lock);
	while (!list_empty(&dev->tx_reqs)) {
		req = container_of(dev->tx_reqs.next, struct usb_request, list);
		list_del(&req->list);

		spin_unlock(&dev->req_lock);
		if (dev->sg_enabled) {
			kfree(req->context);
			kfree(req->buf);
			kfree(req->sg);
		}

		usb_ep_free_request(dev->mbim_data_port.in, req);
		spin_lock(&dev->req_lock);
	}

	spin_unlock(&dev->req_lock);

	dev->mbim_data_port.in->desc = NULL;

	usb_ep_disable(dev->mbim_data_port.out);
	spin_lock(&dev->req_lock);

	while (!list_empty(&dev->rx_reqs)) {
		req = container_of(dev->rx_reqs.next, struct usb_request, list);
		list_del(&req->list);

		spin_unlock(&dev->req_lock);
		usb_ep_free_request(dev->mbim_data_port.out, req);
		spin_lock(&dev->req_lock);
	}

	spin_unlock(&dev->req_lock);
	dev->mbim_data_port.out->desc = NULL;
}

static void mbim_disable(struct usb_function *f)
{
	struct f_mbim	*mbim = func_to_mbim(f);

	pr_info("SET DEVICE OFFLINE\n");
	atomic_set(&mbim->online, 0);

	mbim->not_port.notify_state = NCM_NOTIFY_NONE;

	mbim_clear_queues(mbim);
	mbim_reset_function_queue(mbim);

	mbim_disconnect(mbim);

	if (mbim->not_port.notify->driver_data) {
		usb_ep_disable(mbim->not_port.notify);
		mbim->not_port.notify->driver_data = NULL;
	}

	atomic_set(&mbim->not_port.notify_count, 0);
}

/*---------------------- function driver setup/binding ---------------------*/

static int
mbim_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_mbim		*mbim = func_to_mbim(f);
	int			status;
	struct usb_ep		*ep;


	mbim->cdev = cdev;

	/* allocate instance-specific interface IDs */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	mbim->ctrl_id = status;
	mbim_iad_desc.bFirstInterface = status;

	mbim_control_intf.bInterfaceNumber = status;
	mbim_union_desc.bMasterInterface0 = status;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	mbim->data_id = status;
	mbim->data_alt_int = 0;

	mbim_data_nop_intf.bInterfaceNumber = status;
	mbim_data_intf.bInterfaceNumber = status;
	mbim_union_desc.bSlaveInterface0 = status;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &fs_mbim_in_desc);
	if (!ep) {
		pr_err("usb epin autoconfig failed\n");
		goto fail;
	}
	ep->driver_data = cdev;	/* claim */
	mbim->mbim_data_port.in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &fs_mbim_out_desc);
	if (!ep) {
		pr_err("usb epout autoconfig failed\n");
		goto fail;
	}
	ep->driver_data = cdev;	/* claim */
	mbim->mbim_data_port.out = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &fs_mbim_notify_desc);
	if (!ep) {
		pr_err("usb notify ep autoconfig failed\n");
		goto fail;
	}
	mbim->not_port.notify = ep;
	ep->driver_data = cdev;	/* claim */

	status = -ENOMEM;

	/* allocate notification request and buffer */
	mbim->not_port.notify_req = mbim_alloc_req(ep, NCM_STATUS_BYTECOUNT);
	if (!mbim->not_port.notify_req) {
		pr_info("failed to allocate notify request\n");
		goto fail;
	}

	mbim->not_port.notify_req->context = mbim;
	mbim->not_port.notify_req->complete = mbim_notify_complete;

	/* copy descriptors, and track endpoint copies */
	f->fs_descriptors = usb_copy_descriptors(mbim_fs_function);
	if (!f->fs_descriptors)
		goto fail;

	/*
	 * support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		hs_mbim_in_desc.bEndpointAddress =
				fs_mbim_in_desc.bEndpointAddress;
		hs_mbim_out_desc.bEndpointAddress =
				fs_mbim_out_desc.bEndpointAddress;
		hs_mbim_notify_desc.bEndpointAddress =
				fs_mbim_notify_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->hs_descriptors = usb_copy_descriptors(mbim_hs_function);
		if (!f->hs_descriptors)
			goto fail;
	}

	/*
	 * If MBIM is bound in a config other than the first, tell Windows
	 * about it by returning the num as a string in the OS descriptor's
	 * subCompatibleID field. Windows only supports up to config #4.
	 */
	if (c->bConfigurationValue >= 2 && c->bConfigurationValue <= 4) {
		pr_debug("MBIM in configuration %d", c->bConfigurationValue);
		mbim_ext_config_desc.function.subCompatibleID[0] =
			c->bConfigurationValue + '0';
	}

	pr_info("mbim(%d): %s speed IN/%s OUT/%s NOTIFY/%s\n",
			mbim->port_num,
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
				mbim->mbim_data_port.in->name,
				mbim->mbim_data_port.out->name,
				mbim->not_port.notify->name);

	return 0;

fail:
	pr_err("%s failed to bind, err %d\n", f->name, status);

	if (f->fs_descriptors)
		usb_free_descriptors(f->fs_descriptors);

	if (mbim->not_port.notify_req) {
		kfree(mbim->not_port.notify_req->buf);
		usb_ep_free_request(mbim->not_port.notify,
				    mbim->not_port.notify_req);
	}

	/* we might as well release our claims on endpoints */
	if (mbim->not_port.notify)
		mbim->not_port.notify->driver_data = NULL;
	if (mbim->mbim_data_port.out)
		mbim->mbim_data_port.out->driver_data = NULL;
	if (mbim->mbim_data_port.in)
		mbim->mbim_data_port.in->driver_data = NULL;

	return status;
}

static void mbim_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_mbim	*mbim = func_to_mbim(f);

	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);
	usb_free_descriptors(f->fs_descriptors);

	fmbim_free_req(mbim->not_port.notify, mbim->not_port.notify_req);

	mbim_ext_config_desc.function.subCompatibleID[0] = 0;
}

static struct mbim_instance *to_mbim_instance(struct config_item *item)
{
	return container_of(to_config_group(item),
		 struct mbim_instance, func_inst.group);
}

static void mbim_attr_release(struct config_item *item)
{
	struct mbim_instance *fi_mbim = to_mbim_instance(item);

	usb_put_function_instance(&fi_mbim->func_inst);
}

static struct configfs_item_operations mbim_item_ops = {
	.release = mbim_attr_release,
};

static struct config_item_type mbim_func_type = {
	.ct_item_ops = &mbim_item_ops,
	.ct_owner = THIS_MODULE,
};

static struct mbim_instance *to_fi_mbim(struct usb_function_instance *fi)
{
	return container_of(fi, struct mbim_instance, func_inst);
}

static void mbim_free_inst(struct usb_function_instance *fi)
{
	struct mbim_instance *fi_mbim = to_fi_mbim(fi);

	kfree(fi_mbim->name);
	kfree(fi_mbim);

	destroy_workqueue(mbim_tx_wq);
	destroy_workqueue(mbim_rx_wq);

	fmbim_cleanup();
}
static struct usb_function_instance *mbim_alloc_inst(void)
{
	struct mbim_instance *fi_mbim;
	int ret = 0;

	fi_mbim = kzalloc(sizeof(*fi_mbim), GFP_KERNEL);
	if (!fi_mbim)
		return ERR_PTR(-ENOMEM);

	fi_mbim->func_inst.free_func_inst = mbim_free_inst;

	ret = mbim_init(1);
	if (ret) {
		kfree(fi_mbim);
		return ERR_PTR(ret);
	}
	config_group_init_type_name(&fi_mbim->func_inst.group,
			 "", &mbim_func_type);
	pr_info("name:%s\n", fi_mbim->name);
	mbim_rx_wq  = create_singlethread_workqueue("mbim_rx");
	if (!mbim_rx_wq) {
		pr_err("%s: Unable to create workqueue: mbim_rx\n", __func__);
		kfree(fi_mbim);
		return ERR_PTR(-ENOMEM);
	}
	mbim_tx_wq = alloc_workqueue("mbim_tx",
		 WQ_CPU_INTENSIVE | WQ_UNBOUND, 1);
	if (!mbim_tx_wq) {
		kfree(fi_mbim);
		destroy_workqueue(mbim_rx_wq);
		pr_err("%s: Unable to create workqueue: mbim_tx\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	return &fi_mbim->func_inst;
}

static struct usb_function *mbim_alloc(struct usb_function_instance *fi)
{
	struct f_mbim *mbim;

	/* allocate and initialize one new instance */
	mbim = mbim_ports[0].port;
	if (!mbim) {
		pr_info("mbim struct not allocated");
		return NULL;
	}

	mbim_reset_values(mbim);

	mbim->function.name = "usb_mbim";
	mbim->function.strings = mbim_strings;
	mbim->function.bind = mbim_bind;
	mbim->function.unbind = mbim_unbind;
	mbim->function.set_alt = mbim_set_alt;
	mbim->function.get_alt = mbim_get_alt;
	mbim->function.setup = mbim_setup;
	mbim->function.disable = mbim_disable;

	INIT_LIST_HEAD(&mbim->cpkt_req_q);
	INIT_LIST_HEAD(&mbim->cpkt_resp_q);
	INIT_LIST_HEAD(&mbim->tx_data_raw);
	INIT_LIST_HEAD(&mbim->rx_data_raw);

	INIT_WORK(&mbim->work, defer_work);
	INIT_WORK(&mbim->rx_work, process_rx_w);
	INIT_WORK(&mbim->tx_work, process_tx_w);
	INIT_LIST_HEAD(&mbim->tx_reqs);
	INIT_LIST_HEAD(&mbim->rx_reqs);

	return &mbim->function;
}

DECLARE_USB_FUNCTION_INIT(mbim, mbim_alloc_inst, mbim_alloc);

/* ------------ MBIM DRIVER File Operations API for USER SPACE ------------ */

static ssize_t
mbim_read(struct file *fp, char __user *buf, size_t count, loff_t *pos)
{
	struct f_mbim *dev = fp->private_data;
	struct ctrl_pkt *cpkt = NULL;
	int ret;

	pr_debug("Enter(%zu)\n", count);

	if (!dev) {
		pr_err("Received NULL mbim pointer\n");
		return -ENODEV;
	}

	if (count > MBIM_BULK_BUFFER_SIZE) {
		pr_err("Buffer size is too big %zu, should be at most %d\n",
			count, MBIM_BULK_BUFFER_SIZE);
		return -EINVAL;
	}

	if (mbim_lock(&dev->read_excl)) {
		pr_err("Previous reading is not finished yet\n");
		return -EBUSY;
	}

	/* block until mbim online */
	while (!(atomic_read(&dev->online) || atomic_read(&dev->error))) {
		pr_err("USB cable not connected. Wait.\n");
		ret = wait_event_interruptible(dev->read_wq,
			(atomic_read(&dev->online) ||
			atomic_read(&dev->error)));
		if (ret < 0) {
			mbim_unlock(&dev->read_excl);
			return -ERESTARTSYS;
		}
	}

	if (atomic_read(&dev->error)) {
		mbim_unlock(&dev->read_excl);
		return -EIO;
	}

	while (list_empty(&dev->cpkt_req_q)) {
		pr_debug("Requests list is empty. Wait.\n");
		ret = wait_event_interruptible(dev->read_wq,
			!list_empty(&dev->cpkt_req_q));
		if (ret < 0) {
			pr_err("Waiting failed\n");
			mbim_unlock(&dev->read_excl);
			return -ERESTARTSYS;
		}
		pr_debug("Received request packet\n");
	}

	cpkt = list_first_entry(&dev->cpkt_req_q, struct ctrl_pkt,
							list);
	if (cpkt->len > count) {
		mbim_unlock(&dev->read_excl);
		pr_err("cpkt size too big:0x%x > buf size:%zu\n",
				cpkt->len, count);
		return -EINVAL;
	}

	pr_debug("cpkt size:%d\n", cpkt->len);

	list_del(&cpkt->list);
	mbim_unlock(&dev->read_excl);

	ret = copy_to_user(buf, cpkt->buf, cpkt->len);
	if (ret) {
		pr_err("copy_to_user failed: err %d\n", ret);
		ret = -EFAULT;
	} else {
		pr_debug("copied %d bytes to user\n", cpkt->len);
		ret = cpkt->len;
	}

	mbim_free_ctrl_pkt(cpkt);

	return ret;
}

#define MAX_CTRL_PKT_SIZE   4096

static ssize_t
mbim_write(struct file *fp, const char __user *buf, size_t count, loff_t *pos)
{
	struct f_mbim *dev = fp->private_data;
	struct ctrl_pkt *cpkt = NULL;
	int ret = 0;

	pr_debug("Enter(%zu)\n", count);

	if (!dev) {
		pr_err("Received NULL mbim pointer\n");
		return -ENODEV;
	}

	if (!count) {
		pr_err("zero length ctrl pkt\n");
		return -EINVAL;
	}

	if (count > MAX_CTRL_PKT_SIZE) {
		pr_err("given pkt size too big:%zu > max_pkt_size:0x%x\n",
				count, MAX_CTRL_PKT_SIZE);
		return -EINVAL;
	}

	if (mbim_lock(&dev->write_excl)) {
		pr_err("Previous writing not finished yet\n");
		return -EBUSY;
	}

	if (!atomic_read(&dev->online)) {
		pr_err("USB cable not connected\n");
		mbim_unlock(&dev->write_excl);
		return -EPIPE;
	}

	cpkt = mbim_alloc_ctrl_pkt(count, GFP_KERNEL);
	if (!cpkt) {
		mbim_unlock(&dev->write_excl);
		return -ENOMEM;
	}

	ret = copy_from_user(cpkt->buf, buf, count);
	if (ret) {
		pr_err("copy_from_user failed err:%d\n", ret);
		mbim_free_ctrl_pkt(cpkt);
		mbim_unlock(&dev->write_excl);
		return -EFAULT;
	}

	fmbim_send_cpkt_response(dev, cpkt);

	mbim_unlock(&dev->write_excl);

	pr_debug("Exit(%zu)", count);

	return count;
}

static int mbim_open(struct inode *ip, struct file *fp)
{

	if (!_mbim_dev) {
		pr_err("mbim_dev not created yet\n");
		return -ENODEV;
	}

	if (mbim_lock(&_mbim_dev->open_excl)) {
		pr_err("Already opened\n");
		return -EBUSY;
	}


	if (!atomic_read(&_mbim_dev->online))
		pr_warn("USB cable not connected\n");

	fp->private_data = _mbim_dev;

	atomic_set(&_mbim_dev->error, 0);

	spin_lock(&_mbim_dev->lock);
	_mbim_dev->is_open = true;
	spin_unlock(&_mbim_dev->lock);

	pr_info("mbim device opened\n");

	return 0;
}

static int mbim_release(struct inode *ip, struct file *fp)
{
	struct f_mbim *mbim = fp->private_data;

	pr_info("Close mbim file");

	spin_lock(&mbim->lock);
	mbim->is_open = false;
	spin_unlock(&mbim->lock);

	mbim_unlock(&_mbim_dev->open_excl);

	return 0;
}

static long mbim_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	struct f_mbim *mbim = fp->private_data;
	int ret = 0;

	pr_debug("Received command %d", cmd);

	if (mbim_lock(&mbim->ioctl_excl))
		return -EBUSY;

	switch (cmd) {
	case MBIM_GET_NTB_SIZE:
		ret = copy_to_user((void __user *)arg,
			&mbim->ntb_input_size, sizeof(mbim->ntb_input_size));
		if (ret) {
			pr_err("copying to user space failed");
			ret = -EFAULT;
		}
		pr_info("Sent NTB size %d", mbim->ntb_input_size);
		break;
	case MBIM_GET_DATAGRAM_COUNT:
		ret = copy_to_user((void __user *)arg,
			&mbim->ntb_max_datagrams,
			sizeof(mbim->ntb_max_datagrams));
		if (ret) {
			pr_err("copying to user space failed");
			ret = -EFAULT;
		}
		pr_info("Sent NTB datagrams count %d",
			mbim->ntb_max_datagrams);
		break;
	default:
		pr_err("wrong parameter");
		ret = -EINVAL;
	}

	mbim_unlock(&mbim->ioctl_excl);

	return ret;
}

/* file operations for MBIM device /dev/sprd_mbim */
static const struct file_operations mbim_fops = {
	.owner = THIS_MODULE,
	.open = mbim_open,
	.release = mbim_release,
	.read = mbim_read,
	.write = mbim_write,
	.unlocked_ioctl	= mbim_ioctl,
};

static struct miscdevice mbim_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sprd_mbim",
	.fops = &mbim_fops,
};

static ssize_t mbim_debug_show(struct device *dev,
		 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", "[Mbim]show OK");
}

static ssize_t mbim_debug_store(struct device *dev,
	 struct device_attribute *attr, const char *buf, size_t count)
{
	int re;

	re = simple_strtoull(buf, NULL, 16);
	pr_debug("Enter:re = %d\n", re);
	return count;
}

static DEVICE_ATTR_RW(mbim_debug);

static ssize_t mbim_statistics_show(struct device *dev,
	 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "u32 in_discards= %u;\nu32 in_errors= %u;\n"
	"u64 in_octets= %llu;\nu64 in_packets= %llu;\nu64 out_octets= %llu;\n"
	"u64 out_packets= %llu;\nu32 out_errors= %u;\nu32 out_discards= %u;\n",
	_mbim_dev->stats.in_discards,
	_mbim_dev->stats.in_errors,
	_mbim_dev->stats.in_octets,
	_mbim_dev->stats.in_packets,
	_mbim_dev->stats.out_octets,
	_mbim_dev->stats.out_packets,
	_mbim_dev->stats.out_errors,
	_mbim_dev->stats.out_discards);
}

static DEVICE_ATTR_RO(mbim_statistics);

static int mbim_init(int instances)
{
	int i;
	struct f_mbim *dev = NULL;
	int ret;


	if (instances > NR_MBIM_PORTS) {
		pr_err("Max-%d instances supported\n", NR_MBIM_PORTS);
		return -EINVAL;
	}

	for (i = 0; i < instances; i++) {
		dev = kzalloc(sizeof(struct f_mbim), GFP_KERNEL);
		if (!dev) {
			ret = -ENOMEM;
			goto fail_probe;
		}

		dev->port_num = i;
		dev->sg_enabled = true;
		dev->fix_header = false;
		spin_lock_init(&dev->lock);

		mbim_ports[i].port = dev;
		mbim_ports[i].port_num = i;

		init_waitqueue_head(&dev->read_wq);
		init_waitqueue_head(&dev->write_wq);

		atomic_set(&dev->open_excl, 0);
		atomic_set(&dev->ioctl_excl, 0);
		atomic_set(&dev->read_excl, 0);
		atomic_set(&dev->write_excl, 0);

		nr_mbim_ports++;
	}

	_mbim_dev = dev;
	ret = misc_register(&mbim_device);
	if (ret) {
		pr_err("mbim driver failed to register");
		goto fail_probe;
	}

	device_create_file(mbim_device.this_device, &dev_attr_mbim_debug);
	device_create_file(mbim_device.this_device, &dev_attr_mbim_statistics);
	pr_info("Initialized %d ports\n", nr_mbim_ports);

	return ret;

fail_probe:
	pr_err("Failed");
	for (i = 0; i < nr_mbim_ports; i++) {
		kfree(mbim_ports[i].port);
		mbim_ports[i].port = NULL;
	}

	return ret;
}

static void fmbim_cleanup(void)
{
	int i = 0;

	device_remove_file(mbim_device.this_device, &dev_attr_mbim_debug);
	device_remove_file(mbim_device.this_device, &dev_attr_mbim_statistics);

	for (i = 0; i < nr_mbim_ports; i++) {
		kfree(mbim_ports[i].port);
		mbim_ports[i].port = NULL;
	}
	nr_mbim_ports = 0;

	misc_deregister(&mbim_device);

	_mbim_dev = NULL;
}

