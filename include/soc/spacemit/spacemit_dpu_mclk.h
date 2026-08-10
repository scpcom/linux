// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Spacemit Co., Ltd.
 *
 */
#ifndef __SPACEMIT_DPU_MCLK_H__
#define __SPACEMIT_DPU_MCLK_H__

extern bool dpu_mclk_exclusive_get(void);
extern void dpu_mclk_exclusive_put(void);

#endif
