// SPDX-License-Identifier: GPL-2.0
/*
 * Spacemit k1x PCIe rc && ep driver
 *
 * Copyright (c) 2023, spacemit Corporation.
 *
 */
#ifndef _PCIE_K1X_H
#define _PCIE_K1X_H

struct k1x_pcie;

int k1x_pcie_enable_clocks(struct k1x_pcie *k1x);
void k1x_pcie_disable_clocks(struct k1x_pcie *k1x);

#endif /* _PCIE_K1X_H */
