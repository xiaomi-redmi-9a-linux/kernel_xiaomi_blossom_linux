// SPDX-License-Identifier: GPL-2.0+

#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <drm/drm_prime.h>

#include "vkms_drv.h"

static struct vkms_gem_object *__vkms_gem_create(struct drm_device *dev,
						 u64 size)
{
	struct vkms_gem_object *obj;
	int ret;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return ERR_PTR(-ENOMEM);

	size = roundup(size, PAGE_SIZE);
	ret = drm_gem_object_init(dev, &obj->gem, size);
	if (ret) {
		kfree(obj);
		return ERR_PTR(ret);
	}

	mutex_init(&obj->pages_lock);

	return obj;
}

void vkms_gem_free_object(struct drm_gem_object *obj)
{
	struct vkms_gem_object *gem = container_of(obj, struct vkms_gem_object,
						   gem);

	mutex_lock(&gem->pages_lock);
	if (gem->pages) {
		if (obj->import_attach)
			kvfree(gem->pages);
		else
			drm_gem_put_pages(obj, gem->pages, true, false);
		gem->pages = NULL;
	}
	mutex_unlock(&gem->pages_lock);
	mutex_destroy(&gem->pages_lock);

	if (obj->import_attach)
		drm_prime_gem_destroy(obj, gem->table);

	drm_gem_object_release(obj);
	kfree(gem);
}

int vkms_gem_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct vkms_gem_object *obj = vma->vm_private_data;
	unsigned long vaddr = vmf->address;
	pgoff_t page_offset;
	loff_t num_pages;
	int ret;

	page_offset = (vaddr - vma->vm_start) >> PAGE_SHIFT;
	num_pages = DIV_ROUND_UP(obj->gem.size, PAGE_SIZE);

	if (page_offset > num_pages)
		return VM_FAULT_SIGBUS;

	ret = -ENOENT;
	mutex_lock(&obj->pages_lock);
	if (obj->pages) {
		get_page(obj->pages[page_offset]);
		vmf->page = obj->pages[page_offset];
		ret = 0;
	}
	mutex_unlock(&obj->pages_lock);
	if (ret) {
		struct page *page;
		struct address_space *mapping;

		mapping = file_inode(obj->gem.filp)->i_mapping;
		page = shmem_read_mapping_page(mapping, page_offset);

		if (!IS_ERR(page)) {
			vmf->page = page;
			ret = 0;
		} else {
			switch (PTR_ERR(page)) {
			case -ENOSPC:
			case -ENOMEM:
				ret = VM_FAULT_OOM;
				break;
			case -EBUSY:
				ret = VM_FAULT_RETRY;
				break;
			case -EFAULT:
			case -EINVAL:
				ret = VM_FAULT_SIGBUS;
				break;
			default:
				WARN_ON(PTR_ERR(page));
				ret = VM_FAULT_SIGBUS;
				break;
			}
		}
	}
	return ret;
}

static struct drm_gem_object *vkms_gem_create(struct drm_device *dev,
					      struct drm_file *file,
					      u32 *handle,
					      u64 size)
{
	struct vkms_gem_object *obj;
	int ret;

	if (!file || !dev || !handle)
		return ERR_PTR(-EINVAL);

	obj = __vkms_gem_create(dev, size);
	if (IS_ERR(obj))
		return ERR_CAST(obj);

	ret = drm_gem_handle_create(file, &obj->gem, handle);
	if (ret)
		return ERR_PTR(ret);

	return &obj->gem;
}

int vkms_dumb_create(struct drm_file *file, struct drm_device *dev,
		     struct drm_mode_create_dumb *args)
{
	struct drm_gem_object *gem_obj;
	u64 pitch, size;

	if (!args || !dev || !file)
		return -EINVAL;

	pitch = args->width * DIV_ROUND_UP(args->bpp, 8);
	size = pitch * args->height;

	if (!size)
		return -EINVAL;

	gem_obj = vkms_gem_create(dev, file, &args->handle, size);
	if (IS_ERR(gem_obj))
		return PTR_ERR(gem_obj);

	args->size = gem_obj->size;
	args->pitch = pitch;

	drm_gem_object_put_unlocked(gem_obj);

	DRM_DEBUG_DRIVER("Created object of size %lld\n", size);

	return 0;
}

int vkms_dumb_map(struct drm_file *file, struct drm_device *dev,
		  u32 handle, u64 *offset)
{
	struct drm_gem_object *obj;
	int ret;

	obj = drm_gem_object_lookup(file, handle);
	if (!obj)
		return -ENOENT;

	if (!obj->filp) {
		ret = -EINVAL;
		goto unref;
	}

	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		goto unref;

	*offset = drm_vma_node_offset_addr(&obj->vma_node);
unref:
	drm_gem_object_put_unlocked(obj);

	return ret;
}

struct drm_gem_object *vkms_prime_import(struct drm_device *dev,
					 struct dma_buf *dma_buf)
{
	struct vkms_device *vkms = drm_device_to_vkms_device(dev);

	return drm_gem_prime_import_dev(dev, dma_buf, &vkms->platform->dev);
}

struct drm_gem_object *
vkms_prime_import_sg_table(struct drm_device *dev,
			   struct dma_buf_attachment *attach,
			   struct sg_table *sg)
{
	struct vkms_gem_object *obj;
	int npages;

	obj = __vkms_gem_create(dev, attach->dmabuf->size);
	if (IS_ERR(obj))
		return ERR_CAST(obj);

	npages = PAGE_ALIGN(attach->dmabuf->size) / PAGE_SIZE;

	obj->table = sg;
	obj->pages = kvmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (!obj->pages) {
		vkms_gem_free_object(&obj->gem);
		return ERR_PTR(-ENOMEM);
	}

	drm_prime_sg_to_page_addr_arrays(obj->table, obj->pages, NULL, npages);
	return &obj->gem;
}

static struct page **vkms_pin_pages(struct vkms_gem_object *bo)
{
	mutex_lock(&bo->pages_lock);
	if (bo->pages_pin_count++ == 0) {
		struct page **pages;

		if (bo->pages) {
			mutex_unlock(&bo->pages_lock);
			return bo->pages;
		}

		pages = drm_gem_get_pages(&bo->gem);
		if (IS_ERR(pages)) {
			bo->pages_pin_count--;
			mutex_unlock(&bo->pages_lock);
			return pages;
		}

		bo->pages = pages;
	}
	mutex_unlock(&bo->pages_lock);

	return bo->pages;
}

static void vkms_unpin_pages(struct vkms_gem_object *bo)
{
	mutex_lock(&bo->pages_lock);
	if (--bo->pages_pin_count == 0) {
		if (!bo->gem.import_attach) {
			drm_gem_put_pages(&bo->gem, bo->pages, true, false);
			bo->pages = NULL;
		}
	}
	mutex_unlock(&bo->pages_lock);
}

int vkms_prime_pin(struct drm_gem_object *obj)
{
	struct vkms_gem_object *bo = container_of(obj, struct vkms_gem_object, gem);
	struct page **pages = vkms_pin_pages(bo);
	return PTR_ERR_OR_ZERO(pages);
}

void vkms_prime_unpin(struct drm_gem_object *obj)
{
	struct vkms_gem_object *bo = container_of(obj, struct vkms_gem_object, gem);
	vkms_unpin_pages(bo);
}

struct sg_table *vkms_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct vkms_gem_object *bo = container_of(obj, struct vkms_gem_object, gem);
	int npages = PAGE_ALIGN(obj->size) / PAGE_SIZE;

	if (bo->table)
		return bo->table;

	return drm_prime_pages_to_sg(bo->pages, npages);
}

void *vkms_prime_vmap(struct drm_gem_object *obj)
{
	struct vkms_gem_object *bo = container_of(obj, struct vkms_gem_object, gem);
	long n_pages = obj->size >> PAGE_SHIFT;
	struct page **pages;

	pages = vkms_pin_pages(bo);
	if (IS_ERR(pages))
		return NULL;

	return vmap(pages, n_pages, 0, PAGE_KERNEL);
}

void vkms_prime_vunmap(struct drm_gem_object *obj, void *vaddr)
{
	struct vkms_gem_object *bo = container_of(obj, struct vkms_gem_object, gem);

	vunmap(vaddr);
	vkms_unpin_pages(bo);
}

int vkms_prime_mmap(struct drm_gem_object *obj,
		    struct vm_area_struct *vma)
{
	int ret;

	if (obj->size < vma->vm_end - vma->vm_start)
		return -EINVAL;

	if (!obj->filp)
		return -ENODEV;

	ret = call_mmap(obj->filp, vma);
	if (ret)
		return ret;

	fput(vma->vm_file);
	vma->vm_file = get_file(obj->filp);
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;

	return 0;
}
