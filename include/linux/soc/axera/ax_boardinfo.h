#ifndef _AX_BOARDINFO_H_
#define _AX_BOARDINFO_H_
typedef enum chip_type {
	NONE_CHIP_TYPE = 0x0,
	AX620Q_CHIP = 0x1,
	AX620QX_CHIP = 0x2,
	AX630C_CHIP = 0x4,
	AX620E_CHIP_MAX = 0x5,
} chip_type_e;

typedef enum ax630c_board_type {
	PHY_AX630C_EVB_V1_0 = 0,
	PHY_AX630C_DEMO_V1_0 = 1,
	PHY_AX630C_DEMO_DDR3_V1_0 = 3,
	PHY_AX630C_SLT_V1_0 = 8,
	PHY_AX630C_DEMO_V1_1 = 6,
	PHY_AX630C_DEMO_LP4_V1_0 = 12,
	PHY_AX630C_DEMO_LP4_V1_1 = 14,
} ax630c_board_type_e;

typedef enum ax620q_board_type {
	PHY_AX620Q_LP4_EVB_V1_0 = 4,
	PHY_AX620Q_LP4_DEMO_V1_0 = 5,
	PHY_AX620Q_LP4_SLT_V1_0 = 10,
	PHY_AX620Q_LP4_DEMO_V1_1 = 11,
	PHY_AX620Q_LP4_38BOARD_V1_0 = 14,
	PHY_AX620Q_LP4_MINION_BOARD = 15,
} ax620q_board_type_e;

typedef enum board_type {
	AX630C_EVB_V1_0 = 0,
	AX630C_DEMO_V1_0,
	AX630C_SLT_V1_0,
	AX620Q_LP4_EVB_V1_0,
	AX620Q_LP4_DEMO_V1_0,
	AX620Q_LP4_SLT_V1_0,
	AX630C_DEMO_V1_1,
	AX620Q_LP4_DEMO_V1_1,
	AX630C_DEMO_LP4_V1_0,
	AX620Q_LP4_38BOARD_V1_0,
	AX620Q_LP4_MINION_BOARD,
	//AX630C_DEMO_DDR3_V1_0,
	AX630C_DEMO_LP4_V1_1,
	AX620E_BOARD_MAX,
} board_type_e;

#define MISC_INFO_ADDR 0x740 //iram0 addr
typedef struct misc_info {
	u32 pub_key_hash[8];
	u32 aes_key[8];
	u32 board_id;
	u32 chip_type;
	u32 uid_l;
	u32 uid_h;
	u32 thm_vref;
	u32 thm_temp;
	u16 bgs;
	u16 trim;
	u32 phy_board_id;
} misc_info_t;
u32 ax_info_get_board_id(void);
#endif