/*
 * (C) Copyright 2009 Intel Corporation
 * Author: Jacob Pan (jacob.jun.pan@intel.com)
 *
 * Shared with ARM platforms, Jamie Iles, Picochip 2011
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Support for the Synopsys DesignWare APB Timers.
 */
#include<linux/module.h>
#include<linux/init.h>
#include<linux/fs.h>
#include<linux/sched.h>

#include<linux/device.h>
#include<linux/string.h>
#include<linux/errno.h>
#include <linux/delay.h>
#include<linux/types.h>
#include<linux/slab.h>
#include<asm/uaccess.h>
#include <linux/of_platform.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/ioctl.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/reset.h>

static struct proc_dir_entry *slp_stat_root;
static unsigned long user_sleep_wake_times = 0;
static unsigned long user_sleep_time = 0;
static unsigned long user_wakeup_time = 0;
static unsigned long user_last_duration = 0;
static unsigned long user_max_duration = 0;
static unsigned long user_min_duration = ((unsigned long)-1);
static unsigned long user_total_duration = 0;
static unsigned long user_average_duration = 0;

extern unsigned long k_suspend_wakeup_times;
extern unsigned long k_suspend_wakeup_max_duration;
extern unsigned long k_suspend_wakeup_min_duration;
extern unsigned long k_suspend_wakeup_average_duration;
extern unsigned long k_suspend_wakeup_total_duration;
extern unsigned long k_suspend_wakeup_duration;

#define PROC_NODE_ROOT_NAME	"ax_proc/slp_stat"
#define SLP_STAT 	"state"

#define AX_SLEEP_IOC_MAGIC	'u'
#define CMD_ENTER_SLEEP_TIME	_IOWR(AX_SLEEP_IOC_MAGIC, 1, unsigned long*)
#define CMD_ENTER_WAKRUP_TIME	_IOWR(AX_SLEEP_IOC_MAGIC, 2, unsigned long*)
#define CMD_CLR_SLEEP_WAKRUP_TIME	_IO(AX_SLEEP_IOC_MAGIC, 3)

static DEFINE_MUTEX(ax_slp_stat_mutex);

static int ax_slp_stat_show(struct seq_file *m, void *v)
{
	mutex_lock(&ax_slp_stat_mutex);
	seq_printf(m, "\t\t------ User Space Sleep Wakeup Time Statistics ------\n\n");
	seq_printf(m, "Total Times\tMax Duration\tMin Duration\tAverage Duration\tLast Duration\n");
	seq_printf(m,"%lu\t\t%lu ms\t\t%lu ms\t\t%lu ms\t\t\t%lu ms\n", user_sleep_wake_times, user_max_duration,
		user_min_duration, user_average_duration, user_last_duration);
	seq_printf(m, "\t\t------ Kernel Space Sleep Wakeup Time Statistics ------\n\n");
	seq_printf(m, "Total Times\tMax Duration\tMin Duration\tAverage Duration\tLast Duration\n");
	seq_printf(m,"%lu\t\t%lu ms\t\t%lu ms\t\t%lu ms\t\t\t%lu ms\n", k_suspend_wakeup_times, k_suspend_wakeup_max_duration,
		k_suspend_wakeup_min_duration, k_suspend_wakeup_average_duration, k_suspend_wakeup_duration);
	seq_printf(m, "\n\n");
	mutex_unlock(&ax_slp_stat_mutex);
	return 0;
}

static int ax_slp_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, ax_slp_stat_show, NULL);
}

static long ax_slp_stat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned long k_arg;

	mutex_lock(&ax_slp_stat_mutex);
	switch(cmd) {
	case CMD_ENTER_SLEEP_TIME:
		if(copy_from_user(&k_arg, (void*)arg, sizeof(k_arg))) {
			mutex_unlock(&ax_slp_stat_mutex);
			return -1;
		}
		user_sleep_time = k_arg;
		if (user_sleep_wake_times > 0) {
			user_last_duration = (user_sleep_time - user_wakeup_time);
			if ((user_sleep_time - user_wakeup_time) > user_max_duration)
				user_max_duration = (user_sleep_time - user_wakeup_time);
			if ((user_sleep_time - user_wakeup_time) < user_min_duration) {
				user_min_duration = (user_sleep_time - user_wakeup_time);
			}
			user_total_duration += (user_sleep_time - user_wakeup_time);
			user_average_duration = (user_total_duration / user_sleep_wake_times);
		}
		user_sleep_wake_times++;
		break;
	case CMD_ENTER_WAKRUP_TIME:
		if(copy_from_user(&k_arg, (void*)arg, sizeof(k_arg))) {
			mutex_unlock(&ax_slp_stat_mutex);
			return -1;
		}
		user_wakeup_time = k_arg;
		break;
	case CMD_CLR_SLEEP_WAKRUP_TIME:
		user_sleep_wake_times = 0;
		user_max_duration = 0;
		user_min_duration = ((unsigned long)-1);
		user_total_duration = 0;
		user_average_duration = 0;
		k_suspend_wakeup_times = 0;
		k_suspend_wakeup_max_duration = 0;
		k_suspend_wakeup_min_duration = ((unsigned long)-1);
		k_suspend_wakeup_total_duration = 0;
		k_suspend_wakeup_average_duration = 0;
		break;
	default:
		mutex_unlock(&ax_slp_stat_mutex);
		return -1;
	}

	mutex_unlock(&ax_slp_stat_mutex);

	return 0;
}

static const struct file_operations ax_slp_stat_fsops = {
	.open = ax_slp_stat_open,
	.read = seq_read,
	.unlocked_ioctl = ax_slp_stat_ioctl,
	.release = single_release,
};

static int axera_slp_stat_probe(struct platform_device *pdev)
{
	slp_stat_root = proc_mkdir(PROC_NODE_ROOT_NAME, NULL);
	if (slp_stat_root == NULL) {
		return -ENODATA;
	}

	proc_create_data(SLP_STAT, 0644, slp_stat_root,
			 &ax_slp_stat_fsops, NULL);

	return 0;
}

static int axera_slp_stat_remove(struct platform_device *pdev)
{
	remove_proc_entry(SLP_STAT, slp_stat_root);
	remove_proc_entry(PROC_NODE_ROOT_NAME, NULL);
	return 0;
}

static const struct of_device_id axera_slp_stat_of_id_table[] = {
	{ .compatible = "ax_slp_stat" },
	{}
};
MODULE_DEVICE_TABLE(of, axera_slp_stat_of_id_table);

static struct platform_driver axera_slp_stat_driver = {
	.probe	= axera_slp_stat_probe,
	.remove = axera_slp_stat_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = axera_slp_stat_of_id_table,
	},
};

module_platform_driver(axera_slp_stat_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("axera slp stat driver");