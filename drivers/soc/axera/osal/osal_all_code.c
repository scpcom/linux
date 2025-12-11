/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/uaccess.h>
#include "osal_ax.h"
#include "osal_dev_ax.h"

void *AX_OSAL_DEV_ioremap(unsigned long phys_addr, unsigned long size)
{
	return ioremap(phys_addr, size);
}

EXPORT_SYMBOL(AX_OSAL_DEV_ioremap);

void *AX_OSAL_DEV_ioremap_nocache(unsigned long phys_addr, unsigned long size)
{
	return ioremap_wc(phys_addr, size);
}

EXPORT_SYMBOL(AX_OSAL_DEV_ioremap_nocache);

void *AX_OSAL_DEV_ioremap_cache(unsigned long phys_addr, unsigned long size)
{
	return ioremap_cache(phys_addr, size);
}

EXPORT_SYMBOL(AX_OSAL_DEV_ioremap_cache);

void AX_OSAL_DEV_iounmap(void * addr)
{
	iounmap(addr);
}

EXPORT_SYMBOL(AX_OSAL_DEV_iounmap);

unsigned long AX_OSAL_DEV_copy_from_user(void * to, const void * from, unsigned long n)
{
	return copy_from_user(to, from, n);
}

EXPORT_SYMBOL(AX_OSAL_DEV_copy_from_user);

unsigned long AX_OSAL_DEV_copy_to_user(void * to, const void * from, unsigned long n)
{
	return copy_to_user(to, from, n);
}

EXPORT_SYMBOL(AX_OSAL_DEV_copy_to_user);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/atomic.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/version.h>
#include "osal_ax.h"
#include "osal_logdebug_ax.h"

int AX_OSAL_SYNC_atomic_init(AX_ATOMIC_T * atomic)
{
	atomic_t *p;
	if (atomic == NULL) {
		printk("%s error atomic == NULL\n", __func__);
		return -1;
	}
	p = (atomic_t *) kmalloc(sizeof(atomic_t), GFP_KERNEL);
	if (p == NULL) {
		printk("%s - kmalloc failed\n", __func__);
		return -1;
	}
	atomic->atomic = p;
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_atomic_init);

int AX_OSAL_SYNC_atomic_read(AX_ATOMIC_T * atomic)
{
	atomic_t *p;
	if (atomic == NULL) {
		printk("%s error atomic == NULL\n", __func__);
		return -1;
	}
	p = (atomic_t *) (atomic->atomic);
	return atomic_read(p);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_atomic_read);

void AX_OSAL_SYNC_atomic_set(AX_ATOMIC_T * atomic, int val)
{
	atomic_t *p;
	p = (atomic_t *) (atomic->atomic);
	atomic_set(p, val);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_atomic_set);

int AX_OSAL_SYNC_atomic_inc_return(AX_ATOMIC_T * atomic)
{
	atomic_t *p;
	if (atomic == NULL) {
		printk("%s error atomic == NULL\n", __func__);
		return -1;
	}
	p = (atomic_t *) (atomic->atomic);
	return atomic_inc_return(p);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_atomic_inc_return);

int AX_OSAL_SYNC_atomic_dec_return(AX_ATOMIC_T * atomic)
{
	atomic_t *p;
	if (atomic == NULL) {
		printk("%s error atomic == NULL\n", __func__);
		return -1;
	}
	p = (atomic_t *) (atomic->atomic);
	return atomic_dec_return(p);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_atomic_dec_return);

int AX_OSAL_SYNC_atomic_cmpxchg(AX_ATOMIC_T * atomic, int old, int new)
{
	atomic_t *p;
	if (atomic == NULL) {
		printk("%s error atomic == NULL\n", __func__);
		return -1;
	}
	p = (atomic_t *) (atomic->atomic);
	return atomic_cmpxchg(p, old, new);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_atomic_cmpxchg);

bool AX_OSAL_SYNC_atomic_try_cmpxchg(AX_ATOMIC_T * atomic, int *old, int new)
{
	atomic_t *p;
	if (atomic == NULL) {
		printk("%s error atomic == NULL\n", __func__);
		return -1;
	}
	p = (atomic_t *) (atomic->atomic);
	return atomic_try_cmpxchg(p, old, new);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_atomic_try_cmpxchg);

void AX_OSAL_SYNC_atomic_and(int val,AX_ATOMIC_T * atomic)
{
       atomic_t *p;
       if (atomic == NULL) {
               printk("%s error atomic == NULL\n", __func__);
               return ;
       }
       p = (atomic_t *) (atomic->atomic);
       return atomic_and(val ,p);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_atomic_and);

void AX_OSAL_SYNC_atomic_or(int val,AX_ATOMIC_T * atomic)
{
       atomic_t *p;
       if (atomic == NULL) {
               printk("%s error atomic == NULL\n", __func__);
               return ;
       }
       p = (atomic_t *) (atomic->atomic);
       return atomic_or(val ,p);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_atomic_or);

int AX_OSAL_SYNC_atomic_fetch_add_ge(AX_ATOMIC_T * atomic, int add, int used)
{
	atomic_t *p;
	int c;
	if (atomic == NULL) {
		printk("error atomic == NULL\n");
		return -1;
	}
	p = (atomic_t *) (atomic->atomic);

	do {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
		c = arch_atomic_read(p);
#else
		c = atomic_read(p);
#endif
		if(unlikely(c < used))
			break;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	} while (!arch_atomic_try_cmpxchg(p, &c, c + add));
#else
	} while (!atomic_try_cmpxchg(p, &c, c + add));
#endif
	return c;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_atomic_fetch_add_ge);

void AX_OSAL_SYNC_atomic_destroy(AX_ATOMIC_T * atomic)
{
	kfree(atomic->atomic);
	atomic->atomic = NULL;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_atomic_destroy);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/kobject.h>
#include <linux/device.h>

#include "axdev_log.h"
#include "base.h"
/*****************************************************************************/
/**** axdev bus  ****/
/*****************************************************************************/
static void axdev_bus_release(struct device *dev)
{
	//printk("axdev bus release\n");
	return;
}

struct device axdev_bus = {
	.init_name = "axdev",
	.release = axdev_bus_release
};


/*bus match & uevent*/
static int axdev_match(struct device *dev, struct device_driver *drv)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	return (strncmp(pdev->devfs_name, drv->name, sizeof(pdev->devfs_name)) == 0);
}

static int axdev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	add_uevent_var(env, "MODALIAS=axdev:%s", pdev->devfs_name);
	return 0;
}

/*****************************************************************************/
//pm methods
static int axdev_pm_prepare(struct device *dev)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	struct axdev_driver *pdrv = to_axdev_driver(dev->driver);

	if (!pdrv->ops || !pdrv->ops->pm_prepare) {
		return 0;
	}

	return pdrv->ops->pm_prepare(pdev);
}

static void axdev_pm_complete(struct device *dev)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	struct axdev_driver *pdrv = to_axdev_driver(dev->driver);

	if (!pdrv->ops || !pdrv->ops->pm_complete) {
		return;
	}

	pdrv->ops->pm_complete(pdev);
}

static int axdev_pm_suspend(struct device *dev)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	struct axdev_driver *pdrv = to_axdev_driver(dev->driver);

	if (!pdrv->ops || !pdrv->ops->pm_suspend) {
		return 0;
	}

	return pdrv->ops->pm_suspend(pdev);
}

static int axdev_pm_resume(struct device *dev)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	struct axdev_driver *pdrv = to_axdev_driver(dev->driver);

	if (!pdrv->ops || !pdrv->ops->pm_resume) {
		return 0;
	}

	return pdrv->ops->pm_resume(pdev);
}

static int axdev_pm_freeze(struct device *dev)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	struct axdev_driver *pdrv = to_axdev_driver(dev->driver);
	if (!pdrv->ops || !pdrv->ops->pm_freeze) {
		return 0;
	}

	return pdrv->ops->pm_freeze(pdev);
}

static int axdev_pm_thaw(struct device *dev)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	struct axdev_driver *pdrv = to_axdev_driver(dev->driver);

	if (!pdrv->ops || !pdrv->ops->pm_thaw) {
		return 0;
	}

	return pdrv->ops->pm_thaw(pdev);
}

static int axdev_pm_poweroff(struct device *dev)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	struct axdev_driver *pdrv = to_axdev_driver(dev->driver);
	if (!pdrv->ops || !pdrv->ops->pm_poweroff) {
		return 0;
	}

	return pdrv->ops->pm_poweroff(pdev);
}

static int axdev_pm_restore(struct device *dev)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	struct axdev_driver *pdrv = to_axdev_driver(dev->driver);

	if (!pdrv->ops || !pdrv->ops->pm_restore) {
		return 0;
	}

	return pdrv->ops->pm_restore(pdev);
}

static int axdev_pm_suspend_noirq(struct device *dev)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	struct axdev_driver *pdrv = to_axdev_driver(dev->driver);

	if (!pdrv->ops || !pdrv->ops->pm_suspend_noirq) {
		return 0;
	}

	return pdrv->ops->pm_suspend_noirq(pdev);
}

static int axdev_pm_resume_noirq(struct device *dev)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	struct axdev_driver *pdrv = to_axdev_driver(dev->driver);

	if (!pdrv->ops || !pdrv->ops->pm_resume_noirq) {
		return 0;
	}

	return pdrv->ops->pm_resume_noirq(pdev);
}

static int axdev_pm_freeze_noirq(struct device *dev)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	struct axdev_driver *pdrv = to_axdev_driver(dev->driver);

	if (!pdrv->ops || !pdrv->ops->pm_freeze_noirq) {
		return 0;
	}

	return pdrv->ops->pm_freeze_noirq(pdev);
}

static int axdev_pm_thaw_noirq(struct device *dev)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	struct axdev_driver *pdrv = to_axdev_driver(dev->driver);

	if (!pdrv->ops || !pdrv->ops->pm_thaw_noirq) {
		return 0;
	}

	return pdrv->ops->pm_thaw_noirq(pdev);
}

static int axdev_pm_poweroff_noirq(struct device *dev)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	struct axdev_driver *pdrv = to_axdev_driver(dev->driver);

	if (!pdrv->ops || !pdrv->ops->pm_poweroff_noirq) {
		return 0;
	}

	return pdrv->ops->pm_poweroff_noirq(pdev);
}

static int axdev_pm_restore_noirq(struct device *dev)
{
	struct axdev_device *pdev = to_axdev_device(dev);
	struct axdev_driver *pdrv = to_axdev_driver(dev->driver);

	if (!pdrv->ops || !pdrv->ops->pm_restore_noirq) {
		return 0;
	}

	return pdrv->ops->pm_restore_noirq(pdev);
}

static struct dev_pm_ops axdev_bus_pm_ops = {
	.prepare = axdev_pm_prepare,
	.complete = axdev_pm_complete,

	//with irq
	.suspend = axdev_pm_suspend,
	.resume = axdev_pm_resume,

	.freeze = axdev_pm_freeze,
	.thaw = axdev_pm_thaw,
	.poweroff = axdev_pm_poweroff,
	.restore = axdev_pm_restore,

	//with noirq
	.suspend_noirq = axdev_pm_suspend_noirq,
	.resume_noirq = axdev_pm_resume_noirq,
	.freeze_noirq = axdev_pm_freeze_noirq,
	.thaw_noirq = axdev_pm_thaw_noirq,
	.poweroff_noirq = axdev_pm_poweroff_noirq,
	.restore_noirq = axdev_pm_restore_noirq,
};

struct bus_type axdev_bus_type = {
	.name = "axosalBus",
	//.dev_attrs    = axdev_dev_attrs,
	.match = axdev_match,
	.uevent = axdev_uevent,
	.pm = &axdev_bus_pm_ops,
};

int axdev_bus_init(void)
{
	int ret;
	ret = device_register(&axdev_bus);
	if (ret)
		return ret;

	ret = bus_register(&axdev_bus_type);
	if (ret)
		goto error;

	return 0;
error:

	device_unregister(&axdev_bus);
	return ret;
}

void axdev_bus_exit(void)
{
	bus_unregister(&axdev_bus_type);
	device_unregister(&axdev_bus);
}

static void axdev_device_release(struct device *dev)
{
}

extern struct file_operations axdev_fops;

int axdev_device_register(struct axdev_device *pdev)
{
	dev_t dev_id;
	int rval = 0;

	/*step 1/6, generate dev_id by major and minor of device */
	dev_id = MKDEV(pdev->major, pdev->minor);
	/*step 2/6, register dev_id and dev_name to device FW */
	rval = register_chrdev_region(dev_id, 1, pdev->devfs_name);
	if (rval) {
		printk("failed to get dev region for %s.\n",
				pdev->devfs_name);
		return rval;
	}

	/*step 3/6,only initialzie cdev */
	cdev_init(&pdev->cdev, &axdev_fops);
	pdev->cdev.owner = THIS_MODULE;
	/*step 4/6,add cdev to device FW, dev_id mapped to cdev */
	rval = cdev_add(&pdev->cdev, dev_id, 1);
	if (rval) {
		printk("cdev_add failed for %s, error = %d.\n",
			      pdev->devfs_name, rval);
		goto FAIL;
	}

	/*step 5/6,initialize device's parameter manually */
	pdev->device.devt = dev_id;
	pdev->device.parent = NULL;
	dev_set_drvdata(&pdev->device, NULL);

	//dev_set_name(&pdev->device,pdev->devfs_name );
	dev_set_name(&pdev->device, "%s", pdev->devfs_name);
	pdev->device.release = axdev_device_release;
	pdev->device.bus = &axdev_bus_type;

	pdev->devt = dev_id;
	/*step 6/6,create a device and register it with sysfs */
	rval = device_register(&pdev->device);
	if (rval) {
		printk("device_register failed for %s, error = %d.\n",pdev->devfs_name, rval);
		goto FAIL1;
	}

	return rval;
FAIL1:
	cdev_del(&pdev->cdev);
FAIL:
	unregister_chrdev_region(dev_id, 1);
	return rval;
}

void axdev_device_unregister(struct axdev_device *pdev)
{
	printk("begin to unregister axmeida-device\n");

	device_unregister(&pdev->device);
	cdev_del(&pdev->cdev);
	unregister_chrdev_region(pdev->devt, 1);
}

struct axdev_driver *axdev_driver_register(const char *name,struct module *owner,struct axdev_ops *pmops)
{
	int ret;
	struct axdev_driver *pdrv;

	if ((name == NULL) /*|| (owner == NULL) ||(ops == NULL) */ )
		return ERR_PTR(-EINVAL);

	pdrv = kzalloc(sizeof(struct axdev_driver) + strnlen(name, HIMIDIA_MAX_DEV_NAME_LEN), GFP_KERNEL);
	if (!pdrv)
		return ERR_PTR(-ENOMEM);

	/*init driver object */
	strncpy(pdrv->name, name, strnlen(name, HIMIDIA_MAX_DEV_NAME_LEN));

	pdrv->ops = pmops;
	pdrv->driver.name = pdrv->name;
	pdrv->driver.owner = THIS_MODULE;
	pdrv->driver.bus = &axdev_bus_type;

	ret = driver_register(&pdrv->driver);
	if (ret) {
		printk("Error, Failed to register driver[%s] \n", name);
		kfree(pdrv);
		return ERR_PTR(ret);
	}

	return pdrv;
}

void axdev_driver_unregister(struct axdev_driver *pdrv)
{
	if (pdrv) {
		driver_unregister(&pdrv->driver);
		kfree(pdrv);
	}
}
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/kmod.h>
#include <linux/clk.h>

#include "osal_lib_ax.h"

#include "base.h"
#include "axdev.h"
static OSAL_LIST_HEAD(axdev_list);
static DEFINE_MUTEX(axdev_sem);
#define CLASS_NAME "ax_osal_class"
struct class *g_axdev_class;

static int axdev_open(struct inode *inode, struct file *file)
{
	unsigned minor = iminor(inode);
	//unsigned major = imajor(inode);

	struct axdev_device *c = NULL;
	int err = -ENODEV;
	const struct file_operations *old_fops, *new_fops = NULL;

	mutex_lock(&axdev_sem);
	osal_list_for_each_entry(c, &axdev_list, list) {
		if (c->minor == minor) {
			new_fops = fops_get(c->fops);
			break;
		}
	}

	if (!new_fops) {
		mutex_unlock(&axdev_sem);
		request_module("char-major-%d-%d", AXDEV_DEVICE_MAJOR, minor);
		mutex_lock(&axdev_sem);

		osal_list_for_each_entry(c, &axdev_list, list) {
			if (c->minor == minor) {
				new_fops = fops_get(c->fops);
				break;
			}
		}

		if (!new_fops)
			goto fail;
	}

	err = 0;
	old_fops = file->f_op;
	file->f_op = new_fops;
	if (file->f_op->open) {
		file->private_data = c;
		err = file->f_op->open(inode, file);
		if (err) {
			fops_put(file->f_op);
			file->private_data = NULL;
			file->f_op = fops_get(old_fops);
		}
	}

	fops_put(old_fops);
fail:
	mutex_unlock(&axdev_sem);
	return err;
}

struct file_operations axdev_fops = {
	.owner = THIS_MODULE,
	.open = axdev_open,
};

int axdev_register(struct axdev_device *axdev)
{
	struct axdev_device *ptmp = NULL;
	struct axdev_driver *pdrv = NULL;

	int err = 0;

	mutex_lock(&axdev_sem);

	/*step 1/7 check minor of axdev-deivce, if it has been registed */
	osal_list_for_each_entry(ptmp, &axdev_list, list) {
		if (ptmp->minor == axdev->minor) {
			mutex_unlock(&axdev_sem);
			printk("Func[%s], conflict with axdev minor[id =%d]\n",__func__, axdev->minor);
			return -EBUSY;
		}
	}

	/*step 2/7 get auto minor of axdev-device */
	if (axdev->minor == AXDEV_DYNAMIC_MINOR) {
		int i = AXDEV_DYNAMIC_MINOR - 1;
		while (--i >= 0)
			if ((ax_dev_minors[i >> 3] & (1 << (i & 7))) == 0)
				break;
		if (i < 0) {
			printk("Func[%s], Failed to get axdev minor[id =%d]\n",__func__, i);
			mutex_unlock(&axdev_sem);
			return -EBUSY;
		}
		axdev->minor = i;
	}

	if (axdev->minor < AXDEV_DYNAMIC_MINOR)
		ax_dev_minors[axdev->minor >> 3] |= 1 << (axdev->minor & 7);

	/*step 3/7 set class of axdev-device */
	axdev->axdev_class = g_axdev_class;

	/*step 4/7 register axdev-device */
	err = axdev_device_register(axdev);
	if (err < 0) {
		ax_dev_minors[axdev->minor >> 3] &= ~(1 << (axdev->minor & 7));
		printk("Func[%s], ERROR, Failed to register device of axdev [minor id=%d]\n",__func__, axdev->minor);
		goto out;
	}

	/*step 5/7 register driver of axdev-device */
	pdrv = axdev_driver_register(axdev->devfs_name, axdev->owner,axdev->drvops);
	if (IS_ERR(pdrv)) {
		axdev_device_unregister(axdev);
		ax_dev_minors[axdev->minor >> 3] &= ~(1 << (axdev->minor & 7));

		err = PTR_ERR(pdrv);
		printk("Func[%s], ERROR, Failed to register driver of axdev [minor id=%d]\n",__func__, axdev->minor);
		goto out;
	}

	/*step 6/7 attach to driver of axdev */
	axdev->driver = pdrv;
	/*step 7/7 insert new axdev-device to device list */
	osal_list_add(&axdev->list, &axdev_list);

out:
	mutex_unlock(&axdev_sem);
	return err;
}

EXPORT_SYMBOL(axdev_register);

int axdev_unregister(struct axdev_device *axdev)
{
	struct axdev_device *ptmp = NULL, *_ptmp = NULL;
	if (osal_list_empty(&axdev->list))
		return -EINVAL;

	mutex_lock(&axdev_sem);
	osal_list_for_each_entry_safe(ptmp, _ptmp, &axdev_list, list) {

		/*if found, unregister device & driver */
		if (ptmp->minor == axdev->minor) {
			osal_list_del(&axdev->list);
			axdev_driver_unregister(axdev->driver);
			axdev->driver = NULL;
			axdev_device_unregister(axdev);
			ax_dev_minors[axdev->minor >> 3] &= ~(1 << (axdev->minor & 7));
			break;
		}
	}
	mutex_unlock(&axdev_sem);
	return 0;
}

EXPORT_SYMBOL(axdev_unregister);

extern void osal_device_init(void);

static int axdev_init(void)
{
	int ret;
	osal_device_init();
	/*step 1/ create bus of axdev */
	ret = axdev_bus_init();
	if (ret) {
		printk("Func [%s] failed to axdev_bus_init\n", __func__);
		goto err0;
	}

	/*step 1/ create class of axdev */
	g_axdev_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(g_axdev_class)) {
		printk("%s: failed to create class\n", __func__);
		goto err0;
	}

	return 0;
err0:
	axdev_bus_exit();
	return ret;
}

static void __exit axdev_exit(void)
{
	if (!osal_list_empty(&axdev_list)) {
		printk("!!! Module axdev: sub module in list\n");
		return;
	}

	pr_info("destroy class [%s] \n", CLASS_NAME);
	class_destroy(g_axdev_class);
	g_axdev_class = NULL;
	axdev_bus_exit();

}

module_init(axdev_init);
module_exit(axdev_exit);

MODULE_AUTHOR("Axera");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.1");
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/barrier.h>
#include "osal_ax.h"

void AX_OSAL_SYNC_mb(void)
{
	if (IS_ENABLED(CONFIG_SMP)) {
		smp_mb();
	} else {
		mb();
	}
}

EXPORT_SYMBOL(AX_OSAL_SYNC_mb);

void AX_OSAL_SYNC_rmb(void)
{
	if (IS_ENABLED(CONFIG_SMP)) {
		smp_rmb();
	} else {
		rmb();
	}
}

EXPORT_SYMBOL(AX_OSAL_SYNC_rmb);

void AX_OSAL_SYNC_wmb(void)
{
	if (IS_ENABLED(CONFIG_SMP)) {
		smp_wmb();
	} else {
		wmb();
	}
}

EXPORT_SYMBOL(AX_OSAL_SYNC_wmb);

void AX_OSAL_SYNC_isb(void)
{
	isb();
}

EXPORT_SYMBOL(AX_OSAL_SYNC_isb);

void AX_OSAL_SYNC_dsb(void)
{
	dsb(sy);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_dsb);

void AX_OSAL_SYNC_dmb(void)
{
	dmb(sy);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_dmb);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/device.h>
#include <linux/soc/axera/ax_bw_limiter.h>
#include "osal_ax.h"

int __weak ax_bw_limiter_register_with_clk(SUB_SYS_BW_LIMITERS sub_sys_bw, void *clk)
{
	return 0;
}

int __weak ax_bw_limiter_register_with_val(SUB_SYS_BW_LIMITERS sub_sys_bw, u32 work_clk)
{
	return 0;
}

int __weak ax_bw_limiter_unregister(SUB_SYS_BW_LIMITERS sub_sys_bw, void *clk)
{
	return 0;
}

int __weak ax_bw_limiter_refresh_limiter(SUB_SYS_BW_LIMITERS sub_sys_bw)
{
	return 0;
}

int AX_OSAL_DEV_bwlimiter_register_with_clk(SUB_SYS_BW_LIMITERS sub_sys_bw, void * pclk)
{
	return ax_bw_limiter_register_with_clk(sub_sys_bw, pclk);
}

EXPORT_SYMBOL(AX_OSAL_DEV_bwlimiter_register_with_clk);

int AX_OSAL_DEV_bwlimiter_register_with_val(SUB_SYS_BW_LIMITERS sub_sys_bw, unsigned int clk)
{
	return ax_bw_limiter_register_with_val(sub_sys_bw, clk);
}

EXPORT_SYMBOL(AX_OSAL_DEV_bwlimiter_register_with_val);

int AX_OSAL_DEV_bwlimiter_unregister(SUB_SYS_BW_LIMITERS sub_sys_bw, void * pclk)
{
	return ax_bw_limiter_unregister(sub_sys_bw, pclk);
}

EXPORT_SYMBOL(AX_OSAL_DEV_bwlimiter_unregister);

int AX_OSAL_DEV_bwlimiter_refresh_limiter(SUB_SYS_BW_LIMITERS sub_sys_bw)
{
	return ax_bw_limiter_refresh_limiter(sub_sys_bw);
}

EXPORT_SYMBOL(AX_OSAL_DEV_bwlimiter_refresh_limiter);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <asm/cacheflush.h>
#include <linux/dma-direction.h>
#include <linux/version.h>

#include "osal_ax.h"
#include "osal_dev_ax.h"

void AX_OSAL_DEV_invalidate_dcache_area(void * addr, int size)
{
#ifdef CONFIG_ARM64
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	dcache_inval_poc((unsigned long) addr, (unsigned long) (addr + size));
#else
	__inval_dcache_area(addr, (size_t)size);
#endif
#else
	dmac_inv_range(addr, addr+size);
#endif

	return;
}

EXPORT_SYMBOL(AX_OSAL_DEV_invalidate_dcache_area);

void AX_OSAL_DEV_flush_dcache_area(void * kvirt, unsigned long length)
{
#ifdef CONFIG_ARM64
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	dcache_clean_inval_poc((unsigned long) kvirt, (unsigned long) (kvirt + length));
#else
	__flush_dcache_area(kvirt, (size_t)length);
#endif
#else
	__cpuc_flush_dcache_area(kvirt, length);
#endif
}

EXPORT_SYMBOL(AX_OSAL_DEV_flush_dcache_area);

void AX_OSAL_DEV_outer_dcache_area(u64 phys_addr_start, u64 phys_addr_end)
{
#ifdef CONFIG_ARM64
	return;
#else
	outer_flush_range(phys_addr_start, phys_addr_end);
#endif
}

EXPORT_SYMBOL(AX_OSAL_DEV_outer_dcache_area);

void AX_OSAL_DEV_flush_dcache_all(void)
{
#ifdef CONFIG_ARM64
	//axera_armv8_flush_cache_all();
#else

#ifdef CONFIG_SMP
    on_each_cpu((smp_call_func_t)__cpuc_flush_kern_all, NULL, 1);
#else
    __cpuc_flush_kern_all();
#endif

    outer_flush_all();

#endif
}

EXPORT_SYMBOL(AX_OSAL_DEV_flush_dcache_all);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/device.h>

#include <linux/clk-provider.h>
#include <linux/clk/clk-conf.h>
#include <linux/clkdev.h>

#include "linux/platform_device.h"
#include "linux/device.h"

#include "osal_ax.h"
#include "osal_dev_ax.h"
#include "axdev.h"
#include "axdev_log.h"
#include "osal_lib_ax.h"

void *AX_OSAL_DEV_devm_clk_get(void * pdev, const char *id)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	return devm_clk_get(&pvdev->dev, id);
}

EXPORT_SYMBOL(AX_OSAL_DEV_devm_clk_get);

void AX_OSAL_DEV_devm_clk_put(void * pdev, void * pclk)
{
	struct platform_device *pvdev;
	struct clk *clk;
	pvdev = (struct platform_device *)pdev;
	clk = (struct clk *)pclk;
	devm_clk_put(&pvdev->dev, clk);
}

EXPORT_SYMBOL(AX_OSAL_DEV_devm_clk_put);

void AX_OSAL_DEV_clk_disable(void * pclk)
{
	struct clk *clk = (struct clk *)pclk;
	clk_disable(clk);
}

EXPORT_SYMBOL(AX_OSAL_DEV_clk_disable);

int AX_OSAL_DEV_clk_enable(void * pclk)
{
	struct clk *clk = (struct clk *)pclk;
	return clk_enable(clk);
}

EXPORT_SYMBOL(AX_OSAL_DEV_clk_enable);

bool AX_OSAL_DEV_clk_is_enabled(void * pclk)
{
	struct clk *clk = (struct clk *)pclk;
	return __clk_is_enabled(clk);
}

EXPORT_SYMBOL(AX_OSAL_DEV_clk_is_enabled);

int AX_OSAL_DEV_clk_prepare_enable(void * pclk)
{
	struct clk *clk = (struct clk *)pclk;
	return clk_prepare_enable(clk);
}

EXPORT_SYMBOL(AX_OSAL_DEV_clk_prepare_enable);

int AX_OSAL_DEV_clk_set_rate(void * pclk, unsigned long rate)
{
	struct clk *clk = (struct clk *)pclk;
	return clk_set_rate(clk, rate);
}

EXPORT_SYMBOL(AX_OSAL_DEV_clk_set_rate);

unsigned long AX_OSAL_DEV_clk_get_rate(void * pclk)
{
	struct clk *clk = (struct clk *)pclk;
	return clk_get_rate(clk);
}

EXPORT_SYMBOL(AX_OSAL_DEV_clk_get_rate);

void AX_OSAL_DEV_clk_disable_unprepare(void * pclk)
{
	struct clk *clk = (struct clk *)pclk;
	clk_disable_unprepare(clk);
}

EXPORT_SYMBOL(AX_OSAL_DEV_clk_disable_unprepare);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include "osal_lib_ax.h"
#include "osal_logdebug_ax.h"

#define ax_vprintk vprintk

static AX_OSAL_LIST_HEAD(list);

int AX_OSAL_DBG_printk(const char * fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);
	r = ax_vprintk(fmt, args);
	va_end(args);
	return r;
}

EXPORT_SYMBOL(AX_OSAL_DBG_printk);

void AX_OSAL_DBG_panic(const char * fmt, const char * fun, int line, const char * cond)
{
	panic(fmt);
}

EXPORT_SYMBOL(AX_OSAL_DBG_panic);

void AX_OSAL_DBG_LogOutput(int target, int level, const char *tag, const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ax_vprintk(fmt, args);
	va_end(args);
	return;
}

EXPORT_SYMBOL(AX_OSAL_DBG_LogOutput);

void AX_OSAL_DBG_ISPLogoutput(int level, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	ax_vprintk(fmt, args);
	va_end(args);

	return;
}

EXPORT_SYMBOL(AX_OSAL_DBG_ISPLogoutput);

void AX_OSAL_DBG_NPULogoutput(int level, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	ax_vprintk(fmt, args);
	va_end(args);

	return;
}

EXPORT_SYMBOL(AX_OSAL_DBG_NPULogoutput);

int AX_OSAL_DBG_SetLogLevel(int level)
{
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_DBG_SetLogLevel);

int AX_OSAL_DBG_SetLogTarget(int level)
{
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_DBG_SetLogTarget);

int AX_OSAL_DBG_EnableTimestamp(int enable)
{
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_DBG_EnableTimestamp);

int AX_OSAL_DBG_EnableTraceEvent(int module, int enable)
{
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_DBG_EnableTraceEvent);

int AX_OSAL_DBG_seq_show(struct seq_file *s, void *p)
{
	AX_PROC_DIR_ENTRY_T *oldsentry = s->private;
	AX_PROC_DIR_ENTRY_T sentry;
	if (oldsentry == NULL) {
		printk("%s error oldsentry == NULL\n", __func__);
		return -1;
	}
	memset(&sentry, 0, sizeof(AX_PROC_DIR_ENTRY_T));

	sentry.seqfile = s;
	sentry.private_data = oldsentry->private_data;
	oldsentry->read(&sentry);
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_DBG_seq_show);

ssize_t AX_OSAL_DBG_procwrite(struct file * file, const char __user * buf, size_t count, loff_t * ppos)
{
	AX_PROC_DIR_ENTRY_T *item = PDE_DATA(file_inode(file));

	if (item && item->write) {
		return item->write(item, buf, count, (long long *)ppos);
	}

	return -ENOSYS;
}

EXPORT_SYMBOL(AX_OSAL_DBG_procwrite);

int AX_OSAL_DBG_procopen(struct inode *inode, struct file *file)
{
	AX_PROC_DIR_ENTRY_T *sentry = PDE_DATA(inode);
	if (sentry != NULL && sentry->open != NULL) {
		sentry->open(sentry);
	}
	return single_open(file, AX_OSAL_DBG_seq_show, sentry);
}

EXPORT_SYMBOL(AX_OSAL_DBG_procopen);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static struct proc_ops osal_proc_ops = {

	.proc_open = AX_OSAL_DBG_procopen,
	.proc_read = seq_read,
	.proc_write = AX_OSAL_DBG_procwrite,
	.proc_lseek = seq_lseek,
	.proc_release = single_release
};
#else
static struct file_operations osal_proc_ops = {

	.open = AX_OSAL_DBG_procopen,
	.read = seq_read,
	.write = AX_OSAL_DBG_procwrite,
	.llseek = seq_lseek,
	.release = single_release
};
#endif

AX_PROC_DIR_ENTRY_T *AX_OSAL_DBG_create_proc_entry(const char * name, AX_PROC_DIR_ENTRY_T * parent)
{
	struct proc_dir_entry *entry = NULL;
	AX_PROC_DIR_ENTRY_T *sentry = NULL;
	sentry = kmalloc(sizeof(AX_PROC_DIR_ENTRY_T), GFP_KERNEL);
	if (!sentry) {
		printk("%s alloc AX_PROC_DIR_ENTRY_T failed\n", __FUNCTION__);
		return NULL;
	}

	AX_OSAL_LIB_memset(sentry, 0, sizeof(AX_PROC_DIR_ENTRY_T));

	AX_OSAL_LIB_strncpy(sentry->name, name, sizeof(sentry->name) - 1);

	if (parent == NULL) {
		entry = proc_create_data(name, 0, NULL, &osal_proc_ops, sentry);
	} else {
		entry = proc_create_data(name, 0, parent->proc_dir_entry, &osal_proc_ops, sentry);
	}
	if (entry == NULL) {
		printk("%s create data failed\n", __FUNCTION__);
		kfree(sentry);
		sentry = NULL;
		return NULL;
	}
	sentry->proc_dir_entry = entry;
	sentry->open = NULL;
	sentry->parent = parent;
	AX_OSAL_LIB_init_list_head(&(sentry->node));
	AX_OSAL_LIB_list_add_tail(&(sentry->node), &list);
	return sentry;
}

EXPORT_SYMBOL(AX_OSAL_DBG_create_proc_entry);

AX_PROC_DIR_ENTRY_T *AX_OSAL_DBG_proc_mkdir(const char * name, AX_PROC_DIR_ENTRY_T * parent)
{
	struct proc_dir_entry *proc = NULL;
	AX_PROC_DIR_ENTRY_T *sproc = NULL;
	sproc = kmalloc(sizeof(AX_PROC_DIR_ENTRY_T), GFP_KERNEL);
	if (!sproc) {
		printk("%s alloc AX_PROC_DIR_ENTRY_T failed\n", __func__);
		return NULL;
	}
	AX_OSAL_LIB_memset(sproc, 0, sizeof(AX_PROC_DIR_ENTRY_T));

	AX_OSAL_LIB_strncpy(sproc->name, name, sizeof(sproc->name) - 1);

	if (parent != NULL) {
		proc = proc_mkdir_data(name, 0, parent->proc_dir_entry, sproc);
	} else {
		proc = proc_mkdir_data(name, 0, NULL, sproc);
	}
	if (proc == NULL) {
		printk("%s proc error\n", __func__);
		kfree(sproc);
		sproc = NULL;
		return NULL;
	}
	sproc->proc_dir_entry = proc;
	sproc->parent = parent;
	AX_OSAL_LIB_init_list_head(&(sproc->node));
	AX_OSAL_LIB_list_add_tail(&(sproc->node), &list);
	return sproc;

}

EXPORT_SYMBOL(AX_OSAL_DBG_proc_mkdir);

void AX_OSAL_DBG_remove_proc_entry(const char * name, AX_PROC_DIR_ENTRY_T * parent)
{
	struct AX_PROC_DIR_ENTRY *sproc = NULL;

	if (name == NULL) {
		printk("%s - parameter invalid!\n", __func__);
		return;
	}
	if (parent != NULL)
		remove_proc_entry(name, parent->proc_dir_entry);
	else
		remove_proc_entry(name, NULL);
	AX_OSAL_LIB_list_for_each_entry(sproc, &list, node) {
		if ((AX_OSAL_LIB_strncmp(sproc->name, name, sizeof(sproc->name)) == 0) && (parent == sproc->parent)) {
			AX_OSAL_LIB_list_del(&(sproc->node));
			break;
		}
	}
	if (sproc != NULL)
		kfree(sproc);
}

EXPORT_SYMBOL(AX_OSAL_DBG_remove_proc_entry);

void AX_OSAL_DBG_seq_printf(AX_PROC_DIR_ENTRY_T * entry, const char * fmt, ...)
{
	struct seq_file *s = (struct seq_file *)(entry->seqfile);
	va_list args;

	va_start(args, fmt);
	seq_vprintf(s, fmt, args);
	va_end(args);
	return;
}

EXPORT_SYMBOL(AX_OSAL_DBG_seq_printf);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/pm_opp.h>
#include <linux/devfreq.h>
#include <linux/pm_qos.h>

#include <linux/platform_device.h>
#include "linux/device.h"
#include <linux/version.h>

#include "osal_ax.h"
#include "osal_dev_ax.h"
#include "axdev.h"
#include "axdev_log.h"
#include "osal_lib_ax.h"

static int AX_OSAL_DEV_comon_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct dev_pm_opp *opp;
	unsigned long rate;
	unsigned long volt;
	struct platform_device *pdev = to_platform_device(dev);

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(opp)) {
		return PTR_ERR(opp);
	}

	rate = dev_pm_opp_get_freq(opp);
	volt = dev_pm_opp_get_voltage(opp);

	((struct AX_DEVFREQ_DEV_PROFILE *)pdev->axera_devdfs_ptr)->target((void *) dev, rate, volt);
	dev_pm_opp_put(opp);
	return 0;
}

static int AX_OSAL_DEV_comon_get_dev_status(struct device *dev, struct devfreq_dev_status *stat)
{
	return 0;
}

static int AX_OSAL_DEV_comon_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct platform_device *pdev = to_platform_device(dev);
	*freq = ((struct AX_DEVFREQ_DEV_PROFILE *)pdev->axera_devdfs_ptr)->get_cur_freq((void *) dev);
	return 0;
}

static void AX_OSAL_DEV_comon_exit(struct device *dev)
{
	return;
}

int AX_OSAL_DEV_pm_opp_of_add_table(void * pdev)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	return dev_pm_opp_of_add_table(dev);
}

EXPORT_SYMBOL(AX_OSAL_DEV_pm_opp_of_add_table);

void AX_OSAL_DEV_pm_opp_of_remove_table(void * pdev)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	dev_pm_opp_remove_table(dev);
}

EXPORT_SYMBOL(AX_OSAL_DEV_pm_opp_of_remove_table);

int AX_OSAL_DEV_devm_devfreq_add_device(void * pdev, struct AX_DEVFREQ_DEV_PROFILE *ax_profile,
					   const char *governor_name, void *data)
{
	struct devfreq *axdevdfs;
	struct devfreq_dev_profile *pdevfile;
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	pdevfile = devm_kzalloc(dev, sizeof(struct devfreq_dev_profile), GFP_KERNEL);

	pdevfile->initial_freq = ax_profile->initial_freq;
	pdevfile->polling_ms = ax_profile->polling_ms;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	pdevfile->timer = ax_profile->timer;
	pdevfile->is_cooling_device = ax_profile->is_cooling_device;
#endif

	pdevfile->target = AX_OSAL_DEV_comon_target;
	pdevfile->get_dev_status = AX_OSAL_DEV_comon_get_dev_status;
	pdevfile->get_cur_freq = AX_OSAL_DEV_comon_get_cur_freq;
	pdevfile->exit = AX_OSAL_DEV_comon_exit;
	pdevfile->freq_table = ax_profile->freq_table;
	pdevfile->max_state = ax_profile->max_state;

	pvdev->axera_devdfs_ptr = ax_profile;
	axdevdfs = devm_devfreq_add_device(dev, pdevfile, governor_name, data);
	if (!axdevdfs) {
		printk("cannot add device to devfreq!\n");
		return -1;
	}

	return 0;
}

EXPORT_SYMBOL(AX_OSAL_DEV_devm_devfreq_add_device);

int AX_OSAL_DEV_pm_opp_of_disable(void * pdev, unsigned long freq)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	return dev_pm_opp_disable(dev, freq);
}

EXPORT_SYMBOL(AX_OSAL_DEV_pm_opp_of_disable);

void AX_OSAL_DEV_pm_opp_of_remove(void * pdev, unsigned long freq)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	dev_pm_opp_remove(dev, freq);
}

EXPORT_SYMBOL(AX_OSAL_DEV_pm_opp_of_remove);

int AX_OSAL_DEV_pm_opp_of_add(void * pdev, unsigned long freq, unsigned long volt)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	return dev_pm_opp_add(dev, freq, volt);
}

EXPORT_SYMBOL(AX_OSAL_DEV_pm_opp_of_add);/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/kmod.h>
#include <linux/fs.h>

#include <linux/wait.h>

#include "linux/platform_device.h"
#include "linux/device.h"
#include "osal_ax.h"
#include "osal_dev_ax.h"
#include "axdev.h"
#include "axdev_log.h"
#include "osal_lib_ax.h"
#define DRVAL_DEBUG 0

static DEFINE_MUTEX(ax_dev_sem);

#define GET_FILE(file) do\
{\
    if (__get_file(file) < 0)\
        return -1;\
}while(0)

#define PUT_FILE(file) do\
{\
    if (__put_file(file) < 0)\
        return -1;\
}while(0)

typedef struct osal_axera_dev {
	struct AX_DEV osal_dev;
	struct axdev_device axdev_dev;
} osal_axera_dev_t;

spinlock_t f_lock;

void osal_device_init(void)
{
	spin_lock_init(&f_lock);
}

static int __get_file(struct file *file)
{
	AX_DEV_PRIVATE_DATA_T *pdata;

	spin_lock(&f_lock);
	pdata = (AX_DEV_PRIVATE_DATA_T *) (file->private_data);
	if (pdata == NULL) {
		spin_unlock(&f_lock);
		return -1;
	}

	pdata->f_ref_cnt++;
	spin_unlock(&f_lock);

	return 0;
}

static int __put_file(struct file *file)
{
	AX_DEV_PRIVATE_DATA_T *pdata;

	spin_lock(&f_lock);
	pdata = file->private_data;
	if (pdata == NULL) {
		spin_unlock(&f_lock);
		return -1;
	}

	pdata->f_ref_cnt--;
	spin_unlock(&f_lock);

	return 0;
}

static int osal_open(struct inode *inode, struct file *file)
{
	struct axdev_device *axdev;
	osal_axera_dev_t *axera_dev;
	AX_DEV_PRIVATE_DATA_T *pdata;
	unsigned minor = iminor(inode);
	unsigned major = imajor(inode);

	pdata = NULL;
	/*
	   if (!capable(CAP_SYS_RAWIO) || !capable(CAP_SYS_ADMIN))
	   return -EPERM;
	 */

	AXDEV_LOG_DEBUG(" OSAL OPEN (major = %u)(minor = %u). \n", major, minor);

	//axdev = getaxdev(inode);
	axdev = (struct axdev_device *)file->private_data;
	if (axdev == NULL) {
		printk("%s - get axdev device error!\n", __func__);
		return -1;
	}
	axera_dev = osal_container_of(axdev, struct osal_axera_dev, axdev_dev);
	pdata = (AX_DEV_PRIVATE_DATA_T *) kmalloc(sizeof(AX_DEV_PRIVATE_DATA_T), GFP_KERNEL);
	if (pdata == NULL) {
		printk("%s - kmalloc error!\n", __func__);
		return -1;
	}

	memset(pdata, 0, sizeof(AX_DEV_PRIVATE_DATA_T));

	file->private_data = pdata;
	pdata->dev = &(axera_dev->osal_dev);
	pdata->f_flags = file->f_flags;
	pdata->file = file;

	//pdata->data = __waitpoll_create()
	/*
	   if (axera_dev->osal_dev.fops->open != NULL)
	   return axera_dev->osal_dev.fops->open((void *) & (pdata->data));
	 */
	if (axera_dev->osal_dev.fops->open != NULL)
		return axera_dev->osal_dev.fops->open((void *)pdata);

	return 0;
}

static ssize_t osal_read(struct file *file, char __user * buf, size_t size, loff_t * offset)
{
	AX_DEV_PRIVATE_DATA_T *pdata = file->private_data;
	int ret = 0;

	GET_FILE(file);

	if (pdata->dev->fops->read != NULL) {
		ret = pdata->dev->fops->read(buf, (int)size, (long *)offset, (void *)pdata);
	}

	PUT_FILE(file);
	return ret;
}

static ssize_t osal_write(struct file *file, const char __user * buf, size_t size, loff_t * offset)
{
	AX_DEV_PRIVATE_DATA_T *pdata = file->private_data;
	int ret = 0;

	GET_FILE(file);
	if (pdata->dev->fops->write != NULL) {
		ret = pdata->dev->fops->write(buf, (int)size, (long *)offset, (void *)pdata);
	}
	PUT_FILE(file);
	return ret;
}

static loff_t osal_llseek(struct file *file, loff_t offset, int whence)
{
	AX_DEV_PRIVATE_DATA_T *pdata = file->private_data;
	int ret = 0;

	GET_FILE(file);
	if (DRVAL_DEBUG)
		printk("%s - file->private_data=%p!\n", __func__, pdata);

	if (whence == SEEK_SET) {
		if (pdata->dev->fops->llseek != NULL) {
			ret = pdata->dev->fops->llseek((long)offset, AX_OSAL_SEEK_SET, (void *)pdata);
		}
	} else if (whence == SEEK_CUR) {
		if (pdata->dev->fops->llseek != NULL) {
			ret = pdata->dev->fops->llseek((long)offset, AX_OSAL_SEEK_CUR, (void *)pdata);
		}
	} else if (whence == SEEK_END) {
		if (pdata->dev->fops->llseek != NULL) {
			ret = pdata->dev->fops->llseek((long)offset, AX_OSAL_SEEK_END, (void *)pdata);
		}
	}

	PUT_FILE(file);
	return (loff_t) ret;
}

static int osal_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	AX_DEV_PRIVATE_DATA_T *pdata = file->private_data;

	GET_FILE(file);

	if (DRVAL_DEBUG)
		printk("%s - file->private_data=%p!\n", __func__, pdata);

	if ((pdata == NULL) || (pdata->dev == NULL) || (pdata->dev->fops == NULL)) {
		printk("WARNING !!!! MEMORY LEAK. you are woring to invoke AX_OSAL_DEV_destroydev early  ,Func:%s - \n",__func__);
		return 0;
	}

	if (pdata->dev->fops->release != NULL)
		ret = pdata->dev->fops->release((void *)pdata);

	if (ret != 0) {
		PUT_FILE(file);
		printk("%s - release failed!\n", __func__);
		return ret;
	}

	PUT_FILE(file);
	spin_lock(&f_lock);
	if (pdata->f_ref_cnt != 0) {
		printk("%s - release failed!\n", __func__);
		spin_unlock(&f_lock);
		return -1;
	}
	kfree(file->private_data);
	file->private_data = NULL;
	spin_unlock(&f_lock);

	return 0;
}

static long __osal_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -1;
	AX_DEV_PRIVATE_DATA_T *pdata = file->private_data;

	if (DRVAL_DEBUG) {
		printk("%s - file->private_data=%p, cmd = %u!\n", __func__, pdata, cmd);
	}

	if (pdata->dev->fops->unlocked_ioctl != NULL) {
		ret = pdata->dev->fops->unlocked_ioctl(cmd, arg, (void *)pdata);
	}

	return ret;
}

static long osal_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	GET_FILE(file);

	ret = __osal_unlocked_ioctl(file, cmd, arg);
	PUT_FILE(file);

	return ret;
}

static unsigned int osal_poll(struct file *file, struct poll_table_struct *table)
{
	AX_DEV_PRIVATE_DATA_T *pdata = file->private_data;
	struct AX_POLL t;
	unsigned int ret = 0;

	GET_FILE(file);

	if (DRVAL_DEBUG)
		printk("%s - table=%p, file=%p!\n", __func__, table, file);
	t.poll_table = table;
	t.data = file;
	/*device fw, enhance to add API of poll_wait */
	t.wait = &(pdata->dev->dev_wait);
	/*
	   if (pdata->dev->fops->poll != NULL)
	   ret = pdata->dev->fops->poll(&t, (void *) & (pdata->data));
	 */
	if (pdata->dev->fops->poll != NULL)
		ret = pdata->dev->fops->poll(&t, (void *)pdata);

	PUT_FILE(file);
	return ret;
}

static int osal_mmap(struct file *file, struct vm_area_struct *vm)
{
	struct AX_VM osal_vm;
	AX_DEV_PRIVATE_DATA_T *pdata = file->private_data;
	osal_vm.vm = vm;

	if (DRVAL_DEBUG)
		printk("%s - start=%lx, end=%lx!, off=%lx\n", __func__, vm->vm_start, vm->vm_end,
			    vm->vm_pgoff);

	if (pdata->dev->fops->mmap != NULL)
		return pdata->dev->fops->mmap(&osal_vm, vm->vm_start, vm->vm_end, vm->vm_pgoff, (void *)pdata);

	return 0;
}

static struct file_operations s_osal_fops = {
	.owner = THIS_MODULE,
	.open = osal_open,
	.read = osal_read,
	.write = osal_write,
	.llseek = osal_llseek,
	.unlocked_ioctl = osal_unlocked_ioctl,
	.release = osal_release,
	.poll = osal_poll,
	.mmap = osal_mmap,
};

static int osal_pm_prepare(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_prepare)
		return axera_dev->osal_dev.osal_pmops->pm_prepare(&(axera_dev->osal_dev));
	return 0;
}

static void osal_pm_complete(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_complete)
		axera_dev->osal_dev.osal_pmops->pm_complete(&(axera_dev->osal_dev));
}

static int osal_pm_suspend(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_suspend)
		return axera_dev->osal_dev.osal_pmops->pm_suspend(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_resume(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_resume)
		return axera_dev->osal_dev.osal_pmops->pm_resume(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_freeze(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_freeze)
		return axera_dev->osal_dev.osal_pmops->pm_freeze(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_thaw(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_thaw)
		return axera_dev->osal_dev.osal_pmops->pm_thaw(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_poweroff(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_poweroff)
		return axera_dev->osal_dev.osal_pmops->pm_poweroff(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_restore(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_restore)
		return axera_dev->osal_dev.osal_pmops->pm_restore(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_suspend_late(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_suspend_late)
		return axera_dev->osal_dev.osal_pmops->pm_suspend_late(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_resume_early(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_resume_early)
		return axera_dev->osal_dev.osal_pmops->pm_resume_early(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_freeze_late(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_freeze_late)
		return axera_dev->osal_dev.osal_pmops->pm_freeze_late(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_thaw_early(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_thaw_early)
		return axera_dev->osal_dev.osal_pmops->pm_thaw_early(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_poweroff_late(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_poweroff_late)
		return axera_dev->osal_dev.osal_pmops->pm_poweroff_late(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_restore_early(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_restore_early)
		return axera_dev->osal_dev.osal_pmops->pm_restore_early(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_suspend_noirq(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_suspend_noirq)
		return axera_dev->osal_dev.osal_pmops->pm_suspend_noirq(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_resume_noirq(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_resume_noirq)
		return axera_dev->osal_dev.osal_pmops->pm_resume_noirq(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_freeze_noirq(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_freeze_noirq)
		return axera_dev->osal_dev.osal_pmops->pm_freeze_noirq(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_thaw_noirq(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_thaw_noirq)
		return axera_dev->osal_dev.osal_pmops->pm_thaw_noirq(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_poweroff_noirq(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_poweroff_noirq)
		return axera_dev->osal_dev.osal_pmops->pm_poweroff_noirq(&(axera_dev->osal_dev));
	return 0;
}

static int osal_pm_restore_noirq(struct axdev_device *axdev)
{
	osal_axera_dev_t *axera_dev = container_of(axdev, struct osal_axera_dev, axdev_dev);
	if (axera_dev->osal_dev.osal_pmops && axera_dev->osal_dev.osal_pmops->pm_restore_noirq)
		return axera_dev->osal_dev.osal_pmops->pm_restore_noirq(&(axera_dev->osal_dev));
	return 0;
}

static struct axdev_ops s_osal_pmops = {
	.pm_prepare = osal_pm_prepare,
	.pm_complete = osal_pm_complete,
	.pm_suspend = osal_pm_suspend,
	.pm_resume = osal_pm_resume,
	.pm_freeze = osal_pm_freeze,
	.pm_thaw = osal_pm_thaw,
	.pm_poweroff = osal_pm_poweroff,
	.pm_restore = osal_pm_restore,
	.pm_suspend_late = osal_pm_suspend_late,
	.pm_resume_early = osal_pm_resume_early,
	.pm_freeze_late = osal_pm_freeze_late,
	.pm_thaw_early = osal_pm_thaw_early,
	.pm_poweroff_late = osal_pm_poweroff_late,
	.pm_restore_early = osal_pm_restore_early,
	.pm_suspend_noirq = osal_pm_suspend_noirq,
	.pm_resume_noirq = osal_pm_resume_noirq,
	.pm_freeze_noirq = osal_pm_freeze_noirq,
	.pm_thaw_noirq = osal_pm_thaw_noirq,
	.pm_poweroff_noirq = osal_pm_poweroff_noirq,
	.pm_restore_noirq = osal_pm_restore_noirq,
};

unsigned short ax_dev_minors[AXDEV_DYNAMIC_MINOR / 8];

AX_DEV_T *AX_OSAL_DEV_createdev(char * name)
{
	osal_axera_dev_t *pdev;
	if (name == NULL) {
		printk("%s - parameter invalid!\n", __func__);
		return NULL;
	}
	/*step 1/4. allocate memory for cost dev */
	pdev = (osal_axera_dev_t *) kmalloc(sizeof(osal_axera_dev_t), GFP_KERNEL);
	if (pdev == NULL) {
		printk("%s - kmalloc error!\n", __func__);
		return NULL;
	}
	memset(pdev, 0, sizeof(osal_axera_dev_t));

	/*step 2/4. get device name, and set to OSAL Device */
	strncpy(pdev->osal_dev.name, name, sizeof(pdev->osal_dev.name) - 1);

	/*step 3/4. get cost dev, and set to OSAL Device */
	pdev->osal_dev.dev = pdev;

	/*step 4/4. get cost dev, and set to OSAL Device */
	return &(pdev->osal_dev);
}

EXPORT_SYMBOL(AX_OSAL_DEV_createdev);

int AX_OSAL_DEV_destroydev(AX_DEV_T * osal_dev)
{
	osal_axera_dev_t *pdev;
	if (osal_dev == NULL) {
		pr_info("%s - parameter invalid!\n", __func__);
		return -1;
	}
	pdev = osal_dev->dev;
	if (pdev == NULL) {
		pr_info("%s - parameter invalid!\n", __func__);
		return -1;
	}
	kfree(pdev);
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_DEV_destroydev);

unsigned int AX_OSAL_DEV_get_minor(void)
{

	int i = AXDEV_DYNAMIC_MINOR - 1;
	unsigned int minor = 0;
	mutex_lock(&ax_dev_sem);
	while (--i >= 0) {
		if ((ax_dev_minors[i >> 3] & (1 << (i & 7))) == 0)
			break;
	}

	if (i < 0) {
		mutex_unlock(&ax_dev_sem);
		return -EBUSY;
	}
	minor = i;
	if (minor < AXDEV_DYNAMIC_MINOR) {
		ax_dev_minors[minor >> 3] |= 1 << (minor & 7);
	}

	mutex_unlock(&ax_dev_sem);
	return i;
}

unsigned int AX_OSAL_DEV_release_minor(unsigned int minor)
{
	if (minor >= AXDEV_DYNAMIC_MINOR) {
		return -1;
	}

	mutex_lock(&ax_dev_sem);
	ax_dev_minors[minor >> 3] &= ~(1 << (minor & 7));
	mutex_unlock(&ax_dev_sem);

	return 0;
}

static int __waitqueue_init(struct AX_WAIT *osal_wait)
{
	if (osal_wait == NULL) {
		printk("%s error osal_wait == NULL\n", __func__);
		return -1;
	}

	return AX_OSAL_SYNC_waitqueue_init(osal_wait);
}

int AX_OSAL_DEV_device_register(AX_DEV_T * osal_dev)
{
	int ret = 0;
	struct axdev_device *axdev;
	if (osal_dev == NULL || osal_dev->fops == NULL) {
		printk("%s parameter error\n", __func__);
		return -1;
	}

	if (__waitqueue_init(&osal_dev->dev_wait)) {
		printk("%s failed !!!\n", __func__);
		return -1;
	}

	/*step 1/5 get axdev-device by osal-device  */
	axdev = &(((osal_axera_dev_t *) (osal_dev->dev))->axdev_dev);

	/*step 2/5 get minor of axdev-device  */
	if (osal_dev->minor != 0)
		axdev->minor = osal_dev->minor;
	else
		axdev->minor = AXDEV_DYNAMIC_MINOR;

	/*step 3/5 initialize axdev-device */
	axdev->owner = THIS_MODULE;
	axdev->fops = &s_osal_fops;
	axdev->drvops = &s_osal_pmops;
	axdev->major = AXDEV_DEVICE_MAJOR;

	/*step 4/5 get name of axdev-device */
	strncpy(axdev->devfs_name, osal_dev->name, sizeof(axdev->devfs_name) - 1);

	/*step 5/5 register axdev-device */
	ret = axdev_register(axdev);
	if (ret) {
		printk("%s Failed to register axdev \n", __func__);
	}
	osal_dev->minor = axdev->minor;

	return ret;
}

EXPORT_SYMBOL(AX_OSAL_DEV_device_register);

void AX_OSAL_DEV_device_unregister(AX_DEV_T * osal_dev)
{
	if (osal_dev == NULL) {
		printk("%s error osal_dev == NULL\n", __func__);
		return;
	}
	AX_OSAL_SYNC_wait_destroy(&osal_dev->dev_wait);

	axdev_unregister((struct axdev_device *)&(((osal_axera_dev_t *) (osal_dev->dev))->axdev_dev));

	return;
}

EXPORT_SYMBOL(AX_OSAL_DEV_device_unregister);

void AX_OSAL_DEV_poll_wait(AX_POLL_T * table, AX_WAIT_T * wait)
{
	if (DRVAL_DEBUG)
		printk("%s - call poll_wait +!, table=%p, file=%p\n", __func__, table->poll_table,table->data);

	poll_wait((struct file *)table->data, (wait_queue_head_t *) (wait->wait), table->poll_table);

	if (DRVAL_DEBUG)
		printk("%s call poll_wait \n", __func__);

	return;
}

EXPORT_SYMBOL(AX_OSAL_DEV_poll_wait);

void AX_OSAL_DEV_pgprot_noncached(AX_VM_T * vm)
{
	struct vm_area_struct *v = (struct vm_area_struct *)(vm->vm);
	v->vm_page_prot = pgprot_writecombine(v->vm_page_prot);
	return;
}

EXPORT_SYMBOL(AX_OSAL_DEV_pgprot_noncached);

void AX_OSAL_DEV_pgprot_cached(AX_VM_T * vm)
{
	struct vm_area_struct *v = (struct vm_area_struct *)(vm->vm);

#ifdef CONFIG_ARM64
	v->vm_page_prot = __pgprot(pgprot_val(v->vm_page_prot)
				   | PTE_VALID | PTE_DIRTY | PTE_AF);
#else

	v->vm_page_prot = __pgprot(pgprot_val(v->vm_page_prot) | L_PTE_PRESENT
				   | L_PTE_YOUNG | L_PTE_DIRTY | L_PTE_MT_DEV_CACHED);
#endif

	return;
}

EXPORT_SYMBOL(AX_OSAL_DEV_pgprot_cached);

void AX_OSAL_DEV_pgprot_writecombine(AX_VM_T * vm)
{
	struct vm_area_struct *v = (struct vm_area_struct *)(vm->vm);
	v->vm_page_prot = pgprot_writecombine(v->vm_page_prot);

	return;
}

EXPORT_SYMBOL(AX_OSAL_DEV_pgprot_writecombine);

void AX_OSAL_DEV_pgprot_stronglyordered(AX_VM_T * vm)
{
	struct vm_area_struct *v = (struct vm_area_struct *)(vm->vm);

#ifdef CONFIG_ARM64
	v->vm_page_prot = pgprot_device(v->vm_page_prot);
#else
	v->vm_page_prot = pgprot_stronglyordered(v->vm_page_prot);
#endif

	return;
}

EXPORT_SYMBOL(AX_OSAL_DEV_pgprot_stronglyordered);

int AX_OSAL_DEV_remap_pfn_range(AX_VM_T * vm, unsigned long addr, unsigned long pfn, unsigned long size)
{
	struct vm_area_struct *v = (struct vm_area_struct *)(vm->vm);
	if (0 == size) {
		return -EPERM;
	}
	return remap_pfn_range(v, addr, pfn, size, v->vm_page_prot);
}

EXPORT_SYMBOL(AX_OSAL_DEV_remap_pfn_range);

int AX_OSAL_DEV_io_remap_pfn_range(AX_VM_T * vm, unsigned long addr, unsigned long pfn, unsigned long size)
{
	struct vm_area_struct *v = (struct vm_area_struct *)(vm->vm);
	v->vm_flags |= VM_IO;
	if (0 == size) {
		return -EPERM;
	}
	return io_remap_pfn_range(v, addr, pfn, size, v->vm_page_prot);
}

EXPORT_SYMBOL(AX_OSAL_DEV_io_remap_pfn_range);

void *AX_OSAL_DEV_to_dev(AX_DEV_T *ax_dev)
{
	if (ax_dev)
		return &((((osal_axera_dev_t *) (ax_dev->dev))->axdev_dev).device);
	else
		return NULL;
}

EXPORT_SYMBOL(AX_OSAL_DEV_to_dev);/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/kmod.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "linux/platform_device.h"
#include "linux/device.h"

#include "osal_ax.h"

#include "osal_dev_ax.h"
#include "axdev.h"
#include "axdev_log.h"


int  __attribute__ ((__noinline__)) AX_OSAL_DEV_of_property_read_string(void * pdev, const char *propname,const char **out_string)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	return of_property_read_string(dev->of_node, propname, out_string);
}

EXPORT_SYMBOL(AX_OSAL_DEV_of_property_read_string);

#if 0
const __attribute__ ((__noinline__))
void *AX_OSAL_DEV_of_get_property(void * pdev, const char * name, int * lenp)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	return of_get_property(dev->of_node, name, lenp);
}

EXPORT_SYMBOL(AX_OSAL_DEV_of_get_property);

__attribute__ ((__noinline__))
int AX_OSAL_DEV_of_property_read_string_array(void * pdev, const char * propname, const char **out_strs,
						 AX_U64 sz)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	return of_property_read_string_array(dev->of_node, propname, out_strs, sz);
}

__attribute__ ((__noinline__))
int AX_OSAL_DEV_of_property_count_strings(void * pdev, const char * propname)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	return of_property_count_strings(dev->of_node, propname);
}

__attribute__ ((__noinline__))
int AX_OSAL_DEV_of_property_read_string_index(void * pdev, const char * propname, int index,
						 const char **output)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	return of_property_read_string_index(dev->of_node, propname, index, output);
}
#endif

__attribute__ ((__noinline__))
bool AX_OSAL_DEV_of_property_read_bool(void * pdev, const char * propname)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	return of_property_read_bool(dev->of_node, propname);
}

EXPORT_SYMBOL(AX_OSAL_DEV_of_property_read_bool);

#if 0
__attribute__ ((__noinline__))
int AX_OSAL_DEV_of_property_read_u8(void * pdev, const char * propname, char * out_value)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	return of_property_read_u8(dev->of_node, propname, out_value);
}

EXPORT_SYMBOL(AX_OSAL_DEV_of_property_read_u8);
__attribute__ ((__noinline__))
int AX_OSAL_DEV_of_property_read_u16(void * pdev, const char * propname, AX_U16 * out_value)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	return of_property_read_u16(dev->of_node, propname, out_value);
}

EXPORT_SYMBOL(AX_OSAL_DEV_of_property_read_u16);
#endif

__attribute__ ((__noinline__))
int AX_OSAL_DEV_of_property_read_u32(void * pdev, const char *propname, unsigned int * out_value)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	return of_property_read_u32(dev->of_node, propname, out_value);
}

EXPORT_SYMBOL(AX_OSAL_DEV_of_property_read_u32);

__attribute__ ((__noinline__))
int AX_OSAL_DEV_of_property_read_s32(void * pdev, const char * propname, int * out_value)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	struct device *dev = &pvdev->dev;
	return of_property_read_s32(dev->of_node, propname, out_value);
}

EXPORT_SYMBOL(AX_OSAL_DEV_of_property_read_s32);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/fs.h>
#include <asm/uaccess.h>

#include "osal_ax.h"

void *AX_OSAL_FS_filp_open(const char * filename, int flags, int mode)
{
	struct file *filp = filp_open(filename, flags, mode);
	return (IS_ERR(filp)) ? NULL : filp;
}

EXPORT_SYMBOL(AX_OSAL_FS_filp_open);

void AX_OSAL_FS_filp_close(void * filp)
{
	if (filp) {
		filp_close(filp, NULL);
	}
	return;
}

EXPORT_SYMBOL(AX_OSAL_FS_filp_close);

int AX_OSAL_FS_filp_write(char * buf, int len, void * k_file)
{
	int writelen = 0;
	struct file *filp;

	if (k_file == NULL) {
		return -ENOENT;
	}

	filp = (struct file *)k_file;

	writelen = kernel_write(filp, buf, len, &filp->f_pos);
	return writelen;
}

EXPORT_SYMBOL(AX_OSAL_FS_filp_write);

int AX_OSAL_FS_filp_read(char * buf, int len, void * k_file)
{
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_FS_filp_read);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/kmod.h>
#include <linux/i2c-dev.h>
#include <linux/version.h>
#include <linux/i2c.h>
#include "osal_ax.h"
#include "osal_dev_ax.h"
#include "axdev.h"
#include "axdev_log.h"
#include "osal_lib_ax.h"

#define I2C_MAX_NUM     (16)

static struct i2c_board_info ax_i2c_info = {
	I2C_BOARD_INFO("sensor_i2c", (0x6c)),
};

static struct i2c_client *sensor_client[I2C_MAX_NUM];

int AX_OSAL_DEV_i2c_write(unsigned char i2c_dev, unsigned char dev_addr,
			     unsigned int reg_addr, unsigned int reg_addr_num,
			     unsigned int data, unsigned int data_byte_num)
{
	int ret = 0;
	int idx = 0;
	int msg_count = 0;
	struct i2c_client client;
	unsigned char tmp_buf[8] = { 0 };
	struct i2c_msg msgs[1] = { 0 };

	if (I2C_MAX_NUM <= i2c_dev) {
		return -EIO;
	}

	if (NULL == sensor_client[i2c_dev]) {
		return -EIO;
	}

	AX_OSAL_LIB_memcpy(&client, sensor_client[i2c_dev], sizeof(struct i2c_client));
	client.addr = dev_addr;	/* notes */

	/* reg_addr config */
	if (reg_addr_num == 1) {
		tmp_buf[idx++] = reg_addr & 0xff;
	} else {
		tmp_buf[idx++] = (reg_addr >> 8) & 0xff;
		tmp_buf[idx++] = reg_addr & 0xff;
	}

	/* data config */
	if (data_byte_num == 1) {
		tmp_buf[idx++] = data & 0xff;
	} else {
		tmp_buf[idx++] = (data >> 8) & 0xff;
		tmp_buf[idx++] = data & 0xff;
	}

	msgs[0].addr = client.addr;
	msgs[0].flags = 0;
	msgs[0].len = idx;
	msgs[0].buf = tmp_buf;

	msg_count = 1;
	ret = i2c_transfer(client.adapter, msgs, msg_count);
	if (ret < 0) {
		printk("msg %s i2c write error: %d\n", __func__, ret);
		return -EIO;
	}

	return 0;
}

EXPORT_SYMBOL(AX_OSAL_DEV_i2c_write);

int AX_OSAL_DEV_i2c_read(unsigned char i2c_dev, unsigned char dev_addr,
			    unsigned int reg_addr, unsigned int reg_addr_num,
			    unsigned int *pRegData, unsigned int data_byte_num)
{
	int ret = 0;
	int idx = 0;
	struct i2c_client client;
	int msg_count = 0;
	unsigned char tmp_buf[4] = { 0 };
	struct i2c_msg msgs[2] = { 0 };

	if (I2C_MAX_NUM <= i2c_dev) {
		return -EIO;
	}
	if (NULL == sensor_client[i2c_dev]) {
		return -EIO;
	}
	if (NULL == pRegData) {
		return -EIO;
	}

	AX_OSAL_LIB_memcpy(&client, sensor_client[i2c_dev], sizeof(struct i2c_client));
	client.addr = dev_addr;	/* notes */

	/* reg_addr config */
	if (reg_addr_num == 1) {
		tmp_buf[idx++] = reg_addr & 0xff;
	} else {
		tmp_buf[idx++] = (reg_addr >> 8) & 0xff;
		tmp_buf[idx++] = reg_addr & 0xff;
	}

	msgs[0].addr = client.addr;
	msgs[0].flags = 0;
	msgs[0].len = reg_addr_num;
	msgs[0].buf = tmp_buf;

	msgs[1].addr = client.addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = data_byte_num;
	msgs[1].buf = tmp_buf;

	msg_count = sizeof(msgs) / sizeof(msgs[0]);
	ret = i2c_transfer(client.adapter, msgs, msg_count);
	if (ret != 2) {
		printk("msg %s i2c write error: %d\n", __func__, ret);
		return -EIO;
	}

	if (data_byte_num == 1) {

		*pRegData = tmp_buf[0];
	} else {
		*pRegData = (tmp_buf[0] << 8) | (tmp_buf[1]);
	}

	return ret;
}

EXPORT_SYMBOL(AX_OSAL_DEV_i2c_read);

int AX_OSAL_DEV_i2c_dev_init(void)
{
	int i = 0;
	struct i2c_adapter *i2c_adap = NULL;

	for (i = 0; i < I2C_MAX_NUM; i++) {
		i2c_adap = i2c_get_adapter(i);
		if (NULL != i2c_adap) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
			sensor_client[i] = i2c_new_client_device(i2c_adap, &ax_i2c_info);
#else
			sensor_client[i] = i2c_new_device(i2c_adap, &ax_i2c_info);
#endif
			i2c_put_adapter(i2c_adap);
		}
	}
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_DEV_i2c_dev_init);

void AX_OSAL_DEV_i2c_dev_exit(void)
{
	int i = 0;
	for (i = 0; i < I2C_MAX_NUM; i++) {
		if (NULL != sensor_client[i]) {
			i2c_unregister_device(sensor_client[i]);
		}
	}
	return;
}

EXPORT_SYMBOL(AX_OSAL_DEV_i2c_dev_exit);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/interrupt.h>

#include "osal_ax.h"
#include "osal_dev_ax.h"

int AX_OSAL_DEV_request_threaded_irq_ex(unsigned int irq, AX_IRQ_HANDLER_T handler, AX_IRQ_HANDLER_T thread_fn,
					   unsigned long flags, const char * name, void * dev)
{
	return request_threaded_irq(irq, (irq_handler_t) handler, (irq_handler_t) thread_fn, flags, name, dev);
}

EXPORT_SYMBOL(AX_OSAL_DEV_request_threaded_irq_ex);

int AX_OSAL_DEV_request_threaded_irq(unsigned int irq, AX_IRQ_HANDLER_T handler, AX_IRQ_HANDLER_T thread_fn,
					const char * name, void * dev)
{
	unsigned long flags = IRQF_SHARED;

	return request_threaded_irq(irq, (irq_handler_t) handler, (irq_handler_t) thread_fn, flags, name, dev);
}

EXPORT_SYMBOL(AX_OSAL_DEV_request_threaded_irq);

const void *AX_OSAL_DEV_free_irq(unsigned int irq, void * dev)
{
	const char *devname = free_irq(irq, dev);

	return devname;
}

EXPORT_SYMBOL(AX_OSAL_DEV_free_irq);

int AX_OSAL_DEV_in_interrupt(void)
{
	return in_interrupt();
}

EXPORT_SYMBOL(AX_OSAL_DEV_in_interrupt);

void AX_OSAL_DEV_enable_irq(unsigned int irq)
{
	enable_irq(irq);
}

EXPORT_SYMBOL(AX_OSAL_DEV_enable_irq);

void AX_OSAL_DEV_disable_irq(unsigned int irq)
{
	disable_irq(irq);
}

EXPORT_SYMBOL(AX_OSAL_DEV_disable_irq);

void AX_OSAL_DEV_disable_irq_nosync(unsigned int irq)
{
	disable_irq_nosync(irq);
}

EXPORT_SYMBOL(AX_OSAL_DEV_disable_irq_nosync);

int AX_OSAL_DEV_irq_get_irqchip_state(unsigned int irq, enum AX_OSAL_irqchip_irq_state which, int * state)
{
	return irq_get_irqchip_state(irq, which, (bool *) state);
}

EXPORT_SYMBOL(AX_OSAL_DEV_irq_get_irqchip_state);

int AX_OSAL_DEV_irq_set_irqchip_state(unsigned int irq, enum AX_OSAL_irqchip_irq_state which, int val)
{
	return irq_set_irqchip_state(irq, which, val);
}

EXPORT_SYMBOL(AX_OSAL_DEV_irq_set_irqchip_state);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/log2.h>
#include <linux/uaccess.h>
#include "osal_ax.h"
#include "osal_kfifo.h"
#include "osal_dev_ax.h"


/*
 * internal helper to calculate the unused elements in a fifo
 */
static inline unsigned int ax_kfifo_unused(struct __ax_kfifo *fifo)
{
	return (fifo->mask + 1) - (fifo->in - fifo->out);
}

int __ax_kfifo_alloc(struct __ax_kfifo *fifo, unsigned int size,
		size_t esize, gfp_t gfp_mask)
{
	/*
	 * round up to the next power of 2, since our 'let the indices
	 * wrap' technique works only in this case.
	 */
	size = roundup_pow_of_two(size);

	fifo->in = 0;
	fifo->out = 0;
	fifo->esize = esize;

	if (size < 2) {
		fifo->data = NULL;
		fifo->mask = 0;
		return -EINVAL;
	}

	fifo->data = kmalloc_array(esize, size, gfp_mask);

	if (!fifo->data) {
		fifo->mask = 0;
		return -ENOMEM;
	}
	fifo->mask = size - 1;

	return 0;
}

EXPORT_SYMBOL(__ax_kfifo_alloc);

void __ax_kfifo_free(struct __ax_kfifo *fifo)
{
	kfree(fifo->data);
	fifo->in = 0;
	fifo->out = 0;
	fifo->esize = 0;
	fifo->data = NULL;
	fifo->mask = 0;
}

EXPORT_SYMBOL(__ax_kfifo_free);

int __ax_kfifo_init(struct __ax_kfifo *fifo, void *buffer,
		unsigned int size, size_t esize)
{
	size /= esize;

	if (!is_power_of_2(size))
		size = rounddown_pow_of_two(size);

	fifo->in = 0;
	fifo->out = 0;
	fifo->esize = esize;
	fifo->data = buffer;

	if (size < 2) {
		fifo->mask = 0;
		return -EINVAL;
	}
	fifo->mask = size - 1;

	return 0;
}

EXPORT_SYMBOL(__ax_kfifo_init);

static void ax_kfifo_copy_in(struct __ax_kfifo *fifo, const void *src,
		unsigned int len, unsigned int off)
{
	unsigned int size = fifo->mask + 1;
	unsigned int esize = fifo->esize;
	unsigned int l;

	off &= fifo->mask;
	if (esize != 1) {
		off *= esize;
		size *= esize;
		len *= esize;
	}
	l = min(len, size - off);

	memcpy(fifo->data + off, src, l);
	memcpy(fifo->data, src + l, len - l);
	/*
	 * make sure that the data in the fifo is up to date before
	 * incrementing the fifo->in index counter
	 */
	smp_wmb();
}

unsigned int __ax_kfifo_in(struct __ax_kfifo *fifo,
		const void *buf, unsigned int len)
{
	unsigned int l;

	l = ax_kfifo_unused(fifo);
	if (len > l)
		len = l;

	ax_kfifo_copy_in(fifo, buf, len, fifo->in);
	fifo->in += len;
	return len;
}

EXPORT_SYMBOL(__ax_kfifo_in);

static void ax_kfifo_copy_out(struct __ax_kfifo *fifo, void *dst,
		unsigned int len, unsigned int off)
{
	unsigned int size = fifo->mask + 1;
	unsigned int esize = fifo->esize;
	unsigned int l;

	off &= fifo->mask;
	if (esize != 1) {
		off *= esize;
		size *= esize;
		len *= esize;
	}
	l = min(len, size - off);

	memcpy(dst, fifo->data + off, l);
	memcpy(dst + l, fifo->data, len - l);
	/*
	 * make sure that the data is copied before
	 * incrementing the fifo->out index counter
	 */
	smp_wmb();
}

unsigned int __ax_kfifo_out_peek(struct __ax_kfifo *fifo,
		void *buf, unsigned int len)
{
	unsigned int l;

	l = fifo->in - fifo->out;
	if (len > l)
		len = l;

	ax_kfifo_copy_out(fifo, buf, len, fifo->out);
	return len;
}

EXPORT_SYMBOL(__ax_kfifo_out_peek);

unsigned int __ax_kfifo_out(struct __ax_kfifo *fifo,
		void *buf, unsigned int len)
{
	len = __ax_kfifo_out_peek(fifo, buf, len);
	fifo->out += len;
	return len;
}

EXPORT_SYMBOL(__ax_kfifo_out);

static unsigned long ax_kfifo_copy_from_user(struct __ax_kfifo *fifo,
	const void __user *from, unsigned int len, unsigned int off,
	unsigned int *copied)
{
	unsigned int size = fifo->mask + 1;
	unsigned int esize = fifo->esize;
	unsigned int l;
	unsigned long ret;

	off &= fifo->mask;
	if (esize != 1) {
		off *= esize;
		size *= esize;
		len *= esize;
	}
	l = min(len, size - off);

	ret = copy_from_user(fifo->data + off, from, l);
	if (unlikely(ret))
		ret = DIV_ROUND_UP(ret + len - l, esize);
	else {
		ret = copy_from_user(fifo->data, from + l, len - l);
		if (unlikely(ret))
			ret = DIV_ROUND_UP(ret, esize);
	}
	/*
	 * make sure that the data in the fifo is up to date before
	 * incrementing the fifo->in index counter
	 */
	smp_wmb();
	*copied = len - ret * esize;
	/* return the number of elements which are not copied */
	return ret;
}


int __ax_kfifo_from_user(struct __ax_kfifo *fifo, const void __user *from,
		unsigned long len, unsigned int *copied)
{
	unsigned int l;
	unsigned long ret;
	unsigned int esize = fifo->esize;
	int err;

	if (esize != 1)
		len /= esize;

	l = ax_kfifo_unused(fifo);
	if (len > l)
		len = l;

	ret = ax_kfifo_copy_from_user(fifo, from, len, fifo->in, copied);
	if (unlikely(ret)) {
		len -= ret;
		err = -EFAULT;
	} else
		err = 0;
	fifo->in += len;
	return err;
}

EXPORT_SYMBOL(__ax_kfifo_from_user);


unsigned int __ax_kfifo_max_r(unsigned int len, size_t recsize)
{
	unsigned int max = (1 << (recsize << 3)) - 1;

	if (len > max)
		return max;
	return len;
}
EXPORT_SYMBOL(__ax_kfifo_max_r);

#define	__ax_kfifo_PEEK(data, out, mask) \
	((data)[(out) & (mask)])
/*
 * __ax_kfifo_peek_n internal helper function for determinate the length of
 * the next record in the fifo
 */
static unsigned int __ax_kfifo_peek_n(struct __ax_kfifo *fifo, size_t recsize)
{
	unsigned int l;
	unsigned int mask = fifo->mask;
	unsigned char *data = fifo->data;

	l = __ax_kfifo_PEEK(data, fifo->out, mask);

	if (--recsize)
		l |= __ax_kfifo_PEEK(data, fifo->out + 1, mask) << 8;

	return l;
}

#define	__ax_kfifo_POKE(data, in, mask, val) \
	( \
	(data)[(in) & (mask)] = (unsigned char)(val) \
	)

/*
 * __ax_kfifo_poke_n internal helper function for storing the length of
 * the record into the fifo
 */
static void __ax_kfifo_poke_n(struct __ax_kfifo *fifo, unsigned int n, size_t recsize)
{
	unsigned int mask = fifo->mask;
	unsigned char *data = fifo->data;

	__ax_kfifo_POKE(data, fifo->in, mask, n);

	if (recsize > 1)
		__ax_kfifo_POKE(data, fifo->in + 1, mask, n >> 8);
}

unsigned int __ax_kfifo_len_r(struct __ax_kfifo *fifo, size_t recsize)
{
	return __ax_kfifo_peek_n(fifo, recsize);
}

EXPORT_SYMBOL(__ax_kfifo_len_r);

unsigned int __ax_kfifo_in_r(struct __ax_kfifo *fifo, const void *buf,
		unsigned int len, size_t recsize)
{
	if (len + recsize > ax_kfifo_unused(fifo))
		return 0;

	__ax_kfifo_poke_n(fifo, len, recsize);

	ax_kfifo_copy_in(fifo, buf, len, fifo->in + recsize);
	fifo->in += len + recsize;
	return len;
}

EXPORT_SYMBOL(__ax_kfifo_in_r);

static unsigned int ax_kfifo_out_copy_r(struct __ax_kfifo *fifo,
	void *buf, unsigned int len, size_t recsize, unsigned int *n)
{
	*n = __ax_kfifo_peek_n(fifo, recsize);

	if (len > *n)
		len = *n;

	ax_kfifo_copy_out(fifo, buf, len, fifo->out + recsize);
	return len;
}


unsigned int __ax_kfifo_out_peek_r(struct __ax_kfifo *fifo, void *buf,
		unsigned int len, size_t recsize)
{
	unsigned int n;

	if (fifo->in == fifo->out)
		return 0;

	return ax_kfifo_out_copy_r(fifo, buf, len, recsize, &n);
}

EXPORT_SYMBOL(__ax_kfifo_out_peek_r);

unsigned int __ax_kfifo_out_r(struct __ax_kfifo *fifo, void *buf,
		unsigned int len, size_t recsize)
{
	unsigned int n;

	if (fifo->in == fifo->out)
		return 0;

	len = ax_kfifo_out_copy_r(fifo, buf, len, recsize, &n);
	fifo->out += n + recsize;
	return len;
}

EXPORT_SYMBOL(__ax_kfifo_out_r);

void __ax_kfifo_skip_r(struct __ax_kfifo *fifo, size_t recsize)
{
	unsigned int n;

	n = __ax_kfifo_peek_n(fifo, recsize);
	fifo->out += n + recsize;
}

EXPORT_SYMBOL(__ax_kfifo_skip_r);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include "osal_ax.h"


void *AX_OSAL_MEM_kmalloc(unsigned int size, unsigned int osal_gfp_flag)
{
    if (osal_gfp_flag == AX_OSAL_GFP_KERNEL) {
        return kmalloc(size, GFP_KERNEL);
    } else if (osal_gfp_flag == AX_OSAL_GFP_ATOMIC) {
        return kmalloc(size, GFP_ATOMIC);
    }

    return kmalloc(size, GFP_KERNEL);
}
EXPORT_SYMBOL(AX_OSAL_MEM_kmalloc);

void *AX_OSAL_MEM_kzalloc(unsigned int size, unsigned int osal_gfp_flag)
{
    if (osal_gfp_flag == AX_OSAL_GFP_KERNEL) {
        return kzalloc(size, GFP_KERNEL);
    } else if (osal_gfp_flag == AX_OSAL_GFP_ATOMIC) {
        return kzalloc(size, GFP_ATOMIC);
    }

    return kzalloc(size, GFP_KERNEL);
}
EXPORT_SYMBOL(AX_OSAL_MEM_kzalloc);

void AX_OSAL_MEM_kfree(const void *addr)
{
    kfree(addr);
}
EXPORT_SYMBOL(AX_OSAL_MEM_kfree);

void *AX_OSAL_MEM_vmalloc(unsigned int size)
{
    return vmalloc(size);
}
EXPORT_SYMBOL(AX_OSAL_MEM_vmalloc);

void AX_OSAL_MEM_vfree(const void *addr)
{
    vfree(addr);
}
EXPORT_SYMBOL(AX_OSAL_MEM_vfree);

int AX_OSAL_MEM_VirtAddrIsValid(unsigned long vm_start, unsigned long vm_end)
{
    struct vm_area_struct *pvma1;
    struct vm_area_struct *pvma2;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
    mmap_read_lock(current->mm);
#else
   down_read(&current->mm->mmap_sem);
#endif
    pvma1 = find_vma(current->mm, vm_start);
    if (NULL == pvma1) {
        printk(" user vaddr 1 is null. user add = 0x%lx\n", vm_start);
        goto badAddr;
    }

    pvma2 = find_vma(current->mm, vm_end - 1);
    if (NULL == pvma2) {
        printk(" user vaddr 2 is null. user add = 0x%lx\n", vm_start);
        goto badAddr;
    }

    if (pvma1 != pvma2) {
        printk(" user vaddr:[0x%lx,0x%lx) and user vaddr:[0x%lx,0x%lx) are not equal\n",
                       pvma1->vm_start, pvma1->vm_end, pvma2->vm_start, pvma2->vm_end);
        goto badAddr;
    }

    if (!(pvma1->vm_flags & VM_WRITE)) {
        printk("ERROR vma flag:0x%lx\n", pvma1->vm_flags);
        goto badAddr;
    }

    if (pvma1->vm_start > vm_start) {
        printk("cannot find corresponding vma, vm[%lx, %lx], user range[%lx,%lx]\n", pvma1->vm_start, pvma1->vm_end,
                       vm_start, vm_end);
        goto badAddr;
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
    mmap_read_unlock(current->mm);
#else
    up_read(&current->mm->mmap_sem);
#endif
    return 0;
badAddr:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
    mmap_read_unlock(current->mm);
#else
    up_read(&current->mm->mmap_sem);
#endif
    return -1;
}
EXPORT_SYMBOL(AX_OSAL_MEM_VirtAddrIsValid);

unsigned long AX_OSAL_MEM_AddrMmap(void *file, unsigned long addr,
	unsigned long len, unsigned long prot, unsigned long flag, unsigned long offset)
{
    return vm_mmap((struct file *)file, addr, len, prot, flag, offset);
}
EXPORT_SYMBOL(AX_OSAL_MEM_AddrMmap);

int AX_OSAL_MEM_AddrMunmap(unsigned long start, unsigned int size)
{
    return vm_munmap(start, size);
}
EXPORT_SYMBOL(AX_OSAL_MEM_AddrMunmap);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include "osal_mailbox.h"
#include <linux/export.h>

#ifdef CONFIG_AXERA_MAILBOX
void AX_OSAL_mailbox_set_callback(mbox_callback_t *callback, void *pri_data)
{
	ax_mailbox_set_callback(callback, pri_data);
}
EXPORT_SYMBOL(AX_OSAL_mailbox_set_callback);

int AX_OSAL_mailbox_send_message(unsigned int send_masterid, unsigned int receive_masterid, mbox_msg_t *data)
{
	int ret;
	ret = ax_mailbox_send_message(send_masterid, receive_masterid, data);
	return ret;
}
EXPORT_SYMBOL(AX_OSAL_mailbox_send_message);
#else
void AX_OSAL_mailbox_set_callback(mbox_callback_t *callback, void *pri_data)
{
}
EXPORT_SYMBOL(AX_OSAL_mailbox_set_callback);

int AX_OSAL_mailbox_send_message(unsigned int send_masterid, unsigned int receive_masterid, mbox_msg_t *data)
{
	return 0;
}
EXPORT_SYMBOL(AX_OSAL_mailbox_send_message);
#endif/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/random.h>

#include "osal_lib_ax.h"

u64 AX_OSAL_LIB_div_u64(u64 dividend, unsigned int divisor)
{
	return div_u64(dividend, divisor);
}

EXPORT_SYMBOL(AX_OSAL_LIB_div_u64);

s64 AX_OSAL_LIB_div_s64(s64 dividend, int divisor)
{
	return div_s64(dividend, divisor);
}

EXPORT_SYMBOL(AX_OSAL_LIB_div_s64);

u64 AX_OSAL_LIB_div64_u64(u64 dividend, u64 divisor)
{
	return div64_u64(dividend, divisor);
}

EXPORT_SYMBOL(AX_OSAL_LIB_div64_u64);

s64 AX_OSAL_LIB_LIB_div64_s64(s64 dividend, s64 divisor)
{
	return div64_s64(dividend, divisor);
}

EXPORT_SYMBOL(AX_OSAL_LIB_LIB_div64_s64);

u64 AX_OSAL_LIB_div_u64_rem(u64 dividend, unsigned int divisor)
{
	unsigned int remainder;

	div_u64_rem(dividend, divisor, &remainder);

	return remainder;
}

EXPORT_SYMBOL(AX_OSAL_LIB_div_u64_rem);

s64 AX_OSAL_LIB_div_s64_rem(s64 dividend, int divisor)
{
	int remainder;

	div_s64_rem(dividend, divisor, &remainder);

	return remainder;
}

EXPORT_SYMBOL(AX_OSAL_LIB_div_s64_rem);

u64 AX_OSAL_LIB_div64_u64_rem(u64 dividend, u64 divisor)
{
	unsigned long long remainder;

	div64_u64_rem(dividend, divisor, &remainder);

	return remainder;
}

EXPORT_SYMBOL(AX_OSAL_LIB_div64_u64_rem);

unsigned int AX_OSAL_LIB_random()
{
	return get_random_int();
}

EXPORT_SYMBOL(AX_OSAL_LIB_random);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/cache.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/kexec.h>
#include <linux/libfdt.h>
#include <linux/mman.h>
#include <linux/nodemask.h>
#include <linux/memblock.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>

#include <asm/barrier.h>
#include <asm/cputype.h>
#include <asm/fixmap.h>
#include <linux/kasan.h>
#ifdef CONFIG_ARM64
#include <asm/kernel-pgtable.h>
#endif
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/tlb.h>
#include <asm/mmu_context.h>
#include <asm/ptdump.h>
#include <asm/tlbflush.h>
#include <linux/version.h>

#include "osal_ax.h"
#include "osal_dev_ax.h"
#include "axdev.h"
#include "axdev_log.h"

unsigned long AX_OSAL_DEV_usr_virt_to_phys(unsigned long virt)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	unsigned int cacheable = 0;
	unsigned int val = 0;
	unsigned long page_addr = 0;
	unsigned long page_offset = 0;
	unsigned long phys_addr = 0;

	if (virt > TASK_SIZE) {
		printk("Illegal user address\n");
		return 0;
	}

	if (virt & 0x3) {
		pr_info("invalid virt addr 0x%08lx[not 4 bytes align]\n", virt);
		return 0;
	}

	if (virt >= PAGE_OFFSET) {
		pr_info("invalid user space virt addr 0x%08lx\n", virt);
		return 0;
	}

	pgd = pgd_offset(current->mm, virt);

	if (pgd_none(*pgd)) {
		pr_info("no mapped in pg,dvirt addr 0x%08lx\n", virt);
		return 0;
	}

	p4d = p4d_offset(pgd, virt);
	if (p4d_none(*p4d)) {
		pr_info("no mapped in p4d,dvirt addr 0x%08lx\n", virt);
		return 0;
	}

	pud = pud_offset(p4d, virt);

	if (pud_none(*pud)) {
		pr_info(" not mapped in pud! addr 0x%08lx\n", virt);
		return 0;
	}

	pmd = pmd_offset(pud, virt);

	if (pmd_none(*pmd)) {
		pr_info(" not mapped in pmd! addr 0x%08lx\n", virt);
		return 0;
	}

	pte = pte_offset_map(pmd, virt);

	if (pte_none(*pte)) {
		pr_info(" not mapped in pte!addr 0x%08lx\n", virt);
		pte_unmap(pte);
		return 0;
	}

	page_addr = (pte_val(*pte) & PHYS_MASK) & PAGE_MASK;
	page_offset = virt & ~PAGE_MASK;
	phys_addr = page_addr | page_offset;
#ifdef CONFIG_ARM64
	/* bit[4:2] */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	val = (pte_val(*pte) & 0x1c) >> 2;
	if (val == 0 || val == 1) {
		cacheable = 1;
	}
#else
	if (pte_val(*pte) & (1 << 4)) {
		cacheable = 1;
	}
#endif

#else
	if (pte_val(*pte) & (1 << 3)) {
		cacheable = 1;
	}
#endif
	/*
	 * phys_addr: the lowest bit indicates its cache attribute
	 * 1: cacheable
	 * 0: uncacheable
	 */
	phys_addr |= cacheable;

	pte_unmap(pte);

	return phys_addr;
}

EXPORT_SYMBOL(AX_OSAL_DEV_usr_virt_to_phys);

unsigned long AX_OSAL_DEV_kernel_virt_to_phys(void *virt)
{
#ifdef CONFIG_ARM64
	unsigned long vaddr = (unsigned long)virt;
        unsigned long offset = vaddr & ~PAGE_MASK;
	if (vaddr <= TASK_SIZE) {
		printk("Illegal kernel address\n");
		return 0;
	} else if (__is_lm_address(vaddr)) {
		return virt_to_phys(virt);
	} else if (is_vmalloc_addr(virt)) {
		return page_to_phys(vmalloc_to_page(virt)) | offset;
	} else {
		printk("unknow kernel address\n");
		return 0;
	}
#endif
}
EXPORT_SYMBOL(AX_OSAL_DEV_kernel_virt_to_phys);

/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include "osal_logdebug_ax.h"
#include <linux/rtmutex.h>
#include "osal_ax.h"

#ifndef CONFIG_DEBUG_MUTEXES
int AX_OSAL_SYNC_mutex_init(AX_MUTEX_T * mutex)
{
	struct mutex *p = NULL;
	if (mutex == NULL) {
		printk("%s error mutex == NULL\n", __func__);
		return -1;
	}
	p = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (p == NULL) {
		printk("%s error mutex == NULL\n", __func__);
		return -1;
	}
	mutex_init(p);
	mutex->mutex = p;
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_mutex_init);
#else
void *AX_OSAL_DBG_mutex_init(AX_MUTEX_T * mutex)
{
	struct mutex *p = NULL;
	if (mutex == NULL) {
		printk("%s error mutex == NULL\n", __func__);
		return NULL;
	}
	p = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (p == NULL) {
		printk("%s error mutex == NULL\n", __func__);
		return NULL;
	}

	mutex->mutex = p;
	return p;
}

EXPORT_SYMBOL(AX_OSAL_DBG_mutex_init);
#endif

int AX_OSAL_SYNC_mutex_lock(AX_MUTEX_T * mutex)
{
	struct mutex *p = NULL;
	if (mutex == NULL) {
		printk("%s error mutex == NULL\n", __func__);
		return -1;
	}
	p = (struct mutex *)(mutex->mutex);
	mutex_lock(p);
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_mutex_lock);

int AX_OSAL_SYNC_mutex_lock_interruptible(AX_MUTEX_T * mutex)
{
	struct mutex *p = NULL;
	if (mutex == NULL) {
		printk("%s error mutex == NULL\n", __func__);
		return -1;
	}
	p = (struct mutex *)(mutex->mutex);
	return mutex_lock_interruptible(p);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_mutex_lock_interruptible);

int AX_OSAL_SYNC_mutex_trylock(AX_MUTEX_T * mutex)
{
	struct mutex *p = NULL;
	if (mutex == NULL) {
		printk("%s error mutex == NULL\n", __func__);
		return -1;
	}
	p = (struct mutex *)(mutex->mutex);

	return mutex_trylock(p);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_mutex_trylock);

void AX_OSAL_SYNC_mutex_unlock(AX_MUTEX_T * mutex)
{
	struct mutex *p = NULL;
	p = (struct mutex *)(mutex->mutex);

	mutex_unlock(p);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_mutex_unlock);

void AX_OSAL_SYNC_mutex_destroy(AX_MUTEX_T * mutex)
{
	struct mutex *p = NULL;
	p = (struct mutex *)(mutex->mutex);
	kfree(p);
	mutex->mutex = NULL;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_mutex_destroy);

#ifdef CONFIG_DEBUG_RT_MUTEXES
int AX_OSAL_DBG_rt_mutex_init(AX_RT_MUTEX_T * rt_mutex,struct lock_class_key *__key)
{
	struct rt_mutex *p = NULL;
	if (rt_mutex == NULL) {
        	printk("%s error rt mutex == NULL\n", __func__);
		return -1;
	}
	p = kmalloc(sizeof(struct rt_mutex), GFP_KERNEL);
	if (p == NULL) {
        	printk("%s error mutex == NULL\n", __func__);
		return -1;
	}

	rt_mutex->rt_mutex = p;
	__rt_mutex_init(p, __func__,__key);
	return 0;
}
EXPORT_SYMBOL(AX_OSAL_DBG_rt_mutex_init);
#else
int AX_OSAL_SYNC_rt_mutex_init(AX_RT_MUTEX_T * rt_mutex)
{
	struct rt_mutex *p = NULL;
	if (rt_mutex == NULL) {
        	printk("%s error rt mutex == NULL\n", __func__);
		return -1;
	}
	p = kmalloc(sizeof(struct rt_mutex), GFP_KERNEL);
	if (p == NULL) {
        	printk("%s error mutex == NULL\n", __func__);
		return -1;
	}

	rt_mutex->rt_mutex = p;
	__rt_mutex_init(p, NULL, NULL);
	return 0;
}
EXPORT_SYMBOL(AX_OSAL_SYNC_rt_mutex_init);
#endif

void AX_OSAL_SYNC_rt_mutex_lock(AX_RT_MUTEX_T * rt_mutex)
{
	struct rt_mutex *p = NULL;
	p = (struct rt_mutex *)(rt_mutex->rt_mutex);
	rt_mutex_lock(p);
}
EXPORT_SYMBOL(AX_OSAL_SYNC_rt_mutex_lock);

void AX_OSAL_SYNC_rt_mutex_unlock(AX_RT_MUTEX_T * rt_mutex)
{
	struct rt_mutex *p = NULL;
	p = (struct rt_mutex *)(rt_mutex->rt_mutex);
	rt_mutex_unlock(p);
}
EXPORT_SYMBOL(AX_OSAL_SYNC_rt_mutex_unlock);

void AX_OSAL_SYNC_rt_mutex_destroy(AX_RT_MUTEX_T * rt_mutex)
{
	struct rt_mutex *p = NULL;
	p = (struct rt_mutex *)(rt_mutex->rt_mutex);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
	rt_mutex_destroy(p);
#endif
	kfree(p);
	rt_mutex->rt_mutex = NULL;
}
EXPORT_SYMBOL(AX_OSAL_SYNC_rt_mutex_destroy);

/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include "osal_ax.h"

unsigned int AX_OSAL___percpu *AX_OSAL_alloc_percpu_u32(void)
{
	return alloc_percpu(unsigned int);
}
EXPORT_SYMBOL(AX_OSAL_alloc_percpu_u32);

unsigned int *AX_OSAL_this_cpu_ptr_u32(unsigned int AX_OSAL___percpu *ptr)
{
	return this_cpu_ptr(ptr);
}
EXPORT_SYMBOL(AX_OSAL_this_cpu_ptr_u32);

unsigned int *AX_OSAL_get_cpu_ptr_u32(unsigned int AX_OSAL___percpu *ptr)
{
	return get_cpu_ptr(ptr);
}
EXPORT_SYMBOL(AX_OSAL_get_cpu_ptr_u32);

void AX_OSAL_put_cpu_ptr_u32(unsigned int AX_OSAL___percpu *ptr)
{
	put_cpu_ptr(ptr);
}
EXPORT_SYMBOL(AX_OSAL_put_cpu_ptr_u32);

unsigned int *AX_OSAL_per_cpu_ptr_u32(unsigned int AX_OSAL___percpu *ptr, int cpu)
{
	return per_cpu_ptr(ptr, cpu);
}
EXPORT_SYMBOL(AX_OSAL_per_cpu_ptr_u32);

void AX_OSAL_free_percpu_u32(unsigned int AX_OSAL___percpu *ptr)
{
	free_percpu(ptr);
}
EXPORT_SYMBOL(AX_OSAL_free_percpu_u32);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/kmod.h>
#include <linux/fs.h>

#include <linux/wait.h>

#include "linux/platform_device.h"
#include "linux/device.h"
#include "osal_ax.h"
#include "osal_dev_ax.h"
#include "axdev.h"
#include "axdev_log.h"
#include "osal_lib_ax.h"

static int axera_osal_common_probe(struct platform_device *pdev)
{
	struct platform_driver *axera_osal_platform_drv = NULL;
	struct AX_PLATFORM_DRIVER *axera_pdrv = NULL;
	struct device_driver *drv = pdev->dev.driver;
	if (drv == NULL)
		return -ENODEV;
	axera_osal_platform_drv = to_platform_driver(drv);
	if (axera_osal_platform_drv == NULL)
		return -EINVAL;
	axera_pdrv = (struct AX_PLATFORM_DRIVER *)axera_osal_platform_drv->axera_driver_ptr;
	if (axera_pdrv == NULL)
		return -EINVAL;
	if (axera_pdrv->probe == NULL)
		return -EINVAL;
	axera_pdrv->probe(pdev);
	return 0;
}

static int axera_osal_common_remove(struct platform_device *pdev)
{
	struct platform_driver *axera_osal_platform_drv = NULL;
	struct AX_PLATFORM_DRIVER *axera_pdrv = NULL;
	struct device_driver *drv = pdev->dev.driver;
	if (drv == NULL)
		return -EINVAL;
	axera_osal_platform_drv = to_platform_driver(drv);
	if (axera_osal_platform_drv == NULL)
		return -EINVAL;
	axera_pdrv = (struct AX_PLATFORM_DRIVER *)axera_osal_platform_drv->axera_driver_ptr;
	if (axera_pdrv == NULL)
		return -EINVAL;
	if (axera_pdrv->remove == NULL)
		return -EINVAL;
	axera_pdrv->remove(pdev);
	return 0;
}

static int axera_osal_common_pm_suspend(struct device *dev)
{
	struct platform_driver *axera_osal_platform_drv = NULL;
	struct AX_PLATFORM_DRIVER *axera_pdrv = NULL;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_driver *drv = pdev->dev.driver;

	if (IS_ERR_OR_NULL(drv))
		return -ENODEV;
	axera_osal_platform_drv = to_platform_driver(drv);
	if (IS_ERR_OR_NULL(axera_osal_platform_drv))
		return -EINVAL;
	axera_pdrv = (struct AX_PLATFORM_DRIVER *)axera_osal_platform_drv->axera_driver_ptr;
	if (IS_ERR_OR_NULL(axera_pdrv))
		return -EINVAL;
	if (IS_ERR_OR_NULL(axera_pdrv->suspend))
		return -EINVAL;
	axera_pdrv->suspend(pdev);
	return 0;
}

static int axera_osal_common_pm_resume(struct device *dev)
{
	struct platform_driver *axera_osal_platform_drv = NULL;
	struct AX_PLATFORM_DRIVER *axera_pdrv = NULL;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_driver *drv = pdev->dev.driver;

	if (IS_ERR_OR_NULL(drv))
		return -ENODEV;
	axera_osal_platform_drv = to_platform_driver(drv);
	if (IS_ERR_OR_NULL(axera_osal_platform_drv))
		return -EINVAL;
	axera_pdrv = (struct AX_PLATFORM_DRIVER *)axera_osal_platform_drv->axera_driver_ptr;
	if (IS_ERR_OR_NULL(axera_pdrv))
		return -EINVAL;
	if (IS_ERR_OR_NULL(axera_pdrv->resume))
		return -EINVAL;
	axera_pdrv->resume(pdev);
	return 0;
}

static int axera_osal_common_pm_suspend_noirq(struct device *dev)
{
	struct platform_driver *axera_osal_platform_drv = NULL;
	struct AX_PLATFORM_DRIVER *axera_pdrv = NULL;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_driver *drv = pdev->dev.driver;

	if (IS_ERR_OR_NULL(drv))
		return -ENODEV;
	axera_osal_platform_drv = to_platform_driver(drv);
	if (IS_ERR_OR_NULL(axera_osal_platform_drv))
		return -EINVAL;
	axera_pdrv = (struct AX_PLATFORM_DRIVER *)axera_osal_platform_drv->axera_driver_ptr;
	if (IS_ERR_OR_NULL(axera_pdrv))
		return -EINVAL;
	if (IS_ERR_OR_NULL(axera_pdrv->suspend_noirq))
		return -EINVAL;
	axera_pdrv->suspend_noirq(pdev);
	return 0;
}

static int axera_osal_common_pm_resume_early(struct device *dev)
{
	struct platform_driver *axera_osal_platform_drv = NULL;
	struct AX_PLATFORM_DRIVER *axera_pdrv = NULL;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_driver *drv = pdev->dev.driver;

	if (IS_ERR_OR_NULL(drv))
		return -ENODEV;
	axera_osal_platform_drv = to_platform_driver(drv);
	if (IS_ERR_OR_NULL(axera_osal_platform_drv))
		return -EINVAL;
	axera_pdrv = (struct AX_PLATFORM_DRIVER *)axera_osal_platform_drv->axera_driver_ptr;
	if (IS_ERR_OR_NULL(axera_pdrv))
		return -EINVAL;
	if (IS_ERR_OR_NULL(axera_pdrv->resume_early))
		return -EINVAL;
	axera_pdrv->resume_early(pdev);
	return 0;
}

static int axera_osal_common_pm_suspend_late(struct device *dev)
{
	struct platform_driver *axera_osal_platform_drv = NULL;
	struct AX_PLATFORM_DRIVER *axera_pdrv = NULL;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_driver *drv = pdev->dev.driver;

	if (IS_ERR_OR_NULL(drv))
		return -ENODEV;
	axera_osal_platform_drv = to_platform_driver(drv);
	if (IS_ERR_OR_NULL(axera_osal_platform_drv))
		return -EINVAL;
	axera_pdrv = (struct AX_PLATFORM_DRIVER *)axera_osal_platform_drv->axera_driver_ptr;
	if (IS_ERR_OR_NULL(axera_pdrv))
		return -EINVAL;
	if (IS_ERR_OR_NULL(axera_pdrv->suspend_late))
		return -EINVAL;
	axera_pdrv->suspend_late(pdev);
	return 0;
}

static int axera_osal_common_pm_resume_noirq(struct device *dev)
{
	struct platform_driver *axera_osal_platform_drv = NULL;
	struct AX_PLATFORM_DRIVER *axera_pdrv = NULL;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_driver *drv = pdev->dev.driver;

	if (IS_ERR_OR_NULL(drv))
		return -ENODEV;
	axera_osal_platform_drv = to_platform_driver(drv);
	if (IS_ERR_OR_NULL(axera_osal_platform_drv))
		return -EINVAL;
	axera_pdrv = (struct AX_PLATFORM_DRIVER *)axera_osal_platform_drv->axera_driver_ptr;
	if (IS_ERR_OR_NULL(axera_pdrv))
		return -EINVAL;
	if (IS_ERR_OR_NULL(axera_pdrv->resume_noirq))
		return -EINVAL;
	axera_pdrv->resume_noirq(pdev);
	return 0;
}



int AX_OSAL_DEV_platform_driver_register(void * drv)
{
	struct dev_pm_ops *axera_pm_ops;
	struct platform_driver *axera_osal_platform_drv = NULL;
	struct AX_PLATFORM_DRIVER *axera_pdrv = (struct AX_PLATFORM_DRIVER *)drv;
	if (axera_pdrv == NULL)
		return -EINVAL;
	axera_osal_platform_drv = kzalloc(sizeof(struct platform_driver), GFP_KERNEL);
	if (axera_osal_platform_drv == NULL)
		return -ENOMEM;
	axera_pdrv->axera_ptr = axera_osal_platform_drv;
	axera_osal_platform_drv->probe = axera_osal_common_probe;
	axera_osal_platform_drv->remove = axera_osal_common_remove;
	axera_osal_platform_drv->driver.name = axera_pdrv->driver.name;
	axera_osal_platform_drv->driver.of_match_table = axera_pdrv->driver.of_match_table;
#ifdef CONFIG_PM
	if(axera_pdrv->suspend != NULL || axera_pdrv->resume != NULL ||
		axera_pdrv->suspend_noirq != NULL || axera_pdrv->resume_noirq != NULL ||
		axera_pdrv->suspend_late != NULL || axera_pdrv->resume_early != NULL ) {
		axera_pm_ops = kzalloc(sizeof(struct dev_pm_ops), GFP_KERNEL);
		if(axera_pm_ops == NULL)
			return -ENOMEM;
		if(axera_pdrv->suspend != NULL)
			axera_pm_ops->suspend = axera_osal_common_pm_suspend;
		if(axera_pdrv->resume != NULL)
			axera_pm_ops->resume = axera_osal_common_pm_resume;

		if(axera_pdrv->suspend_noirq != NULL)
			axera_pm_ops->suspend_noirq = axera_osal_common_pm_suspend_noirq;
		if(axera_pdrv->resume_noirq != NULL)
			axera_pm_ops->resume_noirq = axera_osal_common_pm_resume_noirq;

		if(axera_pdrv->suspend_late != NULL)
			axera_pm_ops->suspend_late = axera_osal_common_pm_suspend_late;
		if(axera_pdrv->resume_early != NULL)
			axera_pm_ops->resume_early = axera_osal_common_pm_resume_early;

		axera_osal_platform_drv->driver.pm = axera_pm_ops;
	} else {
		axera_osal_platform_drv->driver.pm = NULL;
	}
#endif
	axera_osal_platform_drv->axera_driver_ptr = drv;
	return __platform_driver_register(axera_osal_platform_drv, THIS_MODULE);
}

EXPORT_SYMBOL(AX_OSAL_DEV_platform_driver_register);

void AX_OSAL_DEV_platform_driver_unregister(void * drv)
{
	struct AX_PLATFORM_DRIVER *axera_pdrv = (struct AX_PLATFORM_DRIVER *)drv;
	struct platform_driver *axera_osal_platform_drv = axera_pdrv->axera_ptr;
	if (axera_osal_platform_drv == NULL) {
		printk("platform_driver axera_osal_platform_drv is invalid\n");
		return;
	}

	platform_driver_unregister(axera_osal_platform_drv);

	if(axera_osal_platform_drv->driver.pm != NULL)
		kfree(axera_osal_platform_drv->driver.pm);
	kfree(axera_osal_platform_drv);
}

EXPORT_SYMBOL(AX_OSAL_DEV_platform_driver_unregister);

int AX_OSAL_DEV_platform_get_resource_byname(void * dev, u32 type, const char * name,
						struct AXERA_RESOURCE * res)
{
	struct resource *res_tmp = NULL;
	res_tmp = platform_get_resource_byname((struct platform_device *)dev, type, name);
	if (!res_tmp) {
		return -EINVAL;
	} else {
		AX_OSAL_LIB_memset(res, 0, sizeof(struct AXERA_RESOURCE));
		res->start = res_tmp->start;
		res->end = res_tmp->end;
		AX_OSAL_LIB_memcpy(res->name, res_tmp->name, AX_OSAL_LIB_strlen(res_tmp->name));
		res->flags = res_tmp->flags;
		res->desc = res_tmp->desc;
	}
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_DEV_platform_get_resource_byname);

int AX_OSAL_DEV_platform_get_resource(void * dev, u32 type, u32 num, struct AXERA_RESOURCE * res)
{
	struct resource *res_tmp = NULL;
	res_tmp = platform_get_resource((struct platform_device *)dev, type, num);
	if (!res_tmp) {
		return -EINVAL;
	} else {
		AX_OSAL_LIB_memset(res, 0, sizeof(struct AXERA_RESOURCE));
		res->start = res_tmp->start;
		res->end = res_tmp->end;
		AX_OSAL_LIB_memcpy(res->name, res_tmp->name, AX_OSAL_LIB_strlen(res_tmp->name));
		res->flags = res_tmp->flags;
		res->desc = res_tmp->desc;
	}
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_DEV_platform_get_resource);

int AX_OSAL_DEV_platform_get_irq(void * dev, u32 num)
{
	return platform_get_irq((struct platform_device *)dev, num);
}

EXPORT_SYMBOL(AX_OSAL_DEV_platform_get_irq);

int AX_OSAL_DEV_platform_get_irq_byname(void * dev, const char * name)
{
	return platform_get_irq_byname((struct platform_device *)dev, name);
}

EXPORT_SYMBOL(AX_OSAL_DEV_platform_get_irq_byname);

unsigned long AX_OSAL_DEV_resource_size(const struct AXERA_RESOURCE *res)
{
	return res->end - res->start + 1;
}

EXPORT_SYMBOL(AX_OSAL_DEV_resource_size);

void *AX_OSAL_DEV_platform_get_drvdata(void * pdev)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	return platform_get_drvdata(pvdev);
}

EXPORT_SYMBOL(AX_OSAL_DEV_platform_get_drvdata);

void AX_OSAL_DEV_platform_set_drvdata(void * pdev, void * data)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	platform_set_drvdata(pvdev, data);
}

EXPORT_SYMBOL(AX_OSAL_DEV_platform_set_drvdata);

int AX_OSAL_DEV_platform_irq_count(void * pdev)
{
	struct platform_device *pvdev = (struct platform_device *)pdev;
	return platform_irq_count(pvdev);
}

EXPORT_SYMBOL(AX_OSAL_DEV_platform_irq_count);

void *AX_OSAL_DEV_to_platform_device(void *dev)
{
	void *pdev = (void *)to_platform_device((struct device *)dev);
	return pdev;
}

EXPORT_SYMBOL(AX_OSAL_DEV_to_platform_device);

void *AX_OSAL_DEV_to_platform_driver(void *drv)
{
	void *pdrv = (void *)to_platform_driver((struct device_driver *)drv);
	return pdrv;
}

EXPORT_SYMBOL(AX_OSAL_DEV_to_platform_driver);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include "osal_pm_ax.h"

int AX_OSAL_PM_WakeupLock(char * lock_name)
{
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_PM_WakeupLock);

int AX_OSAL_PM_WakeupUnlock(char * lock_name)
{
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_PM_WakeupUnlock);

int AX_OSAL_PM_SetLevel(int pm_level)
{
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_PM_SetLevel);

int AX_OSAL_PM_GetLevel(int * pm_level)
{
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_PM_GetLevel);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include "osal_ax.h"

void AX_OSAL_prefetch(void *addr)
{
	prefetch(addr);
}
EXPORT_SYMBOL(AX_OSAL_prefetch);

void AX_OSAL_prefetchw(void *addr)
{
	prefetchw(addr);
}
EXPORT_SYMBOL(AX_OSAL_prefetchw);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/reset.h>

#include "linux/platform_device.h"
#include "linux/device.h"

#include "osal_ax.h"
#include "osal_dev_ax.h"
#include "axdev.h"
#include "axdev_log.h"
#include "osal_lib_ax.h"

void *AX_OSAL_DEV_devm_reset_control_get_optional(void * pdev, const char *id, int flag)
{
	struct platform_device *pvdev;
	if (flag == 0) {
		pvdev = (struct platform_device *)pdev;
		return devm_reset_control_get_optional(&pvdev->dev, id);
	} else {
		return 0;
	}
}

EXPORT_SYMBOL(AX_OSAL_DEV_devm_reset_control_get_optional);

int __attribute__ ((__noinline__)) AX_OSAL_DEV_reset_control_assert(void * rstc)
{
	struct reset_control *prstc = (struct reset_control *)rstc;
	return reset_control_assert(prstc);
}

EXPORT_SYMBOL(AX_OSAL_DEV_reset_control_assert);

int __attribute__ ((__noinline__)) AX_OSAL_DEV_reset_control_deassert(void * rstc)
{
	struct reset_control *prstc = (struct reset_control *)rstc;
	return reset_control_deassert(prstc);
}

EXPORT_SYMBOL(AX_OSAL_DEV_reset_control_deassert);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include "osal_logdebug_ax.h"
#include "osal_ax.h"

int AX_OSAL_SYNC_sema_init(AX_SEMAPHORE_T * sem, int val)
{
	struct semaphore *p = NULL;
	if (sem == NULL) {
		printk("%s error sem == NULL\n", __func__);
		return -1;
	}
	p = kmalloc(sizeof(struct semaphore), GFP_KERNEL);
	if (p == NULL) {
		printk("%s alloc semaphore failed\n", __func__);
		return -1;
	}
	sema_init(p, val);
	sem->sem = p;
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_sema_init);

int AX_OSAL_SYNC_sema_down_interruptible(AX_SEMAPHORE_T * sem)
{
	struct semaphore *p = NULL;
	if (sem == NULL) {
		printk("%s error sem == NULL\n", __func__);
		return -1;
	}
	p = (struct semaphore *)(sem->sem);
	return down_interruptible(p);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_sema_down_interruptible);

int AX_OSAL_SYNC_sema_down(AX_SEMAPHORE_T * sem)
{
	struct semaphore *p = NULL;
	if (sem == NULL) {
		printk("%s error sem == NULL\n", __func__);
		return -1;
	}
	p = (struct semaphore *)(sem->sem);
	down(p);
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_sema_down);

int AX_OSAL_SYNC_sema_down_timeout(AX_SEMAPHORE_T * sem, long timeout)
{
	struct semaphore *p = NULL;
	if (sem == NULL) {
		printk("%s error sem == NULL\n", __func__);
		return -1;
	}
	p = (struct semaphore *)(sem->sem);
	return down_timeout(p, timeout);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_sema_down_timeout);

int AX_OSAL_SYNC_sema_down_trylock(AX_SEMAPHORE_T * sem)
{
	struct semaphore *p = NULL;
	if (sem == NULL) {
		printk("%s error sem == NULL\n", __func__);
		return -1;
	}
	p = (struct semaphore *)(sem->sem);
	return down_trylock(p);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_sema_down_trylock);

void AX_OSAL_SYNC_sema_up(AX_SEMAPHORE_T * sem)
{
	struct semaphore *p = NULL;
	p = (struct semaphore *)(sem->sem);
	up(p);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_sema_up);

void AX_OSAL_SYNC_sema_destroy(AX_SEMAPHORE_T * sem)
{
	struct semaphore *p = NULL;
	p = (struct semaphore *)(sem->sem);
	kfree(p);
	sem->sem = NULL;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_sema_destroy);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/export.h>
#include "osal_type_ax.h"

/**
 * is_aligned - is this pointer & size okay for word-wide copying?
 * @base: pointer to data
 * @size: size of each element
 * @align: required alignment (typically 4 or 8)
 *
 * Returns true if elements can be copied using word loads and stores.
 * The size must be a multiple of the alignment, and the base address must
 * be if we do not have CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS.
 *
 * For some reason, gcc doesn't know to optimize "if (a & mask || b & mask)"
 * to "if ((a | b) & mask)", so we do that by hand.
 */
__attribute_const__ __always_inline
static bool is_aligned(const void *base, size_t size, unsigned char align)
{
	unsigned char lsbits = (unsigned char)size;

	(void)base;
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	lsbits |= (unsigned char)(uintptr_t)base;
#endif
	return (lsbits & (align - 1)) == 0;
}

/**
 * swap_words_32 - swap two elements in 32-bit chunks
 * @a: pointer to the first element to swap
 * @b: pointer to the second element to swap
 * @n: element size (must be a multiple of 4)
 *
 * Exchange the two objects in memory.  This exploits base+index addressing,
 * which basically all CPUs have, to minimize loop overhead computations.
 *
 * For some reason, on x86 gcc 7.3.0 adds a redundant test of n at the
 * bottom of the loop, even though the zero flag is still valid from the
 * subtract (since the intervening mov instructions don't alter the flags).
 * Gcc 8.1.0 doesn't have that problem.
 */
static void swap_words_32(void *a, void *b, size_t n)
{
	do {
		u32 t = *(u32 *)(a + (n -= 4));
		*(u32 *)(a + n) = *(u32 *)(b + n);
		*(u32 *)(b + n) = t;
	} while (n);
}

/**
 * swap_words_64 - swap two elements in 64-bit chunks
 * @a: pointer to the first element to swap
 * @b: pointer to the second element to swap
 * @n: element size (must be a multiple of 8)
 *
 * Exchange the two objects in memory.  This exploits base+index
 * addressing, which basically all CPUs have, to minimize loop overhead
 * computations.
 *
 * We'd like to use 64-bit loads if possible.  If they're not, emulating
 * one requires base+index+4 addressing which x86 has but most other
 * processors do not.  If CONFIG_64BIT, we definitely have 64-bit loads,
 * but it's possible to have 64-bit loads without 64-bit pointers (e.g.
 * x32 ABI).  Are there any cases the kernel needs to worry about?
 */
static void swap_words_64(void *a, void *b, size_t n)
{
	do {
#ifdef CONFIG_64BIT
		u64 t = *(u64 *)(a + (n -= 8));
		*(u64 *)(a + n) = *(u64 *)(b + n);
		*(u64 *)(b + n) = t;
#else
		/* Use two 32-bit transfers to avoid base+index+4 addressing */
		u32 t = *(u32 *)(a + (n -= 4));
		*(u32 *)(a + n) = *(u32 *)(b + n);
		*(u32 *)(b + n) = t;

		t = *(u32 *)(a + (n -= 4));
		*(u32 *)(a + n) = *(u32 *)(b + n);
		*(u32 *)(b + n) = t;
#endif
	} while (n);
}

/**
 * swap_bytes - swap two elements a byte at a time
 * @a: pointer to the first element to swap
 * @b: pointer to the second element to swap
 * @n: element size
 *
 * This is the fallback if alignment doesn't allow using larger chunks.
 */
static void swap_bytes(void *a, void *b, size_t n)
{
	do {
		char t = ((char *)a)[--n];
		((char *)a)[n] = ((char *)b)[n];
		((char *)b)[n] = t;
	} while (n);
}

/*
 * The values are arbitrary as long as they can't be confused with
 * a pointer, but small integers make for the smallest compare
 * instructions.
 */
#define SWAP_WORDS_64 (swap_func_t)0
#define SWAP_WORDS_32 (swap_func_t)1
#define SWAP_BYTES    (swap_func_t)2

/*
 * The function pointer is last to make tail calls most efficient if the
 * compiler decides not to inline this function.
 */
static void do_swap(void *a, void *b, size_t size, swap_func_t swap_func)
{
	if (swap_func == SWAP_WORDS_64)
		swap_words_64(a, b, size);
	else if (swap_func == SWAP_WORDS_32)
		swap_words_32(a, b, size);
	else if (swap_func == SWAP_BYTES)
		swap_bytes(a, b, size);
	else
		swap_func(a, b, (int)size);
}

#define _CMP_WRAPPER ((cmp_r_func_t)0L)

static int do_cmp(const void *a, const void *b, cmp_r_func_t cmp, const void *priv)
{
	if (cmp == _CMP_WRAPPER)
		return ((cmp_func_t)(priv))(a, b);
	return cmp(a, b, priv);
}

/**
 * parent - given the offset of the child, find the offset of the parent.
 * @i: the offset of the heap element whose parent is sought.  Non-zero.
 * @lsbit: a precomputed 1-bit mask, equal to "size & -size"
 * @size: size of each element
 *
 * In terms of array indexes, the parent of element j = @i/@size is simply
 * (j-1)/2.  But when working in byte offsets, we can't use implicit
 * truncation of integer divides.
 *
 * Fortunately, we only need one bit of the quotient, not the full divide.
 * @size has a least significant bit.  That bit will be clear if @i is
 * an even multiple of @size, and set if it's an odd multiple.
 *
 * Logically, we're doing "if (i & lsbit) i -= size;", but since the
 * branch is unpredictable, it's done with a bit of clever branch-free
 * code instead.
 */
__attribute_const__ __always_inline
static size_t parent(size_t i, unsigned int lsbit, size_t size)
{
	i -= size;
	i -= size & -(i & lsbit);
	return i / 2;
}

/**
 * sort_r - sort an array of elements
 * @base: pointer to data to sort
 * @num: number of elements
 * @size: size of each element
 * @cmp_func: pointer to comparison function
 * @swap_func: pointer to swap function or NULL
 * @priv: third argument passed to comparison function
 *
 * This function does a heapsort on the given array.  You may provide
 * a swap_func function if you need to do something more than a memory
 * copy (e.g. fix up pointers or auxiliary data), but the built-in swap
 * avoids a slow retpoline and so is significantly faster.
 *
 * Sorting time is O(n log n) both on average and worst-case. While
 * quicksort is slightly faster on average, it suffers from exploitable
 * O(n*n) worst-case behavior and extra memory requirements that make
 * it less suitable for kernel use.
 */
void AX_OSAL_LIB_sort_r(void *base, size_t num, size_t size,
	    cmp_r_func_t cmp_func,
	    swap_func_t swap_func,
	    const void *priv)
{
	/* pre-scale counters for performance */
	size_t n = num * size, a = (num/2) * size;
	const unsigned int lsbit = size & -size;  /* Used to find parent */

	if (!a)		/* num < 2 || size == 0 */
		return;

	if (!swap_func) {
		if (is_aligned(base, size, 8))
			swap_func = SWAP_WORDS_64;
		else if (is_aligned(base, size, 4))
			swap_func = SWAP_WORDS_32;
		else
			swap_func = SWAP_BYTES;
	}

	/*
	 * Loop invariants:
	 * 1. elements [a,n) satisfy the heap property (compare greater than
	 *    all of their children),
	 * 2. elements [n,num*size) are sorted, and
	 * 3. a <= b <= c <= d <= n (whenever they are valid).
	 */
	for (;;) {
		size_t b, c, d;

		if (a)			/* Building heap: sift down --a */
			a -= size;
		else if (n -= size)	/* Sorting: Extract root to --n */
			do_swap(base, base + n, size, swap_func);
		else			/* Sort complete */
			break;

		/*
		 * Sift element at "a" down into heap.  This is the
		 * "bottom-up" variant, which significantly reduces
		 * calls to cmp_func(): we find the sift-down path all
		 * the way to the leaves (one compare per level), then
		 * backtrack to find where to insert the target element.
		 *
		 * Because elements tend to sift down close to the leaves,
		 * this uses fewer compares than doing two per level
		 * on the way down.  (A bit more than half as many on
		 * average, 3/4 worst-case.)
		 */
		for (b = a; c = 2*b + size, (d = c + size) < n;)
			b = do_cmp(base + c, base + d, cmp_func, priv) >= 0 ? c : d;
		if (d == n)	/* Special case last leaf with no sibling */
			b = c;

		/* Now backtrack from "b" to the correct location for "a" */
		while (b != a && do_cmp(base + a, base + b, cmp_func, priv) >= 0)
			b = parent(b, lsbit, size);
		c = b;			/* Where "a" belongs */
		while (b != a) {	/* Shift it into place */
			b = parent(b, lsbit, size);
			do_swap(base + b, base + c, size, swap_func);
		}
	}
}
EXPORT_SYMBOL(AX_OSAL_LIB_sort_r);

void AX_OSAL_LIB_sort(void *base, size_t num, size_t size,
	  cmp_func_t cmp_func,
	  swap_func_t swap_func)
{
	return AX_OSAL_LIB_sort_r(base, num, size, _CMP_WRAPPER, swap_func, cmp_func);
}
EXPORT_SYMBOL(AX_OSAL_LIB_sort);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include "osal_logdebug_ax.h"
#include "osal_ax.h"


#ifndef CONFIG_DEBUG_SPINLOCK
int AX_OSAL_SYNC_spin_lock_init(AX_SPINLOCK_T * lock)
{
	spinlock_t *p = NULL;
	if (lock == NULL) {
		printk("%s error lock == NULL\n", __func__);
		return -1;
	}
	p = (spinlock_t *) kmalloc(sizeof(spinlock_t), GFP_KERNEL);
	if (p == NULL) {
		printk("%s alloc spinlock failed\n", __func__);
		return -1;
	}
	spin_lock_init(p);
	lock->lock = p;
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_spin_lock_init);

#else
spinlock_t *AX_OSAL_DBG_spin_lock_init(AX_SPINLOCK_T *lock)
{
	spinlock_t *p = NULL;
	if (lock == NULL) {
		printk("%s error lock == NULL\n", __func__);
		return NULL;
	}

	p = (spinlock_t *) kmalloc(sizeof(spinlock_t), GFP_KERNEL);
	if (p == NULL) {
		printk("%s alloc spinlock failed\n", __func__);
		return  NULL;
	}

	lock->lock = p;
	return p;
}

EXPORT_SYMBOL(AX_OSAL_DBG_spin_lock_init);
#endif


void AX_OSAL_SYNC_spin_lock(AX_SPINLOCK_T * lock)
{
	spinlock_t *p = NULL;

	p = (spinlock_t *) (lock->lock);
	spin_lock(p);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_spin_lock);

int AX_OSAL_SYNC_spin_trylock(AX_SPINLOCK_T * lock)
{
	spinlock_t *p = NULL;
	if (lock == NULL) {
		printk("%s error lock == NULL\n", __func__);
		return -1;
	}
	p = (spinlock_t *) (lock->lock);
	return spin_trylock(p);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_spin_trylock);

void AX_OSAL_SYNC_spin_unlock(AX_SPINLOCK_T * lock)
{
	spinlock_t *p = NULL;

	p = (spinlock_t *) (lock->lock);
	spin_unlock(p);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_spin_unlock);

void AX_OSAL_SYNC_spin_lock_irqsave(AX_SPINLOCK_T * lock, u32 * flags)
{
	spinlock_t *p = NULL;
	unsigned long f;

	p = (spinlock_t *) (lock->lock);
	spin_lock_irqsave(p, f);
	*flags = f;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_spin_lock_irqsave);

void AX_OSAL_SYNC_spin_unlock_irqrestore(AX_SPINLOCK_T * lock, u32 * flags)
{
	spinlock_t *p = NULL;
	unsigned long f;

	p = (spinlock_t *) (lock->lock);
	f = *flags;
	spin_unlock_irqrestore(p, f);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_spin_unlock_irqrestore);

void AX_OSAL_SYNC_spinLock_destory(AX_SPINLOCK_T * lock)
{
	spinlock_t *p = NULL;
	p = (spinlock_t *) (lock->lock);
	kfree(p);
	lock->lock = NULL;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_spinLock_destory);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/slab.h>
#include "osal_ax.h"
#include "osal_logdebug_ax.h"

#ifndef CONFIG_DEBUG_LOCK_ALLOC
int AX_OSAL_init_srcu_struct(AX_OSAL_srcu_struct_t *ssp)
{
	struct srcu_struct *p = NULL;
	int ret;
	if (ssp == NULL) {
		printk("%s error ssp == NULL\n", __func__);
		return -1;
	}
	p = (struct srcu_struct *)kmalloc(sizeof(struct srcu_struct), GFP_KERNEL);
	if (p == NULL) {
		printk("%s alloc srcu_struct failed\n", __func__);
		return -1;
	}
	ret = init_srcu_struct(p);
	if (ret != 0) {
		kfree(p);
		ssp->ssp = NULL;
		printk("%s init_srcu_struct failed\n", __func__);
		return -1;
	}
	ssp->ssp = p;

	return 0;
}
EXPORT_SYMBOL(AX_OSAL_init_srcu_struct);
#else
void *AX_OSAL_DBG_init_srcu_struct(AX_OSAL_srcu_struct_t *ssp)
{
	struct srcu_struct *p = NULL;
	if (ssp == NULL) {
		printk("%s error ssp == NULL\n", __func__);
		return NULL;
	}
	p = (struct srcu_struct *)kmalloc(sizeof(struct srcu_struct), GFP_KERNEL);
	if (p == NULL) {
		ssp->ssp = NULL;
		printk("%s alloc srcu_struct failed\n", __func__);
		return NULL;
	}
	ssp->ssp = p;

	return p;
}
EXPORT_SYMBOL(AX_OSAL_DBG_init_srcu_struct);
#endif

void AX_OSAL_cleanup_srcu_struct(AX_OSAL_srcu_struct_t *ssp)
{
	struct srcu_struct *p = NULL;
	if (ssp == NULL || ssp->ssp == NULL) {
		printk("%s error NULL param\n", __func__);
		return;
	}

	p = (struct srcu_struct *)ssp->ssp;
	cleanup_srcu_struct(p);
	kfree(ssp->ssp);
	ssp->ssp = NULL;
}
EXPORT_SYMBOL(AX_OSAL_cleanup_srcu_struct);

int AX_OSAL_srcu_read_lock(AX_OSAL_srcu_struct_t *ssp)
{
	struct srcu_struct *p = NULL;
	if (ssp == NULL || ssp->ssp == NULL) {
		printk("%s error NULL param\n", __func__);
		return -1;
	}

	p = (struct srcu_struct *)ssp->ssp;
	return srcu_read_lock(p);
}
EXPORT_SYMBOL(AX_OSAL_srcu_read_lock);

void AX_OSAL_srcu_read_unlock(AX_OSAL_srcu_struct_t *ssp, int idx)
{
	struct srcu_struct *p = NULL;
	if (ssp == NULL || ssp->ssp == NULL) {
		printk("%s error NULL param\n", __func__);
		return;
	}

	p = (struct srcu_struct *)ssp->ssp;
	srcu_read_unlock(p, idx);
}
EXPORT_SYMBOL(AX_OSAL_srcu_read_unlock);

void AX_OSAL_synchronize_srcu(AX_OSAL_srcu_struct_t *ssp)
{
	struct srcu_struct *p = NULL;
	if (ssp == NULL || ssp->ssp == NULL) {
		printk("%s error NULL param\n", __func__);
		return;
	}

	p = (struct srcu_struct *)ssp->ssp;
	synchronize_srcu(p);
}
EXPORT_SYMBOL(AX_OSAL_synchronize_srcu);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/version.h>
#include "osal_lib_ax.h"

char *AX_OSAL_LIB_strcpy(char * dest, const char * src)
{
	return strcpy(dest, src);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strcpy);

char *AX_OSAL_LIB_strncpy(char * dest, const char * src, int count)
{
	return strncpy(dest, src, count);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strncpy);

int AX_OSAL_LIB_strlcpy(char * dest, const char * src, int count)
{
	return strlcpy(dest, src, count);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strlcpy);

char *AX_OSAL_LIB_strcat(char * dest, const char * src)
{
	return strcat(dest, src);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strcat);

char *AX_OSAL_LIB_strncat(char * dest, const char * src, int count)
{
	return strncat(dest, src, count);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strncat);

int AX_OSAL_LIB_strlcat(char * dest, const char * src, int count)
{
	return strlcat(dest, src, count);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strlcat);

int AX_OSAL_LIB_strcmp(const char * cs, const char * ct)
{
	return strcmp(cs, ct);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strcmp);

int AX_OSAL_LIB_strncmp(const char * cs, const char * ct, int count)
{
	return strncmp(cs, ct, count);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strncmp);

int AX_OSAL_LIB_strnicmp(const char * s1, const char * s2, int len)
{
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_LIB_strnicmp);

int AX_OSAL_LIB_strcasecmp(const char * s1, const char * s2)
{
	return strcasecmp(s1, s2);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strcasecmp);

int AX_OSAL_LIB_strncasecmp(const char * s1, const char * s2, int len)
{
	return strncasecmp(s1, s2, len);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strncasecmp);

char *AX_OSAL_LIB_strchr(const char * s, int c)
{
	return strchr(s, c);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strchr);

char *AX_OSAL_LIB_strnchr(const char * s, int count, int c)
{
	return strnchr(s, count, c);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strnchr);

char *AX_OSAL_LIB_strrchr(const char * s, int c)
{
	return strrchr(s, c);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strrchr);

char *AX_OSAL_LIB_strstr(const char * s1, const char * s2)
{
	return strstr(s1, s2);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strstr);

char *AX_OSAL_LIB_strnstr(const char * s1, const char * s2, int len)
{
	return strnstr(s1, s2, len);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strnstr);

int AX_OSAL_LIB_strlen(const char * s)
{
	return strlen(s);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strlen);

int AX_OSAL_LIB_strnlen(const char * s, int count)
{
	return strnlen(s, count);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strnlen);

char *AX_OSAL_LIB_strpbrk(const char * cs, const char * ct)
{
	return strpbrk(cs, ct);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strpbrk);

char *AX_OSAL_LIB_strsep(const char ** s, const char * ct)
{
	return strsep((char **) s, ct);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strsep);

int AX_OSAL_LIB_strspn(const char * s, const char * accept)
{
	return strspn((char *) s, accept);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strspn);

int AX_OSAL_LIB_strcspn(const char * s, const char * reject)
{
	return strcspn((char *) s, reject);
}

EXPORT_SYMBOL(AX_OSAL_LIB_strcspn);

void *AX_OSAL_LIB_memset(void * str, int c, int count)
{
	return memset(str, c, count);
}

EXPORT_SYMBOL(AX_OSAL_LIB_memset);

void *AX_OSAL_LIB_memmove(void * dest, const void * src, int count)
{
	return memmove(dest, src, count);
}

EXPORT_SYMBOL(AX_OSAL_LIB_memmove);

void *AX_OSAL_LIB_memscan(void * addr, int c, int size)
{
	return memscan(addr, c, size);
}

EXPORT_SYMBOL(AX_OSAL_LIB_memscan);

void *AX_OSAL_LIB_memcpy(void * ct, const void * cs, int count)
{
	return memcpy(ct, cs, count);
}

EXPORT_SYMBOL(AX_OSAL_LIB_memcpy);

void *AX_OSAL_LIB_memchr(const void * s, int c, int n)
{
	return memchr(s, c, n);
}

EXPORT_SYMBOL(AX_OSAL_LIB_memchr);

void *AX_OSAL_LIB_memchar_inv(const void * start, int c, int bytes)
{
	return memchr_inv(start, c, bytes);
}

EXPORT_SYMBOL(AX_OSAL_LIB_memchar_inv);

unsigned long AX_OSAL_LIB_simple_strtoull(const char * cp, char ** endp, unsigned int base)
{
	return simple_strtoull(cp, (char **) endp, base);
}

EXPORT_SYMBOL(AX_OSAL_LIB_simple_strtoull);

unsigned long AX_OSAL_LIB_simple_strtoul(const char * cp, char ** endp, unsigned int base)
{
	return simple_strtoul(cp, (char **) endp, base);
}

EXPORT_SYMBOL(AX_OSAL_LIB_simple_strtoul);

long AX_OSAL_LIB_simple_strtol(const char * cp, char ** endp, unsigned int base)
{
	return simple_strtol(cp, (char **) endp, base);
}

EXPORT_SYMBOL(AX_OSAL_LIB_simple_strtol);

s64 AX_OSAL_LIB_simple_strtoll(const char * cp, char ** endp, unsigned int base)
{
	return simple_strtoll(cp, (char **) endp, base);
}

EXPORT_SYMBOL(AX_OSAL_LIB_simple_strtoll);

int AX_OSAL_LIB_snprintf(char * buf, int size, const char * fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsnprintf(buf, size, fmt, args);
	va_end(args);

	return i;
}

EXPORT_SYMBOL(AX_OSAL_LIB_snprintf);

int AX_OSAL_LIB_scnprintf(char * buf, int size, const char * fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vscnprintf(buf, size, fmt, args);
	va_end(args);

	return i;
}

EXPORT_SYMBOL(AX_OSAL_LIB_scnprintf);

int AX_OSAL_LIB_sprintf(char * buf, const char * fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsnprintf(buf, INT_MAX, fmt, args);
	va_end(args);

	return i;
}

EXPORT_SYMBOL(AX_OSAL_LIB_sprintf);

int AX_OSAL_LIB_vsscanf(char * buf, const char * fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsscanf(buf, fmt, args);
	va_end(args);

	return i;
}

EXPORT_SYMBOL(AX_OSAL_LIB_vsscanf);

int AX_OSAL_LIB_vsnprintf(char * str, int size, const char * fmt, AX_VA_LIST args)
{
	return vsnprintf(str, size, fmt, args);
}

EXPORT_SYMBOL(AX_OSAL_LIB_vsnprintf);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include "osal_logdebug_ax.h"
#include "osal_ax.h"

AX_TASK_T *AX_OSAL_TASK_kthread_run(AX_THREAD_FUNC_T thread, void * data, char * name)
{
	struct task_struct *k;
	AX_TASK_T *p = (AX_TASK_T *) kmalloc(sizeof(AX_TASK_T), GFP_KERNEL);
	if (p == NULL) {
		printk("%s error p == NULL\n", __func__);
		return NULL;
	}

	k = kthread_run(thread, data, name);
	if (IS_ERR(k)) {
		printk("%s kthread run error\n", __func__);
		kfree(p);
		return NULL;
	}
	p->task_struct = k;
	return p;
}

EXPORT_SYMBOL(AX_OSAL_TASK_kthread_run);

AX_TASK_T *AX_OSAL_TASK_kthread_create_ex(AX_THREAD_FUNC_T thread, void * data, char * name, int prioirty)
{
	struct task_struct *k;
	AX_TASK_T *p = (AX_TASK_T *) kmalloc(sizeof(AX_TASK_T), GFP_KERNEL);
	if (p == NULL) {
		printk("%s error p == NULL\n", __func__);
		return NULL;
	}

	k = kthread_run(thread, data, name);
	if (IS_ERR(k)) {
		printk("%s - kthread create error!\n", __func__);
		kfree(p);
		return NULL;
	}
	p->task_struct = k;
	return p;

}

EXPORT_SYMBOL(AX_OSAL_TASK_kthread_create_ex);

int AX_OSAL_TASK_kthread_stop(AX_TASK_T * task, unsigned int stop_flag)
{
	if (task == NULL) {
		printk("%s error task == NULL\n", __func__);
		return -1;
	}

	if (0 != stop_flag) {
		kthread_stop((struct task_struct *)(task->task_struct));
	}
	task->task_struct = NULL;
	kfree(task);

	return 0;
}

EXPORT_SYMBOL(AX_OSAL_TASK_kthread_stop);

int AX_OSAL_TASK_cond_resched(void)
{
	cond_resched();
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_TASK_cond_resched);

bool AX_OSAL_TASK_kthread_should_stop(void)
{
	return kthread_should_stop();
}

EXPORT_SYMBOL(AX_OSAL_TASK_kthread_should_stop);

void AX_OSAL_TASK_schedule(void)
{
	schedule();
	return;
}

EXPORT_SYMBOL(AX_OSAL_TASK_schedule);

void AX_OSAL_set_current_state(int state_value)
{
	set_current_state(state_value);
	return;
}

EXPORT_SYMBOL(AX_OSAL_set_current_state);

int AX_OSAL_sched_setscheduler(AX_TASK_T * task, int policy, const struct AX_TASK_SCHED_PARAM * param)
{
	struct task_struct *p = (struct task_struct *)task->task_struct;
	return sched_setscheduler(p, policy, (struct sched_param *)param);
}

EXPORT_SYMBOL(AX_OSAL_sched_setscheduler);

void AX_OSAL_get_current(struct AX_CURRENT_TASK *ax_task)
{
	ax_task->pid = current->pid;
	ax_task->tgid = current->tgid;
	ax_task->mm = (void *)current->mm;
	memcpy(ax_task->comm,current->comm,16);
}

EXPORT_SYMBOL(AX_OSAL_get_current);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/rtc.h>
#include <linux/clk.h>
#include "linux/sched/clock.h"

#include "osal_ax.h"
#include "osal_logdebug_ax.h"

static void osal_timer_func_entry(struct timer_list *t)
{
	struct AX_TIMER *axTimer = from_timer(axTimer, t, timer);
	if (axTimer == NULL) {
		printk("%s error axTimer == NULL\n", __func__);
		return;
	}

	if (axTimer->function)
		axTimer->function((void *) axTimer->data);
	return;
}

int AX_OSAL_TMR_init_timers(AX_TIMER_T * axTimer)
{
	if (axTimer == NULL) {
		printk("%s error axTimer == NULL\n", __func__);
		return -1;
	}

	timer_setup(&axTimer->timer, osal_timer_func_entry, 0);
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_TMR_init_timers);

unsigned int AX_OSAL_TMR_mod_timer(AX_TIMER_T * axTimer, unsigned long interval)
{
	if (axTimer == NULL || axTimer->function == NULL || interval == 0) {
		printk("%s parameter error!!!\n", __func__);
		return -1;
	}

	return mod_timer(&axTimer->timer, jiffies + msecs_to_jiffies(interval) - 1);
}

EXPORT_SYMBOL(AX_OSAL_TMR_mod_timer);

unsigned int AX_OSAL_TMR_del_timer(AX_TIMER_T * axTimer)
{
	if (axTimer == NULL || axTimer->function == NULL) {
		printk("%s parameter error!!!\n", __func__);
		return -1;
	}
	return del_timer(&axTimer->timer);
}

EXPORT_SYMBOL(AX_OSAL_TMR_del_timer);

int AX_OSAL_TMR_destory_timer(AX_TIMER_T * axTimer)
{
	del_timer(&axTimer->timer);
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_TMR_destory_timer);

unsigned long AX_OSAL_TM_msleep(unsigned int msecs)
{
	msleep(msecs);
	return msecs;
}

EXPORT_SYMBOL(AX_OSAL_TM_msleep);

void AX_OSAL_TM_udelay(unsigned int usecs)
{
	udelay(usecs);
}

EXPORT_SYMBOL(AX_OSAL_TM_udelay);

void AX_OSAL_TM_mdelay(unsigned int msecs)
{
	mdelay(msecs);
}

EXPORT_SYMBOL(AX_OSAL_TM_mdelay);

unsigned int AX_OSAL_TM_jiffies_to_msecs(void)
{
	return jiffies_to_msecs(jiffies);
}

EXPORT_SYMBOL(AX_OSAL_TM_jiffies_to_msecs);

u64 AX_OSAL_TM_sched_clock(void)
{
	return sched_clock();
}

EXPORT_SYMBOL(AX_OSAL_TM_sched_clock);

u64 AX_OSAL_TM_get_microseconds(void)
{
	u64 current_microsecond = 0;
	u64 tmp_remainder = 0;

	current_microsecond = sched_clock();

	tmp_remainder = do_div(current_microsecond, 1000);

	return current_microsecond;
}

EXPORT_SYMBOL(AX_OSAL_TM_get_microseconds);

void AX_OSAL_TM_do_gettimeofday(AX_TIMERVAL_T * tm)
{
	struct timespec64 now;

	if (tm == NULL) {
		printk("%s - parameter invalid!\n", __func__);
		return;
	}

	ktime_get_real_ts64(&now);
	tm->tv_sec = now.tv_sec;
	tm->tv_usec = now.tv_nsec / 1000;
}

EXPORT_SYMBOL(AX_OSAL_TM_do_gettimeofday);

 long __attribute__ ((__noinline__)) AX_OSAL_TM_msecs_to_jiffies( long ax_msecs)
{
	long tmp_msecs = msecs_to_jiffies(ax_msecs);
	return tmp_msecs;
}

EXPORT_SYMBOL(AX_OSAL_TM_msecs_to_jiffies);

void AX_OSAL_TM_get_jiffies(u64 * pjiffies)
{
	*pjiffies = jiffies;
}

EXPORT_SYMBOL(AX_OSAL_TM_get_jiffies);


void AX_OSAL_TM_ktime_get_real_ts64(AX_TIMERSPEC64_T * tm)
{
	struct timespec64 t;
	if (tm == NULL) {
		printk("%s ktime error\n", __func__);
		return;
	}
	ktime_get_real_ts64(&t);

	tm->tv_sec = t.tv_sec;
	tm->tv_nsec = t.tv_nsec;
}

EXPORT_SYMBOL(AX_OSAL_TM_ktime_get_real_ts64);

long AX_OSAL_DEV_usleep(unsigned long use)
{
	return ax_usleep(use);
}

EXPORT_SYMBOL(AX_OSAL_DEV_usleep);


void *AX_OSAL_DEV_hrtimer_alloc(unsigned long use,int (*function)(void *),void *private)
{
	struct ax_hrtimer *timer = ax_hrtimer_alloc();
	timer->delay = use;
	timer->function = function;
	timer->private = private;

	return (void *)timer;

}

EXPORT_SYMBOL(AX_OSAL_DEV_hrtimer_alloc);

void AX_OSAL_DEV_hrtimer_destroy(void *timer)
{
	ax_hrtimer_destroy((struct ax_hrtimer *)timer);
}

EXPORT_SYMBOL(AX_OSAL_DEV_hrtimer_destroy);

int AX_OSAL_DEV_hrtimer_start(void *timer)
{
	return ax_hrtimer_start((struct ax_hrtimer *)timer);
}

EXPORT_SYMBOL(AX_OSAL_DEV_hrtimer_start);

int AX_OSAL_DEV_hrtimer_stop(void *timer)
{
        return ax_hrtimer_stop((struct ax_hrtimer *)timer);
}

EXPORT_SYMBOL(AX_OSAL_DEV_hrtimer_stop);

/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include "osal_ax.h"

int AX_OSAL_SYNC_waitqueue_init(AX_WAIT_T * wait)
{
	wait_queue_head_t *wq;
	if (wait == NULL) {
		printk("%s - parameter invalid!\n", __func__);
		return -1;
	}
	wq = (wait_queue_head_t *) kmalloc(sizeof(wait_queue_head_t), GFP_ATOMIC);
	if (wq == NULL) {
		printk("%s - kmalloc error!\n", __func__);
		return -1;
	}
	init_waitqueue_head(wq);
	wait->wait = wq;
	return 0;

}

EXPORT_SYMBOL(AX_OSAL_SYNC_waitqueue_init);

unsigned int AX_OSAL_SYNC_wait_uninterruptible(AX_WAIT_T * wait, AX_WAIT_COND_FUNC_T func, void * param)
{
	wait_queue_head_t *wq;
	DEFINE_WAIT(__wait);
	long ret = 0;
	int condition = 0;

	if (wait == NULL) {
		printk("%s - parameter invalid!\n", __func__);
		return -1;
	}

	wq = (wait_queue_head_t *) (wait->wait);
	if (wq == NULL) {
		printk("%s - wait->wait is NULL!\n", __func__);
		return -1;
	}
	prepare_to_wait(wq, &__wait, TASK_UNINTERRUPTIBLE);
	/* if wakeup the queue brefore prepare_to_wait, the func will return true. And will not go to schedule */
	if (NULL != func) {
		condition = func(param);
	}

	if (!condition) {
		schedule();
	}

	finish_wait(wq, &__wait);
	return ret;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_wait_uninterruptible);

unsigned int AX_OSAL_SYNC_wait_interruptible(AX_WAIT_T * wait, AX_WAIT_COND_FUNC_T func, void * param)
{
	wait_queue_head_t *wq;
	DEFINE_WAIT(__wait);
	long ret = 0;
	int condition = 0;

	if (wait == NULL) {
		printk("%s - parameter invalid!\n", __func__);
		return -1;
	}

	wq = (wait_queue_head_t *) (wait->wait);
	if (wq == NULL) {
		printk("%s - wait->wait is NULL!\n", __func__);
		return -1;
	}
	prepare_to_wait(wq, &__wait, TASK_INTERRUPTIBLE);

	if (NULL != func) {
		condition = func(param);
	}

	if (!condition) {
		if (!signal_pending(current)) {
			schedule();
		}
		if (signal_pending(current))
			ret = -ERESTARTSYS;
	} else {
		ret = -2;
	}

	finish_wait(wq, &__wait);
	return ret;

}

EXPORT_SYMBOL(AX_OSAL_SYNC_wait_interruptible);

unsigned int AX_OSAL_SYNC_wait_uninterruptible_timeout(AX_WAIT_T * wait, AX_WAIT_COND_FUNC_T func, void * param,
						 unsigned long timeout)
{
	wait_queue_head_t *wq;
	DEFINE_WAIT(__wait);
	long ret = timeout;
	int condition = 0;

	if (wait == NULL) {
		printk("%s wait == NULL\n", __func__);
		return -1;
	}

	wq = (wait_queue_head_t *) (wait->wait);
	if (wq == NULL) {
		printk("%swq == NULL\n", __func__);
		return -1;
	}
	prepare_to_wait(wq, &__wait, TASK_UNINTERRUPTIBLE);

	if (NULL != func) {
		condition = func(param);
	}

	if (!condition) {
		ret = schedule_timeout(ret);
	}

	finish_wait(wq, &__wait);

	return ret;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_wait_uninterruptible_timeout);

unsigned int AX_OSAL_SYNC_wait_interruptible_timeout(AX_WAIT_T * wait, AX_WAIT_COND_FUNC_T func, void * param,
					       unsigned long timeout)
{
	wait_queue_head_t *wq;
	DEFINE_WAIT(__wait);
	long ret = timeout;
	int condition = 0;

	if (wait == NULL) {
		printk("%s wq == NULL\n", __func__);
		return -1;
	}

	wq = (wait_queue_head_t *) (wait->wait);
	if (wq == NULL) {
		printk("%s wq == NULL\n", __func__);
		return -1;
	}
	prepare_to_wait(wq, &__wait, TASK_INTERRUPTIBLE);
	/* if wakeup the queue brefore prepare_to_wait, the func will return true. And will not go to schedule */
	if (NULL != func) {
		condition = func(param);
	}

	if (!condition) {
		if (!signal_pending(current)) {
			ret = schedule_timeout(ret);
		}
		if (signal_pending(current))
			ret = -ERESTARTSYS;
	}

	finish_wait(wq, &__wait);

	return ret;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_wait_interruptible_timeout);

void AX_OSAL_SYNC_wakeup(AX_WAIT_T * wait, void * key)
{
	wait_queue_head_t *wq;

	wq = (wait_queue_head_t *) (wait->wait);
	if (wq == NULL) {
		printk("%s wq == NULL\n", __func__);
		return;
	}
	wake_up_all(wq);
}

EXPORT_SYMBOL(AX_OSAL_SYNC_wakeup);

void AX_OSAL_SYNC_wake_up_interruptible(AX_WAIT_T * wait, void * key)
{
	wait_queue_head_t *wq;

	wq = (wait_queue_head_t *) (wait->wait);
	if (wq == NULL) {
		printk("%s wq == NULL\n", __func__);
		return;
	}
	wake_up_interruptible(wq);

	return;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_wake_up_interruptible);

void AX_OSAL_SYNC_wake_up_interruptible_all(AX_WAIT_T * wait, void * key)
{
	wait_queue_head_t *wq;

	wq = (wait_queue_head_t *) (wait->wait);
	if (wq == NULL) {
		printk("%s wq == NULL\n", __func__);
		return;
	}
	wake_up_interruptible_all(wq);

	return;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_wake_up_interruptible_all);

void AX_OSAL_SYNC_wait_destroy(AX_WAIT_T * wait)
{
	wait_queue_head_t *wq;

	wq = (wait_queue_head_t *) (wait->wait);
	if (wq == NULL) {
		printk("%s wq == NULL\n", __func__);
		return;
	}
	kfree(wq);
	wait->wait = NULL;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_wait_destroy);
/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include "osal_list_ax.h"
#include "osal_logdebug_ax.h"
#include "osal_ax.h"

AX_OSAL_LIST_HEAD(wq_list);
struct wq_node {
	AX_WORK_T *osal_work;
	struct work_struct *work;
	AX_LIST_HEAD_T node;
};

static AX_WORK_T *AX_OSAL_SYNC_find_work(struct work_struct *work)
{
	AX_LIST_HEAD_T *this = NULL;
	if (AX_OSAL_LIB_list_empty(&wq_list)) {
		printk("%s wq_list is empty\n", __func__);
		return NULL;
	}
	AX_OSAL_LIB_list_for_each(this, &wq_list) {
		struct wq_node *ws = AX_OSAL_LIB_list_entry(this, struct wq_node, node);
		if (ws->work == work) {
			return ws->osal_work;
		}
	}
	printk("%s failed\n", __func__);
	return NULL;
}

static int AX_OSAL_SYNC_del_work(struct work_struct *work)
{
	AX_LIST_HEAD_T *this = NULL;
	if (AX_OSAL_LIB_list_empty(&wq_list)) {
		printk("%s error wq_list is empty\n", __func__);
		return -1;
	}
	AX_OSAL_LIB_list_for_each(this, &wq_list) {
		struct wq_node *ws = AX_OSAL_LIB_list_entry(this, struct wq_node, node);
		if (ws->work == work) {
			AX_OSAL_LIB_list_del(this);
			kfree(ws);
			return 0;
		}
	}
	printk("%s failed\n", __func__);
	return -1;
}

static void AX_OSAL_SYNC_work_func(struct work_struct *work)
{
	//struct osal_work_struct *ow = container_of(work, struct osal_work_struct, work);
	AX_WORK_T *ow = AX_OSAL_SYNC_find_work(work);
	if (ow != NULL && ow->func != NULL)
		ow->func(ow);
}

int AX_OSAL_SYNC_init_work(AX_WORK_T * work, AX_WORK_FUNC_T func)
{
	struct work_struct *w = NULL;
	struct wq_node *w_node = NULL;
	w = kmalloc(sizeof(struct work_struct), GFP_ATOMIC);
	if (w == NULL) {
		printk("osal_init_work kmalloc failed!\n");
		return -1;
	}

	w_node = kmalloc(sizeof(struct wq_node), GFP_ATOMIC);
	if (w_node == NULL) {
		printk("osal_init_work kmalloc failed!\n");
		kfree(w);
		return -1;
	}
	INIT_WORK(w, AX_OSAL_SYNC_work_func);
	work->work = w;
	work->func = func;
	w_node->osal_work = work;
	w_node->work = w;
	AX_OSAL_LIB_list_add(&(w_node->node), &wq_list);
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_init_work);

int AX_OSAL_SYNC_schedule_work(AX_WORK_T * work)
{
	if (work != NULL && work->work != NULL)
		return (int)schedule_work(work->work);
	else
		return (int)0;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_schedule_work);

void AX_OSAL_SYNC_destroy_work(AX_WORK_T * work)
{
	if (work != NULL && work->work != NULL) {
		AX_OSAL_SYNC_del_work(work->work);
		kfree((struct work_struct *)work->work);
		work->work = NULL;
	}
}

EXPORT_SYMBOL(AX_OSAL_SYNC_destroy_work);

int AX_OSAL_SYNC_init_delayed_work(AX_WORK_T * osal_work, AX_WORK_FUNC_T func)
{
	struct delayed_work *w = NULL;
	struct wq_node *w_node = NULL;
	w = kmalloc(sizeof(struct delayed_work), GFP_ATOMIC);
	if (w == NULL) {
		printk("%s alloc delayed_work failed!\n", __func__);
		return -1;
	}

	w_node = kmalloc(sizeof(struct wq_node), GFP_ATOMIC);
	if (w_node == NULL) {
		printk("%s failed\n", __func__);
		kfree(w);
		return -1;
	}
	INIT_DELAYED_WORK(w, AX_OSAL_SYNC_work_func);
	osal_work->work = w;
	osal_work->func = func;
	w_node->osal_work = osal_work;
	w_node->work = &w->work;
	AX_OSAL_LIB_list_add(&(w_node->node), &wq_list);
	return 0;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_init_delayed_work);

int AX_OSAL_SYNC_schedule_delayed_work(AX_WORK_T * osal_work, unsigned long delay)
{
	if (osal_work != NULL && osal_work->work != NULL)
		return (int)schedule_delayed_work(osal_work->work, delay);
	else
		return (int)0;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_schedule_delayed_work);

int AX_OSAL_SYNC_cancel_delayed_work(AX_WORK_T * osal_work)
{
	if (osal_work != NULL && osal_work->work != NULL)
		return (int)cancel_delayed_work(osal_work->work);
	else
		return (int)0;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_cancel_delayed_work);

int AX_OSAL_SYNC_cancel_delayed_work_sync(AX_WORK_T * osal_work)
{
	if (osal_work != NULL && osal_work->work != NULL)
		return (int)cancel_delayed_work_sync(osal_work->work);
	else
		return (int)0;
}

EXPORT_SYMBOL(AX_OSAL_SYNC_cancel_delayed_work_sync);

void AX_OSAL_SYNC_destroy_delayed_work(AX_WORK_T * osal_work)
{
	if (osal_work != NULL && osal_work->work != NULL) {
		AX_OSAL_SYNC_del_work(osal_work->work);
		kfree((struct work_struct *)osal_work->work);
		osal_work->work = NULL;
	}
}

EXPORT_SYMBOL(AX_OSAL_SYNC_destroy_delayed_work);
