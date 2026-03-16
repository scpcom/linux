/* LT7911: GPL-2.0+
 *
 * linux/drivers/misc/lt7911_manage.h
 *
 * Sipeed NanoAgent Management Driver
 *
 * Copyright (c) 2025-2026 Sipeed Technology Co., Ltd.
 */

#ifndef LT7911_MANAGE_H
#define LT7911_MANAGE_H

// #define RE_WRITE_VERSION            // Uncomment to enable version re-write function

// NanoAgent GPIO
#define NANO_AGENT_INT_PIN          46      // LT7911UXC INIT_Pin/GPIO5 GPIO1_A14
#define NANO_AGENT_PWR_PIN          47      // LT7911UXC PWR_Pin GPIO1_A15

#define I2C_BUS                     0       // I2C BUS Number
#define LT7911_ADDR                 0x2b    // LT7911D I2C Addr
#define Int_Action                  IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING
#define REG_ADDR                    0xff    // Target register address
#define WRITE_DATA                  0x80    // To write data command

// LT7911 Register
#define I2C_ADDRESS                 0x2b    // I2C device address (adjust according to actual device)
#define LT7911_REG_OFFSET           0xFF    // LT7911UXC register offset address
#define LT7911_SYS_OFFSET           0x80    // LT7911UXC register offset address
#define LT7911_SYS4_OFFSET          0xA0    // LT7911UXC register offset address
#define LT7911D_HDMI_INFO_OFFSET    0xD2    // LT7911C HDMI info register offset
#define LT7911D_AUDIO_INFO_OFFSET   0xD1    // LT7911C audio info register offset
#define LT7911D_CSI_INFO_OFFSET     0xC2    // LT7911C CSI info register offset

#define EDID_BUFFER_SIZE            256     // Maximum supported bytes
#define LT7911D_WR_SIZE           32      // LT7911UXC max read/write bytes per operation

#define NORMAL_RES                  0
#define NEW_RES                     1
#define UNSUPPORT_RES               2
#define UNKNOWN_RES                 3
#define ERROR_RES                   4

// SOC Register
#define REG_REMAP_SIZE              0x1000

// proc file names
#define PROC_LT7911_DIR             "lt7911_info"
#define PROC_VIDEO_STATUS           "status"
#define PROC_VIDEO_WIDTH            "width"
#define PROC_VIDEO_HEIGHT           "height"
#define PROC_VIDEO_PWR              "power"
#define PROC_VIDEO_FPS              "fps"
#define PROC_VIDEO_HDCP             "hdcp"
#define PROC_AUDIO_SAMPLE_RATE      "asr"
#define PROC_VIDEO_EDID             "edid"
#define PROC_VIDEO_EDID_SNAPSHOT    "edid_snapshot"
#define PROC_VERSION                "version"

#endif /* __LT7911_MANAGE_H */
