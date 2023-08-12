// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 SCP
 *
 *  Based on Zyxel NAS540 vendor kernel.
 */

#ifndef __ZYXEL_NAS_CTRL_H__
#define __ZYXEL_NAS_CTRL_H__

/* define the magic number for ioctl used */
#define BTNCPY_IOC_MAGIC 'g'

#define JIFFIES_1_SEC       (HZ)        /* 1 second */

#define TIMER_RUNNING   0x1
#define TIMER_SLEEPING  0x0

struct nas_ctrl_timer_list {
	struct timer_list timer;
	void (*function)(unsigned long);
	unsigned long data;
};

void nas_ctrl_init_timer(struct nas_ctrl_timer_list *ptimer, void *pfunc, unsigned long cntx);
int nas_ctrl_mod_timer(struct nas_ctrl_timer_list *ptimer, unsigned long expires);
int nas_ctrl_del_timer(struct nas_ctrl_timer_list * ptimer);
int nas_ctrl_del_timer_sync(struct nas_ctrl_timer_list *ptimer);
int nas_ctrl_timer_pending(const struct nas_ctrl_timer_list *ptimer);
int run_usermode_cmd(const char *cmd);

#ifdef CONFIG_ZYXEL_NAS_PWMS
void set_buzzer(unsigned long bz_data);
void Beep(void);
void Beep_Beep(int duty_high, int duty_low);
#else
static inline void Beep(void) {}
static inline void Beep_Beep(int duty_high, int duty_low) {}
#endif

#endif
