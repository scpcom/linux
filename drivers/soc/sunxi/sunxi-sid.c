/*
 * drivers/soc/sunxi-sid.c
 *
 * Copyright(c) 2014-2016 Allwinnertech Co., Ltd.
 *         http://www.allwinnertech.com
 *
 * Author: sunny <sunny@allwinnertech.com>
 * Author: superm <superm@allwinnertech.com>
 * Author: Matteo <duanmintao@allwinnertech.com>
 *
 * Allwinner sunxi soc chip version and chip id manager.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/err.h>
#include <linux/sunxi-smc.h>

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

#include <linux/sunxi-sid.h>

#define SID_DBG(fmt, arg...) pr_debug("%s()%d - "fmt, __func__, __LINE__, ##arg)
#define SID_WARN(fmt, arg...) pr_warn("%s()%d - "fmt, __func__, __LINE__, ##arg)
#define SID_ERR(fmt, arg...) pr_err("%s()%d - "fmt, __func__, __LINE__, ##arg)

#if defined(CONFIG_ARCH_SUN50I) || defined(CONFIG_ARCH_SUN8IW6)
#define SUNXI_SECURITY_SUPPORT	1
#endif

#define SUNXI_VER_MAX_NUM	8
struct soc_ver_map {
	u32 id;
	u32 rev[SUNXI_VER_MAX_NUM];
};

#define SUNXI_SOC_ID_INDEX		1
#define SUNXI_SECURITY_ENABLE_INDEX	2
struct soc_ver_reg {
	s8 compatile[48];
	u32 offset;
	u32 mask;
	u32 shift;
	struct soc_ver_map ver_map;
	u32 in_ce;
};

#define SUNXI_SOC_ID_IN_SID

#if defined(CONFIG_ARCH_SUN50IW6)
#define TYPE_SB (0b001)
#define TYPE_NB (0b010)
#define TYPE_FB (0b011)
#else
#define TYPE_SB (0b001)
#define TYPE_NB (0b011)
#define TYPE_FB (0b111)
#endif

static unsigned int sunxi_soc_chipid[4];
static unsigned int sunxi_soc_ftzone[4];
static unsigned int sunxi_serial[4];
static int sunxi_soc_secure;
static unsigned int sunxi_soc_bin;
static unsigned int sunxi_soc_ver;
static unsigned int sunxi_soc_rotpk_status;

#ifndef CONFIG_SUNXI_SMC
u32 sunxi_smc_readl(phys_addr_t addr)
{
	void __iomem *vaddr = ioremap(addr, 4);
	u32 val;

	val = readl(vaddr);
	iounmap(vaddr);
	return val;
}
#endif

static s32 sid_get_vir_base(struct device_node **pnode, void __iomem **base,
		s8 *compatible)
{
	*pnode = of_find_compatible_node(NULL, NULL, compatible);
	if (IS_ERR_OR_NULL(*pnode)) {
		SID_ERR("Failed to find \"%s\" in dts.\n", compatible);
		return -ENXIO;
	}

	*base = of_iomap(*pnode, 0); /* reg[0] must be accessible. */
	if (*base == NULL) {
		SID_ERR("Unable to remap IO\n");
		return -ENXIO;
	}
	SID_DBG("Base addr of \"%s\" is %p\n", compatible, *base);
	return 0;
}

static s32  sid_get_phy_base(struct device_node **pnode, phys_addr_t **base,
		s8 *compatible)
{
	struct resource res = {0};
	int ret;
	*pnode = of_find_compatible_node(*pnode, NULL, compatible);
	if (IS_ERR_OR_NULL(*pnode)) {
		SID_ERR("Failed to find \"%s\" in dts.\n", compatible);
		return -ENXIO;
	}

	ret = of_address_to_resource(*pnode, 0, &res);
	if (ret) {
		SID_ERR("ret:%d Failed to get \"%s\"  base address\n", ret, compatible);
		return -ENXIO;
	}
	*base = (phys_addr_t *)res.start;
	SID_DBG("Base addr of \"%s\" is %p\n", compatible, (void *)*base);
	return 0;
}

static s32 sid_get_base(struct device_node **pnode,
		void __iomem **base, s8 *compatible, u32 sec)
{
	if (sec == 1)
		return sid_get_phy_base(pnode,
			(phys_addr_t **)base, compatible);
	else
		return sid_get_vir_base(pnode, base, compatible);
}

static void sid_put_base(struct device_node *pnode, void __iomem *base, u32 sec)
{
	SID_DBG("base = %p, Sec = %d\n", base, sec);
	if ((sec == 0) && (base != NULL))
		iounmap(base);
	if (pnode)
		of_node_put(pnode);
}

static u32 sid_readl(void __iomem *base, u32 sec)
{
	if (sec == 0)
		return readl(base);
	else
		return sunxi_smc_readl((phys_addr_t)base);
}

int get_key_map_info(s8 *name, u8 *compatile, u32 *offset, u32 *max_size)
{
	struct device_node *child_pnode;
	struct device_node *pnode = of_find_compatible_node(NULL, NULL, compatile);
	if (IS_ERR_OR_NULL(pnode)) {
		SID_ERR("Failed to find \"%s\" in dts.\n", compatile);
		return -ENXIO;
	}
	child_pnode = of_get_child_by_name(pnode, name);
	if (IS_ERR_OR_NULL(child_pnode)) {
		SID_ERR("Failed to find \"%s\" in dts.\n", name);
		return -ENXIO;
	}
	of_property_read_u32(child_pnode, "offset", offset);
	of_property_read_u32(child_pnode, "size", max_size);
	return 0;
}


static u32 sid_read_key(s8 *key_name, u32 *key_buf, u32 key_size, u32 sec)
{
	u32 i, offset = 0, max_size = 0;
	void __iomem *baseaddr = NULL;
	struct device_node *dev_node = NULL;

	if (sid_get_base(&dev_node, &baseaddr, EFUSE_SID_BASE, sec))
		return -ENXIO;

	get_key_map_info(key_name, EFUSE_SID_BASE, &offset, &max_size);
	SID_DBG("key_name:%s offset:0x%x max_size:0x%x\n", key_name, offset, max_size);
	if (key_size > max_size) {
		key_size = max_size;
	}
	for (i = 0; i < key_size; i += 4) {
		key_buf[i/4] = sid_readl(baseaddr + offset + i, sec);
	}

	sid_put_base(dev_node, baseaddr, sec);
	return 0;
}


static u32 sid_rd_bits(s8 *name, u32 offset, u32 shift, u32 mask, u32 sec)
{
	u32 value = 0;
	void __iomem *baseaddr = NULL;
	struct device_node *dev_node = NULL;

#ifdef SID_REG_READ
	return __sid_reg_read_key(offset);
#else
	if (sid_get_base(&dev_node, &baseaddr, name, sec))
		return 0;

	value = sid_readl(baseaddr + offset, sec);

	value = (value >> shift) & mask;
	SID_DBG("Read \"%s\" + %#x, shift %#x, mask %#x, return %#x, Sec %d\n",
			name, offset, shift, mask, value, sec);

	sid_put_base(dev_node, baseaddr, sec);
	return value;
#endif
}

int get_soc_ver_regs(u8 *name, u8 *compatile, struct soc_ver_reg *reg)
{
	struct device_node *child_pnode;
	struct device_node *pnode = of_find_compatible_node(NULL, NULL, compatile);
	if (IS_ERR_OR_NULL(pnode)) {
		SID_ERR("Failed to find \"%s\" in dts.\n", SRAM_CTRL_BASE);
		return -ENXIO;
	}
	child_pnode = of_get_child_by_name(pnode, name);
	if (IS_ERR_OR_NULL(child_pnode)) {
		SID_ERR("Failed to find \"%s\" in dts.\n", name);
		return -ENXIO;
	}

	of_property_read_u32(child_pnode, "offset", &reg->offset);
	of_property_read_u32(child_pnode, "shift", &reg->shift);
	of_property_read_u32(child_pnode, "mask", &reg->mask);
	of_property_read_u32(child_pnode, "ver_a", &reg->ver_map.rev[0]);
	of_property_read_u32(child_pnode, "ver_b", &reg->ver_map.rev[1]);
	of_property_read_u32(child_pnode, "in_ce", &reg->in_ce);
	return 0;
}


/* from plat-sunxi/soc-detect.c allwinner 3.4 begin */

#define EEPROM_SID_BASE "allwinner,sunxi-eeprom-sid"
#define TIMERC_SID_BASE "allwinner,sunxi-timer"

#define SW_VA_SRAM_IO_BASE                sram_base //0xf1c00000   /* 4KB */

#define SW_VA_SSE_IO_BASE                 ce_base //0xf1c15000

#define SW_VA_TIMERC_IO_BASE              timer_base //0xf1c20c00

#define SW_VA_SID_IO_BASE                 sid_base //0xf1c23800
#define SC_CHIP_ID_REG	(SW_VA_SRAM_IO_BASE + 0x24)

#define SC_CHIP_ID_EN_MASK	0x1
#define SC_CHIP_ID_EN_OFF	15
#define SC_CHIP_ID_EN	(SC_CHIP_ID_EN_MASK<<SC_CHIP_ID_EN_OFF)

#define SC_CHIP_ID_MASK	0xffff
#define SC_CHIP_ID_OFF	16
#define SC_CHIP_ID		(SC_CHIP_ID_MASK<<SC_CHIP_ID_OFF)

enum sunxi_chip_id {
	SUNXI_UNKNOWN_MACH = 0xffffffff,

	SUNXI_MACH_SUN4I = 1623,
	SUNXI_MACH_SUN5I = 1625,
	SUNXI_MACH_SUN6I = 1633,
	SUNXI_MACH_SUN7I = 1651,
};

enum {
	SUNXI_BIT_SUN4I = BIT(30),
	SUNXI_BIT_SUN5I = BIT(29),
	SUNXI_BIT_SUN6I = BIT(28),
	SUNXI_BIT_SUN7I = BIT(27),

	/* SUNXI_BIT_UNKNOWN can't OR anything known */
	SUNXI_BIT_UNKNOWN = BIT(20),

	/* sun4i */
	SUNXI_SOC_A10  = SUNXI_BIT_SUN4I | BIT(4),

	/* sun5i */
	SUNXI_SOC_A13  = SUNXI_BIT_SUN5I | BIT(4),
	SUNXI_SOC_A12  = SUNXI_BIT_SUN5I | BIT(5),
	SUNXI_SOC_A10S = SUNXI_BIT_SUN5I | BIT(6),

	/* sun6i */
	SUNXI_SOC_A31  = SUNXI_BIT_SUN6I | BIT(4),

	/* sun7i */
	SUNXI_SOC_A20  = SUNXI_BIT_SUN7I | BIT(4),

	SUNXI_REV_UNKNOWN = 0,
	SUNXI_REV_A,
	SUNXI_REV_B,
	SUNXI_REV_C,
};

enum sw_ic_ver {
	SUNXI_VER_UNKNOWN = SUNXI_BIT_UNKNOWN,

	/* sun4i */
	SUNXI_VER_A10A = SUNXI_SOC_A10 + SUNXI_REV_A,
	SUNXI_VER_A10B,
	SUNXI_VER_A10C,

	/* sun5i */
	SUNXI_VER_A13 = SUNXI_SOC_A13,
	SUNXI_VER_A13A,
	SUNXI_VER_A13B,
	SUNXI_VER_A12 = SUNXI_SOC_A12,
	SUNXI_VER_A12A,
	SUNXI_VER_A12B,
	SUNXI_VER_A10S = SUNXI_SOC_A10S,
	SUNXI_VER_A10SA,
	SUNXI_VER_A10SB,

	/* sun6i */
	SUNXI_VER_A31 = SUNXI_SOC_A31,

	/* sun7i */
	SUNXI_VER_A20 = SUNXI_SOC_A20,
};

enum sw_ic_ver sw_get_ic_ver(void);

#define _sunxi_is(M)		((sw_get_ic_ver()&M) == M)

/* sunxi_is_sunNi() could also be implemented ORing the ic_ver */
#define sunxi_is_sun4i()	(sunxi_chip_id() == SUNXI_MACH_SUN4I)
#define sunxi_is_sun5i()	(sunxi_chip_id() == SUNXI_MACH_SUN5I)
#define sunxi_is_sun6i()	(sunxi_chip_id() == SUNXI_MACH_SUN6I)
#define sunxi_is_sun7i()	(sunxi_chip_id() == SUNXI_MACH_SUN7I)
#define sunxi_is_a10()		_sunxi_is(SUNXI_SOC_A10)
#define sunxi_is_a13()		_sunxi_is(SUNXI_SOC_A13)
#define sunxi_is_a12()		_sunxi_is(SUNXI_SOC_A12)
#define sunxi_is_a10s()		_sunxi_is(SUNXI_SOC_A10S)
#define sunxi_is_a31()		_sunxi_is(SUNXI_SOC_A31)
#define sunxi_is_a20()		_sunxi_is(SUNXI_SOC_A20)

#define sunxi_soc_rev()		(sw_get_ic_ver() & 0xf)

struct sw_chip_id
{
	unsigned int sid_rkey0;
	unsigned int sid_rkey1;
	unsigned int sid_rkey2;
	unsigned int sid_rkey3;
};

u32 sunxi_sc_chip_id(u32 *mach_id)
{
	u32 chip_id, reg_val;
	void __iomem *sram_base = NULL;
	struct device_node *dev_node = NULL;

	if (sid_get_base(&dev_node, &sram_base, SRAM_CTRL_BASE, 0)) {
		pr_err("SC: failed to get chip-id base");
		return SUNXI_UNKNOWN_MACH;
	}

	/* enable chip_id reading */
	reg_val = readl(SC_CHIP_ID_REG);
	writel(reg_val | SC_CHIP_ID_EN, SC_CHIP_ID_REG);

	reg_val = readl(SC_CHIP_ID_REG);
	chip_id = ((reg_val&SC_CHIP_ID)>>SC_CHIP_ID_OFF) & SC_CHIP_ID_MASK;

        sid_put_base(dev_node, sram_base, 0);

	if (*mach_id)
		*mach_id = chip_id;

	switch (chip_id) {
	case 0x1623:
		return SUNXI_MACH_SUN4I;
	case 0x1625:
		return SUNXI_MACH_SUN5I;
	case 0x1633:
		return SUNXI_MACH_SUN6I;
	case 0x1651:
		return SUNXI_MACH_SUN7I;
	default:
		/*pr_err("SC: failed to identify chip-id 0x%04x (*0x%08x == 0x%08x)\n",
		       chip_id, (u32)SC_CHIP_ID_REG, reg_val);*/
		return SUNXI_UNKNOWN_MACH;
	}
}
EXPORT_SYMBOL(sunxi_sc_chip_id);

/*
 */
u32 sunxi_chip_mach_id(u32 id)
{
	static u32 chip_ids[2];

	if (unlikely(chip_ids[0] == 0)) {
		chip_ids[1] = SUNXI_UNKNOWN_MACH;
		chip_ids[0] = sunxi_sc_chip_id(&chip_ids[1]);
	}

	return chip_ids[id];
}

u32 sunxi_chip_id(void)
{
	return sunxi_chip_mach_id(0);
}

EXPORT_SYMBOL(sunxi_chip_id);

static u32 sunxi_mach_id(void)
{
	return sunxi_chip_mach_id(1);
}

int sunxi_pr_chip_id(void)
{
	u32 chip_id = sunxi_chip_id();
	const char *soc_family, *name;
	int rev;

	if (sunxi_is_sun4i())
		soc_family = "sun4i";
	else if (sunxi_is_sun5i())
		soc_family = "sun5i";
	else if (sunxi_is_sun6i())
		soc_family = "sun6i";
	else if (sunxi_is_sun7i())
		soc_family = "sun7i";
	else
		soc_family = "sunNi?";

	if (sunxi_is_a10())
		name = "A10";
	else if (sunxi_is_a13())
		name = "A13";
	else if (sunxi_is_a12())
		name = "A12";
	else if (sunxi_is_a10s())
		name = "A10s";
	else if (sunxi_is_a31())
		name = "A31";
	else if (sunxi_is_a20())
		name = "A20";
	else
		name = NULL;

	rev = sunxi_soc_rev();
	if (rev)
		pr_info("Allwinner %s revision %c (AW%u/%s) detected.\n",
			name?name:"A??", 'A' + rev - 1, chip_id, soc_family);
	else
		pr_info("Allwinner %s (AW%u/%s) detected.\n",
			name?name:"A??", chip_id, soc_family);

	return name?1:0;
}

static inline void reg_dump(const char *name, void *reg, unsigned len)
{
	unsigned i, j;

	for (i=0; i<len; ) {
#if defined(CONFIG_ARM64) || defined(CONFIG_ARCH_RV64I)
		pr_info("soc-detect: %s (0x%08llx):", name, (u64)reg);
#else
		pr_info("soc-detect: %s (0x%08x):", name, (u32)reg);
#endif

		for (j=0; i<len && j<4; i++, j++, reg += 0x04) {
			u32 val = readl(reg);
			printk(" %08x", val);
		}

		printk("\n");
	}
}

enum sw_ic_ver sw_get_ic_ver(void)
{
	static enum sw_ic_ver ver;
	void __iomem *sid_base = NULL;
	struct device_node *sid_node = NULL;

	if (likely(ver))
		return ver;

	if (sunxi_is_sun4i()) {
		u32 val = 0;
		void __iomem *timer_base = NULL;
		struct device_node *timer_node = NULL;

		if (sid_get_base(&timer_node, &timer_base, TIMERC_SID_BASE, 0))
			goto unknown_chip;

		val = readl(SW_VA_TIMERC_IO_BASE + 0x13c);
		val = (val >> 6) & 0x3;

		sid_put_base(timer_node, timer_base, 0);

		if (val == 0)
			ver = SUNXI_VER_A10A;
		else if (val == 3)
			ver = SUNXI_VER_A10B;
		else
			ver = SUNXI_VER_A10C;
	} else if (sunxi_is_sun5i()) {
		u32 val;

		if (sid_get_base(&sid_node, &sid_base, EEPROM_SID_BASE, 0))
			goto unknown_chip;

		val = readl(SW_VA_SID_IO_BASE + 0x08);
		val = (val >> 12) & 0xf;
		switch (val) {
		case 0:	ver = SUNXI_VER_A12; break;
		case 3: ver = SUNXI_VER_A13; break;
		case 7: ver = SUNXI_VER_A10S; break;
		default:
			sid_put_base(sid_node, sid_base, 0);
			goto unknown_chip;
		}

		val = readl(SW_VA_SID_IO_BASE+0x00);
		val = (val >> 8) & 0xffffff;

		if (val == 0 || val == 0x162541)
			ver += SUNXI_REV_A;
		else if (val == 0x162542)
			ver += SUNXI_REV_B;
		else {
			const char *name;
			if (ver == SUNXI_VER_A13)
				name = "A13";
			else if (ver == SUNXI_VER_A12)
				name = "A12";
			else
				name = "A10S";

			pr_err("unrecongnized %s revision (%x)\n",
			       name, val);

			reg_dump("SID", SW_VA_SID_IO_BASE, 4);
		}

		sid_put_base(sid_node, sid_base, 0);
	} else if (sunxi_is_sun6i())
		ver = SUNXI_VER_A31;
	else if (sunxi_is_sun7i())
		ver = SUNXI_VER_A20;

	goto done;

unknown_chip:
	pr_err("unrecognized IC (chip-id=%u)\n", sunxi_chip_id());
	ver = SUNXI_VER_UNKNOWN;

	if (sunxi_is_sun5i()) {
		void __iomem *ce_base = NULL;
		struct device_node *ce_node = NULL;

		if (sid_get_base(&ce_node, &ce_base, "allwinner,sunxi-ce", 0))
			goto unknown_sse;

		reg_dump("SSE", SW_VA_SSE_IO_BASE, 1);

		sid_put_base(ce_node, ce_base, 0);
	}
unknown_sse:
	if (sid_get_base(&sid_node, &sid_base, EEPROM_SID_BASE, 0))
		goto done;

	reg_dump("SID", SW_VA_SID_IO_BASE, 4);

	sid_put_base(sid_node, sid_base, 0);
done:
	return ver;
}
EXPORT_SYMBOL(sw_get_ic_ver);

int sw_get_chip_id(struct sw_chip_id *chip_id)
{
	void __iomem *sid_base = NULL;
	struct device_node *sid_node = NULL;

	if (sid_get_base(&sid_node, &sid_base, EEPROM_SID_BASE, 0))
		return 0;

	chip_id->sid_rkey0 = readl(SW_VA_SID_IO_BASE);
	chip_id->sid_rkey1 = readl(SW_VA_SID_IO_BASE+0x04);
	chip_id->sid_rkey2 = readl(SW_VA_SID_IO_BASE+0x08);
	chip_id->sid_rkey3 = readl(SW_VA_SID_IO_BASE+0x0C);

	sid_put_base(sid_node, sid_base, 0);
	return 0;
}
EXPORT_SYMBOL(sw_get_chip_id);

/* from plat-sunxi/soc-detect.c allwinner 3.4 end */


bool sid_is_legacy(void)
{
	return (sunxi_chip_id() != SUNXI_UNKNOWN_MACH);
}

void sid_rd_ver_reg(u32 id)
{
	s32 i = 0;
	u32 ver = 0;
	static struct soc_ver_reg reg = {0};
	if (get_soc_ver_regs("soc_ver", SRAM_CTRL_BASE, &reg)) {
		if (sid_is_legacy())
			reg.ver_map.rev[0] = sunxi_mach_id() << 16;
	}
	ver = sid_rd_bits(SRAM_CTRL_BASE, reg.offset,
		reg.shift, reg.mask, 0);
	if (ver >= SUNXI_VER_MAX_NUM/2)
		SID_WARN("ver >= %d, soc ver:%d\n", SUNXI_VER_MAX_NUM/2, ver);

	sunxi_soc_ver = reg.ver_map.rev[0] + ver;

	SID_DBG("%d-%d: soc_ver %#x\n", i, ver, sunxi_soc_ver);
}

static s32 sid_rd_soc_ver_from_sid(void)
{
	u32 id = 0;
	static struct soc_ver_reg reg = {0};
	get_soc_ver_regs("soc_id", SRAM_CTRL_BASE, &reg);
	id = sid_rd_bits(EFUSE_SID_BASE, reg.offset, reg.shift, reg.mask, 0);
	sid_rd_ver_reg(id);

	return 0;
}

static bool soc_id_in_ce(void)
{
	static struct soc_ver_reg reg = {0};
	get_soc_ver_regs("soc_id", SRAM_CTRL_BASE, &reg);

	return reg.in_ce;
}

/* SMP_init maybe call this function, while CCU module wasn't inited.
   So we have to read/write the CCU register directly. */
static s32 sid_rd_soc_ver_from_ce(void)
{
	s32 ret = 0;
	u32 id = 0;
	void __iomem *ccu_base = NULL;
	struct device_node *ccu_node = NULL;
	u32 bus_clk_reg, bus_rst_reg, ce_clk_reg;

	ret = sid_get_base(&ccu_node, &ccu_base, "allwinner,sunxi-clk-init", 0);
	if (ret)
		return ret;

	/* backup ce clock */
	bus_clk_reg = readl(ccu_base + 0x060);
	bus_rst_reg = readl(ccu_base + 0x2c0);
	ce_clk_reg  = readl(ccu_base + 0x09c);

	if ((bus_clk_reg&(1<<5)) && (bus_rst_reg&(1<<5))
			&& (ce_clk_reg&(1<<31))) {
		SID_DBG("The CE module is already enable.\n");
	} else {
		/* enable ce clock */
		writel(bus_clk_reg | (1<<5), ccu_base + 0x060);
		writel(bus_rst_reg | (1<<5), ccu_base + 0x2c0);
		writel(ce_clk_reg | (1<<31), ccu_base + 0x09c);
	}

	id = sid_rd_bits("allwinner,sunxi-ce", 4, 0, 7, 0);

	/* restore ce clock */
	writel(bus_clk_reg, ccu_base + 0x060);
	writel(bus_rst_reg, ccu_base + 0x2c0);
	writel(ce_clk_reg,  ccu_base + 0x09c);

	sid_rd_ver_reg(id);

	sid_put_base(ccu_node, ccu_base, 0);
	return ret;
}

static void sid_soc_ver_init(void)
{
	static s32 init_flag;

	if (init_flag == 1) {
		SID_DBG("It's already inited.\n");
		return;
	}

	if (soc_id_in_ce())
		sid_rd_soc_ver_from_ce();
	else
		sid_rd_soc_ver_from_sid();

	SID_DBG("The SoC version: %#x\n", sunxi_soc_ver);
	init_flag = 1;
}


static void sid_chipid_init(void)
{
	u32 type = 0, offset = 0, max_size;
	bool is_legacy =  false;
	static s32 init_flag;
	static struct soc_ver_reg reg = {0};

	if (init_flag == 1) {
		SID_DBG("It's already inited.\n");
		return;
	}
	if (sid_read_key("chipid", sunxi_soc_chipid, 16, sunxi_soc_is_secure())) {
		is_legacy = sid_is_legacy();
		if (is_legacy) {
			sw_get_chip_id((struct sw_chip_id *)&sunxi_soc_chipid);
			sunxi_pr_chip_id();
		}
	}

	sunxi_serial[0] = sunxi_soc_chipid[3];
	sunxi_serial[1] = sunxi_soc_chipid[2];
	sunxi_serial[2] = (sunxi_soc_chipid[1] >> 16) & 0x0FFFF;

	if (is_legacy) {
		sunxi_serial[0] = 0;
		sunxi_serial[1] = 0;
		sunxi_serial[2] = sunxi_soc_chipid[0];
		sunxi_serial[3] = sunxi_soc_chipid[3];

		if ((sunxi_serial[3] & 0xffffff) == 0)
			sunxi_serial[3] |= 0x800000;
	}

	get_key_map_info("chipid", EFUSE_SID_BASE, &offset, &max_size);
	get_soc_ver_regs("soc_bin", SRAM_CTRL_BASE, &reg);

	type = sid_rd_bits(EFUSE_SID_BASE, reg.offset + offset, reg.shift,
		reg.mask, sunxi_soc_is_secure());

	switch (type) {
	case 0b000001:
		sunxi_soc_bin = 1;
		break;
	case 0b000011:
		sunxi_soc_bin = 2;
		break;
	case 0b000111:
		sunxi_soc_bin = 3;
		break;
	default:
		break;
	}
	SID_DBG("soc bin: %d\n", sunxi_soc_bin);

	init_flag = 1;
}

void sid_ft_zone_init(void)
{
	static s32 init_flag;
	if (init_flag == 1) {
		SID_DBG("It's already inited.\n");
		return;
	}
	sid_read_key(EFUSE_FT_ZONE_NAME, sunxi_soc_ftzone, 0x10, sunxi_soc_is_secure());

	init_flag = 1;

}

void sid_rd_soc_secure_status(void)
{
#if defined(CONFIG_TEE) && \
	(defined(CONFIG_ARCH_SUN8IW7) || defined(CONFIG_ARCH_SUN8IW6))
	sunxi_soc_secure = 1;
#else
	static s32 init_flag;
	void __iomem *base = NULL;
	struct device_node *node = NULL;
	u32 offset = 0, max_size;

	if (init_flag == 1) {
		SID_DBG("It's already inited.\n");
		return;
	}

	if (sid_get_base(&node, &base, EFUSE_SID_BASE, 1))
		return;

	get_key_map_info("secure_status", EFUSE_SID_BASE, &offset, &max_size);

#ifdef CONFIG_ARCH_SUN20IW1
	sunxi_soc_secure = (((sunxi_smc_readl((phys_addr_t)(base + offset))) >> 31) & 0x1);
#else
	sunxi_soc_secure = ((sunxi_smc_readl((phys_addr_t)(base + offset))) & 0x1);
#endif

	sid_put_base(node, base, 1);
	init_flag = 1;
#endif
}

void sid_rotpk_status_init(void)
{
	static s32 init_flag;
	if (init_flag == 1) {
		SID_DBG("It's already inited.\n");
		return;
	}
	sid_read_key(EFUSE_ROTPK_NAME, &sunxi_soc_rotpk_status, 4, sunxi_soc_is_secure());

	init_flag = 1;

}

s32 sunxi_get_platform(s8 *buf, s32 size)
{
	return snprintf(buf, size, "%s", CONFIG_SUNXI_SOC_NAME);
}
EXPORT_SYMBOL(sunxi_get_platform);

/**
 * soc chipid:
 */
int sunxi_get_soc_chipid(u8 *chipid)
{
	sid_chipid_init();
	memcpy(chipid, sunxi_soc_chipid, 16);
	return 0;
}
EXPORT_SYMBOL(sunxi_get_soc_chipid);

/**
 * soc chipid serial:
 */
int sunxi_get_serial(u8 *serial)
{
	sid_chipid_init();
	memcpy(serial, sunxi_serial, 16);
	return 0;
}
EXPORT_SYMBOL(sunxi_get_serial);

/**
 * get module_param:
 * argc[0]---dst buf
 * argc[1]---the sid offset
 * argc[2]---len(btye)
 */
int sunxi_get_module_param_from_sid(u32 *dst, u32 offset, u32 len)
{
	void __iomem *baseaddr = NULL;
	struct device_node *dev_node = NULL;
	int i;

	if (dst == NULL) {
		pr_err("the dst buf is NULL\n");
		return -1;
	}

	if (len & 0x3) {
		pr_err("the len must be word algin\n");
		return -2;
	}

	if (sid_get_base(&dev_node, &baseaddr, EFUSE_SID_BASE, 0)) {
		pr_err("sid_get_base fail \n");
		return 0;
	}

	SID_DBG("baseaddr: 0x%p offset:0x%x len(word):0x%x\n", baseaddr, offset, len);

	for (i = 0; i < len; i += 4) {
		dst[i] = sid_readl(baseaddr + 0x200 + offset + i, 0);
	}

	sid_put_base(dev_node, baseaddr, 0);

	return 0;
}
EXPORT_SYMBOL_GPL(sunxi_get_module_param_from_sid);



/**
 * soc chipid str:
 */
int sunxi_get_soc_chipid_str(char *serial)
{
	size_t size;

	sid_chipid_init();
#if defined(CONFIG_ARCH_SUN50IW9) || defined(CONFIG_ARCH_SUN50IW10)
	size = sprintf(serial, "%08x", sunxi_soc_chipid[0] & 0xffff);
#else
	size = sprintf(serial, "%08x", sunxi_soc_chipid[0] & 0x0ff);
#endif
	return size;
}
EXPORT_SYMBOL(sunxi_get_soc_chipid_str);

/**
 * soc ft zone str:
 */
int sunxi_get_soc_ft_zone_str(char *serial)
{
	size_t size;

	sid_ft_zone_init();
	size = sprintf(serial, "%08x", (sunxi_soc_ftzone[0] & 0xff000000) >> 24);
	return size;
}
EXPORT_SYMBOL(sunxi_get_soc_ft_zone_str);

/**
 * soc rotpk status str:
 */
int sunxi_get_soc_rotpk_status_str(char *status)
{
	size_t size;

	sid_rotpk_status_init();
	size = sprintf(status, "%d", (sunxi_soc_rotpk_status & 0x3) >> 1);
	return size;
}
EXPORT_SYMBOL(sunxi_get_soc_rotpk_status_str);

/**
 * soc chipid:
 */
int sunxi_soc_is_secure(void)
{
	sid_rd_soc_secure_status();
	return sunxi_soc_secure;
}
EXPORT_SYMBOL(sunxi_soc_is_secure);

/**
 * get sunxi soc bin
 *
 * return: the bin of sunxi soc, like that:
 * 0 : fail
 * 1 : slow
 * 2 : normal
 * 3 : fast
 */
unsigned int sunxi_get_soc_bin(void)
{
	sid_chipid_init();
	return sunxi_soc_bin;
}
EXPORT_SYMBOL(sunxi_get_soc_bin);

unsigned int sunxi_get_soc_ver(void)
{
	sid_soc_ver_init();
	return sunxi_soc_ver;
}
EXPORT_SYMBOL(sunxi_get_soc_ver);

s32 sunxi_efuse_readn(s8 *key_name, void *buf, u32 n)
{
	char name[32] = {0};

	if ((key_name == NULL) || (*(s8 *)key_name == 0)
			|| (n == 0) || (buf == NULL)) {
		SID_ERR("Invalid parameter. name: %p, read_buf: %p, size: %d\n",
		key_name, buf, n);
		return -EINVAL;
	}
	WARN_ON(n < 4);

	strncpy(name, key_name, strlen(key_name) - 1);
	sid_read_key(name, buf, n, sunxi_soc_is_secure());
	return 0;
}
EXPORT_SYMBOL(sunxi_efuse_readn);

static int __init sunxi_sid_init(void)
{
	SID_WARN("insmod ok\n");
	return 0;
}

static void __exit sunxi_sid_exit(void)
{
	SID_WARN("rmmod ok\n");
}

module_init(sunxi_sid_init);
module_exit(sunxi_sid_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("weidonghui<weidonghui@allwinnertech.com>");
MODULE_DESCRIPTION("sunxi sid.");
