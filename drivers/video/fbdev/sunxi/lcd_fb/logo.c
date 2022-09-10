/*
 * logo.c
 *
 * Copyright (c) 2007-2020 Allwinnertech Co., Ltd.
 * Author: zhengxiaobin <zhengxiaobin@allwinnertech.com>
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
#include "logo.h"
#include <linux/decompress/unlzma.h>
#include <linux/memblock.h>

static u32 disp_reserve_size;
static u32 disp_reserve_base;

#ifndef MODULE
int disp_reserve_mem(bool reserve)
{
	if (!disp_reserve_size || !disp_reserve_base)
		return -EINVAL;

	if (reserve)
		memblock_reserve(disp_reserve_base, disp_reserve_size);
	else
		memblock_free(disp_reserve_base, disp_reserve_size);

	return 0;
}

static int __init early_disp_reserve(char *str)
{
	u32 temp[3] = {0};

	if (!str)
		return -EINVAL;

	get_options(str, 3, temp);

	disp_reserve_size = temp[1];
	disp_reserve_base = temp[2];
	if (temp[0] != 2 || disp_reserve_size <= 0)
		return -EINVAL;

	/*arch_mem_end = memblock_end_of_DRAM();*/
	disp_reserve_mem(true);
	pr_info("disp reserve base 0x%x ,size 0x%x\n",
					disp_reserve_base, disp_reserve_size);
    return 0;
}
early_param("disp_reserve", early_disp_reserve);
#endif

static int bmp_to_fb(const void *psrc, struct bmp_header *bmp_header,
			  struct fb_info *info, void *pdst)
{
	int srclinesize, dstlinesize, w, h;
	const unsigned char *psrcline = NULL, *psrcdot = NULL;
	unsigned char *pdstline = NULL, *pdstdot = NULL;
	int i = 0, j = 0, zero_num = 0, bmp_real_height = 0;
	int BppBmp = 3;

	if (!psrc || !pdst || !bmp_header || !info) {
		lcd_fb_wrn("Invalid parameter\n");
		return -1;
	}

	if (bmp_header->bit_count == 24)
		zero_num = (4 - ((3 * bmp_header->width) % 4)) & 3;

	w = (info->var.xres < bmp_header->width) ? info->var.xres
						 : bmp_header->width;
	bmp_real_height = (bmp_header->height & 0x80000000) ? (-bmp_header->
						 height) : (bmp_header->height);
	h = (info->var.yres < bmp_real_height) ? info->var.yres : bmp_real_height;

	BppBmp = bmp_header->bit_count >> 3;
	srclinesize = bmp_header->width * BppBmp + zero_num;
	dstlinesize = info->var.xres * (info->var.bits_per_pixel >> 3);
	psrcline = (const unsigned char *)psrc;
	pdstline = (unsigned char *)pdst;

	if (bmp_header->height & 0x80000000) {
		for (i = 0; i < h; ++i) {
			psrcdot = psrcline;
			pdstdot = pdstline;
			if (info->var.bits_per_pixel == bmp_header->bit_count) {
				memcpy(pdstline, psrcline, srclinesize);
			} else {
				for (j = 0; j < w; ++j) {
					if (info->var.bits_per_pixel == 32 &&
					    bmp_header->bit_count == 24) {
						*pdstdot++ = psrcdot[j * BppBmp];
						*pdstdot++ = psrcdot[j * BppBmp + 1];
						*pdstdot++ = psrcdot[j * BppBmp + 2];
						*pdstdot++ = 0xff;
					} else if (info->var.bits_per_pixel == 16 &&
						   bmp_header->bit_count >= 24) {
						/* little endian */
						*pdstdot++ =
							((psrcdot[j * BppBmp + 1] << 3) &
							 0xe0) +
							((psrcdot[j * BppBmp] >> 3) & 0x1f);
						*pdstdot++ =
							(psrcdot[j * BppBmp + 2] & 0xf8) +
							((psrcdot[j * BppBmp + 1] >> 5) &
							 0x07);
					}
				}
			}
			psrcline += srclinesize;
			pdstline += dstlinesize;
		}
	} else {
		psrcline = (const unsigned char *)psrc +
			   (bmp_real_height - 1) * srclinesize;

		for (i = h - 1; i >= 0; --i) {
			psrcdot = psrcline;
			pdstdot = pdstline;
			if (info->var.bits_per_pixel == bmp_header->bit_count) {
				memcpy(pdstline, psrcline, srclinesize);
			} else {
				for (j = 0; j < w; ++j) {
					if (info->var.bits_per_pixel == 32 &&
					    bmp_header->bit_count == 24) {
						*pdstdot++ = psrcdot[j * BppBmp];
						*pdstdot++ = psrcdot[j * BppBmp + 1];
						*pdstdot++ = psrcdot[j * BppBmp + 2];
						*pdstdot++ = 0xff;
					} else if (info->var.bits_per_pixel == 16 &&
						   bmp_header->bit_count >= 24) {
						/* little endian */
						*pdstdot++ =
							((psrcdot[j * BppBmp + 1] << 3) &
							 0xe0) +
							((psrcdot[j * BppBmp] >> 3) & 0x1f);
						*pdstdot++ =
							(psrcdot[j * BppBmp + 2] & 0xf8) +
							((psrcdot[j * BppBmp + 1] >> 5) &
							 0x07);
					}
				}
			}
			psrcline -= srclinesize;
			pdstline += dstlinesize;
		}
	}

	return 0;
}

static void *Fb_map_kernel(unsigned long phys_addr, unsigned long size)
{
	int npages = PAGE_ALIGN(size) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;
	struct page *cur_page = phys_to_page(phys_addr);
	pgprot_t pgprot;
	void *vaddr = NULL;
	int i;

	if (!pages)
		return NULL;

	for (i = 0; i < npages; i++)
		*(tmp++) = cur_page++;

	pgprot = pgprot_noncached(PAGE_KERNEL);
	vaddr = vmap(pages, npages, VM_MAP, pgprot);

	vfree(pages);
	return vaddr;
}

static void Fb_unmap_kernel(void *vaddr)
{
	vunmap(vaddr);
}

#if defined(CONFIG_DECOMPRESS_LZMA)
int lzma_decode(uintptr_t paddr, struct fb_info *info)
{
	void *vaddr = NULL;
	long pos = 0;
	unsigned char *out = NULL;
	int ret = -1, i = 0;
	struct lzma_header lzma_head;
	struct bmp_header bmp_header;
	unsigned int x, y, bmp_bpix, fb_width, fb_height;
	unsigned int effective_width, effective_height;
	int zero_num = 0;
	struct sunxi_bmp_store bmp_info;
	void *screen_offset = NULL, *image_offset = NULL;
	char *tmp_buffer = NULL;
	char *bmp_data = NULL;

	if (!paddr || !info) {
		lcd_fb_wrn("Null pointer!\n");
		goto OUT;
	}

	vaddr = (void *)Fb_map_kernel(paddr, sizeof(struct lzma_header));
	if (vaddr == NULL) {
		lcd_fb_wrn("fb_map_kernel failed, paddr=0x%p,size=0x%x\n",
		      (void *)paddr, (unsigned int)sizeof(struct lzma_header));
		goto OUT;
	}

	memcpy(&lzma_head.signature[0], vaddr, sizeof(struct lzma_header));

	if ((lzma_head.signature[0] != 'L') ||
	    (lzma_head.signature[1] != 'Z') ||
	    (lzma_head.signature[2] != 'M') ||
	    (lzma_head.signature[3] != 'A')) {
		lcd_fb_wrn("this is not a LZMA file.\n");
		Fb_unmap_kernel(vaddr);
		goto OUT;
	}

	Fb_unmap_kernel(vaddr);

	out = kmalloc(lzma_head.original_file_size, GFP_KERNEL | __GFP_ZERO);
	if (!out) {
		lcd_fb_wrn("kmalloc outbuffer fail!\n");
		goto OUT;
	}

	vaddr = (void *)Fb_map_kernel(paddr, lzma_head.file_size +
						 sizeof(struct lzma_header));
	if (vaddr == NULL) {
		lcd_fb_wrn("fb_map_kernel failed, paddr=0x%p,size=0x%x\n",
		      (void *)paddr,
		      (unsigned int)(lzma_head.file_size +
				     sizeof(struct lzma_header)));
		goto FREE_OUT;
	}

	ret = unlzma((unsigned char *)(vaddr + sizeof(struct lzma_header)),
		     lzma_head.file_size, NULL, NULL, out, &pos, NULL);
	if (ret) {
		lcd_fb_wrn("unlzma fail:%d\n", ret);
		goto UMAPKERNEL;
	}

	memcpy(&bmp_header, out, sizeof(struct bmp_header));

	if ((bmp_header.signature[0] != 'B') ||
	    (bmp_header.signature[1] != 'M')) {
		lcd_fb_wrn("%s:this is not a bmp picture.\n", __func__);
		goto UMAPKERNEL;
	}

	bmp_bpix = bmp_header.bit_count / 8;

	if ((bmp_bpix != 3) && (bmp_bpix != 4)) {
		lcd_fb_wrn("%s:not support bmp format:%d.\n", __func__, bmp_bpix);
		goto UMAPKERNEL;
	}

	x = bmp_header.width;
	y = (bmp_header.height & 0x80000000) ? (-bmp_header.height)
					     : (bmp_header.height);
	if (bmp_bpix == 3)
		zero_num = (4 - ((3 * x) % 4)) & 3;
	fb_width = info->var.xres;
	fb_height = info->var.yres;
	bmp_info.x = x;
	bmp_info.y = y;
	bmp_info.bit = bmp_header.bit_count;
	bmp_info.buffer = (void *__force)(info->screen_base);

	if (bmp_bpix == 3)
		info->var.bits_per_pixel = 24;
	else if (bmp_bpix == 4)
		info->var.bits_per_pixel = 32;
	else
		info->var.bits_per_pixel = 32;

	tmp_buffer = (char *)bmp_info.buffer;
	screen_offset = (void *)bmp_info.buffer;
	bmp_data =
	    (char *)(out + bmp_header.data_offset + sizeof(struct lzma_header));
	image_offset = (void *)bmp_data;
	effective_width = (fb_width < x) ? fb_width : x;
	effective_height = (fb_height < y) ? fb_height : y;

	if (bmp_header.height & 0x80000000) {
#if defined(SUPPORT_ROTATE)
		if (info->var.bits_per_pixel == 24) {
			screen_offset =
			    (void *)((void *__force)info->screen_base +
				     (fb_width * (abs(fb_height - y) / 2) +
				      abs(fb_width - x) / 2) *
					 4);
			rgb24_to_rgb32(image_offset, &bmp_header, info,
				       screen_offset, zero_num);
		} else
#endif
		{
			screen_offset =
			    (void *)((void *__force)info->screen_base +
				     (fb_width * (abs(fb_height - y) / 2) +
				      abs(fb_width - x) / 2) *
					 (info->var.bits_per_pixel >> 3));
			for (i = 0; i < effective_height; i++) {
				memcpy((void *)screen_offset, image_offset,
				       effective_width *
					   (info->var.bits_per_pixel >> 3));
				screen_offset =
				    (void *)(screen_offset +
					     fb_width *
						 (info->var.bits_per_pixel >>
						  3));
				image_offset =
				    (void *)image_offset +
				    x * (info->var.bits_per_pixel >> 3);
			}
		}

	} else {

#if defined(SUPPORT_ROTATE)
		if (info->var.bits_per_pixel == 24) {
			screen_offset =
			    (void *)((void *__force)info->screen_base +
				     (fb_width * (abs(fb_height - y) / 2) +
				      abs(fb_width - x) / 2) *
					 4);

			image_offset =
			    (void *)bmp_data +
			    (effective_height - 1) * (x * 3 + zero_num);
			rgb24_to_rgb32(image_offset, &bmp_header, info,
				       screen_offset, zero_num);
		} else
#endif
		{
			screen_offset =
			    (void *)((void *__force)info->screen_base +
				     (fb_width * (abs(fb_height - y) / 2) +
				      abs(fb_width - x) / 2) *
					 (info->var.bits_per_pixel >> 3));

			image_offset = (void *)bmp_data +
				       (effective_height - 1) * x *
					   (info->var.bits_per_pixel >> 3);
			for (i = effective_height - 1; i >= 0; i--) {
				memcpy((void *)screen_offset, image_offset,
				       effective_width *
					   (info->var.bits_per_pixel >> 3));
				screen_offset =
				    (void *)(screen_offset +
					     fb_width *
						 (info->var.bits_per_pixel >>
						  3));
				image_offset =
				    (void *)bmp_data +
				    i * x * (info->var.bits_per_pixel >> 3);
			}
		}
	}

UMAPKERNEL:
	Fb_unmap_kernel(vaddr);
FREE_OUT:
	kfree(out);
OUT:
	return ret;
}
#endif

static int Fb_map_kernel_logo(struct fb_info *info)
{
	void *vaddr = NULL;
	uintptr_t paddr = 0;
	void *screen_offset = NULL, *image_offset = NULL;
	char *bmp_data = NULL;
	struct bmp_pad_header bmp_pad_header;
	struct bmp_header *bmp_header;
	unsigned int x, y, bmp_bpix;
	unsigned int effective_width, effective_height;
	uintptr_t offset;

	paddr = disp_reserve_base;
	if (!paddr || !disp_reserve_size || !info || !info->screen_base) {
		lcd_fb_wrn("Fb_map_kernel_logo failed!");
		return -1;
	}

	/* parser bmp header */
	offset = paddr & ~PAGE_MASK;
	vaddr = (void *)Fb_map_kernel(paddr, sizeof(struct bmp_header));
	if (vaddr == NULL) {
		lcd_fb_wrn("fb_map_kernel failed, paddr=0x%p,size=0x%x\n",
		      (void *)paddr, (unsigned int)sizeof(struct bmp_header));
		return -1;
	}

	memcpy(&bmp_pad_header.signature[0], vaddr + offset,
	       sizeof(struct bmp_header));
	bmp_header = (struct bmp_header *) &bmp_pad_header.signature[0];
	if ((bmp_header->signature[0] != 'B')
	    || (bmp_header->signature[1] != 'M')) {
		Fb_unmap_kernel(vaddr);
#if defined(CONFIG_DECOMPRESS_LZMA)
		return lzma_decode(paddr, info);
#else
		lcd_fb_wrn("this is not a bmp picture.\n");
		return -1;

#endif
	}

	bmp_bpix = bmp_header->bit_count / 8;

	if ((bmp_bpix != 3) && (bmp_bpix != 4)) {
		lcd_fb_wrn("Only bmp24 and bmp32 is supported!\n");
		return -1;
	}

	x = bmp_header->width;
	y = (bmp_header->height & 0x80000000) ? (-bmp_header->
						 height) : (bmp_header->height);
	if (x != info->var.xres || y != info->var.yres) {
		Fb_unmap_kernel(vaddr);
		lcd_fb_wrn("Bmp  [%u x %u] is not equal to fb[%d x %d]\n", x, y,
			   info->var.xres, info->var.yres);
		return -1;
	}

	if ((paddr <= 0) || x <= 1 || y <= 1) {
		lcd_fb_wrn("kernel logo para error!\n");
		return -EINVAL;
	}

	Fb_unmap_kernel(vaddr);

	/* map the total bmp buffer */
	vaddr =
	    (void *)Fb_map_kernel(paddr,
				  disp_reserve_size);
	if (vaddr == NULL) {
		lcd_fb_wrn("fb_map_kernel failed, paddr=0x%p,size=0x%x\n",
		      (void *)paddr,
		      (unsigned int)(x * y * bmp_bpix +
		       sizeof(struct bmp_header)));
		return -1;
	}

	bmp_data = (char *)(vaddr + bmp_header->data_offset + offset);
	image_offset = (void *)bmp_data;
	effective_width = x;
	effective_height = y;
	screen_offset =
		(void *__force)info->screen_base;

	bmp_to_fb(image_offset, bmp_header, info,
		  screen_offset);

	Fb_unmap_kernel(vaddr);
	return 0;
}

int logo_parse(struct fb_info *info)
{
	int ret = -1;

	ret = Fb_map_kernel_logo(info);

#ifndef MODULE
	disp_reserve_mem(false);
#endif
	return ret;;
}
