#ifndef _AX_PINMUX_H_
#define _AX_PINMUX_H_

#include <linux/soc/axera/ax_boardinfo.h>

#define DPHYTX_BASE           0x230A000UL
#define DPHY_REG_LEN          0x1000
#define DPHYTX_SW_RST_SET     0x46000B8
#define DPHYTX_SW_RST_SHIFT   BIT(6)
#define DPHYTX_MIPI_EN        0x23F110C
#define PINMUX_FUNC_SEL       GENMASK(18, 16)

#define REG_REMAP_SIZE        0x1000
struct pinmux {
	unsigned int *data;
	unsigned int size;
};

#endif