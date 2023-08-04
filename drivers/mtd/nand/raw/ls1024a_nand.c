/*
 *  linux/drivers/mtd/nand/ls1024a_nand.c
 *
 *  Copyright (C) Mindspeed Technologies, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This is a device driver for the NAND flash device found on the
 *   Comcerto board which utilizes the Toshiba TC58V64AFT part. This is
 *   a 128Mibit (8MiB x 8 bits) NAND flash device.
 */

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/bch.h>
#include <linux/bitrev.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/ratelimit.h>
#include <linux/platform_device.h>
#if 0
#include <mach/ecc.h>
#include <linux/mtd/exp_lock.h>
#endif

/* ECC Register Set */
/********************/
/* ECC Control Registers */
#define ECC_SHIFT_EN_CFG	(0x0)
#define ECC_GEN_CFG		(0x4)
#define ECC_TAG_CFG		(0x8)
#define ECC_INIT_CFG		(0xC)
#define ECC_PRTY_OUT_SEL_CFG	(0x10)
#define ECC_POLY_START_CFG	(0x14)
#define ECC_CS_SEL_CFG		(0x18)
/* ECC Status Registers */
#define ECC_IDLE_STAT		(0x1C)
#define ECC_POLY_STAT		(0x20)
#define ECC_CORR_STAT		(0x24)
#define ECC_CORR_DONE_STAT	(0x28)
#define ECC_CORR_DATA_STAT	(0x2C)

/* ECC general configuration register parameters */
#define HAMM_MODE	BIT(28)
#define BCH_MODE	(~ HAMM_MODE)

#define PRTY_MODE_MASK	BIT_MASK(24)
#define PRTY_CALC	PRTY_MODE_MASK
#define SYNDROME_CALC	(~ PRTY_CALC)

#define	ECC_LVL_MASK	0x3F0000
#define ECC_LVL_SHIFT	16

#define BLK_SIZE_MASK 0x7FF

#define ECC_LVL_2 0x2
#define ECC_LVL_4 0x4
#define ECC_LVL_6 0x6
#define ECC_LVL_8 0x8
#define ECC_LVL_10 0xA
#define ECC_LVL_12 0xC
#define ECC_LVL_14 0xE
#define ECC_LVL_16 0x10
#define ECC_LVL_18 0x12
#define ECC_LVL_20 0x14
#define ECC_LVL_22 0x16
#define ECC_LVL_24 0x18
#define ECC_LVL_26 0x1A
#define ECC_LVL_28 0x1C
#define ECC_LVL_30 0x1E
#define ECC_LVL_32 0x20

#if defined (CONFIG_NAND_LS1024A_ECC_24_HW_BCH)
	#define ECC_LVL_VAL ECC_LVL_24 /* ECC Level 24 is used */
#elif defined (CONFIG_NAND_LS1024A_ECC_8_HW_BCH)
	#define ECC_LVL_VAL ECC_LVL_8 /* ECC Level 8 is used */
#endif

/* Block size used in Bytes*/
#define ECC_BLOCK_SIZE_512 512
#define ECC_BLOCK_SIZE_1024 1024

/* Maximum value of ECC Block size is 2k-(1+14*ECC_LVL/8) Bytes */
#define ECC_BLOCK_SIZE		ECC_BLOCK_SIZE_1024
#define ECC_BLOCK_SIZE_SHIFT	ECC_BLOCK_SIZE_1024_SHIFT

#define ECC_CS4_SEL 0x10
#define ECC_CS3_SEL 0x08
#define ECC_CS2_SEL 0x04
#define ECC_CS1_SEL 0x02
#define ECC_CS0_SEL 0x01

#define ECC_INIT		0x1
#define ECC_SHIFT_ENABLE	0x1
#define ECC_SHIFT_DISABLE	0x0
#define ECC_PARITY_OUT_EN	0x1
#define ECC_PARITY_OUT_DISABLE	0x0

/* Polynomial Start Configuration (ECC_POLY_START_CFG) */
#define ECC_POLY_START		(1 << 0)

/* Idle Status (ECC_IDLE_STAT) */
#define ECC_IDLE		(1 << 0)

/* Polynomial Status (ECC_POLY_STAT) */
#define ECC_CORR_REQ		(1 << 0)
#define ECC_ERASED_PAGE		(1 << 1)
#define ECC_UNCORR_ERR_HAMM	(1 << 2)

/* Correction Status (ECC_CORR_STAT) */
#define ECC_TAG_MASK		0xFFFF
#define ECC_NUM_ERR_MASK	0x3F
#define ECC_NUM_ERR_SHIFT	16
#define ECC_UNCORR		(1 << 24)

/* Correction Done Status (ECC_CORR_DONE_STAT) */
#define ECC_DONE		(1 << 0)

/* Correction Data Status (ECC_CORR_DATA_STAT), BCH Mode */
#define ECC_BCH_MASK		0xFFFF
#define ECC_BCH_INDEX_MASK	0x7FF
#define ECC_BCH_INDEX_SHIFT	16
#define ECC_BCH_VALID		(1 << 31)

/* Correction Data Status (ECC_CORR_DATA_STAT), Hamming Mode */
#define ECC_HAMM_MASK		0xF
#define ECC_HAMM_INDEX_MASK	0x1FF
#define ECC_HAMM_INDEX_SHIFT	16
#define ECC_HAMM_VALID		(1 << 31)

struct comcerto_bch_control {
	struct bch_control   *bch;
	unsigned int         *errloc;
};

/*
 * MTD structure for Comcerto board
 */
struct comcerto_nand_info {
	struct nand_chip	chip;
	struct mtd_info		mtd;
	struct gpio_desc	*ce_gpio;
	struct gpio_desc	*br_gpio;
	uint8_t			*bit_reversed;
	struct comcerto_bch_control	cbc;
};

static inline
struct comcerto_nand_info *to_comerto_nand_info(struct nand_chip *chip)
{
	return container_of(chip, struct comcerto_nand_info, chip);
}

static void __iomem *ecc_base_addr;

/*
 * Define partitions for flash device
 */

uint32_t COMCERTO_NAND_ALE = 0x00000200;
uint32_t COMCERTO_NAND_CLE = 0x00000400;

#ifdef CONFIG_NAND_LS1024A_ECC_24_HW_BCH
/*
 * spare area layout for BCH ECC bytes calculated over 512-Bytes ECC block size
 */
static struct nand_ecclayout comcerto_ecc_info_512_bch = {
	.eccbytes = 42,
	.eccpos = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
		  11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
		  21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
		  31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41},
	.oobfree = {
		{.offset = 43, .length = 13}
	}
};

/*
 * spare area layout for BCH ECC bytes calculated over 1024-Bytes ECC block size
 */
static struct nand_ecclayout comcerto_ecc_info_1024_bch = {
	.eccbytes = 42,
	.eccpos = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
		  11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
		  21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
		  31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41},
	.oobfree = {
		{.offset = 43, .length = 13}
	}
};

#elif defined(CONFIG_NAND_LS1024A_ECC_8_HW_BCH)

/*
 * spare area layout for BCH ECC bytes calculated over 512-Bytes ECC block size
 */
static struct nand_ecclayout comcerto_ecc_info_512_bch = {
	.eccbytes = 14,
	.eccpos = {0, 1, 2, 3, 4, 5, 6,
		   7, 8, 9, 10, 11, 12, 13},
	.oobfree = {
		{.offset = 15, .length = 1}
	}
};

/*
 * spare area layout for BCH ECC bytes calculated over 1024-Bytes ECC block size
 */
static struct nand_ecclayout comcerto_ecc_info_1024_bch = {
	.eccbytes = 14,
	.eccpos = {0, 1, 2, 3, 4, 5, 6,
		   7, 8, 9, 10, 11, 12, 13},
	.oobfree = {
		{.offset = 15, .length = 17}
	}
};

#else

/*
 * spare area layout for Hamming ECC bytes calculated over 512-Bytes ECC block
 * size
 */
static struct nand_ecclayout comcerto_ecc_info_512_hamm = {
	.eccbytes = 4,
	.eccpos = {0, 1, 2, 3},
	.oobfree = {
		{.offset = 5, .length = 12}
	}
};

/*
 * spare area layout for Hamming ECC bytes calculated over 1024-Bytes ECC block
 * size
 */
static struct nand_ecclayout comcerto_ecc_info_1024_hamm = {
	.eccbytes = 4,
	.eccpos = {0, 1, 2, 3},
	.oobfree = {
		{.offset = 5, .length = 28}
	}
};
#endif

static uint8_t bbt_pattern[] = { 'B', 'b', 't', '0' };
static uint8_t mirror_pattern[] = { '1', 't', 'b', 'B' };

static struct nand_bbt_descr bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_8BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs = 44,
	.len = 4,
	.veroffs = 48,
	.maxblocks = 8,
	.pattern = bbt_pattern,
};

static struct nand_bbt_descr bbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_8BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs = 44,
	.len = 4,
	.veroffs = 48,
	.maxblocks = 8,
	.pattern = mirror_pattern,
};

static uint8_t scan_ff_pattern[] = { 0xff };

#ifdef CONFIG_NAND_LS1024A_ECC_24_HW_BCH
static struct nand_bbt_descr c2000_badblock_pattern = {
	.offs = 42,
	.len = 1,
	.pattern = scan_ff_pattern
};
#elif defined(CONFIG_NAND_LS1024A_ECC_8_HW_BCH)
static struct nand_bbt_descr c2000_badblock_pattern = {
	.offs = 14,
	.len = 1,
	.pattern = scan_ff_pattern
};
#endif

/** Disable/Enable shifting of data to parity module
 *
 * @param[in] en_dis_shift  Enable or disable shift to parity module.
 *
 */
static void comcerto_ecc_shift(uint8_t en_dis_shift)
{
	writel_relaxed(en_dis_shift, ecc_base_addr + ECC_SHIFT_EN_CFG);
}

/** Initializes h/w ECC with proper configuration values.
 *
 * @param[in] mtd	MTD device structure
 * @param[in] mode	Select between BCH and Hamming
 *
 */
static void comcerto_enable_hw_ecc(struct mtd_info *mtd, int mode)
{
	struct nand_chip *nand_device = (struct nand_chip *)(mtd->priv);
	uint32_t ecc_gen_cfg_val = 0;

	if (mode == NAND_ECC_READSYN) return;
	/* CS4 will have the option for ECC calculation */
	writel_relaxed(ECC_CS4_SEL, ecc_base_addr + ECC_CS_SEL_CFG);

	/* parity calculation for write, syndrome calculation for read.*/
	(mode == NAND_ECC_WRITE) ? (ecc_gen_cfg_val |= PRTY_CALC) : (ecc_gen_cfg_val &= SYNDROME_CALC);

#if defined (CONFIG_NAND_LS1024A_ECC_8_HW_BCH) || defined (CONFIG_NAND_LS1024A_ECC_24_HW_BCH)
	ecc_gen_cfg_val &= BCH_MODE;
	ecc_gen_cfg_val = (ecc_gen_cfg_val & ~(ECC_LVL_MASK)) | (ECC_LVL_VAL << ECC_LVL_SHIFT);
#else
	ecc_gen_cfg_val |= HAMM_MODE;
#endif

	ecc_gen_cfg_val = (ecc_gen_cfg_val & ~(BLK_SIZE_MASK)) | nand_device->ecc.size;

	writel_relaxed(ecc_gen_cfg_val, ecc_base_addr + ECC_GEN_CFG);
	/* Reset parity module and latch configured values */
	writel_relaxed(ECC_INIT, ecc_base_addr + ECC_INIT_CFG);
	comcerto_ecc_shift(ECC_SHIFT_ENABLE);
	return;
}

/** writes ECC bytes generated by the parity module into the flash
 *
 * @param[in] mtd	MTD device structure
 * @param[in] dat	raw data
 * @param[in] ecc_code	buffer for ECC
 *
 */
static int comcerto_calculate_ecc(struct mtd_info *mtd,
				  const uint8_t *dat,
				  uint8_t *ecc_code)
{
	struct nand_chip *nand_device = mtd->priv;
	int ecc_bytes = nand_device->ecc.bytes;
	uint8_t *ecc_calc = nand_device->buffers->ecccalc;
	unsigned long timeo;

	comcerto_ecc_shift(ECC_SHIFT_DISABLE);

	/* Wait for syndrome calculation to complete */
	timeo = jiffies + 4;
	for (;;) {
		int is_timeout = time_after_eq(jiffies, timeo);
		int is_idle = readl_relaxed(ecc_base_addr + ECC_IDLE_STAT) & ECC_IDLE;
		if (is_idle)
			break;
		if (is_timeout) {
			pr_err("ECC Timeout waiting for parity module to become idle 1\n");
			return -EIO;
		}
		touch_softlockup_watchdog();
	}

	comcerto_ecc_shift(ECC_SHIFT_ENABLE);

	writel_relaxed(ECC_PARITY_OUT_EN, ecc_base_addr + ECC_PRTY_OUT_SEL_CFG);

	/* Even though we do a dummy write to NAND flash, actual ECC bytes are
	 * written to the ECC location in the flash. The contents of ecc_calc
	 * are irrelevant. The ECC engine overwrites it with real ECC data. */
	nand_device->write_buf(mtd, ecc_calc, ecc_bytes);

	comcerto_ecc_shift(ECC_SHIFT_DISABLE);
	writel_relaxed(ECC_PARITY_OUT_DISABLE, ecc_base_addr + ECC_PRTY_OUT_SEL_CFG);

	return 0;
}

/** Checks ECC registers for errors and will correct them, if correctable
 *
 * @param[in] mtd	MTD device structure
 * @param[in] dat	raw data
 * @param[in] read_ecc  ECC read out from flash
 * @param[in] calc_ecc	ECC calculated over the raw data
 *
 */
static int comcerto_correct_ecc(struct mtd_info *mtd, uint8_t *dat,
		uint8_t *read_ecc, uint8_t *calc_ecc)
{
	struct nand_chip *nand_device = mtd->priv;
	int num_zero_bits = 0;
	int empty = 0;
#if defined (CONFIG_NAND_LS1024A_ECC_8_HW_BCH) || defined (CONFIG_NAND_LS1024A_ECC_24_HW_BCH)
	uint8_t err_count = 0;
	uint32_t err_corr_data_prev;
#endif
	uint32_t err_corr_data;
	uint16_t mask, index;
	unsigned long timeo;
	int ret;

	num_zero_bits = nand_check_erased_ecc_chunk(dat, nand_device->ecc.size,
			read_ecc, nand_device->ecc.bytes,
			NULL, 0,
			nand_device->ecc.strength);
	if (num_zero_bits >= 0) {
		/* Consider ECC chunk empty */
		empty = 1;
		if (num_zero_bits > 2) {
			pr_err_ratelimited("ECC: ECC chunk is mostly empty but has %d zero bits\n", num_zero_bits);
		}
		/*
		 * Even though the ECC chunk is empty, we still continue, wait
		 * for the syndrome calculation to complete and read out all
		 * syndromes even though there should be none.
		 * */
	}

	/* Wait for syndrome calculation to complete */
	timeo = jiffies + 4;
	for (;;) {
		int is_timeout = time_after_eq(jiffies, timeo);
		int is_idle = readl_relaxed(ecc_base_addr + ECC_IDLE_STAT) & ECC_IDLE;
		if (is_idle)
			break;
		if (is_timeout) {
			pr_err("ECC Timeout waiting for parity module to become idle 2\n");
			ret = -EIO;
			goto out;
		}
		touch_softlockup_watchdog();
	}

	 /* If no correction is required */
	if (likely(!((readl_relaxed(ecc_base_addr + ECC_POLY_STAT)) & ECC_CORR_REQ))) {
		ret = 0;
		goto out;
	}

	/* Error found! Correction required */
#if defined (CONFIG_NAND_LS1024A_ECC_8_HW_BCH) || defined (CONFIG_NAND_LS1024A_ECC_24_HW_BCH)
	/* Initiate correction operation */
	writel_relaxed(ECC_POLY_START, ecc_base_addr + ECC_POLY_START_CFG);

	udelay(25);

	timeo = jiffies + 4;
	err_corr_data_prev = 0;
	/* Read Correction data status register till header is 0x7FD */
	for (;;) {
		int is_startcode;
		int is_timeout = time_after_eq(jiffies, timeo);
		err_corr_data_prev = readl_relaxed(ecc_base_addr + ECC_CORR_DATA_STAT);
		is_startcode = (err_corr_data_prev >> ECC_BCH_INDEX_SHIFT) == 0x87FD;
		if (is_startcode)
			break;
		if (is_timeout) {
			pr_err("Timeout waiting for ECC correction data, reg=%08x\n",
				err_corr_data_prev);
			ret = -EIO;
			goto out;
		}
		touch_softlockup_watchdog();
	}

	udelay(25);
	err_corr_data = 0x0;
	/* start reading error locations */
	while (((err_corr_data >> 16) !=  0x87FE)) {
		err_corr_data = readl_relaxed(ecc_base_addr + ECC_CORR_DATA_STAT);
		if ((err_corr_data >> 16) ==  0x87FE)
			break;
		if (err_corr_data == err_corr_data_prev)
			continue;
		err_corr_data_prev = err_corr_data;
		/*
		 * If we determined that the ECC chunk is empty, we ignore all
		 * syndromes.
		 * */
		if (empty)
			continue;
		index = (err_corr_data >> 16) & 0x7FF;
		mask = err_corr_data & 0xFFFF;
		if (index * 2 >= nand_device->ecc.size) {
			pr_err("ECC correction index out of "
					"bounds. ECC_CORR_DATA_STAT %08x\n",
					err_corr_data);
			continue;
		}
		*((uint16_t *)(dat + (index * 2))) ^= mask;
		while (mask) {
			if (mask & 1)
				err_count++;
			mask >>= 1;
		}
	}

	if (!((readl_relaxed(ecc_base_addr + ECC_CORR_DONE_STAT)) & ECC_DONE)) {
		pr_err("ECC: uncorrectable error 1 !!!\n");
		ret = -1;
		goto out;
	}

	/* Check if the block has uncorrectable number of errors */
	if ((readl_relaxed(ecc_base_addr + ECC_CORR_STAT)) & ECC_UNCORR) {
		if (!empty)
			pr_err("ECC: uncorrectable error 2 !!!\n");
		ret = -EIO;
		goto out;
	}

#else		/* Hamming Mode */
		if (readl_relaxed(ecc_base_addr + ECC_POLY_STAT) == ECC_UNCORR_ERR_HAMM) {
			/* 2 or more errors detected and hence cannot
			be corrected */
			ret = -1; /* uncorrectable */
			goto out;
		} else {  /* 1-bit correctable error */
			err_corr_data = readl_relaxed(ecc_base_addr + ECC_CORR_DATA_STAT);
			index = (err_corr_data >> 16) & 0x1FF;

			if (nand_device->options & NAND_BUSWIDTH_16) {
				mask = 1 << (err_corr_data & 0xF);
				*((uint16_t *)(dat + index)) ^= mask;
			} else {
				mask = 1 << (err_corr_data & 0x7);
				*(dat + index) ^= mask;
			}
			ret = 1;
			goto out;

		}
#endif

	ret = err_count;
out:

	if (empty) {
		/*
		 * If we previously determined that this ECC chunk is empty,
		 * just ignore whatever errors were detected while reading
		 * syndromes from the ECC engine.
		 * */
		ret = num_zero_bits;
	}

	comcerto_ecc_shift(ECC_SHIFT_DISABLE);

	return ret;
}

/** writes single page to the NAND device along with the ECC bytes
 *
 * @param[in] mtd	MTD device structure
 * @param[in] chip      nand chip info structure
 * @param[in] buf	data buffer
 *
 */
static int comcerto_nand_write_page_hwecc(struct mtd_info *mtd,
					struct nand_chip *chip,
					const uint8_t *buf, int oob_required)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	const uint8_t *p = buf;
	uint8_t *oob = chip->oob_poi;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {

		chip->ecc.hwctl(mtd, NAND_ECC_WRITE);
		chip->write_buf(mtd, p, eccsize);

		chip->ecc.calculate(mtd, p, oob);
		oob += eccbytes;

		if (chip->ecc.postpad) {
			chip->write_buf(mtd, oob, chip->ecc.postpad);
			oob += chip->ecc.postpad;
		}
	}

	/* Calculate remaining oob bytes */
	i = mtd->oobsize - (oob - chip->oob_poi);
	if (i)
		chip->write_buf(mtd, oob, i);

	return 0;
}


static void comcerto_hybrid_hwctl (struct mtd_info *mtd, int mode) {
	if (mode == NAND_ECC_READ) comcerto_enable_hw_ecc(mtd, mode);
}

#if 0
static void comcerto_fake_hwctl (struct mtd_info *mtd, int mode) {
}
#endif

/* Replicate the LS1024A HW ECC engine in software. Compared to the BCH
 * implemenation in Linux, the LS1024A HW ECC engine (seemingly licensed from
 * Cyclic Design) does a few things differently:
 *
 * - They use 0x4443 as the primitive polynomial for BCH (Linux uses 0x402b by
 *   default).
 * - They reverse the 8 bits in every byte before they calculate the ECC code.
 *   They then also reverse the bits in the ECC code before they write it to
 *   flash.
 * - They don't XOR the inverse of the ECC code of an empty page (all 0xFF's)
 *   onto every calculated ECC code. (Linux does this to ensure that an empty
 *   page has a valid ECC code).
 *   */
static int comcerto_bch_calculate_ecc (struct mtd_info *mtd, const uint8_t *dat,
		uint8_t *ecc_code) {
	struct nand_chip *chip = mtd->priv;
	struct comcerto_nand_info *info = to_comerto_nand_info(chip);
	int i;

	for (i=0;i<chip->ecc.size;i++) {
		info->bit_reversed[i] = bitrev8(dat[i]);
	}
	memset(ecc_code, 0, chip->ecc.bytes);
	encode_bch(info->cbc.bch, info->bit_reversed, chip->ecc.size, ecc_code);
	for (i=0;i<chip->ecc.bytes;i++) {
		ecc_code[i] = bitrev8(ecc_code[i]);
	}
	return 0;
}

/* We currently don't need comcerto_bch_correct_ecc() because we are using a
 * hybrid approach where we use the HW ECC engine when we read from NAND. We
 * only calculate the ECC code in software when we write to the flash.  */
#if 0
/*
 * This function replicates the Cyclic Design ECC engine that can
 * be found in the LS1024A. It calculates the ECC code a little different from
 * nand_bch_correct_data() (nand_bch.c)
 * */
static int comcerto_bch_correct_ecc (struct mtd_info *mtd, uint8_t *dat, uint8_t *read_ecc,
		uint8_t *dummy) {
	struct nand_chip *chip = mtd->priv;
	struct comcerto_nand_info *info = to_comerto_nand_info(chip);
	int num_zero_bits = 0;
	int i, count;

	num_zero_bits = nand_check_erased_ecc_chunk(dat, chip->ecc.size,
			read_ecc, chip->ecc.bytes,
			NULL, 0,
			chip->ecc.strength);
	if (num_zero_bits >= 0) {
		if (num_zero_bits > 2) {
			pr_err_ratelimited("ECC: ECC chunk is mostly empty but has %d zero bits\n", num_zero_bits);
		}
		return num_zero_bits;
	}

	for (i=0;i<chip->ecc.size;i++) {
		info->bit_reversed[i] = bitrev8(dat[i]);
	}
	for (i=0;i<chip->ecc.bytes;i++) {
		read_ecc[i] = bitrev8(read_ecc[i]);
	}
	count = decode_bch(info->cbc.bch, info->bit_reversed, chip->ecc.size, read_ecc, NULL,
			NULL, info->cbc.errloc);
	if (count > 0) {
		for (i = 0; i < count; i++) {
			if (info->cbc.errloc[i] < (chip->ecc.size*8))
				/* error is located in data, correct it */
				dat[info->cbc.errloc[i] >> 3] ^= (1 << (7 - (info->cbc.errloc[i] & 7)));
			/* else error in ecc, no action needed */
		}
	} else if (count < 0) {
		pr_err_ratelimited("ecc unrecoverable error\n");
		count = -1;
	}
	return count;
}
#endif

/** reads single page from the NAND device and will read ECC bytes from flash. A
 * function call to comcerto_correct_ecc() will be used to validate the data.
 *
 * @param[in] mtd	MTD device structure
 * @param[in] chip      nand chip info structure
 * @param[in] buf	data buffer
 *
 */
#if 0
static int comcerto_nand_read_page_hwecc(struct mtd_info *mtd,
		struct nand_chip *chip, uint8_t *buf, int page)
{
	struct nand_chip *nand_device = mtd->priv;
	int i, eccsize = nand_device->ecc.size;
	int eccbytes = nand_device->ecc.bytes;
	int eccsteps = nand_device->ecc.steps;
	uint8_t *p = buf;
	uint8_t *ecc_code = nand_device->buffers->ecccode;
	int ecc_bytes = nand_device->ecc.bytes;
	int stat;
	uint8_t *oob = nand_device->oob_poi;


	for (; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {

		chip->ecc.hwctl(mtd, NAND_ECC_READ);
		chip->read_buf(mtd, p, eccsize);
		chip->read_buf(mtd, ecc_code, ecc_bytes);

		stat = chip->ecc.correct(mtd, p, oob, NULL);
		if (stat < 0) {
			mtd->ecc_stats.failed++;
			pr_err("ECC correction failed for page 0x%08x\n", page);
		} else {
			int idx = eccsteps;
			if (idx >= MTD_ECC_STAT_SUBPAGES) {
				idx = MTD_ECC_STAT_SUBPAGES - 1;
			}

			mtd->ecc_stats.corrected += stat;
			mtd->ecc_subpage_stats.subpage_corrected[idx] += stat;
		}

		if (chip->ecc.postpad) {
			chip->read_buf(mtd, oob, chip->ecc.postpad);
			oob += chip->ecc.postpad;
		}
	}
	/* Calculate remaining oob bytes */
	i = mtd->oobsize - (oob - chip->oob_poi);
	if (i)
		chip->read_buf(mtd, oob, i);

	return 0;
}
#endif

/*********************************************************************
 * NAND Hardware functions
 *
 *********************************************************************/

/*
 *	hardware specific access to control-lines
*/
void comcerto_nand_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip *chip = mtd->priv;
	struct comcerto_nand_info *info = to_comerto_nand_info(chip);

	if (ctrl & NAND_CTRL_CHANGE) {
		gpiod_set_value(info->ce_gpio, !(ctrl & NAND_NCE));
	}

	if (cmd == NAND_CMD_NONE)
		return;

	 if (ctrl & NAND_CLE)
		 writeb(cmd, chip->IO_ADDR_W + COMCERTO_NAND_CLE);
	 else if (ctrl & NAND_ALE)
		writeb(cmd, chip->IO_ADDR_W + COMCERTO_NAND_ALE);
	 else
		return;

}

int comcerto_nand_ready(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct comcerto_nand_info *info = to_comerto_nand_info(chip);

	return gpiod_get_value(info->br_gpio);
}

/*********************************************************************
 * NAND Probe
 *
 *********************************************************************/
static int comcerto_nand_probe(struct platform_device *pdev)
{
	struct comcerto_nand_info *info;
	struct mtd_info *mtd;
	struct nand_chip *chip;
	struct resource *res;
	int err = 0;
	struct mtd_part_parser_data ppdata = {};

	/* Allocate memory for info structure */
	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	mtd = &info->mtd;
	chip = &info->chip;
	mtd->owner = THIS_MODULE;

	/* Link the private data with the MTD structure */
	mtd->priv = &info->chip;

	info->ce_gpio = devm_gpiod_get(&pdev->dev, "ce",
			GPIOD_FLAGS_BIT_DIR_SET | GPIOD_FLAGS_BIT_DIR_OUT);
	if (IS_ERR(info->ce_gpio)) {
		dev_err(&pdev->dev, "Failed to get CE GPIO.\n");
		goto out_info;
	}
	info->br_gpio = devm_gpiod_get(&pdev->dev, "br",
			GPIOD_FLAGS_BIT_DIR_SET);
	if (IS_ERR(info->br_gpio)) {
		dev_err(&pdev->dev, "Failed to get BR GPIO.\n");
		goto out_info;
	}

	/*Map physical address of nand into virtual space */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	chip->IO_ADDR_R = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(chip->IO_ADDR_R)) {
		pr_err("LS1024A NAND: cannot map nand memory\n");
		err = PTR_ERR(chip->IO_ADDR_R);
		goto out_info;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	ecc_base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ecc_base_addr)) {
		pr_err("LS1024A NAND: cannot map ecc config\n");
		err = PTR_ERR(chip->IO_ADDR_R);
		goto out_iorc;
	}

	/* This is the same address to read and write */
	chip->IO_ADDR_W = chip->IO_ADDR_R;

	/* Set address of hardware control function */
	chip->cmd_ctrl = comcerto_nand_hwcontrol;
	chip->dev_ready = comcerto_nand_ready;
	chip->ecc.mode = NAND_ECC_HW_SYNDROME;
//	chip->ecc.mode = NAND_ECC_SOFT_BCH;

#if defined(CONFIG_C2K_ASIC) && defined(CONFIG_NAND_TYPE_SLC)
	chip->options = NAND_BUSWIDTH_16;
#else
	chip->options = 0;
#endif

	/* Scan to find existence of the device */
	if (nand_scan_ident(mtd, 1, NULL)) {
		err = -ENXIO;
		goto out_ior;
	}

	if (1 && chip->ecc.mode == NAND_ECC_HW_SYNDROME) {
		/* We use a hybrid approach here where we use the ECC hardware
		 * in the read path to correct ECC errors on read. For writing,
		 * we calculate the ECC code in software. The reason for this
		 * is to work around a silicon bug where the ECC hardware would
		 * disturb and corrupt the NOR flash if the user tries to write
		 * to NOR flash and NAND flash concurrently.
		 */
		unsigned int m, t;

		chip->ecc.hwctl = comcerto_hybrid_hwctl;
		chip->ecc.calculate = comcerto_bch_calculate_ecc;
		chip->ecc.correct = comcerto_correct_ecc;
		dev_info(&pdev->dev, "Using hybrid hw/sw ECC\n");
		chip->ecc.size =  1024;
		chip->ecc.layout = &comcerto_ecc_info_1024_bch;
		chip->ecc.strength = 24;
		chip->ecc.prepad = 0;
		chip->ecc.postpad = 14;

		chip->ecc.steps = mtd->writesize / chip->ecc.size;
		if(chip->ecc.steps * chip->ecc.size != mtd->writesize) {
			dev_err(&pdev->dev, "Invalid ecc parameters\n");
			BUG();
		}
		chip->ecc.total = chip->ecc.steps * chip->ecc.bytes;
		chip->ecc.bytes = DIV_ROUND_UP(
				chip->ecc.strength * fls(8 * chip->ecc.size), 8);
		if (chip->ecc.bytes != 42) {
			dev_warn(&pdev->dev, "ecc->bytes != 42\n");
		}

		m = fls(1+8*chip->ecc.size);
		t = (chip->ecc.bytes*8)/m;

		/* The LS1024A HW ECC engine (seemingly licensed from Cyclic
		 * Design) uses 0x4443 as the primitive polynomial for BCH
		 * (Linux uses 0x402b by default). */
		info->cbc.bch = init_bch(m, t, 0x4443);
		if (!info->cbc.bch) {
			err = -EINVAL;
			goto out_ior;
		}

		/* verify that eccbytes has the expected value */
		if (info->cbc.bch->ecc_bytes != chip->ecc.bytes) {
			dev_warn(&pdev->dev, "invalid eccbytes %u, should be %u\n",
					chip->ecc.bytes, info->cbc.bch->ecc_bytes);
			free_bch(info->cbc.bch);
			err = -EINVAL;
			goto out_ior;
		}

		/* sanity checks */
		if (8*(chip->ecc.size+chip->ecc.bytes) >= (1 << m)) {
			dev_warn(&pdev->dev, "eccsize %u is too large\n", chip->ecc.size);
			free_bch(info->cbc.bch);
			err = -EINVAL;
			goto out_ior;
		}

#if 0
		info->cbc.errloc = devm_kmalloc(&pdev->dev, t*sizeof(*info->cbc.errloc), GFP_KERNEL);
		if (!info->cbc.errloc) {
			free_bch(info->cbc.bch);
			err = -ENOMEM;
			goto out_ior;
		}
#endif

		info->bit_reversed = devm_kzalloc(&pdev->dev, chip->ecc.size, GFP_KERNEL);
		if (!info->bit_reversed) {
			free_bch(info->cbc.bch);
			err = -ENOMEM;
			goto out_ior;
		}
		chip->bbt_td = &bbt_main_descr;
		chip->bbt_md = &bbt_mirror_descr;
		chip->badblock_pattern = &c2000_badblock_pattern;
		chip->bbt_options |= NAND_BBT_USE_FLASH;

	} else if (chip->ecc.mode == NAND_ECC_HW_SYNDROME) {
		chip->ecc.hwctl = comcerto_enable_hw_ecc;
		chip->ecc.write_page = comcerto_nand_write_page_hwecc;
		// chip->ecc.read_page = comcerto_nand_read_page_hwecc;
		chip->ecc.calculate = comcerto_calculate_ecc;
		chip->ecc.correct = comcerto_correct_ecc;
		pr_info("hw_syndrome correction %d.\n", mtd->writesize);

		switch (mtd->writesize) {
		case 512:
			chip->ecc.size = mtd->writesize;
#if defined (CONFIG_NAND_LS1024A_ECC_24_HW_BCH)
			chip->ecc.layout = &comcerto_ecc_info_512_bch;
			chip->ecc.bytes = 42;
			chip->ecc.strength = 24;
			chip->ecc.prepad = 0;
			chip->ecc.postpad = 14;
#elif defined(CONFIG_NAND_LS1024A_ECC_8_HW_BCH)
			chip->ecc.layout = &comcerto_ecc_info_512_bch;
			chip->ecc.bytes = 14;
			chip->ecc.strength = 8;
			chip->ecc.prepad = 0;
			chip->ecc.postpad = 2;
#else
			chip->ecc.layout = &comcerto_ecc_info_512_hamm;
			chip->ecc.bytes = 4;
			chip->ecc.prepad = 0;
			chip->ecc.postpad = 2;
#endif
			break;
		case 1024:
			chip->ecc.size = mtd->writesize;
#ifdef CONFIG_NAND_LS1024A_ECC_24_HW_BCH
			chip->ecc.layout = &comcerto_ecc_info_1024_bch;
			chip->ecc.bytes = 42;
			chip->ecc.strength = 24;
			chip->ecc.prepad = 0;
			chip->ecc.postpad = 14;
#elif defined(CONFIG_NAND_LS1024A_ECC_8_HW_BCH)
			chip->ecc.layout = &comcerto_ecc_info_1024_bch;
			chip->ecc.bytes = 14;
			chip->ecc.strength = 8;
			chip->ecc.prepad = 0;
			chip->ecc.postpad = 18;
#else
			chip->ecc.layout = &comcerto_ecc_info_1024_hamm;
			chip->ecc.bytes = 4;
			chip->ecc.prepad = 0;
			chip->ecc.postpad = 18;
#endif
			break;
		default:
			pr_err("Using default values for hw ecc\n");
			chip->ecc.size =  1024;
#ifdef CONFIG_NAND_LS1024A_ECC_24_HW_BCH
			chip->ecc.layout = &comcerto_ecc_info_1024_bch;
			chip->ecc.bytes = 42;
			chip->ecc.strength = 24;
			chip->ecc.prepad = 0;
			chip->ecc.postpad = 14;
#elif defined(CONFIG_NAND_LS1024A_ECC_8_HW_BCH)
			chip->ecc.layout = &comcerto_ecc_info_1024_bch;
			chip->ecc.bytes = 14;
			chip->ecc.strength = 8;
			chip->ecc.prepad = 0;
			chip->ecc.postpad = 18;
#else
			chip->ecc.layout = &comcerto_ecc_info_1024_hamm;
			chip->ecc.bytes = 4;
			chip->ecc.prepad = 0;
			chip->ecc.postpad = 18;
#endif
			break;
		}
		chip->ecc.steps = mtd->writesize / chip->ecc.size;
		if(chip->ecc.steps * chip->ecc.size != mtd->writesize) {
			pr_err("Invalid ecc parameters\n");
			BUG();
		}
		chip->ecc.total = chip->ecc.steps * chip->ecc.bytes;


		chip->bbt_td = &bbt_main_descr;
		chip->bbt_md = &bbt_mirror_descr;
		chip->badblock_pattern = &c2000_badblock_pattern;
		chip->bbt_options |= NAND_BBT_USE_FLASH;

	} else {
		pr_info("using soft ecc.\n");
		chip->ecc.size =  1024;
		chip->ecc.strength = 24;
		chip->ecc.bytes = 42;
		chip->ecc.mode = NAND_ECC_SOFT_BCH;
	}


	chip->options |= NAND_NO_SUBPAGE_WRITE;

	if(nand_scan_tail(mtd)) {
		pr_err("nand_scan_tail returned error\n");
		err = -ENXIO;
		goto out_ior;
	}

	/* Name of the mtd device */
	mtd->name = dev_name(&pdev->dev);

	/* Link the info stucture with platform_device */
	platform_set_drvdata(pdev, info);

	ppdata.of_node = pdev->dev.of_node;
	err = mtd_device_parse_register(mtd, NULL, &ppdata, NULL, 0);
	if (err) {
		nand_release(mtd);
		goto out_ior;
	}

	goto out;

      out_ior:
	devm_iounmap(&pdev->dev, ecc_base_addr);
      out_iorc:
	devm_iounmap(&pdev->dev, chip->IO_ADDR_R);
      out_info:
	devm_kfree(&pdev->dev, info);
      out:
	return err;
}

/*********************************************************************
 * NAND Remove
 *
 *********************************************************************/
static int comcerto_nand_remove(struct platform_device *pdev)
{
	struct comcerto_nand_info *info =
	    (struct comcerto_nand_info *)platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	/* Release resources, unregister device */
	nand_release(&info->mtd);

	/*Deregister virtual address */
	iounmap(info->chip.IO_ADDR_R);
	iounmap(ecc_base_addr);
	return 0;
}

/*********************************************************************
 * Driver Registration
 *
 *********************************************************************/

static const struct of_device_id fsl_ls1024a_nand_match[] = {
	{
		.compatible = "fsl,ls1024a-nand",
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, fsl_ls1024a_nand_match);

static struct platform_driver ls1024a_nand_driver = {
	.probe = comcerto_nand_probe,
	.remove = comcerto_nand_remove,
	.driver = {
		.name = "ls1024a_nand",
		.of_match_table = fsl_ls1024a_nand_match,
	},
};

module_platform_driver(ls1024a_nand_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Board-specific glue layer for NAND flash on Comcerto board");
