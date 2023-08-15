/*
 * Zyxel NAS LEDs Driver
 *
 * Copyright (C) 2023 scpcom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Based on the gpio-restart driver and zyxel gpio driver.
 */
#include <linux/reboot.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#include "zyxel-nas-ctrl.h"

/** define the LED settings **/
#define JIFFIES_BLINK_VERYSLOW  (JIFFIES_1_SEC * 2)	// 2s
#define JIFFIES_BLINK_VERYSLOW_ON  (JIFFIES_1_SEC)	// 0.5s - according to the request from ZyXEL, HDD LED flash frequence: 0.5s on and 1.5s off
#define JIFFIES_BLINK_VERYSLOW_OFF  (JIFFIES_1_SEC * 4)	// 1.5s - according to the request from ZyXEL, HDD LED flash frequence: 0.5s on and 1.5s off
#define JIFFIES_BLINK_SLOW  (JIFFIES_1_SEC / 2)
#define JIFFIES_BLINK_FAST  (JIFFIES_1_SEC / 10)

#define LED_COLOR_BITS  0   // 0,1
#define LED_STATE_BITS  2   // 2,3,4,5,6,7
#define LED_NUM_BITS    8   // 8,9,10 ...

#define GET_LED_INDEX(map_addr) ((map_addr >> LED_NUM_BITS) & 0xf)
#define GET_LED_COLOR(map_addr) (map_addr & 0x3)
#define GET_LED_STATE(map_addr) ((map_addr >> LED_STATE_BITS) & 0x7)

enum LED_STATE {
	LED_STATIC_OFF = 0,
	LED_STATIC_ON,
	LED_BLINK_SLOW,
	LED_BLINK_FAST,
	LED_BLINK_VERYSLOW,
};

enum LED_COLOR {
	LED_RED = 0,
	LED_GREEN,
	LED_COLOR_TOTAL,    /* must be last one */
};

struct nas_led {
	unsigned int color;
	unsigned short state;       /* LED_STATIC_OFF, LED_STATIC_ON, LED_BLINK_SLOW, ... */

	unsigned short presence;    /* flag. 0: no such LED color */

#if IS_ENABLED(CONFIG_LEDS_TRIGGERS)
	struct led_trigger *trigger;
	char trigger_name[32];
	unsigned char out_val;
#endif
};

struct nas_led_set {
	unsigned int id;            /* LED ID, LED type */
	char name[32];
	struct nas_led led[LED_COLOR_TOTAL];

	struct nas_ctrl_timer_list timer;
	unsigned short timer_state;
	unsigned short blink_state; /* Binary state, it must be 0 (off) or 1 (on) to present the blinking state */

	spinlock_t lock;

	unsigned short presence;    /* flag. 0: no such LED */
};

static void led_timer_handler(unsigned long);

static struct nas_led_set led_set[LED_TOTAL] = {
	[LED_HDD1] = {
		.presence = 1,
		.id = LED_HDD1,
		.name = "HDD1 LED",
		.timer = {
			.data = 0,
			.function = led_timer_handler,
		},
		.timer_state = TIMER_OFFLINE,
		.led[LED_RED] = {
			.presence = 1,
			.color = RED,
			.trigger_name = "hdd1-err",
		},
		.led[LED_GREEN] = {
			.presence = 1,
			.color = GREEN,
			.trigger_name = "hdd1-act",
		},
	},
	[LED_HDD2] = {
		.presence = 1,
		.id = LED_HDD2,
		.name = "HDD2 LED",
		.timer = {
			.data = 0,
			.function = led_timer_handler,
		},
		.timer_state = TIMER_OFFLINE,
		.led[LED_RED] = {
			.presence = 1,
			.color = RED,
			.trigger_name = "hdd2-err",
		},
		.led[LED_GREEN] = {
			.presence = 1,
			.color = GREEN,
			.trigger_name = "hdd2-act",
		},
	},
	[LED_HDD3] = {
		.presence = 1,
		.id = LED_HDD3,
		.name = "HDD3 LED",
		.timer = {
			.data = 0,
			.function = led_timer_handler,
		},
		.timer_state = TIMER_OFFLINE,
		.led[LED_RED] = {
			.presence = 1,
			.color = RED,
			.trigger_name = "hdd3-err",
		},
		.led[LED_GREEN] = {
			.presence = 1,
			.color = GREEN,
			.trigger_name = "hdd3-act",
		},
	},
	[LED_HDD4] = {
		.presence = 1,
		.id = LED_HDD4,
		.name = "HDD4 LED",
		.timer = {
			.data = 0,
			.function = led_timer_handler,
		},
		.timer_state = TIMER_OFFLINE,
		.led[LED_RED] = {
			.presence = 1,
			.color = RED,
			.trigger_name = "hdd4-err",
		},
		.led[LED_GREEN] = {
			.presence = 1,
			.color = GREEN,
			.trigger_name = "hdd4-act",
		},
	},
	[LED_SYS] = {
		.presence = 1,
		.id = LED_SYS,
		.name = "SYS LED",
		.timer = {
			.data = 0,
			.function = led_timer_handler,
		},
		.timer_state = TIMER_OFFLINE,
		.led[LED_RED] = {
			.presence = 1,
			.color = RED,
			.trigger_name = "sys-err",
		},
		.led[LED_GREEN] = {
			.presence = 1,
			.color = GREEN,
			.trigger_name = "sys-act",
		},
	},
	[LED_COPY] = {
		.presence = 1,
		.id = LED_COPY,
		.name = "COPY LED",
		.timer = {
			.data = 0,
			.function = led_timer_handler,
		},
		.timer_state = TIMER_OFFLINE,
		.led[LED_RED] = {
			.presence = 1,
			.color = RED,
			.trigger_name = "copy-err",
		},
		.led[LED_GREEN] = {
			.presence = 1,
			.color = GREEN,
			.trigger_name = "copy-act",
		},
	},
};


static unsigned char get_led_value(struct nas_led* led)
{
#if IS_ENABLED(CONFIG_LEDS_TRIGGERS)
	return (led->out_val == LED_OFF) ? 0 : 1;
#else
	return 0;
#endif
}

static void set_led_value(struct nas_led* led, unsigned char out_val)
{
#if IS_ENABLED(CONFIG_LEDS_TRIGGERS)
	led->out_val = out_val ? LED_FULL : LED_OFF;
	if (led->trigger)
		led_trigger_event(led->trigger, led->out_val);
#endif
}

void turn_on_led(unsigned int id, unsigned int color)
{
	int i;

	/* System does not have LED_SET[id] */
	if (led_set[id].presence == 0) return;

	for (i = 0; i < LED_COLOR_TOTAL; i++) {
		if ((color & led_set[id].led[i].color) && (led_set[id].led[i].presence != 0)) {
			led_set[id].led[i].state = LED_STATIC_ON;
			set_led_value(&led_set[id].led[i], 1);
		}
	}
}

void turn_off_led(unsigned int id)
{
	int i;

	/* System does not have LED_SET[id] */
	if (led_set[id].presence == 0) return;

	for (i = 0; i < LED_COLOR_TOTAL; i++) {
		if (led_set[id].led[i].presence != 0) {
			led_set[id].led[i].state = LED_STATIC_OFF;
			set_led_value(&led_set[id].led[i], 0);
		}
	}
}

void turn_off_led_all(unsigned int id)
{
	int i;

	/* System does not have LED_SET[id] */
	if (led_set[id].presence == 0) return;

	for (i = 0; i < LED_COLOR_TOTAL; i++) {
		if (led_set[id].led[i].presence == 0) continue;

		led_set[id].led[i].state = LED_STATIC_OFF;
		set_led_value(&led_set[id].led[i], 0);
	}
}

void led_all_red_on(void)
{
	int i = 0;

	for (i = 0; i < LED_TOTAL; i++)
	{
		turn_off_led(i);
		turn_on_led(i, RED);
	}
}

void led_all_colors_off(void)
{
	int i = 0;

	for (i = 0; i < LED_TOTAL; i++)
	{
		turn_off_led(i);
	}
}

//turn on to off, turn off to on
void reverse_on_off_led(unsigned int id, unsigned int color)
{
	int i;

	/* System does not have LED_SET[id] */
	if (led_set[id].presence == 0) return;

	for (i = 0; i < LED_COLOR_TOTAL; i++) {
		if ((color & led_set[id].led[i].color) && (led_set[id].led[i].presence != 0)) {
			set_led_value(&led_set[id].led[i], !get_led_value(&led_set[id].led[i]));
		}
	}
}

static void led_blink_start(unsigned int id, unsigned int color, unsigned int state)
{
	int i;
	unsigned long expire_period[] = { 0, 0, JIFFIES_BLINK_SLOW, JIFFIES_BLINK_FAST, JIFFIES_BLINK_VERYSLOW};
	unsigned long slow_expire_period[] = {JIFFIES_BLINK_VERYSLOW_OFF, JIFFIES_BLINK_VERYSLOW_ON};
	short led_color;

	if (led_set[id].presence == 0) return;
//	if ((state != LED_BLINK_SLOW) && (state != LED_BLINK_FAST) && (state != LED_BLINK_VERYSLOW)) return;
	if (expire_period[state] == 0) return;

	spin_lock(&(led_set[id].lock));

	if (led_set[id].timer_state == TIMER_OFFLINE) {
		nas_ctrl_init_timer(&led_set[id].timer, led_timer_handler, (unsigned long)&led_set[id]);
		led_set[id].timer_state = TIMER_SLEEPING;
	}

	if (led_set[id].timer_state == TIMER_RUNNING) {
		/* Maybe there is already a timer running, restart one. */
		led_set[id].timer_state = TIMER_SLEEPING;
		nas_ctrl_del_timer(&(led_set[id].timer));
	}

	for (i = 0; i < LED_COLOR_TOTAL; i++) {
		if ((color & led_set[id].led[i].color) && (led_set[id].led[i].presence != 0)) {
			led_set[id].led[i].state = state;
			led_color = i;
		}
	}

	led_set[id].timer_state = TIMER_RUNNING;

	if (state == LED_BLINK_VERYSLOW)	// according to the request from ZyXEL, HDD LED flash frequence: 0.5s on and 1.5s off
		for (i = LED_HDD1; i <= LED_HDD4; i++)
		{
			if (led_set[i].led[led_color].state == LED_BLINK_VERYSLOW)
			{
				led_set[i].blink_state = 0;		// synchronize all blink state of the HDD leds blinked very slow

				if (led_set[i].timer_state == TIMER_OFFLINE)
					continue;
				if (i != id)					// reset the led timer for other HDDs
					nas_ctrl_del_timer(&(led_set[i].timer));

				nas_ctrl_mod_timer(&led_set[i].timer, jiffies + slow_expire_period[led_set[i].blink_state]);
			}
		}
	else
		nas_ctrl_mod_timer(&led_set[id].timer, jiffies + expire_period[state]);

#if 0
	if (state == LED_BLINK_FAST)
		nas_ctrl_mod_timer(&led_set[id].timer, jiffies + JIFFIES_BLINK_FAST);
	else if (state == LED_BLINK_SLOW)
		nas_ctrl_mod_timer(&led_set[id].timer, jiffies + JIFFIES_BLINK_SLOW);
#endif

	spin_unlock(&(led_set[id].lock));
}

static void led_blink_stop(unsigned int id)
{
	int i;

	if (led_set[id].presence == 0) return;

	spin_lock(&(led_set[id].lock));

	for (i = 0; i < LED_COLOR_TOTAL; i++) {
		if (led_set[id].led[i].presence == 0) continue;
		led_set[id].led[i].state = LED_STATIC_OFF;
	}

	if (led_set[id].timer_state == TIMER_RUNNING) {
		nas_ctrl_del_timer(&(led_set[id].timer));
	}
	if (led_set[id].timer_state != TIMER_OFFLINE)
		led_set[id].timer_state = TIMER_SLEEPING;

	spin_unlock(&(led_set[id].lock));
}

/* all LED_SET[] timer handler for blinking */
static void led_timer_handler(unsigned long data)
{
	struct nas_led_set*_led_set = (struct nas_led_set*) data;
	int state = LED_BLINK_SLOW;
	int i;
	unsigned long expire_period[] = { 0, 0, JIFFIES_BLINK_SLOW, JIFFIES_BLINK_FAST, JIFFIES_BLINK_VERYSLOW};
	unsigned long slow_expire_period[] = {JIFFIES_BLINK_VERYSLOW_OFF, JIFFIES_BLINK_VERYSLOW_ON};

	spin_lock(&(_led_set->lock));

	if (_led_set->timer_state == TIMER_RUNNING) {
		/* Maybe there is already a timer running, restart one. */
		_led_set->timer_state = TIMER_SLEEPING;
		nas_ctrl_del_timer(&(_led_set->timer));
	}

	/* Invert the previous blinking state for next state. */
	_led_set->blink_state ^= 1;

	for (i = 0; i < LED_COLOR_TOTAL; i++) {
		if (_led_set->led[i].presence == 0) continue;

//		if ((_led_set->led[i].state == LED_BLINK_FAST) || (_led_set->led[i].state == LED_BLINK_SLOW)) {
		if (expire_period[_led_set->led[i].state] != 0) {

			state = _led_set->led[i].state;

			if (_led_set->blink_state == 0) {
				set_led_value(&_led_set->led[i], 0);
			} else {
				set_led_value(&_led_set->led[i], 1);
			}
		}
	}

	_led_set->timer_state = TIMER_RUNNING;

	if (state == LED_BLINK_VERYSLOW)
		nas_ctrl_mod_timer(&_led_set->timer, jiffies + slow_expire_period[_led_set->blink_state]);
	else
		nas_ctrl_mod_timer(&_led_set->timer, jiffies + expire_period[state]);

#if 0
	if (state == LED_BLINK_FAST)
		nas_ctrl_mod_timer(&_led_set->timer, jiffies + JIFFIES_BLINK_FAST);
	else if (state == LED_BLINK_SLOW)
		nas_ctrl_mod_timer(&_led_set->timer, jiffies + JIFFIES_BLINK_SLOW);
#endif

	spin_unlock(&(_led_set->lock));
}

#ifdef CONFIG_ZYXEL_HDD_EXTENSIONS
static void hdd_led_blinking_timer(unsigned long in_data)
{
	struct nas_led_set *_led_set = (struct nas_led_set*) in_data;
	int led_num = _led_set->id;

	int index = 0;
	switch(led_num)
	{
		case LED_HDD1:
			index = 0;
			break;
		case LED_HDD2:
			index = 1;
			break;
		case LED_HDD3:
			index = 2;
			break;
		case LED_HDD4:
			index = 3;
			break;
		default:
			break;
	}


	if(atomic_read(&sata_badblock_idf[index]) == 1) {
		atomic_set(&sata_hd_accessing[index], 0);
		atomic_set(&sata_hd_stop[index], 1);
	}

	if(atomic_read(&sata_hd_accessing[index]) != 0) {
		atomic_set(&sata_hd_stop[index], 0);
		reverse_on_off_led(led_num, GREEN);
		nas_ctrl_mod_timer(&_led_set->timer, jiffies + (HZ)/(4+atomic_read(&sata_hd_accessing[index])));
		atomic_set(&sata_hd_accessing[index], 0);
	} else {
		if (atomic_read(&sata_hd_stop[index]) == 0) {
			turn_off_led_all(led_num);
			turn_on_led(led_num, GREEN);
			atomic_set(&sata_hd_stop[index], 1);
		}
		nas_ctrl_mod_timer(&_led_set->timer, jiffies + (HZ >> 3));
	}
}

int get_hdd_led_num(int index)
{
	int led_num = 0;
	switch(index)
	{
		case 0:
			led_num = LED_HDD1;
			break;
		case 1:
			led_num = LED_HDD2;
			break;
		case 2:
			led_num = LED_HDD3;
			break;
		case 3:
			led_num = LED_HDD4;
			break;
		default:
			break;
	}
	return led_num;
}

static void init_hdd_led_timer(int index)
{
	int i = get_hdd_led_num(index);

	if (led_set[i].timer_state == TIMER_OFFLINE)
	{
		nas_ctrl_init_timer(&led_set[i].timer, hdd_led_blinking_timer, (unsigned long)&led_set[i]);
		led_set[i].timer_state = TIMER_SLEEPING;
	}
	nas_ctrl_mod_timer(&led_set[i].timer, jiffies + (HZ >> 2));
	led_set[i].timer_state = TIMER_RUNNING;
}

int init_hdd_led_control()
{
	int i;

	if (!hdd_workqueue)
		hdd_workqueue = create_singlethread_workqueue("harddrive_hdd");
	if (!hdd_workqueue) {
		destroy_workqueue(hdd_workqueue);
		printk("Error, init disk I/O badblock handler error\n");
		return -ENOMEM;
	}

	/* for STG540 init sata led control */
	for(i = 0 ; i < nas_ctrl_get_drive_bays_count(); i++) {
		atomic_set(&sata_hd_accessing[i], 0);
		atomic_set(&sata_hd_stop[i], 1);
		atomic_set(&sata_badblock_idf[i], 0);

		init_hdd_led_timer(i);
	}

	/* init sata detect error parameter */
	atomic_set(&sata_device_num, -1);

	return 0;
}

void exit_hdd_led_control()
{
	destroy_workqueue(hdd_workqueue);
}
#endif

int set_led_config(unsigned long led_data)
{
	unsigned long led_index, color, state;

	led_index = GET_LED_INDEX(led_data);
	color = GET_LED_COLOR(led_data);
	state = GET_LED_STATE(led_data);

	/* check the value range of LED_SET type */
	if ((led_index < 0) || (led_index >= LED_TOTAL)) return -ENOTTY;

	/* check the LED_SET presence */
	if (led_set[led_index].presence == 0) return -ENOTTY;

	switch (state) {
		case LED_STATIC_OFF:
			led_blink_stop(led_index);
			turn_off_led_all(led_index);
			break;
		case LED_STATIC_ON:
			led_blink_stop(led_index);
			turn_on_led(led_index, color);
			break;
		case LED_BLINK_SLOW:
		case LED_BLINK_FAST:
		case LED_BLINK_VERYSLOW:
			turn_off_led_all(led_index);
			led_blink_start(led_index, color, state);
	}

	return 0;
}

#if IS_ENABLED(CONFIG_LEDS_TRIGGERS)
static void init_led_triggers(void)
{
	int i, j;
	for (i = 0; i < LED_TOTAL; i++) {
		for (j = 0; j < LED_COLOR_TOTAL; j++) {
			struct nas_led* led =  &led_set[i].led[j];

			if (!led->trigger)
				led_trigger_register_simple(led->trigger_name, &led->trigger);

			if (led->state == LED_STATIC_ON)
				set_led_value(led, 1);
		}
	}
}
#endif


static void nas_leds_set_init_timer(void)
{
	int i;

	// init leds timer
	for (i = 0; i < LED_TOTAL; i++) {
		if (led_set[i].presence == 0) continue;

		led_set[i].timer_state = TIMER_OFFLINE;
	}
}

static int nas_leds_probe(struct platform_device *pdev)
{
	nas_leds_set_init_timer();

	return 0;
}

static const struct of_device_id of_nas_leds_match[] = {
	{ .compatible = "zyxel,nas-leds", },
	{},
};

static struct platform_driver nas_leds_driver = {
	.probe = nas_leds_probe,
	.driver = {
		.name = "nas-leds",
		.of_match_table = of_nas_leds_match,
	},
};

module_platform_driver(nas_leds_driver);

#if IS_ENABLED(CONFIG_LEDS_TRIGGERS)
static int __init nas_led_triggers_init(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "zyxel,nas-leds");
	if (!node)
		return 0;

	init_led_triggers();

	return 0;
}
subsys_initcall(nas_led_triggers_init);
#endif

MODULE_AUTHOR("scpcom <scpcom@gmx.de>");
MODULE_DESCRIPTION("Zyxel NAS LEDs driver");
MODULE_LICENSE("GPL");
