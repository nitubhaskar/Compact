/*******************************************************************************
* Copyright 2011 Broadcom Corporation.  All rights reserved.
*
*	@file	drivers/video/broadcom/dsi/dsi_fb.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/acct.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/vt_kern.h>
#include <linux/gpio.h>
#include <mach/io.h>
#include <plat/syscfg.h>
#include <linux/broadcom/lcd.h>//for LCD_DirtyRect_t


#ifdef CONFIG_FRAMEBUFFER_FPS
#include <linux/fb_fps.h>
#endif

#ifdef CONFIG_ANDROID_POWER
#include <linux/android_power.h>
#endif

//#define DSI_FB_DEBUG 
#include "dsi_fb.h"
#include <plat/mobcom_types.h>
#include "lcd/display_drv.h"
#include "lcd/dsic.h"

struct dsi_fb {
	dma_addr_t phys_fbbase;
	spinlock_t lock;
	struct task_struct *thread;
	struct semaphore thread_sem;
	struct semaphore update_sem;
	struct semaphore prev_buf_done_sem;
	atomic_t buff_idx;
	atomic_t is_fb_registered;
	atomic_t is_graphics_started;
	int base_update_count;
	int rotation;
	int is_display_found;
#ifdef CONFIG_FRAMEBUFFER_FPS
	struct fb_fps_info *fps_info;
#endif	
	struct fb_info fb;
	u32	cmap[16];
	const DISPDRV_INFO_T *display_info;
	DISPDRV_HANDLE_T display_hdl; 
#ifdef CONFIG_ANDROID_POWER
	android_early_suspend_t early_suspend;
#endif
};

static struct dsi_fb *g_dsi_fb = NULL;

#ifdef CONFIG_REGULATOR
static struct regulator *lcdc_regulator = NULL;
#endif

static inline u32 convert_bitfield(int val, struct fb_bitfield *bf)
{
	unsigned int mask = (1 << bf->length) - 1;

	return (val >> (16 - bf->length) & mask) << bf->offset;
}

static int
dsi_fb_setcolreg(unsigned int regno, unsigned int red, unsigned int green,
		 unsigned int blue, unsigned int transp, struct fb_info *info)
{
	struct dsi_fb *fb = container_of(info, struct dsi_fb, fb);

	dsifb_debug("RHEA regno = %d r=%d g=%d b=%d\n", regno, red, green, blue);

	if (regno < 16) {
		fb->cmap[regno] = convert_bitfield(transp, &fb->fb.var.transp) |
				  convert_bitfield(blue, &fb->fb.var.blue) |
				  convert_bitfield(green, &fb->fb.var.green) |
				  convert_bitfield(red, &fb->fb.var.red);
		return 0;
	}
	else {
		return 1;
	}
}

static int dsi_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	dsifb_debug("RHEA %s\n", __func__);

	if((var->rotate & 1) != (info->var.rotate & 1)) {
		if((var->xres != info->var.yres) ||
		   (var->yres != info->var.xres) ||
		   (var->xres_virtual != info->var.yres) ||
		   (var->yres_virtual > info->var.xres * 2) ||
		   (var->yres_virtual < info->var.xres )) {
			dsifb_error("fb_check_var_failed\n");
			return -EINVAL;
		}
	} else {
		if((var->xres != info->var.xres) ||
		   (var->yres != info->var.yres) ||
		   (var->xres_virtual != info->var.xres) ||
		   (var->yres_virtual > info->var.yres * 2) ||
		   (var->yres_virtual < info->var.yres )) {
			dsifb_error("fb_check_var_failed\n");
			return -EINVAL;
		}
	}

	if((var->xoffset != info->var.xoffset) ||
	   (var->bits_per_pixel != info->var.bits_per_pixel) ||
	   (var->grayscale != info->var.grayscale)) {
		dsifb_error("fb_check_var_failed\n");
		return -EINVAL;
	}

	if ((var->yoffset != 0) &&
		(var->yoffset != info->var.yres)) {
		dsifb_error("fb_check_var failed\n");
		dsifb_alert("BRCM fb does not support partial FB updates\n");
		return -EINVAL;
	}

	return 0;
}

static int dsi_fb_set_par(struct fb_info *info)
{
	struct dsi_fb *fb = container_of(info, struct dsi_fb, fb);

	dsifb_debug("RHEA %s\n", __func__);

	if(fb->rotation != fb->fb.var.rotate) {
		dsifb_warning("Rotation is not supported yet !\n");
		return -EINVAL;
	}

	return 0;
}

static void dsi_display_done_cb(int status)
{	
	(void)status;
	up(&g_dsi_fb->prev_buf_done_sem);
}

static int dsi_fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	int ret = 0;
	struct dsi_fb *fb = container_of(info, struct dsi_fb, fb);
	uint32_t buff_idx;
#ifdef CONFIG_FRAMEBUFFER_FPS
	void *dst;
#endif


	if (var->reserved[0] == 'TDPU') {//Kevin.Wen:FIXME
			/* Android partial update */
			/* reserved[0] = 0x54445055; "UPDT" */
			/* reserved[1] = left | (top << 16); */
			/* reserved[2] = (left + width) | \
			   ((top + height) << 16); */
			LCD_DirtyRect_t dirtyRect;
			dirtyRect.left = (u16) var->reserved[1];
			dirtyRect.top = var->yoffset + (var->reserved[1] >> 16);
			/* adjust by 1 as dirtyRect.right is inclusive */
			dirtyRect.right = (u16) var->reserved[2] - 1;
			/* adjust by 1 as dirtyRect.bottom is inclusive */
			dirtyRect.bottom = var->yoffset +
			    ((var->reserved[2] >> 16) - 1);

//			pr_err("dsi_fb_pan_display partial update left:%d top:%d right:%d bottom:%d ",
//				dirtyRect.left,
//				dirtyRect.top,
//				dirtyRect.right,
//				dirtyRect.bottom
//				);
		} 
		
	/* We are here only if yoffset is '0' or 'yres',
	 * so if yoffset = 0, update first buffer or update second
	 */
	buff_idx = var->yoffset ? 1 : 0;

	//dsifb_error("dsi %s with buff_idx =%d \n", __func__, buff_idx);

	if (down_killable(&fb->update_sem))
		return -EINTR;

	atomic_set(&fb->buff_idx, buff_idx);

#ifdef CONFIG_FRAMEBUFFER_FPS
	dst = (fb->fb.screen_base) + 
		(buff_idx * fb->fb.var.xres * fb->fb.var.yres * (fb->fb.var.bits_per_pixel/8));
	fb_fps_display(fb->fps_info, dst, 5, 2, 0);
#endif

	if (!atomic_read(&fb->is_fb_registered)) {
		ret = dsic_update(fb->display_hdl, buff_idx, NULL /* Callback */);
	} else {
		atomic_set(&fb->is_graphics_started, 1);
		down(&fb->prev_buf_done_sem);
		ret = dsic_update(fb->display_hdl, buff_idx,(DISPDRV_CB_T)dsi_display_done_cb);
	}
	
	up(&fb->update_sem);
	//dsifb_error("dsi Display is updated once at %d time with yoffset=%d\n", fb->base_update_count, var->yoffset);
	return ret;
}

#ifdef CONFIG_ANDROID_POWER
static void dsi_fb_early_suspend(android_early_suspend_t *h)
{
	struct dsi_fb *fb = container_of(h, struct dsi_fb, early_suspend);
	dsifb_info("TODO: BRCM fb early suspend ...\n");
}

static void dsi_fb_late_resume(android_early_suspend_t *h)
{
	struct dsi_fb *fb = container_of(h, struct dsi_fb, early_suspend);
	dsifb_info("TODO: BRCM fb late resume ...\n");
}
#endif


static void reset_display(u32 gpio)
{
	if (gpio != 0) {
		writel(readl(IO_ADDRESS(0x08880000)) & ~(0x22000000),IO_ADDRESS(0x08880000));
		writel(readl(IO_ADDRESS(0x08880000)) | (0x20000000),IO_ADDRESS(0x08880000));

		gpio_request(gpio, "LCD Reset");
		gpio_direction_output(gpio, 1);

		gpio_set_value(gpio, 1);
		msleep(10);
		gpio_set_value(gpio, 0);
		msleep(10);
		gpio_set_value(gpio, 1);
		msleep(120);
		writel(readl(IO_ADDRESS(0x08880000)) & ~(0x22000000),IO_ADDRESS(0x08880000));
	}
}

static int enable_display(struct dsi_fb *fb, u32 gpio)
{
	int ret = 0;
	DISPDRV_OPEN_PARM_T local_DISPDRV_OPEN_PARM_T;

	ret = dsic_init();
	if (ret != 0) {
		dsifb_error("Failed to init this display device!\n");
		goto fail_to_init;
	}
	
#ifdef CONFIG_REGULATOR
	if (!IS_ERR_OR_NULL(lcdc_regulator))
		regulator_enable(lcdc_regulator);
#endif

	/* Hack
	 * Since the display driver is not using this field, 
	 * we use it to pass the dma addr.
	 */
	//reset_display(gpio);

	local_DISPDRV_OPEN_PARM_T.busId = fb->phys_fbbase;
	local_DISPDRV_OPEN_PARM_T.busCh = 0;
	ret = dsic_open((void *)&local_DISPDRV_OPEN_PARM_T, &fb->display_hdl);
	if (ret != 0) {
		dsifb_error("Failed to open this display device!\n");
		goto fail_to_open;
	}

	ret = dsic_start(fb->display_hdl);
	if (ret != 0) {
		dsifb_error("Failed to start this display device!\n");
		goto fail_to_start;
	}

	reset_display(gpio);
	ret = dsic_power_control(fb->display_hdl, DISPLAY_POWER_STATE_ON);
	if (ret != 0) {
		dsifb_error("Failed to power on this display device!\n");
		goto fail_to_power_control;
 	}

 	dsifb_info("dsi display is enabled successfully\n");
	return 0;
 
fail_to_power_control:
	dsic_stop(fb->display_hdl);
fail_to_start:
	dsic_close(fb->display_hdl);
fail_to_open:
	dsic_exit();
fail_to_init:
 	return ret;

}

static int disable_display(struct dsi_fb *fb)
{
	int ret = 0;

	/* TODO:  HACK
	 * Need to fill the blank.
	 */
	dsic_stop(fb->display_hdl);

	dsic_close(fb->display_hdl);

	dsic_exit();

	dsifb_info("dsi display is disabled successfully\n");
	return ret;
}

static struct fb_ops dsi_fb_ops = {
	.owner          = THIS_MODULE,
	.fb_check_var   = dsi_fb_check_var,
	.fb_set_par     = dsi_fb_set_par,
	.fb_setcolreg   = dsi_fb_setcolreg,
	.fb_pan_display = dsi_fb_pan_display,
	.fb_fillrect    = cfb_fillrect,
	.fb_copyarea    = cfb_copyarea,
	.fb_imageblit   = cfb_imageblit,
};

static int dsi_fb_probe(struct platform_device *pdev)
{
	int ret = -ENXIO;
	struct dsi_fb *fb;
	size_t framesize;
	uint32_t width, height;

	//struct kona_fb_platform_data *fb_data;
	uint32_t screen_width;
	uint32_t screen_height;
	uint32_t input_bpp;
	uint32_t reset_gpio;

	if (g_dsi_fb && (g_dsi_fb->is_display_found == 1)) {
		dsifb_info("A right display device is already found!\n");
		return -EINVAL;
	}

	fb = kzalloc(sizeof(struct dsi_fb), GFP_KERNEL);
	if (fb == NULL) {
		dsifb_error("Unable to allocate framebuffer structure\n");
		ret = -ENOMEM;
		goto err_fb_alloc_failed;
	}
	g_dsi_fb = fb;

	//fb_data = pdev->dev.platform_data;
	//if (!fb_data) {
	//	ret = -EINVAL;
	//	goto fb_data_failed;
	//}

	dsic_info(&screen_width, &screen_height, &input_bpp, &reset_gpio);

	dsifb_error("dsi_fb_probe() screen_width:%0d screen_height:%d input_bpp:%d reset_gpio:%d ", screen_height, screen_height, input_bpp, reset_gpio);

	spin_lock_init(&fb->lock);
	platform_set_drvdata(pdev, fb);

	sema_init(&fb->update_sem, 1);
	atomic_set(&fb->buff_idx, 0);
	atomic_set(&fb->is_fb_registered, 0);
	sema_init(&fb->prev_buf_done_sem, 1);
	atomic_set(&fb->is_graphics_started, 0);
	sema_init(&fb->thread_sem, 0);

	/* Hack
	 * The screen info can only be obtained from the display driver;and, therefore, 
	 * only then the frame buffer mem can be allocated.
	 * However, we need to pass the frame buffer phy addr into the CSL layer right before
	 * the screen info can be obtained.
	 * So either the display driver allocates the memory and pass the pointer to us, or
	 * we allocate memory and pass into the display. 
	 */
#if 0
#if defined(CONFIG_FB_BRCM_LCDC_ALEX_DSI_VGA)
	framesize = 640 * 360 * 4 * 2;
#elif defined(CONFIG_FB_BRCM_LCDC_NT35582_SMI_WVGA)
	framesize = 480 * 800 * 2 * 2;
#elif defined(CONFIG_FB_BRCM_LCDC_R61581_SMI_HVGA)
	framesize = 320 * 480 * 2 * 2;
#else 
#error "Wrong LCD configuration!" 
#endif
#endif
	framesize = screen_width * screen_height * 
				input_bpp * 2;

	fb->fb.screen_base = dma_alloc_writecombine(&pdev->dev,
			framesize, &fb->phys_fbbase, GFP_KERNEL);
	if (fb->fb.screen_base == NULL) {
		ret = -ENOMEM;
		dsifb_error("Unable to allocate fb memory\n");
		goto err_fbmem_alloc_failed;
	}
	dsifb_error("before enable_display()");

#ifdef CONFIG_REGULATOR
	lcdc_regulator = regulator_get(NULL, "lcd_vcc");
	if (!IS_ERR_OR_NULL(lcdc_regulator))
	{
		pr_err("regulator_enable(lcdc_regulator)");
		regulator_enable(lcdc_regulator);
	}
#endif

	ret = enable_display(fb, reset_gpio);
	if (ret) {
		dsifb_error("Failed to enable this display device\n");
		goto err_enable_display_failed;
	} else {
		fb->is_display_found = 1;
 	}
	dsifb_error("after enable_display()");

	fb->display_info = dsic_get_info(fb->display_hdl);

	/* Now we should get correct width and height for this display .. */
	width = fb->display_info->width; 
	height = fb->display_info->height;
	BUG_ON(width != screen_width || height != screen_height);

	fb->fb.fbops		= &dsi_fb_ops;
	fb->fb.flags		= FBINFO_FLAG_DEFAULT;
	fb->fb.pseudo_palette	= fb->cmap;
	fb->fb.fix.type		= FB_TYPE_PACKED_PIXELS;
	fb->fb.fix.visual	= FB_VISUAL_TRUECOLOR;
#if 0
#ifdef CONFIG_FB_BRCM_LCDC_ALEX_DSI_VGA
	fb->fb.fix.line_length	= width * 4;
#else
	fb->fb.fix.line_length	= width * 2;
#endif
#endif
	fb->fb.fix.line_length	= width * input_bpp;

	fb->fb.fix.accel	= FB_ACCEL_NONE;
	fb->fb.fix.ypanstep	= 1;
	fb->fb.fix.xpanstep	= 4;

	fb->fb.var.xres		= width;
	fb->fb.var.yres		= height;
	fb->fb.var.xres_virtual	= width;
	fb->fb.var.yres_virtual	= height * 2;
#if 0
#ifdef CONFIG_FB_BRCM_LCDC_ALEX_DSI_VGA
	fb->fb.var.bits_per_pixel = 32;
#else
	fb->fb.var.bits_per_pixel = 16;
#endif
#endif
	fb->fb.var.bits_per_pixel = input_bpp * 8;
	fb->fb.var.activate	= FB_ACTIVATE_NOW;
	fb->fb.var.height	= height;
	fb->fb.var.width	= width;

#if 0
#ifdef CONFIG_FB_BRCM_LCDC_ALEX_DSI_VGA
	fb->fb.var.red.offset = 16;
	fb->fb.var.red.length = 8;
	fb->fb.var.green.offset = 8;
	fb->fb.var.green.length = 8;
	fb->fb.var.blue.offset = 0;
	fb->fb.var.blue.length = 8;
	fb->fb.var.transp.offset = 24;
	fb->fb.var.transp.length = 8;

	framesize = width * height * 4 * 2;
#else
	fb->fb.var.red.offset = 11;
	fb->fb.var.red.length = 5;
	fb->fb.var.green.offset = 5;
	fb->fb.var.green.length = 6;
	fb->fb.var.blue.offset = 0;
	fb->fb.var.blue.length = 5;

	framesize = width * height * 2 * 2;
#endif
#endif
	switch (input_bpp) {
	case 2:
	fb->fb.var.red.offset = 11;
	fb->fb.var.red.length = 5;
	fb->fb.var.green.offset = 5;
	fb->fb.var.green.length = 6;
	fb->fb.var.blue.offset = 0;
	fb->fb.var.blue.length = 5;

	framesize = width * height * 2 * 2;
	break;

	case 4:
	fb->fb.var.red.offset = 16;
	fb->fb.var.red.length = 8;
	fb->fb.var.green.offset = 8;
	fb->fb.var.green.length = 8;
	fb->fb.var.blue.offset = 0;
	fb->fb.var.blue.length = 8;
	fb->fb.var.transp.offset = 24;
	fb->fb.var.transp.length = 8;

	framesize = width * height * 4 * 2;
	break;

	default:
	dsifb_error("Wrong format!\n");
	break;
	}

	fb->fb.fix.smem_start = fb->phys_fbbase;
	fb->fb.fix.smem_len = framesize;

	pr_err("Framebuffer starts at phys[0x%08x], and virt[0x%08x] with frame size[0x%08x]\n",
			fb->phys_fbbase, (uint32_t)fb->fb.screen_base, framesize);

	ret = fb_set_var(&fb->fb, &fb->fb.var);
	if (ret) {
		dsifb_error("fb_set_var failed\n");
		goto err_set_var_failed;
	}
	/* Paint it black (assuming default fb contents are all zero) */
	ret = dsi_fb_pan_display(&fb->fb.var, &fb->fb);
	if (ret) {
		dsifb_error("Can not enable the LCD!\n");
		goto err_fb_register_failed;;
	}
	ret = register_framebuffer(&fb->fb);
	if (ret) {
		dsifb_error("Framebuffer registration failed\n");
		goto err_fb_register_failed;
	}

#ifdef CONFIG_FRAMEBUFFER_FPS
	fb->fps_info = fb_fps_register(&fb->fb);	
	if (NULL == fb->fps_info )
		printk(KERN_ERR "No fps display");
#endif
	up(&fb->thread_sem);

	atomic_set(&fb->is_fb_registered, 1);
	dsifb_info("dsi Framebuffer probe successfull\n");

#ifdef CONFIG_LOGO
	/*  Display the default logo/splash screen. */
	fb_prepare_logo(&fb->fb, 0);
	fb_show_logo(&fb->fb, 0);
	dsic_update(fb->display_hdl, 0, NULL /* Callback */);
#endif


#ifdef CONFIG_ANDROID_POWER
	fb->early_suspend.suspend = dsi_fb_early_suspend;
	fb->early_suspend.resume = dsi_fb_late_resume;
	android_register_early_suspend(&fb->early_suspend);
#endif

	return 0;

err_fb_register_failed:
err_set_var_failed:
	dma_free_writecombine(&pdev->dev, fb->fb.fix.smem_len,
			      fb->fb.screen_base, fb->fb.fix.smem_start);
	disable_display(fb);

err_enable_display_failed:
err_fbmem_alloc_failed:
fb_data_failed:
	kfree(fb);
	g_dsi_fb = NULL;
err_fb_alloc_failed:
	dsifb_alert("dsi Framebuffer probe FAILED !!\n");

	return ret;
}

static int __devexit dsi_fb_remove(struct platform_device *pdev)
{
	size_t framesize;
	struct dsi_fb *fb = platform_get_drvdata(pdev);
	
	framesize = fb->fb.var.xres_virtual * fb->fb.var.yres_virtual * 2;

#ifdef CONFIG_ANDROID_POWER
        android_unregister_early_suspend(&fb->early_suspend);
#endif
	
#ifdef CONFIG_FRAMEBUFFER_FPS
	fb_fps_unregister(fb->fps_info);
#endif
	unregister_framebuffer(&fb->fb);
	disable_display(fb);
	kfree(fb);
	dsifb_info("dsi FB removed !!\n");
	return 0;
}

static struct platform_driver dsi_fb_driver = {
	.probe		= dsi_fb_probe,
	.remove		= __devexit_p(dsi_fb_remove),
	.driver = {
		.name = "dsi_fb"
	}
};

#if 0
static struct platform_device dsi_fb_device = {
	.name    = "dsi_fb",
	.id      = -1,
	.dev = {
		.dma_mask      = (u64 *) ~(u32)0,
		.coherent_dma_mask   = ~(u32)0,
	},
};
#endif

bool lcdc_showing_dump(void)
{//Kevin.Wen:FIXME

	return 0;
}

int lcdc_disp_img(int img_index)
{//Kevin.Wen:FIXME

	return 0;
}

static int __init dsi_fb_init(void)
{
	int ret;

	ret = platform_driver_register(&dsi_fb_driver);
	if (ret) {
		printk(KERN_ERR"%s : Unable to register dsi framebuffer driver\n", __func__);
		goto fail_to_register;
	}

#if 0
	ret = platform_device_register(&dsi_fb_device);
	if (ret) {
		printk(KERN_ERR"%s : Unable to register dsi framebuffer device\n", __func__);
		platform_driver_unregister(&dsi_fb_driver);
		goto fail_to_register;
	}
#endif

fail_to_register:
	printk(KERN_INFO"BRCM Framebuffer Init %s !\n", ret ? "FAILED" : "OK");

	return ret;
}

static void __exit dsi_fb_exit(void)
{
#ifdef CONFIG_REGULATOR
	if (!IS_ERR_OR_NULL(lcdc_regulator)) {
		regulator_put(lcdc_regulator);
		lcdc_regulator = NULL;
	}
#endif

	/* Clean up .. */
	//platform_device_unregister(&dsi_fb_device);
	platform_driver_unregister(&dsi_fb_driver);

	printk(KERN_INFO"BRCM Framebuffer exit OK\n");
}

late_initcall(dsi_fb_init);
module_exit(dsi_fb_exit);

MODULE_AUTHOR("Broadcom");
MODULE_DESCRIPTION("DSI FB Driver");
