// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#include "atp_device.h"
#include "atp_device_user.h"

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/mod_devicetable.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/dma-buf.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

/* Set of commands programmable into CONTROL */
static const uint8_t CTRL_DMA_W   = 0x1;
static const uint8_t CTRL_DMA_RW  = 0x3;
static const uint8_t CTRL_PLAY    = 0x4;
static const uint8_t CTRL_INT_ACK = 0x8;
static const uint8_t CTRL_UNIQUE  = 0xC;

static const struct file_operations file_ops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = atp_device_ioctl
};

static const struct of_device_id atp_device_match_table[] = {
	{ .compatible = "arm,atpdevice" },
	{ }
};
MODULE_DEVICE_TABLE(of, atp_device_match_table);

static struct platform_driver atp_device_driver = {
	.driver = {
		.name = "atpdevice",
		.owner = THIS_MODULE,
		.of_match_table = atp_device_match_table,
	},
	.probe = atp_device_probe,
	.remove = atp_device_remove
};

static long
atp_device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *mdev;
	struct platform_device *pdev;
	struct atp_device_data *dev_data;
	struct device *dev;
	int error;

	mdev = file->private_data;
	pdev = dev_get_drvdata(mdev->this_device);
	dev_data = platform_get_drvdata(pdev);
	dev = &pdev->dev;
	switch (cmd) {
	case ATP_ATTACH_BUFFER:
	{
		int data;
		struct dma_buf *buf;
		struct dma_buf_attachment *att;

		if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
			return -EFAULT;

		buf = dma_buf_get(data);
		if (IS_ERR(buf))
			return PTR_ERR(buf);

		att = dma_buf_attach(buf, dev);
		if (IS_ERR(att)) {
			dma_buf_put(buf);
			return PTR_ERR(att);
		}

		break;
	}
	case ATP_DETACH_BUFFER:
	{
		int data;
		struct dma_buf *buf;
		struct dma_buf_attachment *att;

		if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
			return -EFAULT;

		buf = dma_buf_get(data);
		if (IS_ERR(buf))
			return PTR_ERR(buf);
		dma_buf_put(buf);

		att = atp_device_attachment_get(buf, dev);
		if (IS_ERR(att))
			return PTR_ERR(att);

		dma_buf_detach(buf, att);
		dma_buf_put(buf);

		break;
	}
	case ATP_PLAY_STREAM:
	{
		struct atp_data_play_stream data;
		struct atp_request_play_stream *req;

		if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
			return -EFAULT;

		if (data.read_fd == -1 && data.write_fd == -1)
			return -EINVAL;

		req = atp_device_request_get_play_stream(
				dev_data, data.atp_stream_id, data.flow_id);

		if (data.read_fd != -1) {
			error = atp_device_attachment_map(
					data.read_fd, dev, DMA_TO_DEVICE,
					&req->raddr, &req->rrange);
			if (error)
				goto atp_end_play_stream;

		}
		if (data.write_fd != -1) {
			error = atp_device_attachment_map(
					data.write_fd, dev, DMA_FROM_DEVICE,
					&req->waddr, &req->wrange);
			if (error)
				goto atp_end_play_stream;
		}

		atp_device_mmio_play_stream(dev_data, req);

		error = wait_event_interruptible(
				req->wait, req->completed == 1);
atp_end_play_stream:
		atp_device_request_put_play_stream(dev_data, req);
		return error;
	}
	case ATP_UNIQUE_STREAM:
	{
		struct atp_data_unique_stream data;
		struct atp_request_unique_stream *req;

		if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
			return -EFAULT;

		req = atp_device_request_get_unique_stream(
				data.atp_stream_name);
		req->stream_name_range =
			strlen(req->stream_name) * sizeof(char);
		req->stream_name_addr = dma_map_single(dev,
				(void *) req->stream_name,
				req->stream_name_range, DMA_TO_DEVICE);
		error = dma_mapping_error(dev, req->stream_name_addr);
		if (error)
			goto atp_end_unique_stream;

		atp_device_mmio_unique_stream(dev_data, req,
					      &data.atp_stream_id);

		dma_unmap_single(dev, req->stream_name_addr,
				 req->stream_name_range, DMA_TO_DEVICE);

		if (copy_to_user((void __user *) arg, &data, sizeof(data)))
			error = -EFAULT;
atp_end_unique_stream:
		atp_device_request_put_unique_stream(req);
		return error;
	}
	}

	return 0;
}

static struct dma_buf_attachment *
atp_device_attachment_get(struct dma_buf *buf, struct device *dev)
{
	struct dma_buf_attachment *pos;

	list_for_each_entry(pos, &buf->attachments, node) {
		if (pos->dev == dev)
			return pos;
	}

	return ERR_PTR(-EINVAL);
}

static int
atp_device_attachment_map(const int buf_fd, struct device *dev,
			  const enum dma_data_direction dir, dma_addr_t *addr,
			  size_t *range)
{
	struct dma_buf *buf;
	struct dma_buf_attachment *att;
	struct sg_table *sgt;

	buf = dma_buf_get(buf_fd);
	if (IS_ERR(buf))
		return PTR_ERR(buf);
	dma_buf_put(buf);

	att = atp_device_attachment_get(buf, dev);
	if (IS_ERR(att))
		return PTR_ERR(att);

	sgt = dma_buf_map_attachment(att, dir);
	if (IS_ERR(sgt))
		return PTR_ERR(sgt);

	*addr = sg_dma_address(sgt->sgl);
	*range = buf->size;

	return 0;
}

static struct atp_request_play_stream *
atp_device_request_get_play_stream(struct atp_device_data *dev_data,
				   const uint64_t stream_id,
				   const uint32_t flow_id)
{
	struct atp_request_play_stream *req;
	unsigned long flags;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	req->stream_id = stream_id;
	req->flow_id = flow_id;

	init_waitqueue_head(&req->wait);

	spin_lock_irqsave(&dev_data->play_requests_lock, flags);
	req->id = dev_data->next_play_request_id++;
	list_add(&req->node, &dev_data->play_requests);
	spin_unlock_irqrestore(&dev_data->play_requests_lock, flags);

	return req;
}

static struct atp_request_play_stream *
atp_device_request_get_play_stream_by_id(struct atp_device_data *dev_data,
					 const uint32_t req_id)
{
	unsigned long flags;
	struct atp_request_play_stream *pos, *ret;

	ret = ERR_PTR(-EINVAL);

	spin_lock_irqsave(&dev_data->play_requests_lock, flags);
	list_for_each_entry(pos, &dev_data->play_requests, node) {
		if (pos->id == req_id) {
			ret = pos;
			break;
		}
	}
	spin_unlock_irqrestore(&dev_data->play_requests_lock, flags);

	return ret;
}

static void
atp_device_request_put_play_stream(struct atp_device_data *dev_data,
				   struct atp_request_play_stream *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev_data->play_requests_lock, flags);
	list_del(&req->node);
	spin_unlock_irqrestore(&dev_data->play_requests_lock, flags);

	kfree(req);
}

static struct atp_request_unique_stream *
atp_device_request_get_unique_stream(const char __user *stream_name)
{
	struct atp_request_unique_stream *req;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	req->stream_name = strndup_user(stream_name,
					ATP_DEVICE_MAX_LEN_STREAM_NAME);

	return req;
}

static void
atp_device_request_put_unique_stream(struct atp_request_unique_stream *req)
{
	kfree(req->stream_name);
	kfree(req);
}

static void
atp_device_mmio_play_stream(struct atp_device_data *dev_data,
			    struct atp_request_play_stream *req)
{
	uint8_t control, status;
	unsigned long flags;

	spin_lock_irqsave(&dev_data->mmio_lock, flags);

	if (req->rrange) {
		iowrite64(req->raddr, dev_data->mmio + READ_BASE);
		iowrite64(req->rrange, dev_data->mmio + READ_RANGE);
	}
	if (req->wrange) {
		iowrite64(req->waddr, dev_data->mmio + WRITE_BASE);
		iowrite64(req->wrange, dev_data->mmio + WRITE_RANGE);
	}

	iowrite64(req->stream_id, dev_data->mmio + STREAM_ID);
	iowrite32(req->flow_id, dev_data->mmio + TASK_ID);
	iowrite32(req->id, dev_data->mmio + IN_REQUEST_ID);

	control = CTRL_PLAY;
	if (req->wrange)
		control |= CTRL_DMA_W;
	if (req->rrange && req->wrange)
		control |= CTRL_DMA_RW;
	iowrite8(control, dev_data->mmio + CONTROL);

	do {
		status = ioread8(dev_data->mmio + STATUS);
	} while (!status);
	iowrite8(0x0, dev_data->mmio + STATUS);

	spin_unlock_irqrestore(&dev_data->mmio_lock, flags);
}

static void
atp_device_mmio_unique_stream(struct atp_device_data *dev_data,
			      struct atp_request_unique_stream *req,
			      uint64_t *stream_id)
{
	uint8_t status;
	unsigned long flags;

	spin_lock_irqsave(&dev_data->mmio_lock, flags);

	iowrite64(req->stream_name_addr, dev_data->mmio + STREAM_NAME_BASE);
	iowrite64(req->stream_name_range, dev_data->mmio + STREAM_NAME_RANGE);

	iowrite8(CTRL_UNIQUE, dev_data->mmio + CONTROL);

	do {
		status = ioread8(dev_data->mmio + STATUS);
	} while (!status);
	iowrite8(0x0, dev_data->mmio + STATUS);

	*stream_id = ioread64(dev_data->mmio + STREAM_ID);

	spin_unlock_irqrestore(&dev_data->mmio_lock, flags);
}

static irqreturn_t
atp_device_isr(int irq, void *handle)
{
	struct atp_device_data *dev_data;
	unsigned long flags;
	uint32_t req_id;
	struct atp_request_play_stream *req;

	dev_data = handle;

	spin_lock_irqsave(&dev_data->mmio_lock, flags);

	req_id = ioread32(dev_data->mmio + OUT_REQUEST_ID);
	iowrite8(CTRL_INT_ACK, dev_data->mmio + CONTROL);

	spin_unlock_irqrestore(&dev_data->mmio_lock, flags);

	req = atp_device_request_get_play_stream_by_id(dev_data, req_id);
	if (IS_ERR(req))
		return IRQ_NONE;

	req->completed = true;
	wake_up_interruptible(&req->wait);

	return IRQ_HANDLED;
}

static int
atp_device_probe(struct platform_device *pdev)
{
	struct atp_device_data *data;
	int error;
	struct resource *res;
	char *mdev_name;

	data = kzalloc(sizeof(struct atp_device_data), GFP_KERNEL);

	error = of_property_read_string(pdev->dev.of_node, "atp-id",
					&data->atp_id);
	if (error)
		goto err;

	INIT_LIST_HEAD(&data->play_requests);
	data->next_play_request_id = 1;
	spin_lock_init(&data->play_requests_lock);

	error = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (error)
		goto err;

	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0) {
		error = data->irq;
		goto err;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		error = -EIO;
		goto err;
	}

	error = devm_request_irq(&pdev->dev, data->irq, atp_device_isr, 0,
				 dev_name(&pdev->dev), data);
	if (error)
		goto err;

	data->mmio = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(data->mmio)) {
		error = PTR_ERR(data->mmio);
		goto err1;
	}
	spin_lock_init(&data->mmio_lock);

	data->mdev.minor = MISC_DYNAMIC_MINOR;
	mdev_name = kzalloc(strlen("atp") + strlen(data->atp_id), GFP_KERNEL);
	strcpy(mdev_name, "atp");
	strcat(mdev_name, data->atp_id);
	data->mdev.name = mdev_name;
	data->mdev.fops = &file_ops;
	error = misc_register(&data->mdev);
	if (error)
		goto err2;

	platform_set_drvdata(pdev, data);
	dev_set_drvdata(data->mdev.this_device, pdev);

	goto ok;
err2:
	kfree(data->mdev.name);
	devm_iounmap(&pdev->dev, data->mmio);
err1:
	devm_free_irq(&pdev->dev, data->irq, data);
err:
	kfree(data);
ok:
	return error;
}

static int
atp_device_remove(struct platform_device *pdev)
{
	struct atp_device_data *data;
	struct list_head *pos, *n;
	struct atp_request_play_stream *req;

	data = platform_get_drvdata(pdev);

	misc_deregister(&data->mdev);
	kfree(data->mdev.name);
	devm_iounmap(&pdev->dev, data->mmio);
	devm_free_irq(&pdev->dev, data->irq, data);

	list_for_each_safe(pos, n, &data->play_requests) {
		req = list_entry(pos, struct atp_request_play_stream, node);
		atp_device_request_put_play_stream(data, req);
	}

	kfree(data);

	return 0;
}

module_platform_driver(atp_device_driver);

MODULE_LICENSE("Dual BSD/GPL");
