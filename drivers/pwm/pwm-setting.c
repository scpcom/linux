/*
 * Axera pwm setting driver
 *
 * Copyright (c) 2019-2020 Axera Technology Co., Ltd.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#include <linux/pwm.h>

#define PROC_NODE_ROOT_NAME	"ax_proc/pwm_setting"
#define PROC_PWM_SET	"set"
static struct device *setting_dev;
static struct proc_dir_entry *pwm_setting_root;


static DEFINE_MUTEX(ax_pwm_setting_mutex);

static int ax_pwm_setting_show(struct seq_file *m, void *v)
{
	char buf[32] = {0};
	char *pwmname;
	struct pwm_device *pwm_chip;
	struct pwm_state state;

	pwmname = m->private;

	mutex_lock(&ax_pwm_setting_mutex);

	pwm_chip = pwm_get(setting_dev, pwmname);
	if (IS_ERR(pwm_chip)) {
		pr_err("%s get pwm device fail\n",
			__func__);
		return -1;
	}

	pwm_get_state(pwm_chip, &state);
	snprintf(buf, sizeof(buf), "%d,%d", state.period, state.duty_cycle);
	seq_printf(m, "%s", buf);

	pwm_put(pwm_chip);

	mutex_unlock(&ax_pwm_setting_mutex);
	return 0;
}

static int ax_pwm_setting_open(struct inode *inode, struct file *file)
{
	char *pwmname;
	pwmname = PDE_DATA(file_inode(file));
	return single_open(file, ax_pwm_setting_show, pwmname);
}

static ssize_t ax_pwm_setting_write(struct file *file, const char __user *buffer,
					  size_t count, loff_t *ppos)
{
	char kbuf[32] = { 0 };
	struct pwm_device *pwm_chip;
	unsigned int period;
	unsigned int duty;
	int ret = 0;
	char *pwmname = PDE_DATA(file_inode(file));

	if (count > 32) {
		return -1;
	}

	if (copy_from_user(kbuf, buffer, count)) {
		return -EFAULT;
	}

	if (sscanf(kbuf, "%d,%d", (unsigned int*)&period, (unsigned int*)&duty) != 2) {
		return -1;
	}

	if (period < duty) {
		pr_err("%s period can't < duty\n", __func__);
		return -1;
	}

	mutex_lock(&ax_pwm_setting_mutex);

	pwm_chip = pwm_get(setting_dev, pwmname);
	if (IS_ERR(pwm_chip)) {
		pr_err("%s get pwm device fail\n",
			__func__);
		return -1;
	}

	ret = pwm_config(pwm_chip, duty, period);
	if (ret < 0) {
		pr_err("%s the pwm can't be configed, must something wrong\n",
			__func__);
		return -1;
	}

	pwm_enable(pwm_chip);
	pwm_put(pwm_chip);

	mutex_unlock(&ax_pwm_setting_mutex);

	return count;
}

static const struct file_operations ax_pwm_setting_fsops = {
	.open = ax_pwm_setting_open,
	.read = seq_read,
	.write = ax_pwm_setting_write,
	.release = single_release,
};

static int ax_pwm_en_show(struct seq_file *m, void *v)
{
	char buf[32] = {0};
	char *pwmname;
	struct pwm_device *pwm_chip;
	struct pwm_state state;

	pwmname = m->private;

	mutex_lock(&ax_pwm_setting_mutex);

	pwm_chip = pwm_get(setting_dev, pwmname);
	if (IS_ERR(pwm_chip)) {
		pr_err("%s get pwm device fail\n",
			__func__);
		return -1;
	}

	pwm_get_state(pwm_chip, &state);
	snprintf(buf, sizeof(buf), "%d", state.enabled);
	seq_printf(m, "%s", buf);

	pwm_put(pwm_chip);

	mutex_unlock(&ax_pwm_setting_mutex);

	return 0;
}

static int ax_pwm_en_open(struct inode *inode, struct file *file)
{
	char *pwmname;
	pwmname = PDE_DATA(file_inode(file));
	return single_open(file, ax_pwm_en_show, pwmname);
}

static ssize_t ax_pwm_en_write(struct file *file, const char __user *buffer,
					  size_t count, loff_t *ppos)
{
	char kbuf[32] = { 0 };
	unsigned int enabled;
	struct pwm_device *pwm_chip;
	char *pwmname = PDE_DATA(file_inode(file));


	if (count > 32) {
		return -1;
	}

	if (copy_from_user(kbuf, buffer, count)) {
		return -EFAULT;
	}

	if (sscanf(kbuf, "%d", (unsigned int*)&enabled) != 1) {
		return -1;
	}

	mutex_lock(&ax_pwm_setting_mutex);

	pwm_chip = pwm_get(setting_dev, pwmname);
	if (IS_ERR(pwm_chip)) {
		pr_err("%s get pwm device fail\n",
			__func__);
		return -1;
	}

	if (enabled)
		pwm_enable(pwm_chip);
	else
		pwm_disable(pwm_chip);

	pwm_put(pwm_chip);

	mutex_unlock(&ax_pwm_setting_mutex);

	return count;
}


static const struct file_operations ax_pwm_en_fsops = {
	.open = ax_pwm_en_open,
	.read = seq_read,
	.write = ax_pwm_en_write,
	.release = single_release,
};

static int axera_pwm_setting_probe(struct platform_device *pdev)
{
	const char *pwmname[12]; /*total 12PWM*/
	char pwmenable[32];
	int pwmnum = 0;
	int index = 0;

	setting_dev = &pdev->dev;

	of_property_read_string_array(setting_dev->of_node, "pwm-names", pwmname, 12);

	pwm_setting_root = proc_mkdir(PROC_NODE_ROOT_NAME, NULL);
	if (pwm_setting_root == NULL) {
		return -ENODATA;
	}

	device_property_read_u32(setting_dev, "pwmnum",
				 &pwmnum);

	for(index = 0; index < pwmnum; index++){
		proc_create_data(pwmname[index], 0644, pwm_setting_root,
			 		&ax_pwm_setting_fsops, (void*)pwmname[index]);
		snprintf(pwmenable, sizeof(pwmenable), "%s_enable", pwmname[index]);
		proc_create_data(pwmenable, 0644, pwm_setting_root, &ax_pwm_en_fsops, (void*)pwmname[index]);
	}

	return 0;
}

static int axera_pwm_setting_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id axera_pwm_setting_of_id_table[] = {
	{ .compatible = "axera, pwm-setting" },
	{}
};
MODULE_DEVICE_TABLE(of, axera_slp_stat_of_id_table);

static struct platform_driver axera_pwm_setting_driver = {
	.probe	= axera_pwm_setting_probe,
	.remove = axera_pwm_setting_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = axera_pwm_setting_of_id_table,
	},
};
module_platform_driver(axera_pwm_setting_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("axera pwm setting driver");
