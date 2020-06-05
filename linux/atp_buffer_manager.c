// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#include "atp_buffer_manager.h"
#include "atp_buffer_manager_user.h"

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/iommu.h>
#include <linux/uaccess.h>

static const struct file_operations file_ops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = atp_buffer_manager_ioctl
};

static const struct dma_buf_ops buf_ops = {
	.cache_sgt_mapping = true,
	.attach = atp_buffer_manager_attach,
	.detach = atp_buffer_manager_detach,
	.map_dma_buf = atp_buffer_manager_map,
	.unmap_dma_buf = atp_buffer_manager_unmap,
	.mmap = atp_buffer_manager_mmap,
	.release = atp_buffer_manager_release
};

static struct miscdevice buffer_manager = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "atpbuffer",
	.fops = &file_ops
};

static long
atp_buffer_manager_ioctl(struct file *file, unsigned int cmd,
			 unsigned long arg)
{
	struct dma_buf *buf;

	switch (cmd) {
	case ATP_GET_BUF:
	{
		struct atp_data_get_buf data;

		if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
			return -EFAULT;
		data.size = PAGE_ALIGN(data.size);

		buf = atp_buffer_manager_export(data.size, data.contig);
		if (IS_ERR(buf))
			return PTR_ERR(buf);

		data.fd = dma_buf_fd(buf, O_CLOEXEC);
		if (copy_to_user((void __user *) arg, &data, sizeof(data))) {
			atp_buffer_manager_release(buf);
			return -EFAULT;
		}

		break;
	}
	case ATP_PUT_BUF:
	{
		int data;
		struct list_head *pos, *n;
		struct dma_buf_attachment *att;

		if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
			return -EFAULT;

		buf = dma_buf_get(data);
		if (IS_ERR(buf))
			return PTR_ERR(buf);

		list_for_each_safe(pos, n, &buf->attachments) {
			att = list_entry(pos, struct dma_buf_attachment, node);
			list_del(pos);
			dma_buf_detach(buf, att);
		}

		dma_buf_put(buf);

		break;
	}
	}

	return 0;
}

static struct dma_buf *
atp_buffer_manager_export(const size_t size, const bool contig)
{
	struct atp_buffer_data *data;
	int i;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *buf;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	data->num_pages = size >> PAGE_SHIFT;
	data->pages = kcalloc(data->num_pages, sizeof(struct page *),
			      GFP_KERNEL);
	data->contig = contig;

	if (data->contig) {
		void *kvaddr;

		kvaddr = alloc_pages_exact(size, GFP_KERNEL);
		if (!kvaddr) {
			buf = ERR_PTR(-ENOMEM);
			goto err;
		}
		for (i = 0; i < data->num_pages; ++i) {
			data->pages[i] = virt_to_page(kvaddr);
			kvaddr += PAGE_SIZE;
		}
	} else {
		if (!iommu_present(&platform_bus_type)) {
			buf = ERR_PTR(-ENODEV);
			goto err;
		}
		for (i = 0; i < data->num_pages; ++i)
			data->pages[i] = alloc_page(GFP_KERNEL);
	}

	exp_info.ops = &buf_ops;
	exp_info.size = size;
	exp_info.flags = O_RDWR;
	exp_info.priv = data;
	buf = dma_buf_export(&exp_info);
	if (IS_ERR(buf))
		goto err;

	goto ok;
err:
	kfree(data->pages);
	kfree(data);
ok:
	return buf;
}

static int
atp_buffer_manager_attach(struct dma_buf *buf, struct dma_buf_attachment *att)
{
	struct sg_table *sgt;
	struct atp_buffer_data *data;
	int error;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return -ENOMEM;

	data = buf->priv;
	error = sg_alloc_table_from_pages(sgt, data->pages, data->num_pages, 0,
					  buf->size, GFP_KERNEL);
	if (error)
		goto err;
	att->priv = sgt;

	goto ok;
err:
	kfree(sgt);
ok:
	return error;
}

static void
atp_buffer_manager_detach(struct dma_buf *buf, struct dma_buf_attachment *att)
{
	struct sg_table *sgt;

	sgt = att->priv;
	sg_free_table(sgt);
	kfree(sgt);
}

static struct sg_table *
atp_buffer_manager_map(struct dma_buf_attachment *att,
		       enum dma_data_direction dir)
{
	struct sg_table *sgt;

	sgt = att->priv;
	sgt->nents = dma_map_sg(att->dev, sgt->sgl, sgt->orig_nents, dir);
	if (!sgt->nents)
		return ERR_PTR(-EIO);

	return sgt;
}

static void
atp_buffer_manager_unmap(struct dma_buf_attachment *att, struct sg_table *sgt,
			 enum dma_data_direction dir)
{
	dma_unmap_sg(att->dev, sgt->sgl, sgt->orig_nents, dir);
}

static int
atp_buffer_manager_mmap(struct dma_buf *buf, struct vm_area_struct *vma)
{
	struct atp_buffer_data *data;
	unsigned long uvaddr;
	int i, error;

	data = buf->priv;
	uvaddr = vma->vm_start;
	for (i = vma->vm_pgoff; i < data->num_pages; ++i) {
		if (uvaddr >= vma->vm_end)
			break;
		error = vm_insert_page(vma, uvaddr, data->pages[i]);
		if (error)
			return error;
		uvaddr += PAGE_SIZE;
	}

	return 0;
}

static void
atp_buffer_manager_release(struct dma_buf *buf)
{
	struct atp_buffer_data *data;
	int i;

	data = buf->priv;
	if (data->contig) {
		free_pages_exact(page_address(data->pages[0]), buf->size);
	} else {
		for (i = 0; i < data->num_pages; ++i)
			free_page((unsigned long)
					page_address(data->pages[i]));
	}
	kfree(data->pages);

	kfree(data);
}

module_misc_device(buffer_manager);

MODULE_LICENSE("Dual BSD/GPL");
