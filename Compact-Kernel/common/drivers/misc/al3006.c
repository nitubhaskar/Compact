/*******************************************************************************
* Copyright 2010 Broadcom Corporation.  All rights reserved.
*
* 	@file	drivers/input/misc/al3006.c
*
* Unless you and Broadcom execute a separate written software license agreement
* governing use of this software, this software is licensed to you under the
* terms of the GNU General Public License version 2, available at
* http://www.gnu.org/copyleft/gpl.html (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a license
* other than the GPL, without Broadcom's express prior written consent.
*******************************************************************************/


#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>
#include <linux/al3006.h>
#include <asm/setup.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>

#include <linux/regulator/consumer.h>

//#define DEBUG
#ifdef DEBUG
#define D(x...) printk(x)
//#define D(x...) pr_info(x)
#else
#define D(x...)
#endif

#define I2C_RETRY_COUNT 10
#define AL3006_I2C_NAME "al3006"

#define POLLING_PROXIMITY 1
#define NO_IGNORE_BOOT_MODE 1

//#define NEAR_DELAY_TIME ((30 * HZ) / 1000)

#ifdef POLLING_PROXIMITY
#define POLLING_DELAY		1000//2000
#define TH_ADD			3
#endif
static int record_init_fail = 0;
static void sensor_irq_do_work(struct work_struct *work);
static DECLARE_WORK(sensor_irq_work, sensor_irq_do_work);

#ifdef POLLING_PROXIMITY
static void polling_do_work(struct work_struct *w);
static DECLARE_DELAYED_WORK(polling_work, polling_do_work);
#endif

struct al3006_info {
	struct class *al3006_class;
	struct device *ls_dev;
	struct device *ps_dev;
	struct input_dev *ls_input_dev;
	struct input_dev *ps_input_dev;
	struct early_suspend early_suspend;
	struct i2c_client *i2c_client;
	struct workqueue_struct *lp_wq;
	int intr_pin;
	int als_enable;
	int ps_enable;
	int ps_irq_flag;
	int led;
	int irq;
	int ls_calibrate;
	struct wake_lock ps_wake_lock;
	int psensor_opened;
	int lightsensor_opened;
	uint16_t check_interrupt_add;
	uint8_t ps_thd_set;
	uint8_t enable_polling_ignore;
	struct regulator *ls_regulator;
};

static struct al3006_info *lp_info;
static int enable_log = 0;
static struct mutex als_enable_mutex, als_disable_mutex, als_get_adc_mutex;
static int lightsensor_enable(struct al3006_info *lpi);
static int lightsensor_disable(struct al3006_info *lpi);
static int initial_al3006(struct al3006_info *lpi);
static void psensor_initial_cmd(struct al3006_info *lpi);

static int al3006_i2c_read(unsigned char reg_addr,unsigned char *data)
{
    int dummy;

    if(!lp_info || !lp_info->i2c_client || (unsigned long)lp_info->i2c_client < 0xc0000000)
            return -1;

    dummy = i2c_smbus_read_byte_data(lp_info->i2c_client, reg_addr);
    if(dummy < 0)
            return -1;

    *data = dummy & 0x000000ff;

    return 0;
}

static int al3006_i2c_write(unsigned char reg_addr,unsigned char data)
{
    int dummy;

    if(!lp_info || !lp_info->i2c_client || (unsigned long)lp_info->i2c_client < 0xc0000000)
            return -1;

    dummy = i2c_smbus_write_byte_data(lp_info->i2c_client, reg_addr, data);
    if(dummy < 0)
            return -1;


    return 0;

}

static int get_adc_value(uint8_t *data)
{
	int ret = 0;
	if (data == NULL)
		return -EFAULT;
	ret = al3006_i2c_read(0x05, data);
	if (ret < 0) {
		pr_err(
			"[AL3006 error]%s: _al3006_I2C_Read_Byte MSB fail\n",
			__func__);
		return -EIO;
	}

	return ret;
}

static void report_psensor_input_event(struct al3006_info *lpi)
{
	uint8_t ps_data;
	int val, ret = 0;

	ret = get_adc_value(&ps_data);/*check i2c result*/
	if (ret == 0) {
		val = (ps_data & 0x80) ? 0 : 1;
		D("[AL3006] proximity %s, ps_data=%d\n", val ? "FAR" : "NEAR", ps_data);
	} else {/*i2c err, report far to workaround*/
		val = 1;
		ps_data = 0;
		D("[AL3006] proximity i2c err, report %s, ps_data=%d, record_init_fail %d\n",
			val ? "FAR" : "NEAR", ps_data, record_init_fail);
	}

		/* 0 is close, 1 is far */
	input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, val);
	input_sync(lpi->ps_input_dev);
//	blocking_notifier_call_chain(&psensor_notifier_list, val+2, NULL);

    wake_lock_timeout(&(lpi->ps_wake_lock), 2*HZ);
}

static int lux_table[64]={1,1,1,2,2,2,3,4,4,5,6,7,9,11,13,16,19,22,27,32,39,46,56,67,80,96,
                          116,139,167,200,240,289,346,416,499,599,720,864,1037,1245,1495,
                          1795,2154,2586,3105,3728,4475,5372,6449,7743,9295,11159,13396,
                          16082,19307,23178,27826,33405,40103,48144,57797,69386,83298,
                          100000};

static void report_lsensor_input_event(struct al3006_info *lpi)
{

	uint8_t adc_value = 0;
	int level = 0, i, ret = 0;

	mutex_lock(&als_get_adc_mutex);

	ret = get_adc_value(&adc_value);

	if (ret == 0) {
        i = adc_value & 0x3f;
        level = i;//lux_table[i];
    }
    else
    {
        printk(KERN_ERR"lsensor read adc fail!\n");
        goto fail_out;
    }

	D("[AL3006] %s: ADC=0x%03X, Level=%d\n",__func__,i, level);

	input_report_abs(lpi->ls_input_dev, ABS_MISC, level);
	input_sync(lpi->ls_input_dev);
//	enable_als_int();

fail_out:
	mutex_unlock(&als_get_adc_mutex);

}

static void sensor_irq_do_work(struct work_struct *work)
{
    struct al3006_info *lpi = lp_info;

    uint8_t status = 0;

    /*check ALS or PS*/
    al3006_i2c_read(0x03,&status); /*read interrupt status register*/

    D("[AL3006] intr status[0x%x]\n",status);
    if(status & 0x02)/*ps trigger interrupt*/
    {
        report_psensor_input_event(lpi);
    }

    //enable_irq(lpi->irq);
}


#ifdef POLLING_PROXIMITY
static void polling_do_work(struct work_struct *w)
{
	struct al3006_info *lpi = lp_info;

	/*D("lpi->ps_enable = %d\n", lpi->ps_enable);*/
	if (!lpi->ps_enable && !lpi->als_enable)
		return;

    if(lpi->ps_enable)
    	report_psensor_input_event(lpi);
    if(lpi->als_enable)
    	report_lsensor_input_event(lpi);

    queue_delayed_work(lpi->lp_wq, &polling_work,
		msecs_to_jiffies(POLLING_DELAY));
}
#endif

static irqreturn_t al3006_irq_handler(int irq, void *data)
{
	struct al3006_info *lpi = data;

	//disable_irq_nosync(lpi->irq);
	if (enable_log)
		D("[AL3006] %s\n", __func__);

	queue_work(lpi->lp_wq, &sensor_irq_work);

	return IRQ_HANDLED;
}

static int psensor_enable(struct al3006_info *lpi)
{
	int ret = 0;

	D("[AL3006] %s by pid[%d] thread [%s]\n", __func__,current->pid,current->comm);
#if 0
	if (lpi->ps_enable) {
		D("[AL3006] %s: already enabled\n", __func__);
		return 0;
	}
#endif
	/* dummy report */
	input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, -1);
	input_sync(lpi->ps_input_dev);

#if 0
    if(lpi->als_enable)
        al3006_i2c_write(0x00,0x02); // both enable ps and als
    else
#endif
        al3006_i2c_write(0x00,0x01); // only enable ps

	msleep(15);
	report_psensor_input_event(lpi);

    lpi->ps_enable = 1;
#if 0    
    if(!lpi->ps_irq_flag){
        enable_irq(lpi->irq);
        lpi->ps_irq_flag = 1;
    }
#endif
    //if(!lpi->als_enable)
	queue_delayed_work(lpi->lp_wq, &polling_work,
		msecs_to_jiffies(POLLING_DELAY));

	return ret;
}

static int psensor_disable(struct al3006_info *lpi)
{
	int ret = -EIO;

	//lpi->ps_pocket_mode = 0;

	D("[AL3006] %s by pid[%d] thread [%s]\n", __func__,current->pid,current->comm);
	//D("[AL3006] %s\n", __func__);
#if 0
    if(lpi->ps_irq_flag){
    	disable_irq_nosync(lpi->irq);
        lpi->ps_irq_flag = 0;
    }
#endif
    if(lpi->als_enable)
        al3006_i2c_write(0x00,0x00); // both enable ps and als
    else
        al3006_i2c_write(0x00,0x0b);//0x03); // only enable als

    lpi->ps_enable = 0;

#ifdef POLLING_PROXIMITY
    if(!lpi->als_enable)
	    cancel_delayed_work(&polling_work);
	/* settng command code(0x01) = 0x03*/
#endif
	return ret;
}

static int psensor_open(struct inode *inode, struct file *file)
{
	struct al3006_info *lpi = lp_info;

	D("[AL3006] %s\n", __func__);

	if (lpi->psensor_opened)
		return -EBUSY;

	lpi->psensor_opened = 1;

	return 0;
}

static int psensor_release(struct inode *inode, struct file *file)
{
	struct al3006_info *lpi = lp_info;

	D("[AL3006] %s\n", __func__);

	lpi->psensor_opened = 0;

	return psensor_disable(lpi);
}

static long psensor_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	int val;
	struct al3006_info *lpi = lp_info;

	D("[AL3006] %s cmd %d\n", __func__, _IOC_NR(cmd));

	switch (cmd) {
	case DYNA_AL3006_PSENSOR_IOCTL_ENABLE:
		if (get_user(val, (unsigned long __user *)arg))
			return -EFAULT;
		if (val)
			return psensor_enable(lpi);
		else
			return psensor_disable(lpi);
		break;
	case DYNA_AL3006_PSENSOR_IOCTL_GET_ENABLED:
		return put_user(lpi->ps_enable, (unsigned long __user *)arg);
		break;
	default:
		pr_err("[AL3006 error]%s: invalid cmd %d\n",
			__func__, _IOC_NR(cmd));
		return -EINVAL;
	}
}

static const struct file_operations psensor_fops = {
	.owner = THIS_MODULE,
	.open = psensor_open,
	.release = psensor_release,
	.unlocked_ioctl = psensor_ioctl
};

struct miscdevice psensor_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "proximity",
	.fops = &psensor_fops
};


static int lightsensor_enable(struct al3006_info *lpi)
{
	int ret = 0;
    
    if(!lpi)
        return -1;

	mutex_lock(&als_enable_mutex);
	D("[AL3006] %s by pid[%d] thread [%s]\n", __func__,current->pid,current->comm);

    if(lpi->ps_enable)
        al3006_i2c_write(0x00,0x01); //02 for both enable ps and als, not stable
    else
        al3006_i2c_write(0x00,0x00); // only enable als

    msleep(10);
		/* report an invalid value first to ensure we
		* trigger an event when adc_level is zero.
		*/
    lpi->als_enable = 1;

	input_report_abs(lpi->ls_input_dev, ABS_MISC, -1);
	input_sync(lpi->ls_input_dev);
	report_lsensor_input_event(lpi);/*resume, IOCTL and DEVICE_ATTR*/
	mutex_unlock(&als_enable_mutex);

#ifdef POLLING_PROXIMITY
    //if(!lpi->ps_enable)
    	queue_delayed_work(lpi->lp_wq, &polling_work,
		msecs_to_jiffies(POLLING_DELAY));
	/* settng command code(0x01) = 0x03*/
#endif

	return ret;
}

static int lightsensor_disable(struct al3006_info *lpi)
{
	int ret = 0;

    if(!lpi)
            return -1;

	mutex_lock(&als_disable_mutex);

    if(lpi->ps_enable)
        al3006_i2c_write(0x00,0x01); // both enable ps and als
    else{
        al3006_i2c_write(0x00,0x0b);//0x03); // only enable als
    }
	//D("[AL3006] %s\n", __func__);
	D("[AL3006] %s by pid[%d] thread [%s]\n", __func__,current->pid,current->comm);
	lpi->als_enable = 0;
	mutex_unlock(&als_disable_mutex);

#ifdef POLLING_PROXIMITY
    if(!lpi->ps_enable)
	    cancel_delayed_work(&polling_work);
#endif
	return ret;
}

static int lightsensor_open(struct inode *inode, struct file *file)
{
	struct al3006_info *lpi = lp_info;
	int rc = 0;

	D("[AL3006] %s\n", __func__);
	if (lpi->lightsensor_opened) {
		pr_err("[AL3006 error]%s: already opened\n", __func__);
		rc = -EBUSY;
	}
	lpi->lightsensor_opened = 1;
	return rc;
}

static int lightsensor_release(struct inode *inode, struct file *file)
{
	struct al3006_info *lpi = lp_info;

	D("[AL3006] %s\n", __func__);
	lpi->lightsensor_opened = 0;
	return 0;
}

static long lightsensor_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int rc, val;
	struct al3006_info *lpi = lp_info;

	D("[AL3006] %s cmd %d\n", __func__, _IOC_NR(cmd));

	switch (cmd) {
	case DYNA_AL3006_AMBIENT_IOCTL_ENABLE:
		if (get_user(val, (unsigned long __user *)arg)) {
			rc = -EFAULT;
			break;
		}
		D("[AL3006] %s LIGHTSENSOR_IOCTL_ENABLE, value = %d\n",
			__func__, val);
		rc = val ? lightsensor_enable(lpi) : lightsensor_disable(lpi);
		break;
	case DYNA_AL3006_AMBIENT_IOCTL_GET_ENABLED:
		val = lpi->als_enable;
		D("[AL3006] %s LIGHTSENSOR_IOCTL_GET_ENABLED, enabled %d\n",
			__func__, val);
		rc = put_user(val, (unsigned long __user *)arg);
		break;
	default:
		pr_err("[AL3006 error]%s: invalid cmd %d\n",
			__func__, _IOC_NR(cmd));
		rc = -EINVAL;
	}

	return rc;
}

static const struct file_operations lightsensor_fops = {
	.owner = THIS_MODULE,
	.open = lightsensor_open,
	.release = lightsensor_release,
	.unlocked_ioctl = lightsensor_ioctl
};

static struct miscdevice lightsensor_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "lightsensor",
	.fops = &lightsensor_fops
};

//=========================================
static int lightsensor_setup(struct al3006_info *lpi)
{
	int ret;

	lpi->ls_input_dev = input_allocate_device();
	if (!lpi->ls_input_dev) {
		pr_err(
			"[AL3006 error]%s: could not allocate ls input device\n",
			__func__);
		return -ENOMEM;
	}
	lpi->ls_input_dev->name = "lightsensor";
	set_bit(EV_ABS, lpi->ls_input_dev->evbit);
	input_set_abs_params(lpi->ls_input_dev, ABS_MISC, 0, 9, 0, 0);

	ret = input_register_device(lpi->ls_input_dev);
	if (ret < 0) {
		pr_err("[AL3006 error]%s: can not register ls input device\n",
				__func__);
		goto err_free_ls_input_device;
	}

	ret = misc_register(&lightsensor_misc);
	if (ret < 0) {
		pr_err("[AL3006 error]%s: can not register ls misc device\n",
				__func__);
		goto err_unregister_ls_input_device;
	}

	return ret;

err_unregister_ls_input_device:
	input_unregister_device(lpi->ls_input_dev);
err_free_ls_input_device:
	input_free_device(lpi->ls_input_dev);
	return ret;
}

static int psensor_setup(struct al3006_info *lpi)
{
	int ret;

	lpi->ps_input_dev = input_allocate_device();
	if (!lpi->ps_input_dev) {
		pr_err(
			"[AL3006 error]%s: could not allocate ps input device\n",
			__func__);
		return -ENOMEM;
	}
	lpi->ps_input_dev->name = "proximity";
	set_bit(EV_ABS, lpi->ps_input_dev->evbit);
	input_set_abs_params(lpi->ps_input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	ret = input_register_device(lpi->ps_input_dev);
	if (ret < 0) {
		pr_err(
			"[AL3006 error]%s: could not register ps input device\n",
			__func__);
		goto err_free_ps_input_device;
	}

	ret = misc_register(&psensor_misc);
	if (ret < 0) {
		pr_err(
			"[AL3006 error]%s: could not register ps misc device\n",
			__func__);
		goto err_unregister_ps_input_device;
	}
    /*Need add psensor interrupt gpio setup*/
    ret = request_irq(lpi->irq, al3006_irq_handler,
                    IRQF_TRIGGER_FALLING,"al3006",lpi);
    if(ret < 0) {
        pr_err("[AL3006]%s: req_irq(%d) fail (%d)\n", __func__,
                        lpi->irq, ret);
        goto err_request_irq_ps;
    }
	return ret;

err_request_irq_ps:
	misc_deregister(&psensor_misc);
err_unregister_ps_input_device:
	input_unregister_device(lpi->ps_input_dev);
err_free_ps_input_device:
	input_free_device(lpi->ps_input_dev);
	return ret;
}


static int initial_al3006(struct al3006_info *lpi)
{
	int ret;
	uint8_t add,val;
    add = 0;    
    ret = al3006_i2c_read(add,&val);
    if((0x03 == val)||(!ret))
        printk("al3006 device found!! id = 0x%x\n",val);
    else
        return -1;

//	msleep(10);
#if 1 //dummy init
    al3006_i2c_write(0x00,0x0b);
    al3006_i2c_write(0x01,0x01);
    al3006_i2c_write(0x02,0xa0);
    al3006_i2c_write(0x04,0x4a);
    al3006_i2c_write(0x08,0x00);
    al3006_i2c_write(0x00,0x08);
    al3006_i2c_write(0x00,0x0b);//0x03);
#endif
	return 0;
}


static int al3006_setup(struct al3006_info *lpi)
{
	int ret = 0;
	
    ret = initial_al3006(lpi);
	if (ret < 0) {
		pr_err(
			"[AL3006 error]%s: fail to initial al3006 (%d)\n",
			__func__, ret);
	}

	return ret;
}

static void al3006_early_suspend(struct early_suspend *h)
{
	struct al3006_info *lpi = lp_info;

	D("[AL3006] %s\n", __func__);

#if 0
	if (lpi->als_enable)
		lightsensor_disable(lpi);
	if (lpi->ps_enable)
		psensor_disable(lpi);
#endif
    if(lpi->als_enable && !lpi->ps_enable)
    {
	    cancel_delayed_work(&polling_work);
    }
    if(lpi->ps_enable)/*Need ps interrupt to wake up phone*/{
        D("3006 suspend ps enabled\n");
        al3006_i2c_write(0x0,0x01);
#if 1
        if(!lpi->ps_irq_flag)
        {
            enable_irq(lpi->irq);
            lpi->ps_irq_flag = 1;
        }
#endif
    }
    else
        al3006_i2c_write(0x0,0x0b); //0x03 set chip to idle mode

}

static void al3006_late_resume(struct early_suspend *h)
{
	struct al3006_info *lpi = lp_info;

	D("[AL3006] %s\n", __func__);

	if (lpi->als_enable)
		lightsensor_enable(lpi);

	if (lpi->ps_enable){
        D("3006 resume ps enabled\n");
#if 1
        if(lpi->ps_irq_flag)
        {
    	    disable_irq_nosync(lpi->irq);
            lpi->ps_irq_flag = 0;
        }
#endif
		psensor_enable(lpi);
    }

}
/**************sys interface start ****************/

static ssize_t ps_adc_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{

    uint8_t value;
    int ret;
    struct al3006_info *lpi = lp_info;
    int value1;

    ret = get_adc_value(&value);

    ret = sprintf(buf, "ADC[0x%03X]\n ps[%d]\n als[%d]\n irq[%d]\n", \
              value, lpi->ps_enable,lpi->als_enable,lpi->ps_irq_flag);

    return ret;
}

static ssize_t ps_enable_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
    int ps_en;
    struct al3006_info *lpi = lp_info;

    ps_en = -1;
    sscanf(buf, "%d", &ps_en);

    if (ps_en != 0 && ps_en != 1
        && ps_en != 10 && ps_en != 13 && ps_en != 16)
        return -EINVAL;

    if (ps_en) {
        D("[AL3006] %s: ps_en=%d\n",
            __func__, ps_en);
        psensor_enable(lpi);
    } else
        psensor_disable(lpi);

    D("[AL3006] %s\n", __func__);

    return count;
}

static ssize_t als_enable_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
    int als_en;
    struct al3006_info *lpi = lp_info;

    als_en = -1;
    sscanf(buf, "%d", &als_en);

    if (als_en != 0 && als_en != 1
        && als_en != 10 && als_en != 13 && als_en != 16)
        return -EINVAL;

    if (als_en) {
        D("[AL3006] %s: ps_en=%d\n",
            __func__, als_en);
        lightsensor_enable(lpi);
    } else
        lightsensor_disable(lpi);

    D("[AL3006] %s\n", __func__);

    return count;
}
static DEVICE_ATTR(ps_enable, 0664, ps_adc_show, ps_enable_store);
static DEVICE_ATTR(als_enable, 0664, ps_adc_show, als_enable_store);

static ssize_t dump_register_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    unsigned length = 0;
    int i;
    uint8_t val;

    for (i = 0; i < 9; i++) {
        al3006_i2c_read(i,&val);

        length += sprintf(buf + length,
            "[AL3006]Get register[0x%2x] =  0x%2x ;\n",
            i, val);
    }
    return length;
}
static DEVICE_ATTR(dump_register, 0664, dump_register_show, NULL);

/**************sys interface end ****************/
static int al3006_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct al3006_info *lpi;
	struct al3006_platform_data *pdata;

	D("[AL3006] %s\n", __func__);


	lpi = kzalloc(sizeof(struct al3006_info), GFP_KERNEL);
	if (!lpi)
		return -ENOMEM;

	/*D("[AL3006] %s: client->irq = %d\n", __func__, client->irq);*/

	lpi->i2c_client = client;
	pdata = client->dev.platform_data;
	if (!pdata) {
		pr_err("[AL3006 error]%s: Assign platform_data error!!\n",
			__func__);
		ret = -EBUSY;
		goto err_platform_data_null;
	}
    /*start to declare regulator and enable power*/
    lpi->ls_regulator = regulator_get(NULL, "hcldo1_3v3");
	if (!lpi->ls_regulator || IS_ERR(lpi->ls_regulator)) {
		printk(KERN_ERR "al3006 No Regulator available\n");
		ret = -EFAULT;
		goto err_platform_data_null;
	}
    
    if(lpi->ls_regulator){
            regulator_set_voltage(lpi->ls_regulator,3300000,3300000);
            regulator_enable(lpi->ls_regulator);/*enable power*/
    }

	lpi->irq = client->irq;

	//lpi->mfg_mode = board_mfg_mode();

	i2c_set_clientdata(client, lpi);
	//lpi->intr_pin = pdata->intr;

    pdata->init();/*init sensor gpio interrupt pin*/

	lp_info = lpi;

	mutex_init(&als_enable_mutex);
	mutex_init(&als_disable_mutex);
	mutex_init(&als_get_adc_mutex);

	ret = lightsensor_setup(lpi);
	if (ret < 0) {
		pr_err("[AL3006 error]%s: lightsensor_setup error!!\n",
			__func__);
		goto err_lightsensor_setup;
	}

	ret = psensor_setup(lpi);
	if (ret < 0) {
		pr_err("[AL3006 error]%s: psensor_setup error!!\n",
			__func__);
		goto err_psensor_setup;
	}

	lpi->lp_wq = create_singlethread_workqueue("al3006_wq");
	if (!lpi->lp_wq) {
		pr_err("[AL3006 error]%s: can't create workqueue\n", __func__);
		ret = -ENOMEM;
		goto err_create_singlethread_workqueue;
	}

	wake_lock_init(&(lpi->ps_wake_lock), WAKE_LOCK_SUSPEND, "proximity");

#if 0
	psensor_set_kvalue(lpi);

#ifdef POLLING_PROXIMITY
	lpi->original_ps_thd_set = lpi->ps_thd_set;
#endif
#endif
	ret = al3006_setup(lpi);
	if (ret < 0) {
		pr_err("[AL3006 error]%s: al3006_setup error!\n", __func__);
		goto err_al3006_setup;
	}

    lpi->al3006_class = class_create(THIS_MODULE, "optical_sensors");
    if (IS_ERR(lpi->al3006_class)) {
        ret = PTR_ERR(lpi->al3006_class);
        lpi->al3006_class = NULL;
        goto err_create_class;
    }

    lpi->ls_dev = device_create(lpi->al3006_class,
                NULL, 0, "%s", "lightsensor");
    if (unlikely(IS_ERR(lpi->ls_dev))) {
        ret = PTR_ERR(lpi->ls_dev);
        lpi->ls_dev = NULL;
        goto err_create_ls_device;
    }

    /* register the attributes */
    ret = device_create_file(lpi->ls_dev, &dev_attr_als_enable);
    if (ret)
        goto err_create_ls_device_file;

    lpi->ps_dev = device_create(lpi->al3006_class,
                NULL, 0, "%s", "proximity");
    if (unlikely(IS_ERR(lpi->ps_dev))) {
        ret = PTR_ERR(lpi->ps_dev);
        lpi->ps_dev = NULL;
        goto err_create_ps_device;
    }

    /* register the attributes */
    ret = device_create_file(lpi->ps_dev, &dev_attr_ps_enable);
    if (ret)
        goto err_create_ps_device_file1;

    /* register the attributes */
    ret = device_create_file(lpi->ps_dev, &dev_attr_dump_register);
    if (ret)
        goto err_create_ps_device_file2;

#if 0
	/* register the attributes */
	ret = device_create_file(lpi->ps_dev, &dev_attr_ps_led);
	if (ret)
		goto err_create_ps_device;
#endif
	lpi->early_suspend.level =
			EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	lpi->early_suspend.suspend = al3006_early_suspend;
	lpi->early_suspend.resume = al3006_late_resume;
	register_early_suspend(&lpi->early_suspend);

    lpi->als_enable = 0;
    lpi->ps_enable = 0;
    lpi->ps_irq_flag = 0;

	D("[AL3006] %s: Probe success!\n", __func__);
	printk("[AL3006] Probe and setup success!\n");

	return ret;

err_create_ps_device_file2:
    device_remove_file(lpi->ps_dev, &dev_attr_ps_enable);
err_create_ps_device_file1:
	device_unregister(lpi->ps_dev);
err_create_ps_device:
    device_remove_file(lpi->ls_dev, &dev_attr_als_enable);
err_create_ls_device_file:
	device_unregister(lpi->ls_dev);
err_create_ls_device:
	class_destroy(lpi->al3006_class);
err_create_class:
err_al3006_setup:
	wake_lock_destroy(&(lpi->ps_wake_lock));
	destroy_workqueue(lpi->lp_wq);
err_create_singlethread_workqueue:
    free_irq(lpi->irq, lpi);
	input_unregister_device(lpi->ps_input_dev);
	input_free_device(lpi->ps_input_dev);
	misc_deregister(&psensor_misc);
err_psensor_setup:
    regulator_disable(lpi->ls_regulator);/*disable power*/
	input_unregister_device(lpi->ls_input_dev);
	input_free_device(lpi->ls_input_dev);
	misc_deregister(&lightsensor_misc);
err_lightsensor_setup:
	mutex_destroy(&als_enable_mutex);
	mutex_destroy(&als_disable_mutex);
	mutex_destroy(&als_get_adc_mutex);
    pdata->exit(NULL); /* free gpio request */
err_platform_data_null:
	kfree(lpi);
	return ret;
}

static const struct i2c_device_id al3006_i2c_id[] = {
	{AL3006_I2C_NAME, 0},
	{}
};

static struct i2c_driver al3006_driver = {
	.id_table = al3006_i2c_id,
	.probe = al3006_probe,
	.driver = {
		.name = AL3006_I2C_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init al3006_init(void)
{
	return i2c_add_driver(&al3006_driver);
}

static void __exit al3006_exit(void)
{
#if 0
    if (lpi->ls_regulator)
		regulator_put(lpi->ls_regulator);
#endif
	i2c_del_driver(&al3006_driver);
}

module_init(al3006_init);
module_exit(al3006_exit);

MODULE_DESCRIPTION("AL3006 Driver");
MODULE_LICENSE("GPL");
