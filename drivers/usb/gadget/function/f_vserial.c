/*
 * Vendor specific serial Gadget Driver use for put modem log.
 * From the Gadget side, it is a misc device(No TTY),
 * from PC side, sprd's usb2serial driver will enumerate the
 * device as a serial device.
 * This driver basicly from android adb driver, many thanks to
 * Android guys.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/configfs.h>
#include <linux/delay.h>
#include <linux/device.h>
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
#include <linux/usb/composite.h>

#define VSER_BULK_BUFFER_SIZE	(4096 * 4)
#define MAX_INST_NAME_LEN	40
/* number of tx requests to allocate */
#define TX_REQ_MAX		4
/* Device --> Host */
#define SET_IN_SIZE		0x00
/* Host --> Device */
#define SET_OUT_SIZE		0x01
#define START_TEST		0x10
#define STOP_TEST		0x20
#define UPLINK_TEST		0x01
#define DOWNLINK_TEST		0x02
#define LOOP_TEST		0x03

#ifdef CONFIG_SPRD_IQ
extern int in_iqmode(void);
void (*bulk_in_complete_function)(char *buffer, int length) = NULL;
#endif

static const char vser_shortname[] = "vser";
static int tx_req_count;

struct vser_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;

	int online;
	int rd_error;
	int wr_error;

	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;
	int open_count;

	struct list_head tx_idle;

#ifdef CONFIG_SPRD_IQ
	struct list_head tx_iq_idle;
#endif

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *rx_req;
	int rx_done;
	int test_mode;
	int size_in;
	int size_out;
	int total_size;
	struct work_struct test_work;
	struct workqueue_struct *test_wq;
};

struct vser_instance {
	struct usb_function_instance func_inst;
	const char *name;
	struct vser_dev *dev;
};

static struct vser_dev *_vser_dev;

/* interface descriptor */
static struct usb_interface_descriptor vser_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 0,
	.bNumEndpoints          = 2,
	.bInterfaceClass        = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass     = 0,
	.bInterfaceProtocol     = 0,
};

/* super speed support */
static struct usb_endpoint_descriptor vser_ss_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor vser_ss_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor vser_ss_bulk_comp_desc = {
	.bLength =              sizeof(vser_ss_bulk_comp_desc),
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,
};

static struct usb_descriptor_header *vser_ss_function[] = {
	(struct usb_descriptor_header *) &vser_interface_desc,
	(struct usb_descriptor_header *) &vser_ss_in_desc,
	(struct usb_descriptor_header *) &vser_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &vser_ss_out_desc,
	(struct usb_descriptor_header *) &vser_ss_bulk_comp_desc,
	NULL,
};

/* high speed support */
static struct usb_endpoint_descriptor vser_hs_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = cpu_to_le16(512),
};

static struct usb_endpoint_descriptor vser_hs_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = cpu_to_le16(512),
};

static struct usb_descriptor_header *vser_hs_function[] = {
	(struct usb_descriptor_header *) &vser_interface_desc,
	(struct usb_descriptor_header *) &vser_hs_in_desc,
	(struct usb_descriptor_header *) &vser_hs_out_desc,
	NULL,
};

/* full speed support */
static struct usb_endpoint_descriptor vser_fs_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor vser_fs_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *vser_fs_function[] = {
	(struct usb_descriptor_header *) &vser_interface_desc,
	(struct usb_descriptor_header *) &vser_fs_in_desc,
	(struct usb_descriptor_header *) &vser_fs_out_desc,
	NULL,
};

static inline struct vser_dev *func_to_vser(struct usb_function *f)
{
	return container_of(f, struct vser_dev, function);
}

static struct usb_request *vser_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);

	if (!req)
		return NULL;

	/* now allocate buffers for the request */
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void vser_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static inline int vser_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1)
		return 0;

	atomic_dec(excl);
	return -1;
}

static inline void vser_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

/* add a request to the tail of a list */
static void vser_req_put(struct vser_dev *dev, struct list_head *head,
			 struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	tx_req_count++;
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
static struct usb_request *vser_req_get(struct vser_dev *dev,
					struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
		tx_req_count--;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	return req;
}

static void vser_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct vser_dev *dev = _vser_dev;

	if (req->status != 0)
		dev->wr_error = 1;

	vser_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

#ifdef CONFIG_SPRD_IQ
static void vser_iq_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct vser_dev *dev = _vser_dev;

	if (req->status != 0)
		dev->wr_error = 1;

	vser_req_put(dev, &dev->tx_iq_idle, req);

	if (bulk_in_complete_function != NULL)
		bulk_in_complete_function(req->buf, req->length);

	wake_up(&dev->write_wq);
}
#endif

static void vser_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct vser_dev *dev = _vser_dev;

	dev->rx_done = 1;
	if (req->status != 0)
		dev->rd_error = 1;

	wake_up(&dev->read_wq);
}

static void vser_free_all_request(struct vser_dev *dev)
{
	struct usb_request *req;

	vser_request_free(dev->rx_req, dev->ep_out);
	while ((req = vser_req_get(dev, &dev->tx_idle)))
		vser_request_free(req, dev->ep_in);
}

static int vser_alloc_all_request(struct vser_dev *dev)
{
	struct usb_request *req;
	int i;

	req = vser_request_new(dev->ep_out, dev->size_out);
	if (!req)
		return -ENOMEM;

	req->complete = vser_complete_out;
	dev->rx_req = req;

	for (i = 0; i < TX_REQ_MAX; i++) {
		req = vser_request_new(dev->ep_in, dev->size_in);
		if (!req) {
			vser_free_all_request(dev);
			return -ENOMEM;
		}

		req->complete = vser_complete_in;
		vser_req_put(dev, &dev->tx_idle, req);
	}

	tx_req_count = TX_REQ_MAX;
	return 0;
}

static int vser_create_bulk_endpoints(struct vser_dev *dev,
				      struct usb_endpoint_descriptor *in_desc,
				      struct usb_endpoint_descriptor *out_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_ep *ep;
	int ret = 0;

	DBG(cdev, "vser_create_bulk_endpoints dev: %p\n", dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_in failed\n");
		return -ENODEV;
	}

	DBG(cdev, "usb_ep_autoconfig for ep_in got %s\n", ep->name);
	ep->driver_data = dev;
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_out failed\n");
		return -ENODEV;
	}

	DBG(cdev, "usb_ep_autoconfig for vser ep_out got %s\n", ep->name);
	ep->driver_data = dev;
	dev->ep_out = ep;

	/* now allocate requests for our endpoints */
	ret = vser_alloc_all_request(dev);
	if (ret)
		goto fail;

#ifdef CONFIG_SPRD_IQ
	if (in_iqmode()) {
		int i;

		for (i = 0; i < TX_REQ_MAX; i++) {
			struct usb_request *req = NULL;

			req = usb_ep_alloc_request(dev->ep_in, GFP_KERNEL);
			if (!req) {
				vser_free_all_request(dev);
				ret = -ENOMEM;
				goto fail;
			}

			req->buf = NULL;
			req->complete = vser_iq_complete_in;
			vser_req_put(dev, &dev->tx_iq_idle, req);
		}
	}
#endif

	return 0;

fail:
	DBG(cdev, "Allocate requests failed.\n");
	return ret;
}

static ssize_t vser_read(struct file *fp, char __user *buf,
			 size_t count, loff_t *pos)
{
	struct vser_dev *dev = fp->private_data;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	int r = count, xfer, ret;

	DBG(cdev, "vser read %d.\n", r);

	if (dev->test_mode)
		return -EINVAL;

	if (count > dev->size_out)
		return -EINVAL;

	if (vser_lock(&dev->read_excl))
		return -EBUSY;

	/* we will block until we're online */
	while (!(dev->online || dev->rd_error)) {
		DBG(cdev, "vser_read: waiting for online state\n");
		ret = wait_event_interruptible(dev->read_wq,
				(dev->online || dev->rd_error));
		if (ret < 0) {
			vser_unlock(&dev->read_excl);
			return ret;
		}
	}

	if (dev->rd_error) {
		r = -EIO;
		goto done;
	}

requeue_req:
	/* queue a request */
	req = dev->rx_req;
	/* Make bulk-out requests be divisible by the maxpacket size */
	if (dev->cdev->gadget->quirk_ep_out_aligned_size)
		req->length = ALIGN(count, dev->ep_out->maxpacket);
	else
		req->length = count;

	dev->rx_done = 0;
	ret = usb_ep_queue(dev->ep_out, req, GFP_ATOMIC);
	if (ret < 0) {
		DBG(cdev, "vser_read: failed to queue req %p (%d)\n", req, ret);
		r = -EIO;
		dev->rd_error = 1;
		goto done;
	} else {
		DBG(cdev, "rx %p queue\n", req);
	}

	/* wait for a request to complete */
	ret = wait_event_interruptible(dev->read_wq, dev->rx_done);
	if (ret < 0) {
		usb_ep_dequeue(dev->ep_out, req);
		dev->rd_error = 1;
		r = ret;
		goto done;
	}

	if (!dev->rd_error) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (req->actual == 0)
			goto requeue_req;

		DBG(cdev, "rx %p %d\n", req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;
		r = xfer;
		if (copy_to_user(buf, req->buf, xfer))
			r = -EFAULT;
	} else {
		r = -EIO;
	}

done:
	vser_unlock(&dev->read_excl);
	DBG(cdev, "vser read returning %d\n", r);
	return r;
}

static ssize_t vser_write(struct file *fp, const char __user *buf,
			  size_t count, loff_t *pos)
{
	struct vser_dev *dev = fp->private_data;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req = 0;
	int r = count, xfer, ret;

#ifdef CONFIG_SPRD_IQ
	if (in_iqmode()) {
		msleep(100);
		return count;
	}
#endif

	if (dev->test_mode)
		return -EBUSY;

	if (vser_lock(&dev->write_excl))
		return -EBUSY;

	DBG(cdev, "vser write = %d.\n", r);

	/* we will block until we're online */
	while (!(dev->online || dev->wr_error)) {
		DBG(cdev, "vser_write: waiting for online state\n");
		ret = wait_event_interruptible(dev->write_wq,
				(dev->online || dev->wr_error));
		if (ret < 0) {
			vser_unlock(&dev->write_excl);
			return ret;
		}
	}

	while (count > 0) {
		if (dev->wr_error) {
			DBG(cdev, "vser_write dev->wr_error\n");
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;

		if (list_empty(&dev->tx_idle))
			DBG(cdev, "%s: tx buffer is full!\n", __func__);

		ret = wait_event_interruptible(dev->write_wq,
			((req = vser_req_get(dev, &dev->tx_idle)) ||
			 dev->wr_error));
		if (ret < 0) {
			r = ret;
			break;
		}

		if (req != 0) {
			if (count > dev->size_in)
				xfer = dev->size_in;
			else
				xfer = count;

			if (copy_from_user(req->buf, buf, xfer)) {
				r = -EFAULT;
				break;
			}

			req->length = xfer;
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				DBG(cdev, "vser_write: xfer error %d\n", ret);
				dev->wr_error = 1;
				r = -EIO;
				break;
			}

			buf += xfer;
			count -= xfer;

			/* zero this so we don't try to free it on error exit */
			req = 0;
		}
	}

	if (req)
		vser_req_put(dev, &dev->tx_idle, req);

	vser_unlock(&dev->write_excl);
	DBG(cdev, "vser write returning %d\n", r);

	return r;
}

static int vser_open(struct inode *ip, struct file *fp)
{
	struct vser_dev *dev = _vser_dev;
	struct usb_composite_dev *cdev = dev->cdev;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	fp->private_data = dev;

	dev->open_count++;
	if (dev->open_count > 2) {
		dev->open_count--;
		DBG(cdev, "too many user open vser.\n");
		spin_unlock_irqrestore(&dev->lock, flags);
		return -EBUSY;
	}

	/* clear the error latch */
	dev->wr_error = 0;
	dev->rd_error = 0;
	spin_unlock_irqrestore(&dev->lock, flags);
	DBG(cdev, "%s: open %d times.\n", __func__, dev->open_count);

	return 0;
}

static int vser_release(struct inode *ip, struct file *fp)
{
	struct vser_dev *dev = _vser_dev;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	dev->open_count--;
	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}

/* file operations for ADB device /dev/android_vser */
static const struct file_operations vser_fops = {
	.owner = THIS_MODULE,
	.read = vser_read,
	.write = vser_write,
	.open = vser_open,
	.release = vser_release,
};

static struct miscdevice vser_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = vser_shortname,
	.fops = &vser_fops,
};

static void vser_test_works(struct work_struct *work);
static int vser_init(struct vser_instance *fi_vser)
{
	struct vser_dev *dev;
	int ret = 0;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	_vser_dev = dev;
	if (fi_vser != NULL)
		fi_vser->dev = dev;

	spin_lock_init(&dev->lock);

	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);

	atomic_set(&dev->open_excl, 0);
	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev->write_excl, 0);
	dev->open_count = 0;
	dev->size_in = VSER_BULK_BUFFER_SIZE;
	dev->size_out = VSER_BULK_BUFFER_SIZE;
	tx_req_count = 0;

	INIT_LIST_HEAD(&dev->tx_idle);
#ifdef CONFIG_SPRD_IQ
	INIT_LIST_HEAD(&dev->tx_iq_idle);
#endif

	INIT_WORK(&dev->test_work, vser_test_works);
	dev->test_wq = create_singlethread_workqueue("VserTestWq");
	if (!dev->test_wq) {
		ret = -ENOMEM;
		goto err1;
	}

	ret = misc_register(&vser_device);
	if (ret)
		goto err2;

	return 0;

err2:
	destroy_workqueue(dev->test_wq);
err1:
	kfree(dev);
	return ret;
}

static void vser_cleanup(void)
{
	misc_deregister(&vser_device);

	kfree(_vser_dev);
	_vser_dev = NULL;
}

static int vser_function_bind(struct usb_configuration *c,
			      struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct vser_dev	*dev = func_to_vser(f);
	int id, ret;

	dev->cdev = cdev;

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;

	vser_interface_desc.bInterfaceNumber = id;

	/* allocate endpoints */
	ret = vser_create_bulk_endpoints(dev, &vser_fs_in_desc,
					 &vser_fs_out_desc);
	if (ret)
		return ret;

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	vser_hs_in_desc.bEndpointAddress = vser_fs_in_desc.bEndpointAddress;
	vser_hs_out_desc.bEndpointAddress = vser_fs_out_desc.bEndpointAddress;
	vser_ss_in_desc.bEndpointAddress = vser_fs_in_desc.bEndpointAddress;
	vser_ss_out_desc.bEndpointAddress = vser_fs_out_desc.bEndpointAddress;

	DBG(cdev, "%s speed %s IN/%s OUT/%s\n",
	    f->name,
	    gadget_is_superspeed(c->cdev->gadget) ? "super" :
	    gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
	    dev->ep_in->name, dev->ep_out->name);

	return 0;
}

static void vser_function_unbind(struct usb_configuration *c,
				 struct usb_function *f)
{
	struct vser_dev	*dev = func_to_vser(f);

	vser_free_all_request(dev);

#ifdef CONFIG_SPRD_IQ
	if (in_iqmode()) {
		struct usb_request *req;

		while ((req = vser_req_get(dev, &dev->tx_iq_idle)))
			usb_ep_free_request(dev->ep_in, req);
	}
#endif

	dev->online = 0;
	dev->wr_error = 1;
	dev->rd_error = 1;
}

static int vser_function_set_alt(struct usb_function *f,
				 unsigned intf,
				 unsigned alt)
{
	struct vser_dev	*dev = func_to_vser(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	DBG(cdev, "%s: intf: %d alt: %d\n", __func__, intf, alt);

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_in);
	if (ret)
		return ret;

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_out);
	if (ret) {
		usb_ep_disable(dev->ep_in);
		return ret;
	}

	return 0;
}

static void vser_function_disable(struct usb_function *f)
{
	struct vser_dev	*dev = func_to_vser(f);
	struct usb_composite_dev *cdev = dev->cdev;

	dev->online = 0;
	dev->wr_error = 1;
	dev->rd_error = 1;
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	wake_up(&dev->write_wq);

	VDBG(cdev, "%s disabled\n", dev->function.name);
}

static int vser_setup(struct usb_function *f,
		      const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	struct vser_dev	*dev = _vser_dev;
	u16 w_length = le16_to_cpu(ctrl->wLength);
	u16 w_value = le16_to_cpu(ctrl->wValue);
	int value = -EOPNOTSUPP;
	bool port_open = false;

	VDBG(cdev,
	     "request :%x request type: %x, wValue %x, wIndex %x, wLength %x\n",
	     ctrl->bRequest, ctrl->bRequestType, w_value,
	     le16_to_cpu(ctrl->wIndex), w_length);

	/* Handle Bulk-only class-specific requests */
	if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_CLASS) {
		switch (ctrl->bRequest) {
		case 0x22:
			value = 0;
			if ((w_value & 0xff) == 1)
				port_open = true;
			break;
		}
	} else if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_VENDOR) {
		u32 parameter;

		value = 0;
		parameter = (ctrl->wValue << 16) | ctrl->wIndex;
		switch (ctrl->bRequest) {
		case SET_IN_SIZE:
			dev->size_in = parameter;
			dev->test_mode |= 1;
			break;
		case SET_OUT_SIZE:
			dev->size_out = parameter;
			dev->test_mode |= 2;
			break;
		case START_TEST:
			vser_free_all_request(dev);
			if (vser_alloc_all_request(dev) < 0)
				VDBG(cdev, "Vser allocate memory failed!\n");
			dev->test_mode |= 0x80;
			dev->total_size = parameter;
			VDBG(cdev, "total size = 0x%x\n", parameter);

			queue_work(dev->test_wq, &dev->test_work);
			break;
		case STOP_TEST:
			vser_free_all_request(dev);
			break;
		default:
			VDBG(cdev, "wrong request.\n");
			break;
		}
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		int rc;

		cdev->req->zero = value < w_length;
		cdev->req->length = value;
		rc = usb_ep_queue(cdev->gadget->ep0, cdev->req, GFP_ATOMIC);
		if (rc < 0)
			VDBG(cdev, "%s setup response queue error\n", __func__);
	}

	if (value == -EOPNOTSUPP) {
		VDBG(cdev,
		     "unknown class-specific control req "
		     "%02x.%02x v%04x i%04x l%u\n",
		     ctrl->bRequestType, ctrl->bRequest,
		     le16_to_cpu(ctrl->wValue), le16_to_cpu(ctrl->wIndex),
		     le16_to_cpu(ctrl->wLength));
	}

	if (port_open) {
		DBG(cdev, "cmd for port open\n");
		dev->online = 1;
		wake_up(&dev->read_wq);
		wake_up(&dev->write_wq);
	}
	return value;
}

static void vser_free(struct usb_function *f)
{
	/* No need to do */
}

static struct vser_instance *to_vser_instance(struct config_item *item)
{
	return container_of(to_config_group(item), struct vser_instance,
		func_inst.group);
}

static void vser_attr_release(struct config_item *item)
{
	struct vser_instance *fi_vser = to_vser_instance(item);

	usb_put_function_instance(&fi_vser->func_inst);
}

static struct configfs_item_operations vser_item_ops = {
	.release        = vser_attr_release,
};

static struct config_item_type vser_func_type = {
	.ct_item_ops    = &vser_item_ops,
	.ct_owner       = THIS_MODULE,
};

static struct vser_instance *to_fi_vser(struct usb_function_instance *fi)
{
	return container_of(fi, struct vser_instance, func_inst);
}

static int vser_set_inst_name(struct usb_function_instance *fi,
			      const char *name)
{
	struct vser_instance *fi_vser = to_fi_vser(fi);
	char *ptr;
	int name_len;

	name_len = strlen(name) + 1;
	if (name_len > MAX_INST_NAME_LEN)
		return -ENAMETOOLONG;

	ptr = kstrndup(name, name_len, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	fi_vser->name = ptr;
	return 0;
}

static void vser_free_inst(struct usb_function_instance *fi)
{
	struct vser_instance *fi_vser = to_fi_vser(fi);

	kfree(fi_vser->name);
	kfree(fi_vser);

	vser_cleanup();
}

static struct usb_function_instance *vser_alloc_inst(void)
{
	struct vser_instance *fi_vser;
	int ret;

	fi_vser = kzalloc(sizeof(*fi_vser), GFP_KERNEL);
	if (!fi_vser)
		return ERR_PTR(-ENOMEM);

	fi_vser->func_inst.set_inst_name = vser_set_inst_name;
	fi_vser->func_inst.free_func_inst = vser_free_inst;

	ret = vser_init(fi_vser);
	if (ret) {
		kfree(fi_vser);
		return ERR_PTR(ret);
	}

	config_group_init_type_name(&fi_vser->func_inst.group,
				    "", &vser_func_type);

	return &fi_vser->func_inst;
}

static struct usb_function *vser_alloc(struct usb_function_instance *fi)
{
	struct vser_instance *fi_vser = to_fi_vser(fi);
	struct vser_dev *dev;

	if (!fi_vser->dev) {
		pr_err("Error: Create vser function failed.\n");
		return NULL;
	}

	dev = fi_vser->dev;
	dev->function.name = "vser";
	dev->function.fs_descriptors = vser_fs_function;
	dev->function.hs_descriptors = vser_hs_function;
	dev->function.ss_descriptors = vser_ss_function;
	dev->function.bind = vser_function_bind;
	dev->function.unbind = vser_function_unbind;
	dev->function.set_alt = vser_function_set_alt;
	dev->function.setup = vser_setup;
	dev->function.disable = vser_function_disable;
	dev->function.free_func = vser_free;

	return &dev->function;
}

DECLARE_USB_FUNCTION_INIT(vser, vser_alloc_inst, vser_alloc);

#ifdef CONFIG_SPRD_IQ
void kernel_vser_register_callback(void *function)
{
	bulk_in_complete_function = function;
}

ssize_t vser_iq_write(char *buf, size_t count)
{
	struct vser_dev *dev = _vser_dev;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	int r = count, xfer, ret;

	DBG(cdev, "%s: buf:0x%p, count:0x%llx, epname:%s\n",
		__func__, buf, (long long)count, dev->ep_in->name);

	if (vser_lock(&dev->write_excl))
		return -EBUSY;

	while (count > 0) {
		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
			(req = vser_req_get(dev, &dev->tx_iq_idle)));

		if (ret < 0) {
			r = ret;
			DBG(cdev, "%s:line %d wait event failed.\n",
				__func__, __LINE__);
			break;
		}

		xfer = count;
		if (req != 0) {
			req->buf = buf;
			req->length = xfer;
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				DBG(cdev, "%s: xfer error %d\n", __func__, ret);
				dev->wr_error = 1;
				r = -EIO;
				break;
			}
			count -= xfer;
			/* zero this so we don't try to free it on error exit */
			req = 0;
		}
	}

	if (req)
		vser_req_put(dev, &dev->tx_iq_idle, req);

	vser_unlock(&dev->write_excl);
	DBG(cdev, "%s: returning %x\n", __func__, r);
	return r;
}
#endif

static void vser_test_works(struct work_struct *work)
{
	struct vser_dev *dev = _vser_dev;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req_recv = NULL, *req_sent = NULL;
	int count, offset, total, ret;
	int test_mode = dev->test_mode & 0x03;
	int free_req_count = 0;
	unsigned long start_times = jiffies;

	total = dev->total_size;
	req_recv = dev->rx_req;

	DBG(cdev, "%s: start (%d, 0x%x)\n", __func__, dev->test_mode, total);
	if (!req_recv) {
		DBG(cdev, "receive request is NULL!\n");
		return;
	}

	while (dev->test_mode) {
		if (dev->test_mode & 0x80) {
			if (test_mode == LOOP_TEST) { /* loop back test */
				if (total > dev->size_out)
					req_recv->length = dev->size_out;
				else
					req_recv->length = total;

				req_recv->length =
					ALIGN(req_recv->length,
					      dev->ep_out->maxpacket);
				dev->rx_done = 0;
				ret = usb_ep_queue(dev->ep_out,
						   req_recv, GFP_ATOMIC);
				if (ret < 0) {
					dev->rd_error = 1;
					break;
				}

				/* wait for a request to complete */
				ret = wait_event_interruptible(dev->read_wq,
							       dev->rx_done);
				if (ret < 0) {
					dev->rd_error = 1;
					break;
				}

				if (!dev->test_mode)
					break;

				offset = 0;
				count = req_recv->actual;
				do {
					ret = wait_event_interruptible(dev->write_wq,
						((req_sent = vser_req_get(dev, &dev->tx_idle)) ||
						 dev->wr_error));
					if (!req_sent) {
						DBG(cdev, "send request is NULL!\n");
						continue;
					}

					if (count > dev->size_in) {
						memcpy(req_sent->buf,
						       req_recv->buf,
						       dev->size_in);
						count -= dev->size_in;
						offset += dev->size_in;
						req_sent->length = dev->size_in;
					} else {
						memcpy(req_sent->buf,
						       req_recv->buf, count);
						req_sent->length = count;
						offset += count;
						count = 0;
					}

					ret = usb_ep_queue(dev->ep_in,
							   req_sent,
							   GFP_ATOMIC);
					if (ret < 0) {
						dev->wr_error = 1;
						break;
					}
				} while (count > 0);

				total -= req_recv->actual;
				if (total == 0) {
					vser_request_free(req_recv,
							  dev->ep_out);
					do {
						ret = wait_event_interruptible(dev->write_wq,
							((req_sent = vser_req_get(dev, &dev->tx_idle)) ||
							 dev->wr_error));
						if (req_sent == NULL)
							continue;
						free_req_count++;
						vser_request_free(req_sent,
								  dev->ep_in);
					} while (free_req_count < TX_REQ_MAX);
					break;
				}
			} else if (test_mode == DOWNLINK_TEST) {
				/* Device-->Host 0x00 UL */
				if (total > dev->size_out)
					req_recv->length = dev->size_out;
				else {
					req_recv->length = total;
					req_recv->length = ALIGN(req_recv->length,
								 dev->ep_out->maxpacket);
				}

				dev->rx_done = 0;
				ret = usb_ep_queue(dev->ep_out, req_recv,
						   GFP_ATOMIC);
				if (ret < 0) {
					dev->rd_error = 1;
					break;
				}

				/* wait for a request to complete */
				ret = wait_event_interruptible(dev->read_wq,
							       dev->rx_done);
				if (ret < 0) {
					dev->rd_error = 1;
					break;
				}

				total -= req_recv->actual;
				if (total == 0) {
					vser_free_all_request(dev);
					break;
				}
			} else if (test_mode  == UPLINK_TEST) {
				/* Host-->Device 0x01 DL */
				ret = wait_event_interruptible(dev->write_wq,
					((req_sent = vser_req_get(dev, &dev->tx_idle)) ||
					 dev->wr_error));
				if (!req_sent)
					continue;

				if (!total) {
					vser_request_free(req_sent, dev->ep_in);
					free_req_count++;
					if (free_req_count == TX_REQ_MAX) {
						vser_request_free(req_recv,
								  dev->ep_out);
						break;
					}
					continue;
				}

				if (total > dev->size_in)
					req_sent->length = dev->size_in;
				else
					req_sent->length = total;

				ret = usb_ep_queue(dev->ep_in, req_sent,
						   GFP_ATOMIC);
				if (ret < 0) {
					dev->wr_error = 1;
					break;
				}
				total -= req_sent->length;
			}
		}
		req_sent = NULL;
	}

	DBG(cdev, "%s test time = %d\n", __func__, (int)(jiffies - start_times));
	dev->size_in = VSER_BULK_BUFFER_SIZE;
	dev->size_out = VSER_BULK_BUFFER_SIZE;
	if (vser_alloc_all_request(dev) < 0)
		DBG(cdev, "Vser allocate memory failed!!!\n");
	dev->test_mode = 0;
}
