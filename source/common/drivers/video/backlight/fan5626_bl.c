/*
 * linux/drivers/video/backlight/fan5626_bl.c
 *
 * simple PWM based backlight control, board code has to setup
 * 1) pin configuration so PWM waveforms can output
 * 2) platform_data being correctly configured
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <plat/pwm/consumer.h>
#include <linux/gpio.h>
#include <linux/delay.h>

struct pwm_bl_data {
	struct device *dev;
	unsigned int period;
        bool suspend_flag;
	int (*notify) (struct device *, int brightness);
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct backlight_device *bl;
	struct early_suspend early_suspend_desc;
	bool initialised;
#endif
	int last_brightness;
};

#define GPIO_LCD_BACKLIGHT	(17)
#define MAX_BRIGHTNESS_LEVEL (32)
static int pwm_backlight_update_status(struct backlight_device *bl)
{
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);
	int brightness = bl->props.brightness;
	int max = bl->props.max_brightness;
	unsigned long long place_holder;

	if(!pb->initialised)
		return -ENODEV;

	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	if (pb->notify)
		brightness = pb->notify(pb->dev, brightness);
	
	if(brightness > MAX_BRIGHTNESS_LEVEL)
			return 0;
			
	if (brightness == 0) {
		gpio_request(GPIO_LCD_BACKLIGHT, "gpio_bl");
		gpio_direction_output(GPIO_LCD_BACKLIGHT, 1);
		gpio_set_value(GPIO_LCD_BACKLIGHT, 0);
		udelay(10);
		gpio_free(GPIO_LCD_BACKLIGHT);
	} else {
		int loop = 0;
		int i = 0;

		if(pb->suspend_flag == 1)
			return 0;

		if(brightness <= pb->last_brightness)
			loop = pb->last_brightness-brightness;
		else
			loop = MAX_BRIGHTNESS_LEVEL - (brightness - pb->last_brightness);
			
		gpio_request(GPIO_LCD_BACKLIGHT, "gpio_bl");
		gpio_direction_output(GPIO_LCD_BACKLIGHT, 1);
		 
		if((pb->last_brightness == 0)&&(brightness != 0))//turn on to level 32
		{
			gpio_set_value(GPIO_LCD_BACKLIGHT, 1);
			udelay(250);		
			loop = MAX_BRIGHTNESS_LEVEL-brightness;//adjust backlight start from 32 level
		}
	
 		for(i = 0; i < loop; i++)
		{
			gpio_set_value(GPIO_LCD_BACKLIGHT, 0);
			udelay(10);
			gpio_set_value(GPIO_LCD_BACKLIGHT, 1);
			udelay(10);
		}	
		
		gpio_free(GPIO_LCD_BACKLIGHT);
	}

	pb->last_brightness = brightness;	
	return 0;
}

static int pwm_backlight_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static const struct backlight_ops pwm_backlight_ops = {
	.update_status	= pwm_backlight_update_status,
	.get_brightness	= pwm_backlight_get_brightness,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void pwm_backlight_earlysuspend(struct early_suspend *desc)
{
	struct pwm_bl_data *pb = container_of(desc, struct pwm_bl_data,
							early_suspend_desc);
	gpio_request(GPIO_LCD_BACKLIGHT, "gpio_bl");
	gpio_direction_output(GPIO_LCD_BACKLIGHT, 1);
	gpio_set_value(GPIO_LCD_BACKLIGHT, 0);
	udelay(10);
	gpio_free(GPIO_LCD_BACKLIGHT);
	
	pb->initialised = false;
	pb->suspend_flag = 1;
}

static void pwm_backlight_earlyresume(struct early_suspend *desc)
{
	struct pwm_bl_data *pb = container_of(desc, struct pwm_bl_data,
							early_suspend_desc);
	pb->initialised = true;
	pb->suspend_flag = 0;
	backlight_update_status(pb->bl);
}
#else
#ifdef CONFIG_PM
static int pwm_backlight_suspend(struct platform_device *pdev,
						pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	gpio_request(GPIO_LCD_BACKLIGHT, "gpio_bl");
	gpio_direction_output(GPIO_LCD_BACKLIGHT, 1);
	gpio_set_value(GPIO_LCD_BACKLIGHT, 0);
	udelay(10);
	gpio_free(GPIO_LCD_BACKLIGHT);
	
	pb->initialised = false;
	return 0;
}

static int pwm_backlight_resume(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);

	pb->initialised = true;
	backlight_update_status(bl);
	return 0;
}
#else
#define pwm_backlight_suspend          NULL
#define pwm_backlight_resume            NULL
#endif
#endif

static int pwm_backlight_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	struct platform_pwm_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl;
	struct pwm_bl_data *pb;
	int ret;

	if (!data) {
		dev_err(&pdev->dev, "failed to find platform data\n");
		return -EINVAL;
	}

	if (data->init) {
		ret = data->init(&pdev->dev);
		if (ret < 0)
			return ret;
	}

	pb = kzalloc(sizeof(*pb), GFP_KERNEL);
	if (!pb) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	pb->period = data->pwm_period_ns;
	pb->notify = data->notify;
	pb->dev = &pdev->dev;
	pb->suspend_flag = 0;
	
        memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = data->max_brightness;
	bl = backlight_device_register(dev_name(&pdev->dev), &pdev->dev, pb,
				       &pwm_backlight_ops, &props);
	pb->initialised = true;
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_pwm;
	}

	bl->props.brightness = data->dft_brightness;
	pb->last_brightness = data->dft_brightness;

	backlight_update_status(bl);

#ifdef CONFIG_HAS_EARLYSUSPEND
	pb->early_suspend_desc.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	pb->early_suspend_desc.suspend = pwm_backlight_earlysuspend;
	pb->early_suspend_desc.resume = pwm_backlight_earlyresume;
	register_early_suspend(&pb->early_suspend_desc);
	pb->bl = bl;
#endif

	platform_set_drvdata(pdev, bl);
	return 0;

err_pwm:
	kfree(pb);
err_alloc:
	if (data->exit)
		data->exit(&pdev->dev);
	return ret;
}

static int pwm_backlight_remove(struct platform_device *pdev)
{
	struct platform_pwm_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&pb->early_suspend_desc);
#endif
	kfree(pb);
	if (data->exit)
		data->exit(&pdev->dev);
	return 0;
}

static struct platform_driver pwm_backlight_driver = {
	.driver = {
		   .name = "pwm-backlight",
		   .owner = THIS_MODULE,
		   },
	.probe = pwm_backlight_probe,
	.remove = pwm_backlight_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = pwm_backlight_suspend,
	.resume = pwm_backlight_resume,
#endif
};

static int __init pwm_backlight_init(void)
{
	return platform_driver_register(&pwm_backlight_driver);
}
late_initcall(pwm_backlight_init);

static void __exit pwm_backlight_exit(void)
{
	platform_driver_unregister(&pwm_backlight_driver);
}
module_exit(pwm_backlight_exit);

MODULE_DESCRIPTION("PWM based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pwm-backlight");

