/*
 * spacemit-wlan.h -- power on/off wlan part of SoC
 *
 * Copyright 2023, Spacemit Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __SPACEMIT_WLAN_H__
#define __SPACEMIT_WLAN_H__

extern void spacemit_wlan_set_power(bool on_off);
extern int spacemit_wlan_get_oob_irq(void);
extern int spacemit_wlan_get_oob_irq_flags(void);

#endif
