#ifndef __DSI_FB_H__
#define __DSI_FB_H__

#ifdef DSI_FB_DEBUG
#define dsifb_debug(fmt, arg...)	\
	printk("%s:%d " fmt, __func__, __LINE__, ##arg)
#else
#define dsifb_debug(fmt, arg...)	\
	do {	} while (0)
#endif /* DSI_FB_DEBUG */

#define dsifb_info(fmt, arg...)	\
	printk(KERN_INFO"%s:%d " fmt, __func__, __LINE__, ##arg)

#define dsifb_error(fmt, arg...)	\
	printk(KERN_ERR"%s:%d " fmt, __func__, __LINE__, ##arg)

#define dsifb_warning(fmt, arg...)	\
	printk(KERN_WARNING"%s:%d " fmt, __func__, __LINE__, ##arg)

#define dsifb_alert(fmt, arg...)	\
	printk(KERN_ALERT"%s:%d " fmt, __func__, __LINE__, ##arg)

#endif /* __RHEA_FB_H__ */
