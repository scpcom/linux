// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Spacemit Mobile Storage Host Controller
 *
 * Copyright (C) 2023 Spacemit
 */
#ifndef __SPACEMIT_SDHCI_H__
#define __SPACEMIT_SDHCI_H__

struct sdhci_host;

extern void spacemit_save_sdhci_regs(struct sdhci_host *host, u32 cmd);
extern void spacemit_sdio_detect_change(int enable_scan);

#endif
