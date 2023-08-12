/*
 * Zyxel NAS Keys Driver
 *
 * Copyright (C) 2023 scpcom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Based on the zyxel gpio driver.
 */
#include <linux/reboot.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>

#include "zyxel-nas-ctrl.h"

#define QUICK_PRESS_TIME    1
#define PRESS_TIME      5

#define BUTTON_NUM      3

enum BUTTON_NUMBER {
	RESET_BTN_NUM,
	COPY_BTN_NUM,
	POWER_BTN_NUM,
};

static void btnpow_timer_func(unsigned long in_data);
static void btnreset_timer_func(unsigned long in_data);
static void btncpy_timer_func(unsigned long in_data);

struct nas_btn {
	unsigned int id;            /* BTN ID, BTN type */
	char name[32];

	struct gpio_desc *gpiod;
	atomic_t pressed;

	struct nas_ctrl_timer_list timer;
	int polling_times;

	unsigned short presence;    /* flag. 0: no such BTN */
};

struct nas_btn btn_set[BUTTON_NUM] = {
	[RESET_BTN_NUM] = {
		.presence = 1,
		.id = RESET_BTN_NUM,
		.name = "Reset Key",
		.timer = {
			.data = 0,
			.function = btnreset_timer_func,
		},
		.pressed = ATOMIC_INIT(0),
		.polling_times = 0,
	},
	[COPY_BTN_NUM] = {
		.presence = 1,
		.id = COPY_BTN_NUM,
		.name = "Copy Key",
		.timer = {
			.data = 0,
			.function = btncpy_timer_func,
		},
		.pressed = ATOMIC_INIT(0),
		.polling_times = 0,
	},
	[POWER_BTN_NUM] = {
		.presence = 1,
		.id = POWER_BTN_NUM,
		.name = "Power Key",
		.timer = {
			.data = 0,
			.function = btnpow_timer_func,
		},
		.pressed = ATOMIC_INIT(0),
		.polling_times = 0,
	},
};

static atomic_t button_test_enable = ATOMIC_INIT(0);
static atomic_t button_test_num = ATOMIC_INIT(BUTTON_NUM);
static atomic_t halt_one_time = ATOMIC_INIT(0);

struct workqueue_struct *btn_workqueue;

// trigger signal to button daemon in user space
static int  btncpy_pid = 0;
void btncpy_signal_func10(struct work_struct *in);
void btncpy_signal_func12(struct work_struct *in);
void btncpy_signal_func14(struct work_struct *in);
void nsa_shutdown_func(struct work_struct *in);
void Reset_UserInfo_func(struct work_struct *in);
void Open_Backdoor_func(struct work_struct *in);
void Reset_To_Defu_func(struct work_struct *in);
static DECLARE_WORK(btncpy_signal10, btncpy_signal_func10);
static DECLARE_WORK(btncpy_signal12, btncpy_signal_func12);
static DECLARE_WORK(btncpy_signal14, btncpy_signal_func14);
static DECLARE_WORK(halt_nsa, nsa_shutdown_func);
static DECLARE_WORK(Reset_User_Info, Reset_UserInfo_func);
static DECLARE_WORK(Open_Backdoor, Open_Backdoor_func);
static DECLARE_WORK(Reset_To_Default, Reset_To_Defu_func);


static unsigned char get_btn_input(struct nas_btn* btn)
{
	if (btn->gpiod)
		return gpiod_get_value(btn->gpiod);
	else
		return 0;
}

void btncpy_signal_func10(struct work_struct *in)
{
	sys_kill(btncpy_pid, 10);
}

void btncpy_signal_func12(struct work_struct *in)
{
	sys_kill(btncpy_pid, 12);
}

void btncpy_signal_func14(struct work_struct *in)
{
	sys_kill(btncpy_pid, 14);
}

void nsa_shutdown_func(struct work_struct *in)
{
	if(atomic_read(&halt_one_time) == 0)
	{
		int ret;
		ret = run_usermode_cmd("/sbin/poweroff");
		if (ret < 0)
			printk("%s: halt failed: %d\n", __func__, ret);
	}
}

void Reset_UserInfo_func(struct work_struct *in)
{
	run_usermode_cmd("/bin/sh /usr/local/btn/reset_userinfo.sh");
}

void Open_Backdoor_func(struct work_struct *in)
{
	run_usermode_cmd("/bin/sh /usr/local/btn/open_back_door.sh");
}

void Reset_To_Defu_func(struct work_struct *in)
{
	ssleep(1);

	Beep();
	ssleep(1);

	Beep();
	ssleep(1);

	Beep();
	ssleep(1);

	run_usermode_cmd("/bin/sh /usr/local/btn/reset_and_reboot.sh");
}

void zyxel_power_off(void)
{
	printk(KERN_ERR"GPIO[15] is pull high for power off\n");

	if (pm_power_off)
		pm_power_off();
}

static void btnpow_timer_func(unsigned long in_data)
{
	struct nas_btn *_btn = (struct nas_btn*) in_data;

	if (get_btn_input(_btn)) {
		/* handle the button pressed behavior */
		atomic_set(&_btn->pressed, 1);
		++_btn->polling_times;
		if(_btn->polling_times  == (QUICK_PRESS_TIME << 3) || _btn->polling_times  == (PRESS_TIME << 3)) Beep();
	} else {
		if (atomic_read(&_btn->pressed)) {
			if(atomic_read(&button_test_enable) &&
				(atomic_read(&button_test_num) == _btn->id))
			{
				/* handle the testButton for HTP */
				atomic_set(&button_test_enable, 0);
				queue_work(btn_workqueue, &btncpy_signal10);
			}
			else
			{
				/* handle the NAS behavior */
				if(_btn->polling_times >= (QUICK_PRESS_TIME << 3)  && _btn->polling_times  < (PRESS_TIME << 3))
				{
					printk("%s: halt nsa\n", __func__);
					queue_work(btn_workqueue,&halt_nsa);
				} else if(_btn->polling_times >= (PRESS_TIME << 3)) {
					printk(KERN_ERR"Power Off\n");
					zyxel_power_off();
				}
			}
			_btn->polling_times = 0;
			atomic_set(&_btn->pressed, 0);
		}
	}
	nas_ctrl_mod_timer(&_btn->timer, jiffies + (JIFFIES_1_SEC >> 3));
}

static void btnreset_timer_func(unsigned long in_data)
{
	struct nas_btn *_btn = (struct nas_btn*) in_data;

	if (get_btn_input(_btn)) {
		/* handle the button pressed behavior */
		atomic_set(&_btn->pressed, 1);
		++_btn->polling_times;
		if(_btn->polling_times == (10 << 3)) Beep();
		else if(_btn->polling_times == (6 << 3)) Beep();
		else if(_btn->polling_times == (2 << 3)) Beep();
		else;
	} else {
		if (atomic_read(&_btn->pressed)) {
			if(atomic_read(&button_test_enable) &&
				(atomic_read(&button_test_num) == _btn->id))
			{
				/* handle the testButton for HTP */
				atomic_set(&button_test_enable, 0);
				queue_work(btn_workqueue, &btncpy_signal10);
			}
			else
			{
				/* handle the NAS behavior */
				if(_btn->polling_times >= (2 << 3) && _btn->polling_times  <= (3 << 3))
				{
					printk(KERN_INFO"Reset admin password & ip setting ........\n");  // May move to Reset_UserInfo_func
					queue_work(btn_workqueue, &Reset_User_Info);
				}
				else if(_btn->polling_times >= (6 << 3) && _btn->polling_times <= (7 << 3))
				{
					printk(KERN_INFO"Open backdoor ... \n");
					queue_work(btn_workqueue, &Open_Backdoor);
				}
				else if(_btn->polling_times >= (10 << 3))
				{
					printk(KERN_INFO"remove configuration (etc/zyxel/config) and reboot\n");

					queue_work(btn_workqueue, &Reset_To_Default);
				}
				else ;
			}
			_btn->polling_times = 0;
			atomic_set(&_btn->pressed, 0);
		}
	}

	nas_ctrl_mod_timer(&_btn->timer, jiffies + (JIFFIES_1_SEC >> 3));
}

static void btncpy_timer_func(unsigned long in_data)
{
	struct nas_btn *_btn = (struct nas_btn*) in_data;

	if (get_btn_input(_btn)) {
		/* handle the button pressed behavior */
		atomic_set(&_btn->pressed, 1);
		++_btn->polling_times;
		if(_btn->polling_times == (3 << 3))
		{
			//Sync Beep
			Beep();
		} else if(_btn->polling_times == (6 << 3)) {
			//Cancel Beep Beep
			Beep_Beep(500,500);
			Beep_Beep(500,500);
		} else if(_btn->polling_times == (30 << 3)) {
			//Reset Beep
			Beep();
		} else;
	} else {
		if (atomic_read(&_btn->pressed)) {
			if(atomic_read(&button_test_enable) &&
				(atomic_read(&button_test_num) == _btn->id))
			{
				/* handle the testButton for HTP */
				atomic_set(&button_test_enable, 0);
				queue_work(btn_workqueue, &btncpy_signal10);
			}
			else
			{
				/* handle the NAS behavior */
				if(btncpy_pid)
				{
					if(_btn->polling_times >= (6 << 3) && _btn->polling_times < (30 << 3)) {
						//printk(KERN_ERR"btncpy cancel button\n");
						queue_work(btn_workqueue, &btncpy_signal14);
					}
					else if(_btn->polling_times >= (3 << 3) && _btn->polling_times < (6 << 3)) {
						//printk(KERN_ERR"btncpy sync button\n");
						queue_work(btn_workqueue, &btncpy_signal12);
					}
					else
					{
						if(atomic_read(&button_test_enable) == 0) {
							//printk(KERN_ERR"btncpy copy button\n");
							queue_work(btn_workqueue, &btncpy_signal10);
						}
					}
				}
			}
			_btn->polling_times = 0;
			atomic_set(&_btn->pressed, 0);
		}
	}

	nas_ctrl_mod_timer(&_btn->timer, jiffies + (JIFFIES_1_SEC >> 3));
}

static void set_init_keys_timer(void)
{
	int i;

	// init button timers
	for (i = 0; i < BUTTON_NUM; i++) {
		if (btn_set[i].presence == 0) continue;

		nas_ctrl_init_timer(&btn_set[i].timer, btn_set[i].timer.function, (unsigned long)&btn_set[i]);
		nas_ctrl_mod_timer(&btn_set[i].timer, jiffies + (JIFFIES_1_SEC >> 3));
	}
}


static int init_gpiod_input(struct gpio_desc *gpiod, char* name)
{
	int ret;

	ret = gpiod_direction_input(gpiod);
	if (ret < 0)
		return ret;

	/* Set gpiod label to match the corresponding name. */
	gpiod_set_consumer_name(gpiod, name);

	return ret;
}

static int assign_btn_gpiod(struct gpio_desc *gpiod, char* name)
{
	struct nas_btn* btn = NULL;
	int ret;

	if (strcmp(name, "GPIO Key Power") == 0) {
		btn = &btn_set[POWER_BTN_NUM];
	} else if (strcmp(name, "GPIO Key Reset") == 0) {
		btn = &btn_set[RESET_BTN_NUM];
	} else if (strcmp(name, "GPIO Key Copy") == 0) {
		btn = &btn_set[COPY_BTN_NUM];
	}

	if (!btn)
		return -ENODEV;

	ret = init_gpiod_input(gpiod, name);
	if (ret < 0)
		return ret;

	btn->gpiod = gpiod;
	printk("%s: assigned %s\n", __func__, name);

	return 0;
}

static inline struct fwnode_handle *of_node_to_fwnode(struct device_node *node)
{
	return node ? &node->fwnode : NULL;
}

#define GPIO_MAX_NAME_SIZE 64

struct gpio_dev_data {
	struct gpio_desc *gpiod;
	const char	*label;
};

struct gpio_list_priv {
	int num_gpios;
	struct gpio_dev_data gpios[];
};

int parse_gpio_nodes(struct platform_device *pdev, struct device_node *np)
{
	struct device *dev = &pdev->dev;
	struct fwnode_handle *parent = of_node_to_fwnode(np);
	struct fwnode_handle *child;
	struct gpio_list_priv *priv;
	char classdev_name[GPIO_MAX_NAME_SIZE];
	int count, ret;

	count = of_get_child_count(np);
	if (!count)
		return -ENODEV;

	priv = devm_kzalloc(dev, struct_size(priv, gpios, count), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	fwnode_for_each_child_node(parent, child) {
		struct gpio_dev_data *gpio_dat = &priv->gpios[priv->num_gpios];

		/*
		 * Acquire gpiod from DT with uninitialized label, which
		 * will be updated later.
		 */
		gpio_dat->gpiod = devm_fwnode_get_gpiod_from_child(dev, NULL, child,
							     GPIOD_ASIS,
							     NULL);
		if (IS_ERR(gpio_dat->gpiod)) {
			ret = PTR_ERR(gpio_dat->gpiod);
			fwnode_handle_put(child);
			return ret;
		}

		if (fwnode_property_present(child, "label")) {
			ret = fwnode_property_read_string(child, "label", &gpio_dat->label);
			if (ret)
				dev_err(dev, "Error parsing 'label' property (%d)\n", ret);
		}
		if (gpio_dat->label) {
			strscpy(classdev_name, gpio_dat->label,
				GPIO_MAX_NAME_SIZE);
	        } else if (is_of_node(child)) {
			strscpy(classdev_name, to_of_node(child)->name,
				GPIO_MAX_NAME_SIZE);
		} else {
			snprintf(classdev_name, GPIO_MAX_NAME_SIZE, "%s-%d\n",  "key", priv->num_gpios);
		}
		dev_warn(dev, "%s %d = %s\n", "key", priv->num_gpios, classdev_name);

		assign_btn_gpiod(gpio_dat->gpiod, classdev_name);

		priv->num_gpios++;
	}

	devm_kfree(dev, priv);

	return 0;
}

static int nas_keys_probe(struct platform_device *pdev)
{
	int ret;

	ret = parse_gpio_nodes(pdev, pdev->dev.of_node);
	if (ret)
		return ret;

	set_init_keys_timer();

	btn_workqueue = create_workqueue("button controller");

	return 0;
}

static int nas_keys_remove(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < BUTTON_NUM; i++) {
		if (btn_set[i].presence == 0) continue;

		if(nas_ctrl_timer_pending(&btn_set[i].timer))
			nas_ctrl_del_timer(&btn_set[i].timer);
	}

	destroy_workqueue(btn_workqueue);

	return 0;
}

static const struct of_device_id of_nas_keys_match[] = {
	{ .compatible = "zyxel,nas-keys", },
	{},
};

static struct platform_driver nas_keys_driver = {
	.probe = nas_keys_probe,
	.remove = nas_keys_remove,
	.driver = {
		.name = "nas-keys",
		.of_match_table = of_nas_keys_match,
	},
};

module_platform_driver(nas_keys_driver);

MODULE_AUTHOR("scpcom <scpcom@gmx.de>");
MODULE_DESCRIPTION("Zyxel NAS Keys driver");
MODULE_LICENSE("GPL");
