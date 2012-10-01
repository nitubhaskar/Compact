/*
 * AT42QT602240/ATMXT224 Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

/*
 * ATmega168 MSI Touchscreen driver
 *
 * Copyright (C) 2011 Broadcom Communications Co.Ltd
 * Author: yongqiang wang <yongqiang.wang@broadcom.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 * 2011.08.03 add file_operation, to support TP firmware upgrade in user space
 * 2011.08.04 add early suspend
 * 2011.08.08 add eeprom read and write entry to touch drvier
 */

/* #define DEBUG */
#include <linux/device.h>

#define USE_BRCM_WORK_QUEUE
#define DEBUG_PEN_STATUS
//#define TP_FROZEN


#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/i2c/brcmmsi_atmega168_ts.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/earlysuspend.h>
#include <linux/gpio.h>

#define I2C_MAJOR 123
#define  BOOTLOADER_MODE	7
#define  CALIBRATION_FLAG	1
#define  NORMAL_MODE		8

static unsigned char status_reg = 0;

/* Version */
#define  ATMEGA168_MSI_VER_20			20
#define  ATMEGA168_MSI_VER_21			21
#define  ATMEGA168_MSI_VER_22			22
#define  BOOTLOADER_MODE 7
#define  ENABLE_IRQ         10
#define  DISABLE_IRQ        11
#define  TPRESET         12
#define  TPEEWRITE    13
#define  TPEEREAD    14
#define VERSION_FLAG 15


/* Slave addresses */
#define ATMEGA168_MSI_APP_MODE		0x5c
#define ATMEGA168_MSI_BOOT_MODE		0x5d

/* Firmware */



#define reset
extern int atmega168_msi_pen_reset(void);
extern int atmega168_msi_pen_down_state(void);

#define ATMEGA168_MSI_FWRESET_TIME		100	/* msec */


/*Power mode*/
#define ATMEGA168_MSI_PWR_ACTIVE 0x0
#define ATMEGA168_MSI_PWR_SLEEP  0x1
#define ATMEGA168_MSI_PWR_DSLEEP 0x2
#define ATMEGA168_MSI_PWR_FREEZE 0x3
#define ATMEGA168_PWR_IDLE_PERIOD (0<<7|1<<6|1<<5)
#define ATMEGA168_PWR_ENABLE_SLEEP (1<<2)
#define I2C_MINORS 256
#define TP_OFFSET_X 5
#define TP_OFFSET_Y 10

struct i2c_dev
{
	struct list_head list;
	struct i2c_adapter *adap;
	struct device *dev;
};
static struct class *i2c_dev_class;
static LIST_HEAD( i2c_dev_list);
static DEFINE_SPINLOCK( i2c_dev_list_lock);
unsigned char data2eep[3],op2eep[2];
static char tp_reg = 0;





static struct i2c_dev *i2c_dev_get_by_minor(unsigned index)
{
	struct i2c_dev *i2c_dev;
	i2c_dev = NULL;

	spin_lock(&i2c_dev_list_lock);
	list_for_each_entry(i2c_dev, &i2c_dev_list, list)
	{
		if (i2c_dev->adap->nr == index)
		goto found;
	}
	i2c_dev = NULL;
	found: spin_unlock(&i2c_dev_list_lock);
	return i2c_dev;
}

static struct i2c_dev *get_free_i2c_dev(struct i2c_adapter *adap)
{
	struct i2c_dev *i2c_dev;

	if (adap->nr >= I2C_MINORS)
	{
		printk(KERN_ERR "i2c-dev: Out of device minors (%d)\n",
				adap->nr);
		return ERR_PTR(-ENODEV);
	}

	i2c_dev = kzalloc(sizeof(*i2c_dev), GFP_KERNEL);
	if (!i2c_dev)
		return ERR_PTR(-ENOMEM);
	i2c_dev->adap = adap;

	spin_lock(&i2c_dev_list_lock);
	list_add_tail(&i2c_dev->list, &i2c_dev_list);
	spin_unlock(&i2c_dev_list_lock);
	return i2c_dev;
}

#ifdef	VKEY_SUPPORT
static void brcmmsi_atmega168_send_key_event(struct input_dev *input, unsigned int code)
{
	input_report_key(input, code, 1);
	input_sync(input);

	input_report_key(input, code, 0);
	input_sync(input);
}
#endif

static int brcmmsi_atmega168_input_touchevent(struct atmega168_msi_data *data)
{
	struct atmega168_msi_data *tsdata = data;
	struct input_dev *input = tsdata->input_dev;

	u8 touching, oldtouching;
	u16 posx1, posy1, posx2, posy2;
	u8 rdbuf[10], wrbuf[1];
	int ret;
    int z = 50;
	int w = 15;
	memset(wrbuf, 0, sizeof(wrbuf));
	memset(rdbuf, 0, sizeof(rdbuf));
 
	wrbuf[0] = 0;
    mutex_lock(&data->lock);	
	ret = i2c_master_send(tsdata->client, wrbuf, 1);
	mutex_unlock(&data->lock);
	if (ret != 1) 
	{
		tsdata->pen_down = 0;
		//enable_irq(data->irq);
		dev_err(&tsdata->client->dev, "Unable to write to i2c touchscreen!\n");
		goto out;
	}
    mutex_lock(&data->lock);
	ret = i2c_master_recv(tsdata->client, rdbuf, sizeof(rdbuf));
    mutex_unlock(&data->lock);
	if (ret != sizeof(rdbuf)) 
	{
		dev_err(&tsdata->client->dev, "Unable to read i2c page!\n");
		goto out;
	}

	touching = rdbuf[ATMEGA168_TOUCING];
	oldtouching = rdbuf[1];
	posx1 = ((rdbuf[ATMEGA168_POSX_HIGH] << 8) | rdbuf[ATMEGA168_POSX_LOW]);
	posy1 = ((rdbuf[ATMEGA168_POSY_HIGN] << 8) | rdbuf[ATMEGA168_POSY_LOW]);
	posx2 = ((rdbuf[ATMEGA168_POSX2_HIGH] << 8) | rdbuf[ATMEGA168_POSX2_LOW]);
	posy2 = ((rdbuf[ATMEGA168_POSY2_HIGN] << 8) | rdbuf[ATMEGA168_POSY2_LOW]);
    if (!(touching))
	{
		z = 0;
		w = 0;
	}
#ifdef ADD_TP_OFFSET
	if(posx1 > TP_OFFSET_X) 
	posx1 -= TP_OFFSET_X;
	if(posy1 > TP_OFFSET_Y)
	posy1 -= TP_OFFSET_Y;
	if(posx2 > TP_OFFSET_X)
	posx2 -= TP_OFFSET_X;
	if(posy2 > TP_OFFSET_Y)
	posy2 -= TP_OFFSET_Y;
#endif

    if (touching) 
	{
		tsdata->pen_down = 1;
   
        input_report_abs(input, ABS_MT_TOUCH_MAJOR, z);
	    input_report_abs(input, ABS_MT_WIDTH_MAJOR, w);
	    input_report_abs(input, ABS_MT_POSITION_X, posx1);
	    input_report_abs(input, ABS_MT_POSITION_Y, posy1);
	    input_mt_sync(input);
		if(touching == 2)
		{
			input_report_abs(input, ABS_MT_TOUCH_MAJOR, z);
			input_report_abs(input, ABS_MT_WIDTH_MAJOR, w);
			input_report_abs(input, ABS_MT_POSITION_X, posx2);
			input_report_abs(input, ABS_MT_POSITION_Y, posy2);
			input_mt_sync(input);

		}

		input_sync(input);
	} 
	else 
	{
		if (tsdata->pen_down) 
		{	/* pen up */
			input_report_abs(input, ABS_MT_TOUCH_MAJOR, 0);
			input_mt_sync(input);
			input_sync(input);
			tsdata->pen_down = 0;
		}
	}
 
    ret = 0;
   

#ifdef	VKEY_SUPPORT
   u16 usCount;
	
   for (usCount = 0; usCount < 4;    usCount++) 
   {
		if ((posy1 > (tsdata->pk_data + usCount)->up)
		 && (posy1 < (tsdata->pk_data + usCount)->down)
		 && (posx1 > (tsdata->pk_data + usCount)->left)
		 && (posx1 < (tsdata->pk_data + usCount)->right)) 
		{
			brcmmsi_atmega168_send_key_event(input,(tsdata->pk_data+usCount)->code);
			printk(" Send Printkey %d  \r\n", (tsdata->pk_data+usCount)->code); 
			tsdata->pen_down = true;
			goto done;
		}
	}
#endif	

	return 0;

out:
	return -EINVAL;
}

#ifndef USE_BRCM_WORK_QUEUE

static irqreturn_t atmega168_msi_interrupt(int irq, void *dev_id)
{
	struct atmega168_msi_data *data = dev_id;

	do {
		brcmmsi_atmega168_input_touchevent(data);
		data->attb_pin = data->pdata->attb_read_val();
		if (data->attb_pin == ATTB_PIN_LOW)
			wait_event_timeout(data->wait, 1,
					msecs_to_jiffies(20));
	} while (!data->attb_pin);
    return IRQ_HANDLED;
}
#endif

#ifdef USE_BRCM_WORK_QUEUE

#define TS_POLL_DELAY	(0) 	/* ms delay between irq & sample */
#define to_delayed_work(_work) container_of(_work, struct delayed_work, work)


static irqreturn_t atmega168_msi_touch_interrupt (int irq, void *dev_id)
{
	struct atmega168_msi_data *p_data = dev_id;
	//if(!p_data->pen_down)
    {
    	//disable_irq_nosync(p_data->irq);
	    schedule_delayed_work(&p_data->work, msecs_to_jiffies(TS_POLL_DELAY));
	}
	return IRQ_HANDLED;
}


static void atmega168_msi_touch_work (struct work_struct *work)
{	
    int ret = -1;
	struct atmega168_msi_data *p_data = container_of(to_delayed_work(work), struct atmega168_msi_data, work);	
	/* Peform the actual read and send the events to input sub-system */	
	ret = brcmmsi_atmega168_input_touchevent(p_data);   
	//if(p_data->pen_down)
	if(ret < 0)
		schedule_delayed_work(&p_data->work, msecs_to_jiffies(5));	
	//else	/* Re-enable the interrupts from the device */	   
	//	enable_irq(p_data->irq);
}

#endif


#ifdef TP_FROZEN
static int atmega168_msi_initialize(struct atmega168_msi_data *data)
{
	
	char cmd[4];

	cmd[0] = 0x14;
	cmd[1] = 0x00;
	cmd[2] = 0x09;
	mutex_lock(&data->lock);
	i2c_master_send(data->client, cmd, 3);
	mutex_unlock(&data->lock);
	return 0;
}
#endif

static int atmega168_msi_connect(struct atmega168_msi_data *data)
{
	struct i2c_client *client = data->client;
    char cmd[] = {0x14,0x00};
	int det_cnt = 0;
	int ret = -1;
	
	while(ret < 0 && det_cnt < 20)
	{
        mutex_lock(&data->lock);
		ret = i2c_master_send(client, cmd, 2); /* detect */
        mutex_unlock(&data->lock);
		mdelay(2);
		det_cnt++;
    }
	return ret;
}

static int atmega168_msi_read_reg(struct i2c_client *client,
			       u16 reg, u16 len, void *val)
{
	struct i2c_msg xfer[2];
	u8 buf[2];

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 2;
	xfer[0].buf = buf;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = val;

	if (i2c_transfer(client->adapter, xfer, 2) != 2) {
		dev_err(&client->dev, "%s: i2c transfer failed\n", __func__);
		return -EIO;
	}
	return 0;
}


static ssize_t atmega168_msi_tpreset(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{

	struct atmega168_msi_data *p_data =
		   (struct atmega168_msi_data *)dev_get_drvdata(dev);
	p_data->pdata->platform_reset();
    mdelay(60);
	return 0;
}

static DEVICE_ATTR(tpreset, 0664, NULL, atmega168_msi_tpreset);


#if defined(DEBUG_PEN_STATUS)
static ssize_t pen_status_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
        struct atmega168_msi_data *p_data =
            (struct atmega168_msi_data *)dev_get_drvdata(dev);
        return sprintf(buf, "%d\n", p_data->pen_down);
}
static DEVICE_ATTR(pen_status, 0644, pen_status_show, NULL);
#endif


static ssize_t tpversion_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
        struct atmega168_msi_data *p_data =
            (struct atmega168_msi_data *)dev_get_drvdata(dev);

        return sprintf(buf, "0x%x\n", p_data->fw_version);
}
static DEVICE_ATTR(tp_version, 0644, tpversion_show, NULL);


static ssize_t tp_frozen_func(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
    struct atmega168_msi_data *p_data =
            (struct atmega168_msi_data *)dev_get_drvdata(dev);
	
	u8 cmd[2];
    cmd[0] = 0x14;
	cmd[1] = 0x03;
    mutex_lock(&p_data->lock);
	i2c_master_send(p_data->client, cmd, 2);
    mutex_unlock(&p_data->lock);
    return 0;
        
}
static DEVICE_ATTR(tp_frozen, 0644, NULL, tp_frozen_func);


static ssize_t tp_active_func(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct atmega168_msi_data *p_data =
            (struct atmega168_msi_data *)dev_get_drvdata(dev);
    u8 cmd[2];
    cmd[0] = 0x14;
	cmd[1] = 0x0;
    mutex_lock(&p_data->lock);
	i2c_master_send(p_data->client, cmd, 2);
    mutex_unlock(&p_data->lock);
	return 0;
}
static DEVICE_ATTR(tp_active, 0644, NULL, tp_active_func);


static struct attribute *atmega168_msi_attrs[] = 
{
	&dev_attr_tpreset.attr,
	&dev_attr_tp_version.attr,
#if defined(DEBUG_PEN_STATUS)
	&dev_attr_pen_status.attr,
#endif
    &dev_attr_tp_active.attr,
    &dev_attr_tp_frozen.attr,
	NULL
};

static const struct attribute_group atmega168_msi_attr_group = {
	.attrs = atmega168_msi_attrs,
};


#ifdef CONFIG_PM

static int atmega168_msi_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct atmega168_msi_data *data = i2c_get_clientdata(client);
   	char cmd[2];
	client->addr = 0x5c;
    cmd[0] = 0x14;
#ifdef TP_FORZEN	
	cmd[1] = 0x03;
#else
    cmd[1] = 0x02;
#endif
    mutex_lock(&data->lock);
	i2c_master_send(data->client, cmd, 2);
    mutex_unlock(&data->lock);
    //reg = ATMEGA168_PWR_IDLE_PERIOD | ATMEGA168_PWR_ENABLE_SLEEP | ATMEGA168_MSI_PWR_SLEEP;
    cancel_work_sync(&data->work.work);
    disable_irq(data->irq);
	printk(KERN_INFO"++++++TP suspend\n");
	return 0;
}

static int atmega168_msi_resume(struct i2c_client *client)
{
	struct atmega168_msi_data *data = i2c_get_clientdata(client);
   	char cmd[2];
#ifdef TP_FROZEN	
	u8 i = 0;
#endif
    input_report_abs(data->input_dev, ABS_PRESSURE, 0);
	input_report_key(data->input_dev, BTN_TOUCH, 0);
	input_sync(data->input_dev);
#ifdef TP_FROZEN	
	data->pdata->platform_reset();
	mdelay(50);
	for(i = 0; i < 200; i ++)
    {
    	if(data->pdata->attb_read_val())
			break;
	}
	atmega168_msi_initialize(data);
	cmd[0] = 0x14;
	cmd[1] = 0x0;
    mutex_lock(&data->lock);
	i2c_master_send(data->client, cmd, 2);
    mutex_unlock(&data->lock);
#else
    cmd[0] = 0x14;
	cmd[1] = 0x0;
    mutex_lock(&data->lock);
	i2c_master_send(data->client, cmd, 2);
    mutex_unlock(&data->lock);
#endif	
	enable_irq(data->irq);
    printk(KERN_INFO"++++++TP resume\n");
	return 0;
}
#else
#define atmega168_msi_suspend	NULL
#define atmega168_msi_resume    NULL
#endif



#ifdef CONFIG_HAS_EARLYSUSPEND
static void atmega168_msi_early_suspend(struct early_suspend *h)
{
	struct atmega168_msi_data *ts;
	ts = container_of(h, struct atmega168_msi_data, early_suspend);
	atmega168_msi_suspend(ts->client, PMSG_SUSPEND);
}

static void atmega168_msi_late_resume(struct early_suspend *h)
{
	struct atmega168_msi_data *ts;
	ts = container_of(h, struct atmega168_msi_data, early_suspend);
	atmega168_msi_resume(ts->client);
}
#endif


static int __devinit atmega168_msi_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct atmega168_msi_data *data;
	struct input_dev *input_dev;
	struct device *dev;
	struct i2c_dev *i2c_dev;
	int error;
	u32 version = 0;

#ifdef VKEY_SUPPORT
	int iCount;
	/* code ,left ,right ,up ,down */
	struct ts_printkey_data pk_data_tlb[] = {
	{KEY_MENU, 0, 79, 481, 579},
	{KEY_SEARCH, 80, 159, 481, 579},
	{KEY_HOME, 160, 239, 481, 579},
	{KEY_BACK, 240, 319, 481, 579}
	};
#endif
	if (!client->dev.platform_data)
	{	
		return -EINVAL;
	}
   
	data = kzalloc(sizeof(struct atmega168_msi_data), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!data || !input_dev) 
	{
		dev_err(&client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}
	
#ifdef VKEY_SUPPORT
	data->pk_data = kmalloc(sizeof(pk_data_tlb), GFP_KERNEL);
	if (!data->pk_data) 
	{
		error = -ENOMEM;
		goto err_free_mem;
	}

	memcpy(data->pk_data, pk_data_tlb, sizeof(pk_data_tlb));

	data->pk_num = sizeof(pk_data_tlb) / sizeof(struct ts_printkey_data);
    
	for (iCount = 0; iCount < 4; iCount++)
	{
		input_dev->keybit[BIT_WORD(data->pk_data[iCount].code)] =
		    BIT_MASK(data->pk_data[iCount].code);
	}	
#endif
    init_waitqueue_head(&data->wait);
	mutex_init(&data->lock);
	input_dev->name = "atmega168_msi_ts";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
    input_dev->evbit[0] = BIT_MASK(EV_KEY)|BIT_MASK(EV_ABS)|BIT_MASK(EV_SYN);
    input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
    input_dev->absbit[0] = BIT(ABS_X)|BIT(ABS_Y);
	data->client = client;
	data->input_dev = input_dev;
	data->pdata = client->dev.platform_data;
	data->irq = client->irq;


#ifndef CONFIG_TOUCHSCREEN_MSI_ATMEGA168_MULTITOUCH
	/* For multi touch */
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			     0, data->pdata->max_area, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, data->pdata->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, data->pdata->y_max, 0, 0);
#else
	/* For single touch */
	input_set_abs_params(input_dev, ABS_X,
			     data->pdata->x_min, data->pdata->x_size, 0, 0);
	input_set_abs_params(input_dev, ABS_Y,
			     data->pdata->y_min, data->pdata->y_size, 0, 0);
#endif

	input_set_drvdata(input_dev, data);

	i2c_set_clientdata(client, data);
#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = atmega168_msi_early_suspend;
	data->early_suspend.resume = atmega168_msi_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

    if (data->pdata->platform_init)
	    data->pdata->platform_init();
    data->pen_down = 0;

  if(atmega168_msi_connect(data) < 0)
    printk(KERN_INFO"ERROR TP not connected\n");
#ifdef USE_BRCM_WORK_QUEUE
	INIT_DELAYED_WORK(&data->work, atmega168_msi_touch_work);
	error = request_irq(client->irq, atmega168_msi_touch_interrupt, 
			IRQF_TRIGGER_FALLING, "atmega168_msi_touch", data);	
	printk("atmega168_msi_touch: registered the interrupt=%d \r\n",client->irq);
#else
	error = request_threaded_irq(client->irq, NULL, atmega168_msi_interrupt,
			IRQF_TRIGGER_FALLING, client->dev.driver->name, data);

#endif

	error = input_register_device(input_dev);
	atmega168_msi_read_reg(data->client,48,4,&version);
	data->fw_version = version&0xff;
    if (data->pdata->platform_reset)
		data->pdata->platform_reset();
    i2c_dev = get_free_i2c_dev(client->adapter);
	if (IS_ERR(i2c_dev))
	{
		error = PTR_ERR(i2c_dev);
		return error;
	}

	dev = device_create(i2c_dev_class, &client->adapter->dev, MKDEV(I2C_MAJOR,
			client->adapter->nr), NULL, "atmega168_msi_ts%d", client->adapter->nr);
	if (IS_ERR(dev))
	{
		error = PTR_ERR(dev);
		return error;
	}
	if (error)
	    goto err_free_irq;

	error = sysfs_create_group(&client->dev.kobj, &atmega168_msi_attr_group);
	if (error)
		goto err_unregister_device;

	return 0;

err_unregister_device:
	input_unregister_device(input_dev);
	input_dev = NULL;
err_free_irq:
	free_irq(client->irq, data);
err_free_mem:
	input_free_device(input_dev);
	kfree(data);
#ifdef VKEY_SUPPORT
	if (data->pk_data) 
	{
		kfree(data->pk_data);
	}
#endif	
	return error;
}

static int __devexit atmega168_msi_remove(struct i2c_client *client)
{
	struct atmega168_msi_data *data = i2c_get_clientdata(client);
    wake_up(&data->wait);
	sysfs_remove_group(&client->dev.kobj, &atmega168_msi_attr_group);
	free_irq(data->irq, data);
	input_unregister_device(data->input_dev);
	kfree(data);
#ifdef VKEY_SUPPORT
	kfree(data->pk_data);
#endif
	return 0;
}


static const struct i2c_device_id atmega168_msi_id[] = 
{
	{ "atmega168_msi_ts", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, atmega168_msi_id);

static struct i2c_driver atmega168_msi_driver = {
	.driver = 
	{
		.name	= "atmega168_msi_ts",
		.owner	= THIS_MODULE,
	},
	.probe		= atmega168_msi_probe,
	.remove		= __devexit_p(atmega168_msi_remove),
	.suspend	= atmega168_msi_suspend,
	.resume		= atmega168_msi_resume,
	.id_table	= atmega168_msi_id,
};

static int atmega168_msi_open(struct inode *inode, struct file *file)
{
	int subminor;
	struct i2c_client *client;
	struct i2c_adapter *adapter;
	struct i2c_dev *i2c_dev;
	int ret = 0;

	printk("enter atmega168_msi_open function\n");
	subminor = iminor(inode);
	lock_kernel();
	i2c_dev = i2c_dev_get_by_minor(subminor);
	if (!i2c_dev)
	{
		printk("error i2c_dev\n");
		return -ENODEV;
	}

	adapter = i2c_get_adapter(i2c_dev->adap->nr);
	if (!adapter)
	{

		return -ENODEV;
	}
	//printk("after i2c_dev_get_by_minor\n");

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
	{
		i2c_put_adapter(adapter);
		ret = -ENOMEM;
	}
	snprintf(client->name, I2C_NAME_SIZE, "atmega168_msi_ts%d", adapter->nr);
	client->driver = &atmega168_msi_driver;
	client->adapter = adapter;

	file->private_data = client;

	return 0;
}

static long atmega168_msi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *) file->private_data;
    switch (cmd)
	{
		case VERSION_FLAG:
		{
			client->addr = 0x5c;		
			status_reg = 0;
			status_reg = VERSION_FLAG;
			break;
		}
	    case CALIBRATION_FLAG: 
        {
		   
		    client->addr = 0x5c;
		    status_reg = 0;
		    status_reg = CALIBRATION_FLAG;
		    break;
	    } 
        case BOOTLOADER_MODE: 
        {
		    printk(KERN_INFO"BOOTLOADER_MODE\n");
            status_reg = 0;
		    status_reg = BOOTLOADER_MODE;
			client->addr = 0x5d;
		    mdelay(30);
		    break;
        }	
	    case ENABLE_IRQ:
		{
			enable_irq(GPIO_TO_IRQ(29));
		    break;
	    }
	    case DISABLE_IRQ:
	    {
			disable_irq_nosync(GPIO_TO_IRQ(29));
		    break;
	    }
	    case TPRESET:
        {
			atmega168_msi_pen_reset();
		    break;
	    }	
        case TPEEWRITE:
		{
			status_reg = 0;
		    status_reg = TPEEWRITE;
		    break;
        }	
	    case TPEEREAD:
		{
			status_reg = 0;
		    status_reg = TPEEREAD;
		    break;
	    }	
	    default:
		{
			break;//return -ENOTTY;
	    }	
	}
	return 0;
}



static ssize_t atmega168_msi_read (struct file *file, char __user *buf, size_t count,loff_t *offset)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	int ret=0;
	switch(status_reg)
	{
    	case TPEEREAD:
		{
			if(copy_to_user(buf,&tp_reg,count)) 
			{
				ret = -1;
				printk("READ_EEPROM,copy_to_user error, ret = %d\n",ret);
		    }
			break;
		}
		case VERSION_FLAG:
		{
		    unsigned char vaddbuf[1],verbuf[5];
		    memset(vaddbuf, 0, sizeof(vaddbuf));
		    memset(verbuf, 0, sizeof(verbuf));
	        vaddbuf[0] = 48;
	        ret = i2c_master_send(client, vaddbuf, 1); //version address
		    if(ret != 1) 
			{
				printk("VERSION_FLAG,master send %d error, ret = %d\n",vaddbuf[0],ret);
		    }
		    ret = i2c_master_recv(client, verbuf, 5);			
		    if(copy_to_user(buf,verbuf,5)) 
			{
				printk("VERSION_FLAG,copy_to_user error, ret = %d\n",ret);
		    }
			break;
		}
		default:
		{
            break;
		}		
	}
	return ret;
}



static ssize_t atmega168_msi_write(struct file *file, const char __user *buf,size_t count, loff_t *ppos)
{
	struct i2c_client *client;
	char *tmp,bootload_data[143],Rdbuf[1];
	char data1[4];
	static int ret=0,ret2=0,stu;
	static int re_value=0;
    
	client = (struct i2c_client *)file->private_data;
	switch(status_reg)
	{
        case TPEEREAD:
		{
			unsigned char epmdatabuf[2],wr2eep[4];
			memset(epmdatabuf, 0, sizeof(epmdatabuf));
			memset(wr2eep, 0, sizeof(wr2eep));
			client->addr = 0x5c;
			ret = copy_from_user(epmdatabuf, buf, count);
			if(ret)
			{
				ret = -1;
				printk("WRITE_EEPROM,copy_from_user error, ret = %d\n",ret);
				break;
			}
			wr2eep[0] = 0x37;
			wr2eep[1] = 1;
			wr2eep[2] = epmdatabuf[0];
			wr2eep[3] = epmdatabuf[1];
			printk(KERN_INFO"TP read reg addr = %d,%d\n",epmdatabuf[0],epmdatabuf[1]);
			ret = i2c_master_send(client, wr2eep, 4);
			if(ret != 4)
			{
				ret = -1;
				printk("WRITE_EEPROM wr2eep,i2c_master_send, ret = %d\n",ret);
				break;	
			}
			memset(epmdatabuf, 0, sizeof(epmdatabuf));
			i2c_master_recv(client, &tp_reg, 1);
			epmdatabuf[0] = tp_reg;
			printk(KERN_INFO"read value=%d\n",tp_reg);
			break;
		}
	
	    case TPEEWRITE:
		{
			unsigned char epmdatabuf[2];
			memset(epmdatabuf, 0, sizeof(epmdatabuf));
			client->addr = 0x5c;
			ret = copy_from_user(epmdatabuf, buf, count);
			if(ret)
			{
				ret = -1;
				printk("WRITE_EEPROM,copy_from_user error, ret = %d\n",ret);
				break;
			}

			if(2 == count)
			{		// write a byte 2 eeprom
				op2eep[0] = 0x37;
				op2eep[1] = 2;
				data2eep[0] = epmdatabuf[0];
				data2eep[1] = epmdatabuf[1];
				printk(KERN_INFO"tp write count ==2 \n");
			}
			else if(1 == count)
			{	//write addr 2 eeprom 
			    client->addr = 0x5c;
				data2eep[2] = epmdatabuf[0];
				ret = i2c_master_send(client, op2eep, 2);
				printk(KERN_INFO"tp write count ==1 \n");
				if(ret != 2)
				{
					ret = -1;
					printk("WRITE_EEPROM op2eep,i2c_master_send, ret = %d\n",ret);
					break;	
				} 
				client->addr = 0x5c;
				printk(KERN_INFO"tp write 0x%x,0x%x,0x%x\n",data2eep[0],data2eep[1],data2eep[2]);
				ret = i2c_master_send(client, data2eep, 3);
				if(ret != 3)
				{
					ret = -1;
					printk("WRITE_EEPROM data2eep,i2c_master_send, ret = %d\n",ret);
					break;	
				} 
				mdelay(4);
				i2c_master_recv(client, data2eep, 1);
				mdelay(100);
			}
			break;
		} 	
		case CALIBRATION_FLAG: //CALIBRATION_FLAG=1
		{
			disable_irq(GPIO_TO_IRQ(29));
		    tmp = kmalloc(count,GFP_KERNEL);
		    if (tmp == NULL)
			    return -ENOMEM;
	
			tmp[0]= 0x37;
			tmp[1] = 0x3;
			ret = i2c_master_send(client,tmp,2);

			printk("CALIBRATION_FLAG,i2c_master_send ret = %d\n",ret);

			mdelay(1000);
			if(ret != count)
			{
				printk("CALIBRATION_FLAG,Unable to write to i2c page for calibratoion!\n");
		    }

		    kfree(tmp);
            enable_irq(GPIO_TO_IRQ(29));
		    status_reg = 0;
		    break;
		} 
		case BOOTLOADER_MODE: //BOOTLOADER_MODE=7
        {
			printk("BOOT ");
           
		    memset(bootload_data, 0, sizeof(bootload_data));
		    memset(Rdbuf, 0, sizeof(Rdbuf));
            disable_irq((GPIO_TO_IRQ(29)));
		    if (copy_from_user(bootload_data,buf,count))
		    {
			    printk("COPY FAIL ");
			    return -EFAULT;
		    }
		
            client->addr = 0x5d;
            atmega168_msi_pen_reset();
            mdelay(30);
			
            i2c_master_recv(client, data1, 4);
	        printk(KERN_INFO"[WYQ]%d val = %d\n",__LINE__,data1[0]);
	        printk(KERN_INFO"[WYQ]%d val = %d\n",__LINE__,data1[1]);
	        printk(KERN_INFO"[WYQ]%d val = %d\n",__LINE__,data1[2]);
	        printk(KERN_INFO"[WYQ]%d val = %d\n",__LINE__,data1[3]);
     
		   stu = bootload_data[0];
		
		   ret = i2c_master_send(client,bootload_data,count);
		   if(ret!=count)
		   {
		       status_reg = 0;		
			   enable_irq(GPIO_TO_IRQ(29));
			   printk("bootload 143 bytes error,ret = %d\n",ret);
			   break;
		   }
		
		   if(stu!=0x01)
		   {
			   mdelay(1);
			   while(atmega168_msi_pen_down_state());
			   mdelay(1);
			   ret2 = i2c_master_recv(client,Rdbuf,1);
			   if(ret2 != 1)
			   {
			    	ret = 0;
				    printk("read IIC slave status error,ret = %d\n",ret2);
				    break;

			   }
			   re_value = Rdbuf[0];
		   }
		   else
		   {
			   mdelay(100);
		       enable_irq(GPIO_TO_IRQ(29));
		   }
           enable_irq(GPIO_TO_IRQ(29));
		   if((re_value&0x80)&&(stu!=0x01))
		   {
			   printk("Failed:(re_value&0x80)&&(stu!=0x01)=1\n");
			   ret = 0;
		   }
		   break;
		}
		default:
		{
			break;
		}	
	}
	return ret;
}

static int atmega168_msi_release(struct inode *inode, struct file *file)
{
	struct i2c_client *client = file->private_data;
   
	i2c_put_adapter(client->adapter);
	kfree(client);
	file->private_data = NULL;

	return 0;
}

static const struct file_operations atmega168_msi_fops =
{	.owner = THIS_MODULE, 
	.read = atmega168_msi_read, 
	.write = atmega168_msi_write,
	.open = atmega168_msi_open, 
	.unlocked_ioctl = atmega168_msi_ioctl,
	.release = atmega168_msi_release, 
};



static int __init atmega168_msi_init(void)
{
	int ret;
	printk("atmega168_msi_init\n");
	ret = register_chrdev(I2C_MAJOR,"atmega168_msi_ts",&atmega168_msi_fops);
	if(ret)
	{
		printk(KERN_ERR "%s:register chrdev failed\n",__FILE__);
		return ret;
	}

	i2c_dev_class = class_create(THIS_MODULE, "atmega168_msi_ts");
	if (IS_ERR(i2c_dev_class))
	{
		ret = PTR_ERR(i2c_dev_class);
		class_destroy(i2c_dev_class);
	}
	return i2c_add_driver(&atmega168_msi_driver);
}

static void __exit atmega168_msi_exit(void)
{
	i2c_del_driver(&atmega168_msi_driver);
    class_destroy(i2c_dev_class);
	unregister_chrdev(I2C_MAJOR,"atmega168_msi_ts");
}

module_init(atmega168_msi_init);
module_exit(atmega168_msi_exit);

/* Module information */
MODULE_AUTHOR("yongqiang wang <yongqiang.wang@broadcom.com>");
MODULE_DESCRIPTION("ATMEGA168 MSI Touchscreen driver");
MODULE_LICENSE("GPL");

