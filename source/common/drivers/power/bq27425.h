/*
 *   Copyright (C) 2011,  Samsung Electronics Co. Ltd. All Rights Reserved.
 *   Written by Advanced S/W R&D Group 3, Mobile Communication Division.
 *
 */
 
#ifndef __BQ27425__
#define __BQ27425__

/* Bq27425 standard data commands */
#define BQ27425_REG_CNTL		0x00
#define BQ27425_REG_TEMP		0x02
#define BQ27425_REG_VOLT		0x04
#define BQ27425_REG_FLAGS		0x06
#define BQ27425_REG_NAC		0x08
#define BQ27425_REG_FAC		0x0A
#define BQ27425_REG_RM			0x0C
#define BQ27425_REG_FCC		0x0E
#define BQ27425_REG_AI			0x10
#define BQ27425_REG_SI			0x12
#define BQ27425_REG_MLI		0x14
#define BQ27425_REG_AP			0x18
#define BQ27425_REG_SOC		0x1C
#define BQ27425_REG_ITEMP		0x1E
#define BQ27425_REG_SOH		0x20

/* Control subcommands */
#define BQ27425_SUBCMD_CTNL_STATUS  		0x0000
#define BQ27425_SUBCMD_DEVCIE_TYPE		0x0001
#define BQ27425_SUBCMD_FW_VER			0x0002
#define BQ27425_SUBCMD_HW_VER  			0x0003
#define BQ27425_SUBCMD_PREV_MACWRITE	0x0007
#define BQ27425_SUBCMD_BAT_INSERT		0x000C
#define BQ27425_SUBCMD_BAT_REMOVE		0x000D
#define BQ27425_SUBCMD_SET_FULLSLEEP		0x0010
#define BQ27425_SUBCMD_SET_HIBERNATE		0x0011
#define BQ27425_SUBCMD_CLEAR_HIBERNATE	0x0012
#define BQ27425_SUBCMD_SET_CFGUPDATE	0x0013
#define BQ27425_SUBCMD_SEALED				0x0020
#define BQ27425_SUBCMD_RESET				0x0041
#define BQ27425_SUBCMD_SOFT_RESET		0x0042

/* Extended commands */
#define BQ27425_EXTCMD_OPCFG		0x3A
#define BQ27425_EXTCMD_DCAP		0x3C
#define BQ27425_EXTCMD_DFCLS		0x3E
#define BQ27425_EXTCMD_DFBLK		0x3F
#define BQ27425_EXTCMD_DFD		0x40
#define BQ27425_EXTCMD_DFDCKS		0x60
#define BQ27425_EXTCMD_DFDCNTL	0x61
#define BQ27425_EXTCMD_DNAMELEN	0x62
#define BQ27425_EXTCMD_DNAME		0x63

/* ETC */
#define ZERO_DEGREE_CELSIUS_IN_TENTH_KELVIN   (-2731)
#define BQ27425_INIT_DELAY   ((HZ)*2)
#define BQ27425_I2C_UDELAY		(75)

struct bq27425_info
{
	struct i2c_client		*client;
	const struct i2c_device_id	*id;
	
	int				irq;
};

extern int get_bq27425_battery_data(u8 reg, unsigned int* val);
extern int get_bq27425_battery_subdata(u8 reg, unsigned int subcmd, unsigned int* val);
extern int bq27425_reset(void);
extern int get_bq27425_dffs_version(unsigned int* version);

#endif
