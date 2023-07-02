/*
 *
 *  Copyright (C) 2007 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "pfe_mod.h"
#include "pfe_ctrl.h"

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>

#if defined(CONFIG_UNIT_TEST_HIF)
void hif_unit_test(struct pfe *pfe);
#endif

#define DMEM_TEST_BASE_ADDR	0x0	/* !!! For class overlaps with packets in dmem, for tmu overwrites exception vectors !!! */


#define PMEM_TEST_BASE_ADDR	0x10000

#define BUF_SIZE 6

static u32 w[BUF_SIZE] = {0x01234567, 0x8988cdef, 0x00cc2233, 0x4455dd77, 0x8899aabb, 0xccddeeff};
static u32 zero[BUF_SIZE] = {0, };
static u32 r[BUF_SIZE];

volatile void *buf_coherent;
unsigned long buf_coherent_phys;

static int memcmp_io(void *a, void *b, int len)
{
#if defined(CONFIG_PLATFORM_PCI)
	int rc;

	for (;len >= 4; len -= 4)
	{
		rc = readl(a) - readl(b);
		if (rc)
			return rc;

		a += 4;
		b += 4;
	}

	return 0;
#else
	return memcmp(a, b, len);
#endif
}

static void memcpy_io(void *dst, void *src, int len)
{
#if defined(CONFIG_PLATFORM_PCI)
	for (;len >= 4; len -= 4)
	{
		writel(*(u32 *)src, dst);
		dst += 4;
		src += 4;
	}
#else
	memcpy(dst, src, len);
#endif
}

static void pe_request_write(struct pfe_ctrl *ctrl, unsigned int id)
{
	int len, i;
	int rc;

	for (len = 1; len <= BUF_SIZE * sizeof(u32); len += 1) {
		/* Copy to dmem memory */
		pe_dmem_memcpy_to32(id, DMEM_TEST_BASE_ADDR, &w[0], len);

		memset_io(buf_coherent, 0, BUF_SIZE * sizeof(u32));

		/* Request PE to copy it back */
		rc = pe_request(ctrl, id, buf_coherent_phys, DMEM_TEST_BASE_ADDR, len);
		if (rc) {
			printk(KERN_ERR "PE %d: pe_request() failed: %d %#x\n", id, rc, readl(CLASS_PE0_DEBUG));
			continue;
		}

		if (memcmp_io(buf_coherent, w, len))
			printk(KERN_ERR "PE %d: %s failed: %d", id, __func__, len);
		else
			printk(KERN_ERR "PE %d: %s success: %d", id, __func__, len);

		for (i = 0; i < len; i++)
			printk(" %x/%x", ((volatile u8 *)buf_coherent)[i], ((u8 *)w)[i]);

		printk("\n");
	}
}

static void pe_request_read(struct pfe_ctrl *ctrl, unsigned int id)
{
	u8 *rb = (u8 *)&r[0];
	int len, i;
	int rc;

	for (len = 1; len <= BUF_SIZE * sizeof(u32); len += 1) {
		/* Zero memory */
		pe_dmem_memcpy_to32(id, DMEM_TEST_BASE_ADDR, &zero[0], len);

		/* Request PE to copy to internal memory */
		memcpy_io(buf_coherent, w, BUF_SIZE * sizeof(u32));
		rc = pe_request(ctrl, id, DMEM_TEST_BASE_ADDR, buf_coherent_phys, len);
		if (rc) {
			printk(KERN_ERR "PE %d: pe_request() failed: %d %#x\n", id, rc, readl(CLASS_PE0_DEBUG));
			continue;
		}

		/* Read back and compare */
		for (i = 0; i < len; i++)
			rb[i] = pe_dmem_read(id, DMEM_TEST_BASE_ADDR + i, 1);

		if (memcmp_io(rb, buf_coherent, len))
			printk(KERN_ERR "PE %d: %s failed: %d", id, __func__, len);
		else
			printk(KERN_ERR "PE %d: %s success: %d", id, __func__, len);

		for (i = 0; i < len; i++)
			printk(" %x/%x", rb[i], ((volatile u8 *)buf_coherent)[i]);

		printk("\n");
	}
}

static void pcie_mem_dump(struct pfe *pfe)
{
	int i;
	u32 a;
	int print;
	int count;

	for (i = 0, count = 0, print = 1; i < 64 *SZ_1K; i+=4) {
		a = readl(pfe->ddr_baseaddr + i);
		if (a == 0x67452301)
			print = 1;

		if (print) {
			count++;
			printk(KERN_ERR "%#x %#x\n", i, a);

			if (count == 16) {
				count = 0;
				print = 0;
			}
		}
	}
}

static void pcie_mem(struct pfe_ctrl *ctrl)
{
	int i, r;
#if 0
	for (i = 0; i < 100; i++) {
		writeb(i, buf_coherent + i);
		if ((r = readb(buf_coherent + i)) != i)
			printk(KERN_ERR "%s: readb() %d %d\n", __func__, i, r);
	}

	for (i = 0; i < 100/2; i++) {
                writew(i, buf_coherent + i * 2);
                if ((r = readw(buf_coherent + i * 2)) != i)
                        printk(KERN_ERR "%s: readw() %d %d\n", __func__, i, r);
        }
#endif

#if 0
	for (i = 0; i < 100/4; i++) {
		writel(i, buf_coherent + i * 4);
		if ((r = readl(buf_coherent + i * 4)) != i)
			printk(KERN_ERR "%s: readl() %d %d\n", __func__, i, r);
	}
#endif
	for (i = 0; i < 256; i++)
		printk(KERN_ERR "%lx %lx %x %x %x\n", buf_coherent_phys, (unsigned long)buf_coherent, readl(pfe->ddr_baseaddr), readl(buf_coherent), readl(CLASS_PE0_DEBUG));
}

static void pe_stop_start(struct pfe_ctrl *ctrl, int pe_mask)
{
	printk(KERN_INFO "%s\n", __func__);

	pe_sync_stop(ctrl, pe_mask);

	printk(KERN_INFO "%s stopped\n", __func__);

	pe_start(ctrl, pe_mask);

	printk(KERN_INFO "%s re-started\n", __func__);
}


static void pe_running(struct pfe_ctrl *ctrl)
{
	struct pe_sync_mailbox *mbox;
	struct pe_msg_mailbox *msg;
	u32 val;
	u32 r32[2];
	u16 r16[2];
	u8 r8[4];
	int i;

	printk(KERN_INFO "%s\n", __func__);

	mbox = (void *)ctrl->sync_mailbox_baseaddr[CLASS0_ID];

	for (i = 0; i < 100; i++) {
		val = pe_dmem_read(CLASS0_ID, (unsigned long)&mbox->stopped, 4);
		printk(KERN_ERR "%s: %#lx %#x %#10x %#10x %#10x %#10x\n", __func__, (unsigned long)&mbox->stopped, be32_to_cpu(val),
			readl(CLASS_PE0_DEBUG), readl(CLASS_PE1_DEBUG), readl(CLASS_PE2_DEBUG), readl(CLASS_PE3_DEBUG));
	}

	printk(KERN_ERR "%s: stopped", __func__);

	for (i = 0; i < MAX_PE; i++) {
		mbox = (void *)ctrl->sync_mailbox_baseaddr[i];
		val = pe_dmem_read(i, (unsigned long)&mbox->stopped, 4);

		printk(" %x", val);
	}

	printk("\n");

	printk(KERN_ERR "%s: request", __func__);

	for (i = 0; i < MAX_PE; i++) {
		msg = (void *)ctrl->msg_mailbox_baseaddr[i];
		val = pe_dmem_read(i, (unsigned long)&msg->request, 4);
        
		printk(" %x", val);
	}

        printk("\n");


	r32[0] = pe_dmem_read(CLASS0_ID, 0x800, 4);
	r32[1] = pe_dmem_read(CLASS0_ID, 0x804, 4);
	r16[0] = pe_dmem_read(CLASS0_ID, 0x808, 2);
	r16[1] = pe_dmem_read(CLASS0_ID, 0x80a, 2);

	r8[0] = pe_dmem_read(CLASS0_ID, 0x80c, 1);
	r8[1] = pe_dmem_read(CLASS0_ID, 0x80d, 1);
	r8[2] = pe_dmem_read(CLASS0_ID, 0x80e, 1);
	r8[3] = pe_dmem_read(CLASS0_ID, 0x80f, 1);
	

	printk(KERN_ERR "%x %x\n", r32[0], r32[1]);
	printk(KERN_ERR "%x %x\n", r16[0], r16[1]);
	printk(KERN_ERR "%x %x %x %x\n", r8[0], r8[1], r8[2], r8[3]);
	printk(KERN_ERR "%x %x %x %x\n", pe_dmem_read(CLASS0_ID, 0x810, 4), pe_dmem_read(CLASS1_ID, 0x810, 4),
					pe_dmem_read(CLASS2_ID, 0x810, 4), pe_dmem_read(CLASS3_ID, 0x810, 4));
}


static void pmem_writeN_readN(unsigned int id)
{
	u8 *rb = (u8 *)&r[0];
	int len, i;

	/* PMEM can not be modified if CPU is running */
	class_disable();

	for (len = 1; len <= BUF_SIZE * sizeof(u32); len++) {
		pe_pmem_memcpy_to32(id, PMEM_TEST_BASE_ADDR, &zero[0], len);

		pe_pmem_memcpy_to32(id, PMEM_TEST_BASE_ADDR, &w[0], len);

		for (i = 0; i < len; i++)
			rb[i] = pe_pmem_read(id, PMEM_TEST_BASE_ADDR + i, 1);

		if (memcmp(rb, w, len))
			printk(KERN_ERR "PE %d: %s failed: %d\n", id, __func__, len);
		else
			printk(KERN_ERR "PE %d: %s success: %d\n", id, __func__, len);
	}

	class_enable();
}


static void pmem_write4_read4(unsigned int id)
{
	/* PMEM can not be modified if CPU is running */
	class_disable();

	pe_pmem_memcpy_to32(id, PMEM_TEST_BASE_ADDR, &w[0], 4);

	r[0] = pe_pmem_read(id, PMEM_TEST_BASE_ADDR, 4);

	if (r[0] != w[0])
		printk(KERN_ERR "PE %d: %s failed: %#x %#x\n", id, __func__, w[0], r[0]);
	else
		printk(KERN_ERR "PE %d: %s success\n", id, __func__);

	class_enable();
}

static void dmem_writeN_readN(unsigned int id)
{
	u32 zero[3] = {0, };
	u8 *rb = (u8 *)&r[0];
	int len, i;

	for (len = 1; len <= BUF_SIZE * sizeof(u32); len++)
	{
		pe_dmem_memcpy_to32(id, DMEM_TEST_BASE_ADDR, &zero[0], len);

		pe_dmem_memcpy_to32(id, DMEM_TEST_BASE_ADDR, &w[0], len);

		for (i = 0; i < len; i++)
			rb[i] = pe_dmem_read(id, DMEM_TEST_BASE_ADDR + i, 1);

		if (memcmp(rb, w, len))
			printk(KERN_ERR "PE %d: %s failed: %d\n", id, __func__, len);
		else
			printk(KERN_ERR "PE %d: %s success: %d\n", id, __func__, len);
	}
}


static void dmem_write4_read2(unsigned int id)
{
	u16 *h = (u16 *)&r[0];

	pe_dmem_write(id, w[0], DMEM_TEST_BASE_ADDR + 0, 4);

	h[0] = pe_dmem_read(id, DMEM_TEST_BASE_ADDR + 0, 2);
	h[1] = pe_dmem_read(id, DMEM_TEST_BASE_ADDR + 2, 2);

	if (r[0] != w[0])
		printk(KERN_ERR "PE %d: %s failed: %#x %#x\n", id, __func__, w[0], r[0]);
	else
		printk(KERN_ERR "PE %d: %s success\n", id, __func__);
}


static void dmem_write4_read1(unsigned int id)
{
	u8 *b = (u8 *)&r[0];

	pe_dmem_write(id, w[0], DMEM_TEST_BASE_ADDR + 0, 4);

	b[0] = pe_dmem_read(id, DMEM_TEST_BASE_ADDR + 0, 1);
	b[1] = pe_dmem_read(id, DMEM_TEST_BASE_ADDR + 1, 1);
	b[2] = pe_dmem_read(id, DMEM_TEST_BASE_ADDR + 2, 1);
	b[3] = pe_dmem_read(id, DMEM_TEST_BASE_ADDR + 3, 1);

	if (r[0] != w[0])
		printk(KERN_ERR "PE %d: %s failed: %#x %#x\n", id, __func__, w[0], r[0]);
	else
		printk(KERN_ERR "PE %d: %s success\n", id, __func__);
}

static void dmem_write1_read4(unsigned int id)
{
	u8 *b = (u8 *)&w[0];

	pe_dmem_write(id, 0x0, DMEM_TEST_BASE_ADDR, 4);

	pe_dmem_write(id, b[0], DMEM_TEST_BASE_ADDR + 0, 1);
	pe_dmem_write(id, b[1], DMEM_TEST_BASE_ADDR + 1, 1);
	pe_dmem_write(id, b[2], DMEM_TEST_BASE_ADDR + 2, 1);
	pe_dmem_write(id, b[3], DMEM_TEST_BASE_ADDR + 3, 1);

	r[0] = pe_dmem_read(id, DMEM_TEST_BASE_ADDR, 4);

	if (r[0] != w[0])
		printk(KERN_ERR "PE %d: %s failed: %#x %#x\n", id, __func__, w[0], r[0]);
	else
		printk(KERN_ERR "PE %d: %s success\n", id, __func__);
}


static void dmem_write2_read4(unsigned int id)
{
	u16 *h = (u16 *)&w[0];

	pe_dmem_write(id, 0x0, DMEM_TEST_BASE_ADDR, 4);

	pe_dmem_write(id, h[0], DMEM_TEST_BASE_ADDR + 0, 2);
	pe_dmem_write(id, h[1], DMEM_TEST_BASE_ADDR + 2, 2);

	r[0] = pe_dmem_read(id, DMEM_TEST_BASE_ADDR, 4);

	if (r[0] != w[0])
		printk(KERN_ERR "PE %d: %s failed: %#x %#x\n", id, __func__, w[0], r[0]);
	else
		printk(KERN_ERR "PE %d: %s success\n", id, __func__);
}


static void dmem_read4_write4(unsigned int id)
{
	pe_dmem_write(id, w[0], DMEM_TEST_BASE_ADDR, 4);

	r[0] = pe_dmem_read(id, DMEM_TEST_BASE_ADDR, 4);

	if (r[0] != w[0])
		printk(KERN_ERR "PE %d: %s failed: %#x %#x\n", id, __func__, w[0], r[0]);
	else
		printk(KERN_ERR "PE %d: %s success\n", id, __func__);
}


void bmu_unit_test(void *base, BMU_CFG *cfg)
{
	unsigned long buf;
	int i;
	int loop = 2;
	unsigned long *bitmap;
	unsigned int bitoff;

	bitmap = kzalloc((cfg->count + 7) / 8, GFP_KERNEL);
	if (!bitmap)
		return;

	bmu_enable(base);

	do {
		printk(KERN_INFO "%s: testing %d\n", __func__, loop);

		for (i = 0; i < cfg->count; i++) {
			buf = readl(base + BMU_ALLOC_CTRL);
			if (!buf) {
				printk(KERN_ERR "%s: allocation failed %d\n", __func__, i);
				continue;
			}// else
			//	printk(KERN_ERR "%s: allocated %lx\n", __func__, buf);

			if (buf & ((1 << cfg->size) - 1))
				printk(KERN_ERR "%s: non aligned buffer %lx\n", __func__, buf);

			if (buf < cfg->baseaddr)
				printk(KERN_ERR "%s: out of bounds buffer %lx\n", __func__, buf);

			if (buf >= (cfg->baseaddr + cfg->count * (1 << cfg->size)))
				printk(KERN_ERR "%s: out of bounds buffer %lx\n", __func__, buf);

//			if ((readl(base + BMU_BUF_CNT) & 0xffff) != (i + 1))
//				printk(KERN_ERR "%s: used buffer count wrong %d %d\n", __func__, readl(base + BMU_BUF_CNT) & 0xffff, i + 1);

			if (readl(base + BMU_REM_BUF_CNT) != (cfg->count - i - 1))
				printk(KERN_ERR "%s: remaining buffer count wrong %d %d\n", __func__, readl(base + BMU_REM_BUF_CNT), cfg->count - i - 1);

			if (readl(base + BMU_CURR_BUF_CNT) != (i + 1))
				printk(KERN_ERR "%s: allocate buffer count wrong %d %d\n", __func__, readl(base + BMU_CURR_BUF_CNT), i + 1);

			bitoff = (buf - cfg->baseaddr) >> cfg->size;

			if (test_and_set_bit(bitoff, bitmap))
				printk(KERN_ERR "%s: duplicated buffer %lx\n", __func__, buf);
		}

		if (readl(base + BMU_ALLOC_CTRL) != 0)
			printk(KERN_ERR "%s: too many buffers in pool\n", __func__);

		for (i = 0; i < cfg->count; i++) {
			buf = cfg->baseaddr + i * (1 << cfg->size);
			writel(buf, base + BMU_FREE_CTRL);

			bitoff = (buf - cfg->baseaddr) >> cfg->size;
			
			if (!test_and_clear_bit(bitoff, bitmap))
				printk(KERN_ERR "%s: not allocated buffer %lx\n", __func__, buf);
		}

	} while (loop--);

	bmu_disable(base);

	kfree(bitmap);
}


void pfe_unit_test(struct pfe *pfe)
{
	int i;

	printk(KERN_INFO "%s\n", __func__);

#if defined(CONFIG_TMU_DUMMY)
	for (i = 0; i <= CLASS_MAX_ID; i++) {
#else
	for (i = 0; i < MAX_PE; i++) {
#endif
		dmem_read4_write4(i);
		dmem_write2_read4(i);
		dmem_write1_read4(i);
		dmem_write4_read1(i);
		dmem_write4_read2(i);
		dmem_writeN_readN(i);
#if 0
		pmem_write4_read4(i);
		pmem_writeN_readN(i);
#endif
	}

	pe_running(&pfe->ctrl);

	/* Skip TMU, UTIL testing for now */
#if defined(CONFIG_TMU_DUMMY)
	pe_stop_start(&pfe->ctrl, CLASS_MASK);
#else
	pe_stop_start(&pfe->ctrl, CLASS_MASK | TMU_MASK);
#endif

#if defined(CONFIG_UNIT_TEST_HIF)
	hif_unit_test(pfe);
#endif

#if !defined(CONFIG_PLATFORM_PCI)
	buf_coherent = dma_alloc_coherent(pfe->dev, BUF_SIZE * sizeof(u32), &buf_coherent_phys, GFP_KERNEL);
	if (!buf_coherent) {
		printk(KERN_ERR "%s: dma_alloc_coherent() failed\n", __func__);
		goto out;
	}
#else
	buf_coherent = pfe->ddr_baseaddr + 0x8;
	buf_coherent_phys = pfe->ddr_phys_baseaddr + 0x8;

	pcie_mem_dump(pfe);
	pcie_mem(&pfe->ctrl);
#endif

	for (i = CLASS0_ID; i <= CLASS_MAX_ID; i++) {
		pe_request_read(&pfe->ctrl, i);

		pe_request_write(&pfe->ctrl, i);
	}

#if !defined(CONFIG_PLATFORM_PCI)
	dma_free_coherent(pfe->dev, BUF_SIZE * sizeof(u32), buf_coherent, buf_coherent_phys);

out:
#endif

	return;
}

#if defined(CONFIG_UNIT_TEST_HIF)
void hif_unit_test(struct pfe *pfe)
{
	struct hif_client_s *client;
	unsigned char *pkt;
	
	printk(KERN_INFO "%s\n", __func__);
	client = hif_lib_client_register(pfe, NULL, PFE_CL_EVENT, 64, 2, 2);
	if(client==NULL)	{
		printk("Failed to register client\n");
		return;
	}

	printk("hif client registered successfully \n");

	pkt = kmalloc(FPP_SKB_SIZE, GFP_KERNEL);
	if(!pkt) goto client_dereg;

	if(hif_lib_xmit_pkt(client, 0, pkt+PKT_HEADROOM, 100)==0) 
		printk("%s Packet successfully transmitted\n",__func__);
	else
		printk("%s Failed to transmit packet\n",__func__);

	kfree(pkt);

client_dereg:
	if(hif_lib_client_deregister(pfe, client) != 0) {
		printk("Failed to deregister client\n");
		return;
	}
	printk("hif client deregistered successfully \n");
}

#endif
