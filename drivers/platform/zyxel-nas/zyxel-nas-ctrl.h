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

#define TIMER_OFFLINE   0x2
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

#ifdef CONFIG_ZYXEL_NAS_KEYS
void set_btncpy_pid(unsigned long arg);
void set_button_test(unsigned long arg, bool in);
#endif

enum LED_ID {
	LED_HDD1 = 0,
	LED_HDD2,
	LED_HDD3,
	LED_HDD4,
	LED_SYS,
	LED_COPY,
	LED_TOTAL,
};

#define RED 	    (1<<0)
#define GREEN       (2<<0)
#define ORANGE      (RED | GREEN)
#define NO_COLOR	0

#ifdef CONFIG_ZYXEL_NAS_LEDS
int set_led_config(unsigned long led_data);
void turn_on_led(unsigned int id, unsigned int color);
void turn_off_led(unsigned int id);
void turn_off_led_all(unsigned int id);
void reverse_on_off_led(unsigned int id, unsigned int color);
void led_all_colors_off(void);
void led_all_red_on(void);
#else
static inline void turn_on_led(unsigned int id, unsigned int color) {}
static inline void turn_off_led(unsigned int id) {}
static inline void turn_off_led_all(unsigned int id) {}
static inline void reverse_on_off_led(unsigned int id, unsigned int color) {}
static inline void led_all_colors_off(void) {}
static inline void led_all_red_on(void) {}
#endif

#ifdef CONFIG_ZYXEL_NAS_PWMS
void set_buzzer(unsigned long bz_data);
void Beep(void);
void Beep_Beep(int duty_high, int duty_low);
#else
static inline void Beep(void) {}
static inline void Beep_Beep(int duty_high, int duty_low) {}
#endif

#endif
