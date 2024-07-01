/*
 * Based on
 * drivers/amlogic/efuse/efuse64.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "generic-efuse-class.h"

#define EFUSE_DEVICE_NAME   "efuse"
#define EFUSE_CLASS_NAME    "efuse"

struct efusekey_info *efusekey_infos = NULL;
int efusekeynum =  -1;

static dev_t efuse_devno;
struct nvmem_device *efuse_nvmem;

static ssize_t __efuse_user_attr_show(char *name, char *buf, int output_type);

#define  DEFINE_EFUEKEY_STR_SHOW_ATTR(keyname)	\
	static ssize_t  keyname##_show(const struct class *cla, \
					  const struct class_attribute *attr,	\
						char *buf)	\
	{	\
		ssize_t ret;	\
		\
		ret = __efuse_user_attr_show(#keyname, buf, 1); \
		return ret; \
	}

#define  DEFINE_EFUEKEY_HEX_SHOW_ATTR(keyname)	\
	static ssize_t  keyname##_show(const struct class *cla, \
					  const struct class_attribute *attr,	\
						char *buf)	\
	{	\
		ssize_t ret;	\
		\
		ret = __efuse_user_attr_show(#keyname, buf, 2); \
		return ret; \
	}

DEFINE_EFUEKEY_HEX_SHOW_ATTR(mac)
DEFINE_EFUEKEY_HEX_SHOW_ATTR(mac_bt)
DEFINE_EFUEKEY_HEX_SHOW_ATTR(mac_wifi)
DEFINE_EFUEKEY_HEX_SHOW_ATTR(sn)
DEFINE_EFUEKEY_HEX_SHOW_ATTR(userdata)
DEFINE_EFUEKEY_STR_SHOW_ATTR(usid)
DEFINE_EFUEKEY_STR_SHOW_ATTR(bid)

static int efuse_read_usr(void *val, size_t bytes, unsigned int *offset)
{
	if (efuse_nvmem)
		return nvmem_device_read(efuse_nvmem, *offset, bytes, val);
	else
		return -EINVAL;
}

int efuse_getinfo(char *item, struct efusekey_info *info)
{
	int i;
	int ret = -1;

	for (i = 0; i < efusekeynum; i++) {
		if (strcmp(efusekey_infos[i].keyname, item) == 0) {
			strcpy(info->keyname, efusekey_infos[i].keyname);
			strcpy(info->alias, efusekey_infos[i].alias);
			info->nvmem = efusekey_infos[i].nvmem;
			info->offset = efusekey_infos[i].offset;
			info->size = efusekey_infos[i].size;
			ret = 0;
			break;
		}
	}

	if (ret < 0) {
		for (i = 0; i < efusekeynum; i++) {
			if (strcmp(efusekey_infos[i].alias, item) == 0) {
				strcpy(info->keyname, efusekey_infos[i].alias);
				strcpy(info->alias, efusekey_infos[i].keyname);
				info->nvmem = efusekey_infos[i].nvmem;
				info->offset = efusekey_infos[i].offset;
				info->size = efusekey_infos[i].size;
				ret = 0;
				break;
			}
		}
	}

	if (ret < 0)
		pr_err("%s item not found.\n", item);
	return ret;
}

static ssize_t __efuse_user_attr_show(char *name, char *buf, int output_type)
{
	char *local_buf;
	ssize_t ret;
	int i;
	struct efusekey_info info;
	char tmp[5];

	if (efuse_getinfo(name, &info) < 0) {
		pr_err("%s is not found\n", name);
		return -EFAULT;
	}

	local_buf = kzalloc(sizeof(char)*(info.size), GFP_KERNEL);
	memset(local_buf, 0, info.size);

	ret = efuse_read_usr(local_buf, info.size, (unsigned int *)&(info.offset));
	if (ret == -1) {
		pr_err("ERROR: efuse read user data fail!\n");
		goto error_exit;
	}
	if (ret != info.size)
		pr_err("ERROR: read %zd byte(s) not %d byte(s) data\n",
			ret, info.size);

	if (output_type == 2) {
		for(i = 0; i < info.size; i++) {
		    memset(tmp, 0, 5);
		    sprintf(tmp, "%02x:", local_buf[i]);
		    strcat(buf, tmp);
		}
		buf[3*info.size - 1] = '\n'; //replace the last ':'
		ret = strlen(buf);
	}
	else if (output_type == 1) {
		for(i = 0; i < info.size; i++) {
		    switch (local_buf[i]) {
			case 0x7f: // DEL
			    local_buf[i] = 0;
			    break;
			case 0x01 ... 0x08:
			case 0x0e ... 0x1f:
			case 0x80 ... 0xff:
			    local_buf[i] = '.';
			    break;
		    }
		}
		ret = sprintf(buf, "%s\n",  local_buf);
	}
	else
		for (i = 0; i < info.size; i++)
			buf[i] = local_buf[i];

error_exit:
	kfree(local_buf);
	return ret;
}

ssize_t efuse_user_attr_show(char *name, char *buf)
{
	return __efuse_user_attr_show(name, buf, 0);
}

CLASS_ATTR_RO(mac);
CLASS_ATTR_RO(mac_bt);
CLASS_ATTR_RO(mac_wifi);
CLASS_ATTR_RO(sn);
CLASS_ATTR_RO(userdata);
CLASS_ATTR_RO(usid);
CLASS_ATTR_RO(bid);

static struct attribute *efuse_class_attrs[] = {
	&class_attr_mac.attr,
	&class_attr_mac_bt.attr,
	&class_attr_mac_wifi.attr,
	&class_attr_sn.attr,
	&class_attr_userdata.attr,
	&class_attr_usid.attr,
	&class_attr_bid.attr,
	NULL,
};
ATTRIBUTE_GROUPS(efuse_class);

static struct class efuse_class = {

	.name = EFUSE_CLASS_NAME,

	.class_groups = efuse_class_groups,

};

int get_efusekey_info(struct nvmem_device *nvmem, struct device *nvmem_dev)
{
	struct device_node *np_key;
	const __be32 *addr;
	int index, len;
	unsigned int eth_mac_size = 0;

	efuse_nvmem = nvmem;

	efusekeynum = 0;
	for_each_child_of_node(nvmem_dev->of_node, np_key) {
		addr = of_get_property(np_key, "reg", &len);
		if (!addr || (len < 2 * sizeof(u32))) {
			continue;
		}

		addr++;

		if (!strcmp(np_key->name, "eth_mac")) {
			eth_mac_size = be32_to_cpup(addr);

			if (eth_mac_size > 6)
				efusekeynum++;
			if (eth_mac_size > 6*2)
				efusekeynum++;
			if (eth_mac_size > 6*3)
				efusekeynum++;
		}
		efusekeynum++;
	}

	pr_info("efusekeynum: %d\n", efusekeynum);

	if (efusekeynum > 0) {
		efusekey_infos = kzalloc((sizeof(struct efusekey_info))
			*efusekeynum, GFP_KERNEL);
		if (!efusekey_infos) {
			pr_err("%s efuse_keys alloc err\n", __func__);
			return -1;
		}
	}

	index = 0;
	for_each_child_of_node(nvmem_dev->of_node, np_key) {
		addr = of_get_property(np_key, "reg", &len);
		if (!addr || (len < 2 * sizeof(u32))) {
			continue;
		}

		strcpy(efusekey_infos[index].keyname, np_key->name);
		strcpy(efusekey_infos[index].alias, np_key->name);
		efusekey_infos[index].nvmem = nvmem;
		efusekey_infos[index].offset = be32_to_cpup(addr++);
		efusekey_infos[index].size = be32_to_cpup(addr);

		if (!strcmp(efusekey_infos[index].keyname, "ethernet_mac_address") ||
		    !strcmp(efusekey_infos[index].keyname, "ethernet-mac-address") ||
		    !strcmp(efusekey_infos[index].keyname, "eth-mac"))
			strcpy(efusekey_infos[index].keyname, "eth_mac");

		if (!strcmp(efusekey_infos[index].keyname, "eth_mac")) {
			if (eth_mac_size > 6)
                                index++;
			if (eth_mac_size > 6*2)
				index++;
			if (eth_mac_size > 6*3)
				index++;
		}

		index++;
	}

	for (index = 0; index < efusekeynum; index++) {
		if (!strcmp(efusekey_infos[index].keyname, "sn"))
			strcpy(efusekey_infos[index].alias, "usid");
		else if (!strcmp(efusekey_infos[index].keyname, "usid"))
			strcpy(efusekey_infos[index].alias, "sn");
		else if (!strcmp(efusekey_infos[index].keyname, "userdata"))
			strcpy(efusekey_infos[index].alias, "bid");
		else if (!strcmp(efusekey_infos[index].keyname, "bid"))
			strcpy(efusekey_infos[index].alias, "userdata");
		else if (!strcmp(efusekey_infos[index].keyname, "eth_mac")) {
			strcpy(efusekey_infos[index].alias, "mac");

			if (efusekey_infos[index].size > 6)
				efusekey_infos[index].size = 6;

			if (eth_mac_size > 6*1) {
				efusekey_infos[index+1] = efusekey_infos[index];
				strcpy(efusekey_infos[index+1].keyname, "bt_mac");
				efusekey_infos[index+1].offset += 6*1;
				efusekey_infos[index+1].size = eth_mac_size-6*1;

				if (efusekey_infos[index+1].size > 6)
					efusekey_infos[index+1].size = 6;
			}

			if (eth_mac_size > 6*2) {
				efusekey_infos[index+2] = efusekey_infos[index];
				strcpy(efusekey_infos[index+2].keyname, "wifi_mac");
				efusekey_infos[index+2].offset += 6*2;
				efusekey_infos[index+2].size = eth_mac_size-6*2;

				if (efusekey_infos[index+2].size > 6)
					efusekey_infos[index+2].size = 6;
			}

                        if (eth_mac_size > 6*3) {
                                efusekey_infos[index+3] = efusekey_infos[index];
                                strcpy(efusekey_infos[index+3].keyname, "rsvd_mac");
                                efusekey_infos[index+3].offset += 6*3;
                                efusekey_infos[index+3].size = eth_mac_size-6*3;
                        }
		}
		else if (!strcmp(efusekey_infos[index].keyname, "bt_mac"))
                        strcpy(efusekey_infos[index].alias, "mac_bt");
		else if (!strcmp(efusekey_infos[index].keyname, "wifi_mac"))
                        strcpy(efusekey_infos[index].alias, "mac_wifi");
		else if (!strcmp(efusekey_infos[index].keyname, "rsvd_mac"))
			 strcpy(efusekey_infos[index].alias, "mac_rsvd");

		pr_info("efusekeyname: %15s\toffset: %5d\tsize: %5d\n",
			efusekey_infos[index].alias,
			efusekey_infos[index].offset,
			efusekey_infos[index].size);
	}

	return 0;
}

int generic_efuse_class_probe(struct platform_device *pdev, struct nvmem_device *nvmem)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device *devp;

	get_efusekey_info(nvmem, dev);

	ret = alloc_chrdev_region(&efuse_devno, 0, 1, EFUSE_DEVICE_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "efuse: failed to allocate major number\n");
		ret = -ENODEV;
		goto out;
	}

	ret = class_register(&efuse_class);
	if (ret)
		goto error1;

	devp = device_create(&efuse_class, NULL, efuse_devno, NULL, "efuse");
	if (IS_ERR(devp)) {
		dev_err(&pdev->dev, "failed to create device node\n");
		ret = PTR_ERR(devp);
		goto error4;
	}
	dev_dbg(&pdev->dev, "device %s created\n", EFUSE_DEVICE_NAME);

	dev_info(&pdev->dev, "probe OK!\n");
	return 0;

error4:
	class_unregister(&efuse_class);
error1:
	unregister_chrdev_region(efuse_devno, 1);
out:
	return ret;
}
EXPORT_SYMBOL(generic_efuse_class_probe);

int generic_efuse_class_remove(struct platform_device *pdev)
{
	unregister_chrdev_region(efuse_devno, 1);
	device_destroy(&efuse_class, efuse_devno);
	class_unregister(&efuse_class);
	return 0;
}
EXPORT_SYMBOL(generic_efuse_class_remove);
