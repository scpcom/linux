/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Generic GPIO card-detect helper header
 *
 * Copyright (C) 2011, Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 */

#ifndef MMC_SLOT_GPIO_H
#define MMC_SLOT_GPIO_H

#include <linux/interrupt.h>
#include <linux/types.h>

struct mmc_host;

struct mmc_gpio {
	struct gpio_desc *ro_gpio;
	struct gpio_desc *cd_gpio;
	irq_handler_t cd_gpio_isr;
	char *ro_label;
	char *cd_label;
	u32 cd_debounce_delay_ms;
	int cd_irq;
};

int mmc_gpio_get_ro(struct mmc_host *host);
int mmc_gpio_get_cd(struct mmc_host *host);
void mmc_gpio_set_cd_irq(struct mmc_host *host, int irq);
int mmc_gpiod_request_cd(struct mmc_host *host, const char *con_id,
			 unsigned int idx, bool override_active_level,
			 unsigned int debounce);
int mmc_gpiod_request_ro(struct mmc_host *host, const char *con_id,
			 unsigned int idx, unsigned int debounce);
int mmc_gpiod_set_cd_config(struct mmc_host *host, unsigned long config);
void mmc_gpio_set_cd_isr(struct mmc_host *host, irq_handler_t isr);
int mmc_gpio_set_cd_wake(struct mmc_host *host, bool on);
void mmc_gpiod_request_cd_irq(struct mmc_host *host);
bool mmc_can_gpio_cd(struct mmc_host *host);
bool mmc_can_gpio_ro(struct mmc_host *host);

#endif
