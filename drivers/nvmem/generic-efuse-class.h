#ifndef __GENERIC_EFUSE_CLASS_H
#define __GENERIC_EFUSE_CLASS_H

struct efusekey_info {
	char keyname[32];
	char alias[32];
	struct nvmem_device *nvmem;
	unsigned int offset;
	unsigned int size;
};

extern int efusekeynum;

int efuse_getinfo(char *item, struct efusekey_info *info);
ssize_t efuse_user_attr_show(char *name, char *buf);

int generic_efuse_class_probe(struct platform_device *pdev, struct nvmem_device *nvmem);
int generic_efuse_class_remove(struct platform_device *pdev);

#endif
