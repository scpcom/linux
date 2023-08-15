/*
 * Zyxel NAS Control Driver
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
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#include "zyxel-nas-ctrl.h"

/* define the cmd numbers for ioctl used */
#define BTNCPY_IOC_SET_NUM		_IO(BTNCPY_IOC_MAGIC, 1)
#define LED_SET_CTL_IOC_NUM     _IO(BTNCPY_IOC_MAGIC, 2)
#define BUZ_SET_CTL_IOC_NUM 	_IO(BTNCPY_IOC_MAGIC, 4)
#define BUTTON_TEST_IN_IOC_NUM  _IO(BTNCPY_IOC_MAGIC, 9)
#define BUTTON_TEST_OUT_IOC_NUM _IO(BTNCPY_IOC_MAGIC, 10)

/* data structure for passing HDD state */
typedef struct _hdd_ioctl {
	unsigned int port;  /* HDD_PORT_NUM  */
	unsigned int state; /* ON, OFF */
} hdd_ioctl;

#define HDD_SET_CTL_IOC_NUM     _IOW(BTNCPY_IOC_MAGIC, 21, hdd_ioctl)

struct nas_ctrl {
	struct gpio_descs *gpios;

	u32 wait_delay_ms;
	u32 drive_bays;

	struct proc_dir_entry *htp_proc;
	struct proc_dir_entry *hdd1_detect_proc;
	struct proc_dir_entry *hdd2_detect_proc;
	struct proc_dir_entry *hdd3_detect_proc;
	struct proc_dir_entry *hdd4_detect_proc;
	struct proc_dir_entry *pwren_usb_proc;
#ifdef CONFIG_ZYXEL_HDD_EXTENSIONS
	struct proc_dir_entry *disk_io_err_proc_root;
	struct proc_dir_entry *disk1_io_err_proc;
	struct proc_dir_entry *disk2_io_err_proc;
	struct proc_dir_entry *disk3_io_err_proc;
	struct proc_dir_entry *disk4_io_err_proc;
#endif
	struct proc_dir_entry *drive_bays_proc_root;
	struct proc_dir_entry *drive_bays_count_proc;
};

struct nas_ctrl_cdev {
	dev_t dev;
	unsigned int major;
	int nr_devs;
	struct cdev *cdev;
};

static struct nas_ctrl_cdev nas_chrdev = {
	dev: 0,
	major: 253,
	nr_devs: 1,
	cdev: NULL,
};

static struct nas_ctrl *nas_ctrl = NULL;

#ifdef CONFIG_ZYXEL_HDD_EXTENSIONS
atomic_t sata_hd_accessing[MAX_HD_NUM];
atomic_t sata_hd_stop[MAX_HD_NUM];
atomic_t sata_badblock_idf[MAX_HD_NUM];

atomic_t disk_io_err[MAX_HD_NUM];
atomic_t sata_device_num;

struct workqueue_struct *hdd_workqueue = NULL;
static void hdd_error_handler(struct work_struct *in);
static DECLARE_WORK(HDD_ERR_DETECT, hdd_error_handler);
#endif

static int cmdline_drive_bays = 0;

static int __init nas_ctrl_drive_bays_setup(char *str)
{
	if (get_option(&str, &cmdline_drive_bays) != 1)
		return 1;

	if (cmdline_drive_bays < 1)
		cmdline_drive_bays = 2;
	if (cmdline_drive_bays > MAX_HD_NUM)
		cmdline_drive_bays = MAX_HD_NUM;

	pr_debug("cmdline drive_bays: %u\n", cmdline_drive_bays);

	return 1;
}
__setup("drive_bays=", nas_ctrl_drive_bays_setup);

u32 nas_ctrl_get_drive_bays_count(void)
{
	if (nas_ctrl)
		return nas_ctrl->drive_bays;
	else
		return cmdline_drive_bays;
}

static struct gpio_desc *nas_ctrl_get_gpiod(s32 index)
{
        if (IS_ERR(nas_ctrl->gpios))
                return ERR_CAST(nas_ctrl->gpios);

	if (index < 0)
		return ERR_PTR(-EINVAL);

	if (index < nas_ctrl->gpios->ndescs)
		return nas_ctrl->gpios->desc[index];
	else
		return ERR_PTR(-EINVAL);
}

static int nas_ctrl_detect_hdd(unsigned int port)
{
	int need_sleep = 0;
	struct gpio_desc *det_gpiod = nas_ctrl->gpios->desc[port-1];
	struct gpio_desc *ctl_gpiod = nas_ctrl->gpios->desc[port-1+5];

	if (port > nas_ctrl->drive_bays)
		return 0;

	if (gpiod_get_value(det_gpiod))	// HDx inserted
	{
		printk(KERN_WARNING "\033[033mEnable HD%d ...\033[0m\n", port);
		gpiod_set_value(ctl_gpiod, 1);	// enable HDx
		turn_on_led(port-1, GREEN);	// set HDx LED
		need_sleep = 1;
	}

	return need_sleep;
}


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
static inline void _timer_hdl(struct timer_list *in_timer)
#else
static inline void _timer_hdl(unsigned long cntx)
#endif
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
	struct nas_ctrl_timer_list *ptimer = from_timer(ptimer, in_timer, timer);
#else
	struct nas_ctrl_timer_list *ptimer = (_timer *)cntx;
#endif
	ptimer->function(ptimer->data);
}

void nas_ctrl_init_timer(struct nas_ctrl_timer_list *ptimer, void *pfunc, unsigned long cntx)
{
	ptimer->function = pfunc;
	ptimer->data = cntx;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
	timer_setup(&ptimer->timer, _timer_hdl, 0);
#else
	/* setup_timer(ptimer, pfunc,(u32)cntx);	 */
	ptimer->timer.function = _timer_hdl;
	ptimer->timer.data = (unsigned long)ptimer;
	init_timer(&ptimer->timer);
#endif
}

int nas_ctrl_mod_timer(struct nas_ctrl_timer_list *ptimer, unsigned long expires)
{
	return mod_timer(&ptimer->timer, expires);
}

int nas_ctrl_del_timer(struct nas_ctrl_timer_list * ptimer)
{
	return del_timer(&ptimer->timer);
}

int nas_ctrl_del_timer_sync(struct nas_ctrl_timer_list *ptimer)
{
	return del_timer_sync(&ptimer->timer);
}

int nas_ctrl_timer_pending(const struct nas_ctrl_timer_list *ptimer)
{
	return timer_pending(&ptimer->timer);
}

int run_usermode_cmd(const char *cmd)
{
	char **argv;
	static char *envp[] = {
		"HOME=/",
		"PATH=/sbin:/bin:/usr/sbin:/usr/bin",
		NULL
	};
	int ret;
	argv = argv_split(GFP_KERNEL, cmd, NULL);
	if (argv) {
		ret = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_EXEC);
		argv_free(argv);
	} else {
		ret = -ENOMEM;
	}

	return ret;
}


static int hdd_power_set(struct _hdd_ioctl *hdd_data)
{
	unsigned int port = hdd_data->port;
	unsigned int state = hdd_data->state;
	struct gpio_desc *ctl_gpio;

	if (port > 4)
		return -EINVAL;

	ctl_gpio = nas_ctrl_get_gpiod(port-1+5);

	/* HDD Power On */
	if (state == 1)
		gpiod_set_value(ctl_gpio, 1);
	else
		gpiod_set_value(ctl_gpio, 0);

	return 0;
}


static int gpio_open(struct inode *inode , struct file* filp)
{
	return 0;
}

static int gpio_release(struct inode *inode , struct file *filp)
{
	return 0;
}

static ssize_t gpio_read(struct file *file, char *buf, size_t count, loff_t *ptr)
{
	printk(KERN_INFO "Read system call is no useful\n");
	return 0;
}

static ssize_t gpio_write(struct file * file, const char *buf, size_t count, loff_t * ppos)
{
	printk(KERN_INFO "Write system call is no useful\n");
	return 0;
}

static long gpio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
#ifdef CONFIG_ZYXEL_NAS_LEDS
	unsigned long ret = 0;
#endif
	struct _hdd_ioctl hdd_data;

	if (!nas_ctrl)
		return -ENOTTY;

	/* implement a lock scheme by myself */
	/* get the inode ==> file->f_dentry->d_inode */
	switch (cmd) {
#ifdef CONFIG_ZYXEL_NAS_KEYS
		case BTNCPY_IOC_SET_NUM:
			if (!capable(CAP_SYS_ADMIN)) return -EPERM;
			set_btncpy_pid(arg);
			break;
		case BUTTON_TEST_IN_IOC_NUM:
			if (!capable(CAP_SYS_ADMIN)) return -EPERM;
			set_button_test(arg, 1);
			break;
		case BUTTON_TEST_OUT_IOC_NUM:
			if (!capable(CAP_SYS_ADMIN)) return -EPERM;
			set_button_test(arg, 0);
			break;
#endif
#ifdef CONFIG_ZYXEL_NAS_PWMS
		case BUZ_SET_CTL_IOC_NUM:
			set_buzzer(arg);
			break;
#endif
#ifdef CONFIG_ZYXEL_NAS_LEDS
		case LED_SET_CTL_IOC_NUM:       // Just set leds, no check.
			ret = set_led_config(arg);
			if (ret < 0)
				return ret;
			break;
#endif
		case HDD_SET_CTL_IOC_NUM:
			if (!copy_from_user(&hdd_data, (void __user *) arg, sizeof(struct _hdd_ioctl)))
				hdd_power_set(&hdd_data);
			break;

		default :
#ifdef CONFIG_ZYXEL_MCU_BURNING
			return nas_mcu_ioctl(file, cmd, arg);
#else
			return -ENOTTY;
#endif
	}

	return 0;
}

struct file_operations gpio_fops =
{
	owner:              THIS_MODULE,
	read:               gpio_read,
	write:              gpio_write,
	unlocked_ioctl:     gpio_ioctl,
	open:               gpio_open,
	release:            gpio_release,
};


static ssize_t htp_status_read_fun(struct file *file, char __user *buff,
		size_t count, loff_t *pos)
{
	int len;
	char tmpbuf[64];
	struct gpio_desc *det_gpio = nas_ctrl_get_gpiod(4);

	if (IS_ERR(det_gpio)) {
		return PTR_ERR(det_gpio);
	}

	if (gpiod_get_value(det_gpio)) {
		len = sprintf(tmpbuf, "1\n");
	} else {
		len = sprintf(tmpbuf, "0\n");
	}

	if (*pos != 0)
		len = 0;
	if (!buff)
		return len;
	if (copy_to_user(buff, tmpbuf, len))
		len = 0;
	else
		*pos += len;

	return len;
}

static ssize_t htp_status_write_fun(struct file *file, const char __user *buff,
		size_t count, loff_t *pos)
{
	/* do nothing */
	return 0;
}

static const struct proc_ops htp_status_fops = {
	.proc_read = htp_status_read_fun,
	.proc_write = htp_status_write_fun,
};

static ssize_t hdd1_status_read_fun(struct file *file, char __user *buff,
		size_t count, loff_t *pos)
{
	int len;
	char tmpbuf[64];
	struct gpio_desc *det_gpio = nas_ctrl_get_gpiod(0);

	if (IS_ERR(det_gpio)) {
		return PTR_ERR(det_gpio);
	}

	if (gpiod_get_value(det_gpio)) {
		len = sprintf(tmpbuf, "1\n");
	} else {
		len = sprintf(tmpbuf, "0\n");
	}

	if (*pos != 0)
		len = 0;
	if (!buff)
		return len;
	if (copy_to_user(buff, tmpbuf, len))
		len = 0;
	else
		*pos += len;

	return len;
}

static ssize_t hdd1_status_write_fun(struct file *file, const char __user *buff,
		size_t count, loff_t *pos)
{
	/* do nothing */
	return 0;
}

static const struct proc_ops hdd1_status_fops = {
	.proc_read = hdd1_status_read_fun,
	.proc_write = hdd1_status_write_fun,
};

static ssize_t hdd2_status_read_fun(struct file *file, char __user *buff,
		size_t count, loff_t *pos)
{
	int len;
	char tmpbuf[64];
	struct gpio_desc *det_gpio = nas_ctrl_get_gpiod(1);

	if (IS_ERR(det_gpio)) {
		return PTR_ERR(det_gpio);
	}

	if (gpiod_get_value(det_gpio)) {
		len = sprintf(tmpbuf, "1\n");
	} else {
		len = sprintf(tmpbuf, "0\n");
	}

	if (*pos != 0)
		len = 0;
	if (!buff)
		return len;
	if (copy_to_user(buff, tmpbuf, len))
		len = 0;
	else
		*pos += len;

	return len;
}

static ssize_t hdd2_status_write_fun(struct file *file, const char __user *buff,
		size_t count, loff_t *pos)
{
	/* do nothing */
	return 0;
}

static const struct proc_ops hdd2_status_fops = {
	.proc_read = hdd2_status_read_fun,
	.proc_write = hdd2_status_write_fun,
};

static ssize_t hdd3_status_read_fun(struct file *file, char __user *buff,
		size_t count, loff_t *pos)
{
	int len;
	char tmpbuf[64];
	struct gpio_desc *det_gpio = nas_ctrl_get_gpiod(2);

	if (IS_ERR(det_gpio)) {
		return PTR_ERR(det_gpio);
	}

	if (gpiod_get_value(det_gpio)) {
		len = sprintf(tmpbuf, "1\n");
	} else {
		len = sprintf(tmpbuf, "0\n");
	}

	if (*pos != 0)
		len = 0;
	if (!buff)
		return len;
	if (copy_to_user(buff, tmpbuf, len))
		len = 0;
	else
		*pos += len;

	return len;
}

static ssize_t hdd3_status_write_fun(struct file *file, const char __user *buff,
		size_t count, loff_t *pos)
{
	/* do nothing */
	return 0;
}

static const struct proc_ops hdd3_status_fops = {
	.proc_read = hdd3_status_read_fun,
	.proc_write = hdd3_status_write_fun,
};

static ssize_t hdd4_status_read_fun(struct file *file, char __user *buff,
		size_t count, loff_t *pos)
{
	int len;
	char tmpbuf[64];
	struct gpio_desc *det_gpio = nas_ctrl_get_gpiod(3);

	if (IS_ERR(det_gpio)) {
		return PTR_ERR(det_gpio);
	}

	if (gpiod_get_value(det_gpio)) {
		len = sprintf(tmpbuf, "1\n");
	} else {
		len = sprintf(tmpbuf, "0\n");
	}

	if (*pos != 0)
		len = 0;
	if (!buff)
		return len;
	if (copy_to_user(buff, tmpbuf, len))
		len = 0;
	else
		*pos += len;

	return len;
}

static ssize_t hdd4_status_write_fun(struct file *file, const char __user *buff,
		size_t count, loff_t *pos)
{
	/* do nothing */
	return 0;
}

static const struct proc_ops hdd4_status_fops = {
	.proc_read = hdd4_status_read_fun,
	.proc_write = hdd4_status_write_fun,
};

static ssize_t pwren_usb_read_fun(struct file *file, char __user *buff,
		size_t count, loff_t *pos)
{
	/* do nothing */
	return 0;
}

static ssize_t pwren_usb_write_fun(struct file *file, const char __user *buff,
		size_t count, loff_t *pos)
{
	char tmpbuf[64];
	struct gpio_desc *ctl_gpio = nas_ctrl_get_gpiod(9);

	if (IS_ERR(ctl_gpio)) {
		return PTR_ERR(ctl_gpio);
	}

	if (buff && !copy_from_user(tmpbuf, buff, count))
	{
		tmpbuf[count-1] = '\0';
		if ( tmpbuf[0] == '1' )
		{
			// enable usb
			gpiod_set_value(ctl_gpio, 1);
			printk(KERN_NOTICE " \033[033mUSB is enabled!\033[0m\n");
		}
		else
		{
			// disable usb
			gpiod_set_value(ctl_gpio, 0);
			printk(KERN_NOTICE "\033[033mUSB is disabled!\033[0m\n");
		}

	}

	return count;
}

static const struct proc_ops pwren_usb_fops = {
	.proc_read = pwren_usb_read_fun,
	.proc_write = pwren_usb_write_fun,
};

#ifdef CONFIG_ZYXEL_HDD_EXTENSIONS
/* read file operations */
static int disk_io_err_read_proc_func(int index, char __user *buff,
		size_t count, loff_t *pos)
{
	int len;
	char tmpbuf[64];

	len = sprintf(tmpbuf,"%d\n", atomic_read(&disk_io_err[index]));

	if (*pos != 0)
		len = 0;
	if (!buff)
		return len;
	if (copy_to_user(buff, tmpbuf, len))
		len = 0;
	else
		*pos += len;

	return len;
}

static int disk1_io_err_read_proc_func(struct file *file, char __user *buff,
		size_t count, loff_t *pos)
{
	return disk_io_err_read_proc_func(0, buff, count, pos);
}

static int disk2_io_err_read_proc_func(struct file *file, char __user *buff,
		size_t count, loff_t *pos)
{
	return disk_io_err_read_proc_func(1, buff, count, pos);
}

static int disk3_io_err_read_proc_func(struct file *file, char __user *buff,
		size_t count, loff_t *pos)
{
	return disk_io_err_read_proc_func(2, buff, count, pos);
}

static int disk4_io_err_read_proc_func(struct file *file, char __user *buff,
		size_t count, loff_t *pos)
{
	return disk_io_err_read_proc_func(3, buff, count, pos);
}

/* write file operations */
static int disk_io_err_write_proc_func(int index, const char __user *buf,
		size_t count, loff_t *pos)
{
	char num[10];
	int err_num;

	/* no data be written */
	if (!count) {
		printk("count is 0\n");
		return 0;
	}

	/* Input size is too large to write our buffer(num) */
	if (count > (sizeof(num) - 1)) {
		printk("input is too large\n");
		return -EINVAL;
	}

	if (copy_from_user(num, buf, count)) {
		printk("copy from user failed\n");
		return -EFAULT;
	}

	err_num = num[0] - '0';
	if (err_num == 0)
		atomic_set(&disk_io_err[index], DISK_NO_ERR);
	else
		atomic_set(&disk_io_err[index], DISK_ERR);
	return count;
}

static int disk1_io_err_write_proc_func(struct file *file, const char __user *buf,
		size_t count, loff_t *pos)
{
	return disk_io_err_write_proc_func(0, buf, count, pos);
}

static int disk2_io_err_write_proc_func(struct file *file, const char __user *buf,
		size_t count, loff_t *pos)
{
	return disk_io_err_write_proc_func(1, buf, count, pos);
}

static int disk3_io_err_write_proc_func(struct file *file, const char __user *buf,
		size_t count, loff_t *pos)
{
	return disk_io_err_write_proc_func(2, buf, count, pos);
}

static int disk4_io_err_write_proc_func(struct file *file, const char __user *buf,
		size_t count, loff_t *pos)
{
	return disk_io_err_write_proc_func(3, buf, count, pos);
}

static const struct proc_ops disk1_io_err_proc_ops = {
	proc_read: disk1_io_err_read_proc_func,
	proc_write: disk1_io_err_write_proc_func
};

static const struct proc_ops disk2_io_err_proc_ops = {
	proc_read: disk2_io_err_read_proc_func,
	proc_write: disk2_io_err_write_proc_func
};

static const struct proc_ops disk3_io_err_proc_ops = {
	proc_read: disk3_io_err_read_proc_func,
	proc_write: disk3_io_err_write_proc_func
};

static const struct proc_ops disk4_io_err_proc_ops = {
	proc_read: disk4_io_err_read_proc_func,
	proc_write: disk4_io_err_write_proc_func
};
#endif

static int drive_bays_count_read_func(struct file *file, char __user *buff,
		size_t count, loff_t *pos)
{
	int len;
	char tmpbuf[64];

	len = sprintf(tmpbuf, "%i\n", nas_ctrl_get_drive_bays_count());

	if (*pos != 0)
		len = 0;
	if (!buff)
		return len;
	if (copy_to_user(buff, tmpbuf, len))
		len = 0;
	else
		*pos += len;

	return len;
}

static int drive_bays_count_write_func(struct file *file, const char __user *buff,
		size_t count, loff_t *pos)
{
	/* do nothing */
	return 0;
}

static const struct proc_ops drive_bays_count_ops = {
	proc_read: drive_bays_count_read_func,
	proc_write: drive_bays_count_write_func
};


#ifdef CONFIG_ZYXEL_HDD_EXTENSIONS
static void hdd_error_handler(struct work_struct *in)
{
	int ret;
	char diskx[10];
	char cmdPath[] = "/usr/local/disk_error_hander.sh";
	char cmdLine[64];
	int diskid = atomic_read(&sata_device_num);

	sprintf(diskx, "Disk%d", diskid);
	sprintf(cmdLine, "/bin/sh %s %s", cmdPath, diskx);

	if ((diskid < 1) || (diskid > MAX_HD_NUM))
		return;

	if ( atomic_read(&disk_io_err[diskid-1]) != DISK_ERR )
	{
		ret = run_usermode_cmd(cmdLine);
		atomic_set(&disk_io_err[diskid-1], DISK_ERR);
	}
}

void start_hdd_error_handler(int port)
{
	if (!hdd_workqueue)
		return;

	/* for hdd error hander (send zylog and show fail on gui)*/
	atomic_set(&sata_device_num, port);
	queue_work(hdd_workqueue, &HDD_ERR_DETECT);
}
#endif


static int nas_ctrl_register_chrdev(void)
{
	int ret;

	if (nas_chrdev.dev)
		return 0;

	/* create /dev/gpio for ioctl using */
	ret = __register_chrdev(nas_chrdev.major, 0, nas_chrdev.nr_devs, "gpio", &gpio_fops);
	if (ret >= 0) {
		nas_chrdev.dev = MKDEV(nas_chrdev.major, 0);
		return ret;
	}

	ret = alloc_chrdev_region(&nas_chrdev.dev, 0, nas_chrdev.nr_devs, "gpio");
	if (ret < 0)
		return ret;
	printk(KERN_ERR"gpio_dev = %x\n", nas_chrdev.dev);

	nas_chrdev.cdev = cdev_alloc();
	if (!nas_chrdev.cdev) {
		ret = -ENOMEM;
		goto out2;
	}

	nas_chrdev.cdev->ops = &gpio_fops;
	nas_chrdev.cdev->owner = THIS_MODULE;
	kobject_set_name(&nas_chrdev.cdev->kobj, "%s", "gpio");

	ret = cdev_add(nas_chrdev.cdev, nas_chrdev.dev, 1);
	if (ret) {
		goto out;
		printk(KERN_INFO "Error adding device\n");
	}

	return ret;
out:
	kobject_put(&nas_chrdev.cdev->kobj);
out2:
	unregister_chrdev_region(nas_chrdev.dev, nas_chrdev.nr_devs);
	return ret;
}


static int nas_ctrl_probe(struct platform_device *pdev)
{
	int i, need_sleep = 0, ret;
	struct gpio_desc *usb_ctl_gpiod;

	if (nas_ctrl)
		return -ENODEV;

	nas_ctrl = devm_kzalloc(&pdev->dev, sizeof(*nas_ctrl),
			GFP_KERNEL);
	if (!nas_ctrl)
		return -ENOMEM;

	nas_ctrl->gpios = devm_gpiod_get_array(&pdev->dev, NULL, GPIOD_ASIS);
	if (IS_ERR(nas_ctrl->gpios)) {
		dev_err(&pdev->dev, "error getting control GPIOs\n");
		ret = PTR_ERR(nas_ctrl->gpios);
		goto err;
	}

	if (nas_ctrl->gpios->ndescs < 10) {
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < 5; i++) {
		struct gpio_desc *gpiod = nas_ctrl->gpios->desc[i];
		int ret;

		ret = gpiod_direction_input(gpiod);
		if (ret < 0)
			goto err;
	}

	for (i = 5; i < nas_ctrl->gpios->ndescs; i++) {
		struct gpio_desc *gpiod = nas_ctrl->gpios->desc[i];
		int ret, state;

		state = gpiod_get_value_cansleep(gpiod);
		if (state < 0) {
			ret = state;
			goto err;
		}

		ret = gpiod_direction_output(gpiod, state);
		if (ret < 0)
			goto err;
	}

	nas_ctrl->wait_delay_ms = 8000;

	of_property_read_u32(pdev->dev.of_node, "wait-delay",
			&nas_ctrl->wait_delay_ms);

	nas_ctrl->drive_bays = MAX_HD_NUM;

	of_property_read_u32(pdev->dev.of_node, "drive-bays",
			&nas_ctrl->drive_bays);

	if (cmdline_drive_bays > 0)
		nas_ctrl->drive_bays = cmdline_drive_bays;

	pr_debug("drive_bays: %u\n", nas_ctrl->drive_bays);

	platform_set_drvdata(pdev, nas_ctrl);

	// turn off all LEDS
	led_all_colors_off();

	// turn on sys led (it seems that the kernel timer doesn't work here!? only turn on SYS LED now)
	turn_on_led(LED_SYS, GREEN);

	need_sleep |= nas_ctrl_detect_hdd(1);
	need_sleep |= nas_ctrl_detect_hdd(3);

	if (need_sleep)
	{
		msleep(nas_ctrl->wait_delay_ms);
		need_sleep = 0;
	}

	need_sleep |= nas_ctrl_detect_hdd(2);
	need_sleep |= nas_ctrl_detect_hdd(4);

	if (need_sleep)
	{
		msleep(nas_ctrl->wait_delay_ms);
		need_sleep = 0;
	}

	// enable USB
	printk(KERN_WARNING "\033[033mEnable USB ...\033[0m\n");
	usb_ctl_gpiod = nas_ctrl->gpios->desc[9];
	gpiod_set_value(usb_ctl_gpiod, 1);

	/* create /dev/gpio for ioctl using */
	ret = nas_ctrl_register_chrdev();
	if (ret < 0)
	{
		printk(KERN_ERR"%s: failed to allocate char dev region\n", __FILE__);
		goto err;
	}

	/* create /proc/htp_pin */
	nas_ctrl->htp_proc = proc_create_data("htp_pin", 0644, NULL, &htp_status_fops, NULL);
	nas_ctrl->hdd1_detect_proc = proc_create_data("hdd1_detect", 0644, NULL, &hdd1_status_fops, NULL);
	nas_ctrl->hdd2_detect_proc = proc_create_data("hdd2_detect", 0644, NULL, &hdd2_status_fops, NULL);
	nas_ctrl->hdd3_detect_proc = proc_create_data("hdd3_detect", 0644, NULL, &hdd3_status_fops, NULL);
	nas_ctrl->hdd4_detect_proc = proc_create_data("hdd4_detect", 0644, NULL, &hdd4_status_fops, NULL);
	nas_ctrl->pwren_usb_proc = proc_create_data("enable_usb", 0644, NULL, &pwren_usb_fops, NULL);

#ifdef CONFIG_ZYXEL_HDD_EXTENSIONS
	for(i = 0 ; i < MAX_HD_NUM; i++)
	{
		atomic_set(&disk_io_err[i], DISK_NO_ERR);
	}
	nas_ctrl->disk_io_err_proc_root = proc_mkdir("disk_err_detect", NULL);
	if (nas_ctrl->disk_io_err_proc_root != NULL)
	{
		nas_ctrl->disk1_io_err_proc = proc_create_data("disk1_err", 0644, nas_ctrl->disk_io_err_proc_root, &disk1_io_err_proc_ops, NULL);
		nas_ctrl->disk2_io_err_proc = proc_create_data("disk2_err", 0644, nas_ctrl->disk_io_err_proc_root, &disk2_io_err_proc_ops, NULL);
		nas_ctrl->disk3_io_err_proc = proc_create_data("disk3_err", 0644, nas_ctrl->disk_io_err_proc_root, &disk3_io_err_proc_ops, NULL);
		nas_ctrl->disk4_io_err_proc = proc_create_data("disk4_err", 0644, nas_ctrl->disk_io_err_proc_root, &disk4_io_err_proc_ops, NULL);
	}
#endif

	nas_ctrl->drive_bays_proc_root = proc_mkdir("drive_bays", NULL);
	if (nas_ctrl->drive_bays_proc_root != NULL)
	{
		nas_ctrl->drive_bays_count_proc = proc_create_data("count", 0644, nas_ctrl->drive_bays_proc_root, &drive_bays_count_ops, NULL);
	}

	return 0;
err:
	nas_ctrl = NULL;
	return ret;
}

static int nas_ctrl_remove(struct platform_device *pdev)
{
	remove_proc_entry("htp_pin", NULL);
	remove_proc_entry("hdd1_detect", NULL);
	remove_proc_entry("hdd2_detect", NULL);
	remove_proc_entry("hdd3_detect", NULL);
	remove_proc_entry("hdd4_detect", NULL);
	remove_proc_entry("enable_usb", NULL);

#ifdef CONFIG_ZYXEL_HDD_EXTENSIONS
	remove_proc_entry("disk1_err", nas_ctrl->disk_io_err_proc_root);
	remove_proc_entry("disk2_err", nas_ctrl->disk_io_err_proc_root);
	remove_proc_entry("disk3_err", nas_ctrl->disk_io_err_proc_root);
	remove_proc_entry("disk4_err", nas_ctrl->disk_io_err_proc_root);
	proc_remove(nas_ctrl->disk_io_err_proc_root);
#endif

	remove_proc_entry("count", nas_ctrl->drive_bays_proc_root);
	proc_remove(nas_ctrl->drive_bays_proc_root);

	unregister_chrdev_region(nas_chrdev.dev, nas_chrdev.nr_devs);

	return 0;
}

static const struct of_device_id of_nas_ctrl_match[] = {
	{ .compatible = "zyxel,nas-control", },
	{},
};

static struct platform_driver nas_ctrl_driver = {
	.probe = nas_ctrl_probe,
	.remove = nas_ctrl_remove,
	.driver = {
		.name = "nas-control",
		.of_match_table = of_nas_ctrl_match,
	},
};

module_platform_driver(nas_ctrl_driver);

subsys_initcall(nas_ctrl_driver_init);

static int __init nas_ctrl_gpiodev_init(void)
{
	int ret = 0;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "zyxel,nas-control");
	if (!node)
		return 0;

	ret = __register_chrdev(nas_chrdev.major, 0, nas_chrdev.nr_devs, "gpio", &gpio_fops);
	if (ret < 0)
		return ret;

	nas_chrdev.dev = MKDEV(nas_chrdev.major, 0);
	printk(KERN_ERR"gpio_dev = %x\n", nas_chrdev.dev);

	return ret;
}
arch_initcall(nas_ctrl_gpiodev_init);

MODULE_AUTHOR("scpcom <scpcom@gmx.de>");
MODULE_DESCRIPTION("Zyxel NAS Control driver");
MODULE_LICENSE("GPL");
