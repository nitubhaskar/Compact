/*
 *   Copyright (C) 2011,  Samsung Electronics Co. Ltd. All Rights Reserved.
 *   Written by Advanced S/W R&D Group 3, Mobile Communication Division.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/param.h>
#include <mach/irqs.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <asm/unaligned.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/stat.h>
#include "bq27425.h"

struct bq27425_info bq27425;

static int bq27425_subcmd_i2c(u8 reg, unsigned int subcmd);
static int bq27425_read_i2c(u8 reg, unsigned int *rt_value);
static int bq27425_read_burst_i2c(u8 reg, u8 *data, int len);
static int bq27425_write_burst_i2c(u8 *packet, int len);

int get_bq27425_dffs_version(unsigned int* version)
{
	int ret = 0;
	u8 packet[2];
	u8 data[4];

	packet[0] = BQ27425_EXTCMD_DFDCNTL, packet[1] = 0x00;	
	ret = bq27425_write_burst_i2c(packet, 2);	
	udelay(BQ27425_I2C_UDELAY);	
	if (ret < 0)
		goto error;
	
	packet[0] = BQ27425_EXTCMD_DFCLS, packet[1] = 0x3A;	
	ret = bq27425_write_burst_i2c(packet, 2);
	udelay(BQ27425_I2C_UDELAY);	
	if (ret < 0)
		goto error;
	
	packet[0] = BQ27425_EXTCMD_DFBLK, packet[1] = 0x00;	
	ret = bq27425_write_burst_i2c(packet, 2);
	udelay(BQ27425_I2C_UDELAY);	
	if (ret < 0)
		goto error;

	ret = bq27425_read_burst_i2c(BQ27425_EXTCMD_DFD, data, 4);
	udelay(BQ27425_I2C_UDELAY);	
	if (ret < 0)
		goto error;

	*version = get_unaligned_le32(data);
	
	return 0;
	
error:
	printk( "%s: error reading dffs, code: %d\n", __func__, ret);
	return ret;
}

int bq27425_reset(void)
{
	int ret = 0;
	
	ret = bq27425_subcmd_i2c(BQ27425_REG_CNTL, BQ27425_SUBCMD_RESET);
	udelay(BQ27425_I2C_UDELAY);
	printk("%s: reset fuel gauge\n", __func__);

	if (ret < 0) {
		printk( "%s: error reading subcmd, code: %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

int get_bq27425_battery_subdata(u8 reg, unsigned int subcmd, unsigned int* val)
{
	int ret = 0;

	ret = bq27425_subcmd_i2c(reg, subcmd);
	if (ret < 0) {
		printk( "%s: error reading subcmd, code: %d\n", __func__, ret);
		return ret;
	}
	udelay(BQ27425_I2C_UDELAY);
	ret = bq27425_read_i2c(reg, val);
	if (ret < 0) {
		printk( "%s: error reading value, code: %d\n", __func__, ret);
		return ret;
	}
	udelay(BQ27425_I2C_UDELAY);
	
	return ret;
}

int get_bq27425_battery_data(u8 reg, unsigned int* val)
{
	int ret = 0;

	ret = bq27425_read_i2c(reg, val);
	if (ret < 0) {
		printk( "%s: error reading value, code: %d\n", __func__, ret);
		return ret;
	}
	udelay(BQ27425_I2C_UDELAY);

	switch(reg)
	{
	case BQ27425_REG_TEMP:
		*val += ZERO_DEGREE_CELSIUS_IN_TENTH_KELVIN;
	default:
		break;
	}

	return ret;
}

static int bq27425_read_burst_i2c(u8 reg, u8 *data, int len)
{
	struct i2c_client *client = bq27425.client;
	struct i2c_msg msg;
	int err = 0;
	int i;

	if (!client->adapter)
		return -ENODEV;

	for(i=0; i<len; i++)	
		*(data+i) = 0;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = &reg;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err >= 0) {
		msg.len = len;
		msg.flags = I2C_M_RD;
		msg.buf = data;
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err >= 0) 
			return 0;
	}

	return err;
}

static int bq27425_write_burst_i2c(u8 *packet, int len)
{
	struct i2c_client *client = bq27425.client;
	struct i2c_msg msg;
	int err = 0;

	if (!client->adapter)
		return -ENODEV;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = packet;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err < 0)
		return -EIO;

	return 0;
}

static int bq27425_subcmd_i2c(u8 reg, unsigned int subcmd)
{
	struct i2c_client *client = bq27425.client;
	struct i2c_msg msg;
	unsigned char data[3];
	int err = 0;

	if (!client->adapter)
		return -ENODEV;

	memset(data, 0, sizeof(data));
	data[0] = reg;
	data[1] = subcmd & 0x00FF;
	data[2] = (subcmd & 0xFF00) >> 8;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err < 0)
		return -EIO;

	return 0;
}

static int  bq27425_read_i2c(u8 reg, unsigned int *rt_value)
{
	struct i2c_client *client = bq27425.client;
	struct i2c_msg msg;
	unsigned char data[2];
	int err = 0;

	if (!client->adapter)
		return -ENODEV;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = data;

	memset(data, 0, sizeof(data));
	data[0] = reg;
	err = i2c_transfer(client->adapter, &msg, 1);

	if (err >= 0) {
		msg.len = 2;
		msg.flags = I2C_M_RD;
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err >= 0) 
		{
			*rt_value = get_unaligned_le16(data);
			return 0;
		}
	}

	return err;
}

static irqreturn_t fuel_isr(int irq, void *dev_id)
{	
	printk("%s: LOW battery interrupt from fuel gauge\n", __func__);

	return IRQ_HANDLED;
}

static int bq27425_battery_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int retval = 0;
	int status = 0, type = 0, fw_ver = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		printk("%s: i2c_check_functionality error\n", __func__);
		return -ENODEV;
	}

	bq27425.client = client;
	bq27425.id = id;
	bq27425.irq = client->irq;	
	i2c_set_clientdata(client, &bq27425);

	retval = request_threaded_irq(bq27425.irq, NULL, fuel_isr,  (IRQF_ONESHOT |IRQF_NO_SUSPEND|IRQF_TRIGGER_FALLING), "fuel_irq",  &bq27425);
	if(retval < 0)
		goto err1;

	get_bq27425_battery_subdata(BQ27425_REG_CNTL, BQ27425_SUBCMD_CTNL_STATUS, &status);
	get_bq27425_battery_subdata(BQ27425_REG_CNTL, BQ27425_SUBCMD_DEVCIE_TYPE, &type);
	get_bq27425_battery_subdata(BQ27425_REG_CNTL, BQ27425_SUBCMD_FW_VER, &fw_ver);

	printk("%s: DEVICE_TYPE is 0x%02X, FIRMWARE_VERSION is 0x%02X\n", __func__, type, fw_ver);
	printk("%s: Complete bq27425 configuration 0x%02X\n", __func__, status);

err1:
	return retval;
}

static int bq27425_battery_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int bq27425_battery_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id bq27425_id[] = {
	{ "bq27425", 0},
	{},
};

static struct i2c_driver bq27425_battery_driver = {
	.driver		= {
			.name = "bq27425",
	},
	.probe		= bq27425_battery_probe,
	.remove		= bq27425_battery_remove,
	.suspend	= bq27425_battery_suspend,
	.id_table	= bq27425_id,
};

static int __init bq27425_battery_init(void)
{
	int ret;

	ret = i2c_add_driver(&bq27425_battery_driver);
	if (ret)
		printk("%s: Unable to register BQ27425 driver\n", __func__);

	return ret;
}

static void __exit bq27425_battery_exit(void)
{
	i2c_del_driver(&bq27425_battery_driver);
}

module_init(bq27425_battery_init);
module_exit(bq27425_battery_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Samsung");
MODULE_DESCRIPTION("BQ27425 battery monitor driver");
