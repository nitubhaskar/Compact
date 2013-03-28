/*******************************************************************************************
 * Copyright 2010 Broadcom Corporation.  All rights reserved.
 *
 * Unless you and Broadcom execute a separate written software license agreement
 * governing use of this software, this software is licensed to you under the
 * terms of the GNU General Public License version 2, available at
 * http://www.gnu.org/copyleft/gpl.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this software
 * in any way with any other Broadcom software provided under a license other than
 * the GPL, without Broadcom's express prior written consent.
 * *******************************************************************************************/

/*
 * *
 * *****************************************************************************
 * *
 * *  camera.c
 * *
 * *  PURPOSE:
 * *
 * *     This implements the driver for the stv0987 ISP  camera.
 * *
 * *  NOTES:
 * *
 * *****************************************************************************/

/* ---- Include Files ---------------------------------------------------- */

#include <linux/version.h>
//#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/sysctl.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#if 0
#include <mach/reg_camera.h>
#include <mach/reg_lcd.h>
#endif
#include <mach/reg_clkpwr.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>

#include <linux/semaphore.h>
//#include <linux/broadcom/types.h>
#include <linux/broadcom/bcm_major.h>
#include <linux/broadcom/hw_cfg.h>
#include <linux/broadcom/hal_camera.h>
#include <linux/broadcom/lcd.h>
#include <linux/broadcom/bcm_sysctl.h>
#include <plat/dma.h>
#include <linux/dma-mapping.h>
#include <linux/wakelock.h>
#include <linux/clk.h>
#include <mach/clkmgr.h>
#include <linux/regulator/consumer.h>
#include <plat/syscfg.h>
#if defined (CONFIG_CPU_FREQ_GOV_BCM21553)
#include <mach/bcm21553_cpufreq_gov.h>
#endif

/* For OSDAL MEM to MEM DMA */
#include <plat/types.h>
#include <plat/osdal_os_driver.h>
#include <plat/osdal_os_service.h>
#include <plat/dma_drv.h>


#include "hal_cam_drv_ath.h"
//#include "camdrv_dev.h" swsw_dual
#include <plat/csl/csl_cam.h>

#include <linux/videodev2.h> //BYKIM_CAMACQ
#include "camacq_api.h"
#include "camacq_type.h"

/* FIXME: Clear VSYNC interrupt explicitly until handler properly */
//#define CAM_BOOT_TIME_MEMORY_SIZE (640*480*4)
#define CAM_SEC_OFFSET (1024*1024*3/2)
#define CAM_NUM_VFVIDEO 4
#define XFR_SIZE_MAX (4095*4)
#define XFR_SIZE (4095)
#define MAX_QUEUE_SIZE 6
#define MAX_QUEUE_SIZE_MASK MAX_QUEUE_SIZE
#define SWAP(val)   cpu_to_le32(val)
#define SKIP_STILL_FRAMES 3
//#define IF_NAME             "cami2c"
#define I2C_DRIVERID_CAM    0xf000

#define SZ_12M (1024*1024*12)
#define REFCAPTIME 80000000
#define STILL_PING_PANG 2       // Default value
#define VIDEO_PING_PONG 2

#ifdef CONFIG_BCM_CAM_S5K4ECGX   // for only CooperVE
/*s5k4ecgx*/  
extern int bcm_gpio_pull_up(unsigned int gpio, bool up);
extern int bcm_gpio_pull_up_down_enable(unsigned int gpio, bool enable);
/*s5k4ecgx*/
#endif

struct clk *cam_clk;
/* Camera driver generic data. This contains camera variables that are common
 * across all instances */
enum camera_state {
	CAM_OFF,
	CAM_INIT,
	CAM_PAUSE,
	CAM_ON,
	CAM_STOPPING,
};

enum capture_mode {
	CAM_NONE,
	CAM_STILL,
	CAM_STREAM,
};

struct CAM_DATA{
	int vsyncIrqs;
	int dmaCam;
	int level;
	int blocked;

};

struct CAM_DATA gCamState;

static struct ctl_table gSysCtlCam[] = {
	{
	 .procname = "level",
	 .data = &gCamState.level,
	 .maxlen = sizeof(int),
	 .mode = 0644,
	 .proc_handler = &proc_dointvec},
	{
	 .procname = "vsyncIrqs",
	 .data = &gCamState.vsyncIrqs,
	 .maxlen = sizeof(int),
	 .mode = 0644,
	 .proc_handler = &proc_dointvec},
	{
	 .procname = "dmaCam",
	 .data = &gCamState.dmaCam,
	 .maxlen = sizeof(int),
	 .mode = 0644,
	 .proc_handler = &proc_dointvec},
	{
	 .procname = "blocked",
	 .data = &gCamState.blocked,
	 .maxlen = sizeof(int),
	 .mode = 0644,
	 .proc_handler = &proc_dointvec},
	{}
};

static struct ctl_table gSysCtl[] = {
	{
	 .procname = "camera",
	 .mode = 0555,
	 .child = gSysCtlCam},
	{}
};

static struct ctl_table gSysCtl_sub[] = {
	{
	 .procname = "camera2",
	 .mode = 0555,
	 .child = gSysCtlCam},
	{}
};

struct cam_dma_buf_t {
	size_t sizeInBytes;
	void *virt;
	dma_addr_t phy;
};
struct cam_i2c_info {
	struct i2c_client *client;
};

struct buf_q {
	CAM_BufData data[MAX_QUEUE_SIZE];
	unsigned int Num;
	unsigned int ReadIndex;
	unsigned int WriteIndex;
	spinlock_t lock;
	struct semaphore Sem;
	bool isActive;
	bool isWaitQueue;
};

struct camera_sensor_t {
	int devbusy;
	CamIntfConfig_st_t *sensor_intf;
	struct sens_methods *sens_m;
	CAM_Parm_t main;
	/* Either Still or VF/Video but not both at the same time.*/
	CAM_Parm_t th;
	/* Represents thumbnail only valid with still cap*/
	enum capture_mode mode;
	enum camera_state state;
	
	struct cam_dma_buf_t cam_ll[CAM_NUM_VFVIDEO];
	atomic_t captured;
	int sCaptureFrameCountdown;
	int dma_chan;
	int still_ready;
	int video_ready;
	short *framebuf;
	int gProcessFrameRunning;
	int sCaptureJpegSize;
	int sCaptureRawSize;
	spinlock_t c_lock;
	struct buf_q rd_Q;
	struct buf_q wr_Q;
	CAM_BufData gCurrData;
	struct cam_i2c_info *cam_i2c_datap;
	ktime_t prev;
	CSL_CAM_INTF_CFG_st_t cslCamIntfCfg;
	CSL_CAM_HANDLE hdl;
	int cam_irq;
	u8 *tnptr;
	int bufsw;
	int drop_fps;
	CamZoom_t zoom;
};

struct cam_generic_t {
	struct class *cam_class;
	struct regulator *cam_regulator_i;
	struct regulator *cam_regulator_c;
	struct regulator *cam_regulator_a;
	struct wake_lock camera_wake_lock;
	struct ctl_table_header *gSysCtlHeader;
	CamSensorSelect_t curr;
	struct cam_dma_buf_t cam_buf;	
	struct semaphore cam_sem;
	struct camera_sensor_t sens[2];

    /*
     *   Sensor I2c driver (temp) 
     */
    struct stCamacqSensorManager_t* pstSensorManager;
   // int                             iSelectedSensor; swsw_dual

#if defined (CONFIG_CPU_FREQ_GOV_BCM21553)
	struct cpufreq_client_desc *cam_dvfs;
#endif
};

static struct cam_generic_t *cam_g;
static int gNumOfStillPingPongBuf = STILL_PING_PANG;

struct stCamacqSensorManager_t* GetCamacqSensorManager()
{
    //HalcamTraceDbg( "GetCamacqSensorManager : Enter");
    //HalcamTraceDbg( "    cam_g = %x", cam_g);
    //HalcamTraceDbg( "    cam_g->pstSensorManager = %x", cam_g->pstSensorManager);
    return cam_g->pstSensorManager;
}

DECLARE_WAIT_QUEUE_HEAD(gDataReadyQ);
DECLARE_WAIT_QUEUE_HEAD(gVideoReadyQ);
static char banner[] __initdata =
    KERN_INFO "Camera Driver: 1.00 (built on " __DATE__ " " __TIME__ ")\n";
/* Module static functions prototype begin*/
static int camera_enable(CamSensorSelect_t sensor);
static int camera_disable(CamSensorSelect_t sensor);
static int process_frame(CamSensorSelect_t sensor,unsigned char * jpegBuf, unsigned char * thumBuf);
static int cam_power_down(CamSensorSelect_t sensor);
static int cam_power_up(CamSensorSelect_t sensor);
static int cam_open(struct inode *inode, struct file *file);
static int cam_release(struct inode *inode, struct file *file);
static long cam_ioctl(struct file *file, unsigned int cmd,
		     unsigned long arg);

static void push_queue(struct buf_q *queue, CAM_BufData * buf);
static void deinit_queue(struct buf_q *queue);
static void init_queue(struct buf_q *queue);
static bool pull_queue(struct buf_q *queue, CAM_BufData * buf);
static void wakeup_push_queue(struct buf_q *queue, CAM_BufData * buf);
static bool wait_pull_queue(struct buf_q *queue, CAM_BufData * buf);
static void taskcallback(UInt32 intr_status, UInt32 rx_status, UInt32 image_addr, UInt32 image_size, UInt32 raw_intr_status, UInt32 raw_rx_status, void *userdata);
static int mem_mem_dma(dma_addr_t dst_addr, dma_addr_t src_addr, int dma_tx_size);
extern struct sens_methods *CAMDRV_primary_get(void);
//haipeng dual camera
#if (CAM_SENSOR_NUM==2)
extern struct sens_methods *CAMDRV_secondary_get(void);
#endif
ktime_t t;
static short* capture_buffer_global=NULL;
//extern capture_count;
static void taskcallback(UInt32 intr_status, UInt32 rx_status, UInt32 image_addr, UInt32 image_size, UInt32 raw_intr_status, UInt32 raw_rx_status, void *userdata)
{
	struct camera_sensor_t *c = &cam_g->sens[cam_g->curr];
	int fs = 0, fe = 0;
	fs = intr_status & CSL_CAM_INT_FRAME_START;
	fe = intr_status & CSL_CAM_INT_FRAME_END;
	t = ktime_get();
	if((intr_status & CSL_CAM_INT_FRAME_START) && (c->mode == CAM_STREAM)) {
		writel(0x0100200f, io_p2v(BCM21553_MLARB_BASE + 0x100)); 
		writel(0x44444 , io_p2v(BCM21553_MLARB_BASE + 0x108)); 
		udelay(5);
	}
	/*else if ((intr_status & CSL_CAM_INT_FRAME_START) && (c->mode == CAM_STILL)) {
		u32 rdr3_cnt = readl(io_p2v(0x08440094));
		HalcamTraceErr("################### Value of RDR3 in FS is 0x%x ###################", rdr3_cnt);
	} */
	else if(intr_status & CSL_CAM_INT_FRAME_END){
		if(c->mode == CAM_STREAM) {
			//wakeup_push_queue(&c->rd_Q, &c->gCurrData);
			//pull_queue(&c->wr_Q, &c->gCurrData);
			writel(0x3f0f, io_p2v(BCM21553_MLARB_BASE + 0x100));  
			wake_up_interruptible(&gVideoReadyQ);
			if((c->main.fps == 15) && (c->cslCamIntfCfg.intf == CSL_CAM_INTF_CSI)) {
				c->drop_fps ++;
				c->drop_fps %= 2;
				if (c->drop_fps)
					mem_mem_dma(c->gCurrData.busAddress,(dma_addr_t)image_addr,(c->main.size_window.end_pixel * c->main.size_window.end_line * 2));
			}
			else
				mem_mem_dma(c->gCurrData.busAddress,(dma_addr_t)image_addr,(c->main.size_window.end_pixel * c->main.size_window.end_line * 2));
			/* And then do what ?? */
		} else if(c->mode == CAM_STILL) {

			
			//HalcamTraceErr("FE");
			if(c->state != CAM_PAUSE) {
				c->sCaptureFrameCountdown--;
			}
			//capture_count = c->sCaptureFrameCountdown;
			HalcamTraceDbg("task callbackraw_rx_status = 0x%x, raw_intr_status = 0x%x",raw_rx_status,raw_intr_status);
			//HalcamTraceErr("%s mode stream %d sec %d nsec %d",__FUNCTION__,c->sCaptureFrameCountdown,t.tv.sec,t.tv.nsec);
			if(c->sCaptureFrameCountdown == -1){
				if( (fs != 0) && (fe != 0)) {
					HalcamTraceDbg("Rejecting possible corruption :) ");
					//c->sCaptureFrameCountdown=1;
					//return;
				}
				/* Got our JPEG */
				c->state = CAM_PAUSE;
				/**/{
					u32 rdr3_cnt = readl(io_p2v(0x08440094));
				
				csl_cam_rx_stop(c->hdl);
				capture_buffer_global = (short *)image_addr;
				c->still_ready = 1;
				c->sCaptureJpegSize = image_size;
				//HalcamTraceErr("################### Value of RDR3 in FE is 0x%x ###################", rdr3_cnt);
				}
				writel(0x3f0f, io_p2v(BCM21553_MLARB_BASE + 0x100)); //BMARBL_MACONF0 
				/* allow process context to process next frame */
				wake_up_interruptible(&gDataReadyQ);
			}		
		}
	}
	
}

int ccic_sensor_attach( struct stCamacqSensorManager_t *pstSensorManager )
{
    int iRet = 0;
    int s = 0,sensor_count=0;//swsw_dual
    struct i2c_client* client = 0;
	unsigned int i=0;//swsw_dual

    HalcamTraceDbg("%s(): ", __FUNCTION__);
    
    if( pstSensorManager == NULL )
    {
        HalcamTraceErr( "pstSensorManager == NULL");
        return -ENODEV;
    }
//swsw_dual
	sensor_count =1;
#if (CAM_SENSOR_NUM==2)
	sensor_count=2;
#endif
	for(i=0;i<sensor_count;i++)
	{

		client = pstSensorManager->GetSensor(pstSensorManager, i)->m_pI2cClient;//swsw_dual
    if( client == NULL )
    {
        HalcamTraceErr( "client == NULL");
        return -ENODEV;
    }

    cam_g->pstSensorManager = pstSensorManager;
    //cam_g->iSelectedSensor = 0; swsw_dual
//swsw_dual s->i
		cam_g->sens[i].cam_i2c_datap = kmalloc(sizeof(struct cam_i2c_info), GFP_KERNEL);
		memset(cam_g->sens[i].cam_i2c_datap, 0, sizeof(struct cam_i2c_info));
    
	// i2c_set_clientdata( client, cam_g->sens[s].cam_i2c_datap );

		cam_g->sens[i].cam_i2c_datap->client = client;
		cam_g->sens[i].cam_irq = client->irq;
	}

    return iRet;
}


static void mem_mem_dma_isr(DMADRV_CALLBACK_STATUS_t status)
{
 struct camera_sensor_t *c = &cam_g->sens[cam_g->curr];  //-->before it is 0
	if(status == DMADRV_CALLBACK_OK)
	{
		//HalcamTraceDbg("Sending: 0x%x", c->gCurrData.busAddress);
		
		
		wakeup_push_queue(&c->rd_Q, &c->gCurrData);
		pull_queue(&c->wr_Q, &c->gCurrData);
	}
}


static int mem_mem_dma(dma_addr_t dst_addr, dma_addr_t src_addr, int dma_tx_size)
{
	DMA_CHANNEL gDmaChannel;
	OSDAL_Dma_Chan_Info dmaChInfoMem;
	OSDAL_Dma_Buffer_List dmaBuffListMem;
	OSDAL_Dma_Data dmaDataMem;
	struct camera_sensor_t *c = &cam_g->sens[cam_g->curr];



	if((dst_addr == (dma_addr_t)NULL) || (src_addr == (dma_addr_t)NULL))
		{
			HalcamTraceDbg("Dst addr is zero, but no guy to receive");
			pull_queue(&c->wr_Q, &c->gCurrData);
			return 0;
		}
	/*Request the channel */
	if (OSDAL_ERR_OK != OSDAL_DMA_Obtain_Channel(OSDAL_DMA_CLIENT_MEMORY, OSDAL_DMA_CLIENT_MEMORY,(UInt32 *)&gDmaChannel))
	{
		HalcamTraceErr("---->OSDAL_DMA_Obtain_Channel failed for channel %d ",gDmaChannel);
		return -1;
	}

	/* Configuring the DMA channel info */
	dmaChInfoMem.type = OSDAL_DMA_FCTRL_MEM_TO_MEM;
	dmaChInfoMem.srcBstSize = OSDAL_DMA_BURST_SIZE_16;
	dmaChInfoMem.dstBstSize = OSDAL_DMA_BURST_SIZE_16;
	dmaChInfoMem.srcDataWidth = OSDAL_DMA_DATA_SIZE_32BIT;
	dmaChInfoMem.dstDataWidth = OSDAL_DMA_DATA_SIZE_32BIT;
	dmaChInfoMem.dstMaster = 1;
	dmaChInfoMem.srcMaster = 1;

	dmaChInfoMem.incMode = OSDAL_DMA_INC_MODE_BOTH;
	dmaChInfoMem.freeChan = TRUE;
	dmaChInfoMem.priority = 2;
	dmaChInfoMem.bCircular = FALSE;
	dmaChInfoMem.alignment = OSDAL_DMA_ALIGNMENT_32;
	dmaChInfoMem.xferCompleteCb = (OSDAL_DMA_CALLBACK)mem_mem_dma_isr;

	if (OSDAL_DMA_Config_Channel(gDmaChannel, &dmaChInfoMem)!= OSDAL_ERR_OK)
	{
		HalcamTraceErr("---->DMADRV_Config_Channel - Failed for channel %d ",gDmaChannel);
		return -1;
	}
	/* end of DMA channel configuration. */

	//Bind the channel
	dmaBuffListMem.buffers[0].srcAddr = src_addr;
	dmaBuffListMem.buffers[0].destAddr = dst_addr;
	dmaBuffListMem.buffers[0].length = dma_tx_size;
	dmaBuffListMem.buffers[0].bRepeat = 0;
	dmaBuffListMem.buffers[0].interrupt = 1;
	dmaDataMem.numBuffer = 1;
	dmaDataMem.pBufList = (OSDAL_Dma_Buffer_List *)&dmaBuffListMem;

	if (OSDAL_DMA_Bind_Data(gDmaChannel, &dmaDataMem) != OSDAL_ERR_OK)
	{
		HalcamTraceErr("DMADRV_Bind_Data - Failed");
		OSDAL_DMA_Release_Channel(gDmaChannel);
		return -EINVAL;
	}

	// Start transfer
	if (OSDAL_DMA_Start_Transfer(gDmaChannel) != OSDAL_ERR_OK)
	{
		HalcamTraceErr("DMADRV_Start_Transfer - Failed for channel %d",gDmaChannel);
		return -EINVAL;
	}
   return 0;	//BYKIM_PREVENT
}

/* Module static functions prototype end*/
static long cam_ioctl(struct file *file, unsigned int cmd,
		     unsigned long arg)
{
//swsw_dual 0->cam_g->curr;
	int sensor = cam_g->curr;
	int ret;
	int rc = 0;

	switch (cmd) {
	case CAM_IOCTL_ENABLE:
		{
			if (arg)
				camera_enable(sensor);
			else
				camera_disable(sensor);
			break;
		}
	case CAM_IOCTL_SET_PARAMS:
		{
			CAM_Parm_t parm;
			ret =
			    copy_from_user(&parm, (CAM_Parm_t *) arg,
					   sizeof(parm));
			if (ret != 0)
				return ret;
			HalcamTraceDbg("&&&&&& IOCTL called %d %d",parm.size_window.end_pixel,parm.size_window.end_line);
			cam_g->sens[sensor].main = parm;

			/* Old code calls CAM_Enable over here .... Not needed for this */
			break;
		}

	case CAM_IOCTL_SET_SENSOR_SETTING_PARAMS:
	{
		CAM_Parm_t parm;
		ret = copy_from_user(&parm, (CAM_Parm_t *) arg, sizeof(parm));
		if (ret != 0)
		{
			HalcamTraceErr("CAM_IOCTL_SET_SENSOR_SETTING_PARAMS: copy failed!!!! ");
			return ret;
		}
		if (cam_g->sens[sensor].sens_m->DRV_SetSensorParams(parm,sensor) != HAL_CAM_SUCCESS) {
			rc = -EFAULT;
			HalcamTraceErr("CAM_IOCTL_SET_SENSOR_SETTING_PARAMS: DRV_SetSensorParams(): ERROR: ");
		}
		break;
	}
	case CAM_IOCTL_SET_THUMBNAIL_PARAMS:
		ret =copy_from_user(&cam_g->sens[sensor].th, (void *)arg,sizeof(CAM_Parm_t));
		if(ret !=0)
		{
			HalcamTraceErr(" CAM_IOCTL_SET_THUMBNAIL_PARAMS: copy to user ERROR: ");
			return ret;
		}		
		
		break;
	case CAM_IOCTL_MEM_REGISTER:
		{
			CAM_BufData p;
			ret = copy_from_user(&p, (CAM_BufData *) arg,sizeof(CAM_BufData));
 	                if (ret != 0)
 			{
 				HalcamTraceErr(" CAM_IOCTL_MEM_REGISTER: copy to user ERROR: ");
 				return ret;
 			}		
			
			cam_g->cam_buf.phy = (dma_addr_t) p.busAddress;
			cam_g->cam_buf.virt =__va( (dma_addr_t) p.busAddress);			

			break;
		}
	case CAM_IOCTL_MEM_BUFFERS:
		{
			CAM_BufData buf;
			ret =copy_from_user(&buf, (CAM_BufData *) arg,  sizeof(CAM_BufData));
 	                if (ret != 0)
 			{
 				HalcamTraceErr(" CAM_IOCTL_MEM_BUFFERS: copy to user ERROR: ");
 				return ret;
 			}		
			push_queue(&cam_g->sens[sensor].wr_Q, &buf);
			
			break;
		}
	case CAM_IOCTL_GET_FRAME:
		{
			CAM_BufData buf;
			struct camera_sensor_t *c;
			c = &cam_g->sens[sensor];
			/* For VF and Video */
			if(Drv_Scene==CamSceneMode_Auto) //check for only test mode
            {
               if(interruptible_sleep_on_timeout(&gVideoReadyQ, msecs_to_jiffies(1500))==0)
               {
                   HalcamTraceDbg("Video frame timeout, err!");
				   {	// Sreedhar's patch for debugging the timeout error in getting frame
						U32 r;

						HalcamTraceErr(" still =%d \n , buffer =%x",c->still_ready,capture_buffer_global);   

						r= readl(io_p2v(0x08440c00));
						HalcamTraceErr("CAM_CPIS = %ld  ",r);

						r = readl(io_p2v(0x08440c04));
						HalcamTraceErr("CAM_CPIR = %ld  ",r);

						r = readl(io_p2v(0x08440c08));
						HalcamTraceErr("CAM_CPIF = %ld  ",r);

						r = readl(io_p2v(0x08440c0c));
						HalcamTraceErr("CAM_CPIW = %ld  ",r);

						r = readl(io_p2v(0x08440c10));
						HalcamTraceErr("CAM_CPIW VC= %ld  ",r);

						r = readl(io_p2v(0x08440c14));
						HalcamTraceErr("CAM_CPIW VS= %ld  ",r);

						r = readl(io_p2v(0x08440c18));
						HalcamTraceErr("CAM_CPIW HC= %ld  ",r);

						r = readl(io_p2v(0x08440c1c));
						HalcamTraceErr("CAM_CPIW HS= %ld  ",r);

						r = readl(io_p2v(0x08440c20));
						HalcamTraceErr("CAM_CPIW M= %ld  ",r);

						r = readl(io_p2v(0x08440c24));
						HalcamTraceErr("CAM_CPIPIB= %ld  ",r);

                  	//	panic("Forced assertion");
 				   }
                   return -2;
               }
            }
			wait_pull_queue(&cam_g->sens[sensor].rd_Q, &buf);
			ret = copy_to_user((void *)arg, &buf, sizeof(buf));
 	                if (ret != 0)
 			{
 				HalcamTraceErr(" CAM_IOCTL_GET_FRAME: copy to user ERROR: ");
 				return ret;
 			}	
			
			break;
		}
	case CAM_IOCTL_GET_JPEG_AND_THUMBNAIL:
		{
			/* Block for still capture */
			CAM_Jpeg_Thumbnail_t frame;
			int copy = 0;
			HalcamTraceDbg("CAM_IOCTL_GET_JPEG_AND_THUMBNAIL called arg 0x%x frame 0x%x",arg,&frame);
			if (copy_from_user
			    (&frame, (CAM_Jpeg_Thumbnail_t *) arg,
			     sizeof(CAM_Jpeg_Thumbnail_t)) != 0)
			       {
	 				HalcamTraceErr(" CAM_IOCTL_GET_JPEG_AND_THUMBNAIL: copy to user ERROR: ");
				ret = -EFAULT;
	 				return ret;
	 			}   
			else {

#ifdef CONFIG_BCM_CAM_ISX005
//swsw_dual
				extern void CAMDRV_CheckJpegStatus(void);	  //For ISX500 sensor

				CAMDRV_CheckJpegStatus();	
#endif		
	
				if (process_frame(sensor,frame.jpegBuf,frame.thumbBuf) != 1)
				{   
                    rc = -EFAULT;
				       HalcamTraceErr( "process frame error = %d ",rc);
				}
                
				if (cam_g->sens[sensor].sCaptureJpegSize)
					frame.jpegLength = (cam_g->sens[sensor].sCaptureJpegSize + 1) >> 1;
			
				ret = copy_to_user((CAM_Frame1_t *) arg, &frame, sizeof(frame)); //BYKIM_PREVENT
	 	                if (ret != 0)
	 			{
	 				HalcamTraceErr(" CAM_IOCTL_GET_JPEG_AND_THUMBNAIL: copy to user ERROR: ");
	 				return ret;
	 			}	
			}
			break;
		}
	case CAM_IOCTL_ENABLE_AUTOFOCUS:
		{
			if (cam_g->sens[sensor].sens_m->DRV_TurnOnAF(sensor) != HAL_CAM_SUCCESS) {
				rc = -EFAULT;
				HalcamTraceErr("CAM_IOCTL_ENABLE_AUTOFOCUS: CAMDRV_TurnOnAF(): ERROR: ");
			}
			break;
		}
	case CAM_IOCTL_DISABLE_AUTOFOCUS:
		{
			if (cam_g->sens[sensor].sens_m->DRV_TurnOffAF(sensor) != HAL_CAM_SUCCESS) {
				rc = -EFAULT;
				HalcamTraceErr("CAM_IOCTL_DISABLE_AUTOFOCUS: CAMDRV_TurnOffAF(): ERROR: ");
			}
			break;
		}
			
    case CAM_IOCTL_CANCEL_AUTOFOCUS:
		if (cam_g->sens[sensor].sens_m->DRV_CancelAF(sensor) != HAL_CAM_SUCCESS) {
				rc = -EFAULT;
				printk(KERN_INFO"CAM_IOCTL_CANCEL_AUTOFOCUS: CAM_IOCTL_CANCEL_AUTOFOCUS(): ERROR: \r\n");
			}
          else
            printk(KERN_INFO"CAM_IOCTL_CANCEL_AUTOFOCUS: SUCCESS \r\n");
		
		break;
			
	case CAM_IOCTL_GET_STILL_YCbCr:
		{
			CAM_Frame1_t frame;
			int length;
			short *fbuf;
			HalcamTraceDbg("CAM_IOCTL_GET_STILL_YCbCr called");
			if (0 != (copy_from_user(&frame, (void *)arg, sizeof(CAM_Frame1_t)))) {
                HalcamTraceErr("error::0 != (copy_from_user(&frame, (void *)arg, sizeof(CAM_Frame1_t))):: return -EINVAL");
				return -EINVAL;
			}

			if (process_frame(sensor,NULL,NULL) != 1)
			{
	          HalcamTraceErr("error::process_frame(sensor,NULL,NULL) != 1::  rc = -EFAULT");
                rc = -EFAULT;
			}
			length = (cam_g->sens[sensor].sCaptureRawSize + 1) >> 1;
			frame.len = length;
			frame.buffer = capture_buffer_global;
			HalcamTraceErr("frame.buffer = 0x%x ",frame.buffer);
			//HalcamTraceErr("\n Copying to user %d bytes from 0x%x",frame.len,(u32)cam_g->cam_buf_main.virt);
			if (copy_to_user((CAM_Frame1_t *) arg, &frame, sizeof(frame)) != 0)
    {         
                HalcamTraceErr("error::copy_to_user((CAM_Frame1_t *) arg, &frame, sizeof(frame)) != 0::  rc = -EFAULT");
				return -EFAULT;
             }
			break;
		}

#if 1   // Albert_Chung 12-01-2011 : Test for CSP_478144
	case CAM_IOCTL_GET_STILL_YCbCr_BufferVA:
		{
            unsigned char *capBufVA;

			capBufVA = (unsigned char *)__va(capture_buffer_global);

            printk("[Albert] __va(capture_buffer_global) = 0x%x \n",__va(capture_buffer_global));
            printk("[Albert] capBufVA = 0x%x \n",capBufVA);
			if (copy_to_user((void *)arg, capBufVA, sizeof(int)) != 0)
				return -EFAULT;
			break;
		}
#endif

	case CAM_IOCTL_GET_DEFAULT_SETTING:
		{	

		struct camera_sensor_t *c;
		HalcamTraceDbg("%s called ",__FUNCTION__);	
		c = &cam_g->sens[sensor];
		c->sensor_intf = c->sens_m->DRV_GetIntfConfig(sensor);
		if (!c->sensor_intf) {
			HalcamTraceErr("Unable to get sensor interface config ");
            return -EFAULT; //BYKIM_PREVENT
			
		}
#if 0
			HalcamTraceDbg("==== dump sensor default setting=====");
			HalcamTraceDbg(" stillMode_noOf_res = %d",c->sensor_intf->sensor_default_setting->stillMode_noOf_res);
			HalcamTraceDbg(" w = %d, h = %d ",c->sensor_intf->sensor_default_setting->stillMode_res[0][0],c->sensor_intf->sensor_default_setting->stillMode_res[0][1]);
			HalcamTraceDbg(" stillMode_noOf_formats = %d",c->sensor_intf->sensor_default_setting->stillMode_noOf_formats);
#endif
		
			ret = copy_to_user((void *)arg, c->sensor_intf->sensor_default_setting,sizeof(CAM_Sensor_Supported_Params_t)); //BYKIM_PREVENT
	                if (ret != 0)
			{
				HalcamTraceErr(" CAM_IOCTL_GET_DEFAULT_SETTING: copy to user ERROR: ");
				return ret;
			}
		}
		break;

	case CAM_IOCTL_GET_SENSOR_VALUES_FOR_EXIF:
		{
			CAM_Sensor_Values_For_Exif_t exif_param = {NULL};
			
			if (cam_g->sens[sensor].sens_m->DRV_GetSensorValuesForEXIF(&exif_param,sensor) != HAL_CAM_SUCCESS) 
			{
			 	rc = -EFAULT;
				HalcamTraceErr("CAM_IOCTL_GET_SENSOR_VALUES_FOR_EXIF: DRV_GetSensorValuesForEXIF(): ERROR: ");
			}
						
			ret = copy_to_user((void *)arg, &exif_param,sizeof(CAM_Sensor_Values_For_Exif_t));
			if (ret != 0)
			{
				HalcamTraceErr(" CAM_IOCTL_GET_SENSOR_VALUES_FOR_EXIF: copy to user ERROR: ");
				return ret;
			}
		
			break;
		}

	case CAM_IOCTL_GET_ESD_VALUE:
		{
			bool  esdValue;
			
			if (cam_g->sens[sensor].sens_m->DRV_GetESDValue(&esdValue,sensor) != HAL_CAM_SUCCESS) 
			{
			 	rc = -EFAULT;
				HalcamTraceErr("CAM_IOCTL_GET_ESD_VALUE: DRV_GetESDValue(): ERROR: ");
			}
						
			ret = copy_to_user((void *)arg, &esdValue,sizeof(bool));
			if (ret != 0)
			{
				HalcamTraceErr(" CAM_IOCTL_GET_ESD_VALUE: copy to user ERROR: ");
				return ret;
			}
		
			break;
		}
	
	default:
		HalcamTraceDbg("Default cam IOCTL *************");
		break;
	}
	return rc;
}

static int process_frame(CamSensorSelect_t sensor, unsigned char* jpegBuf, unsigned char * thumBuf)
{
	struct camera_sensor_t *c = &cam_g->sens[sensor];
	int tp;
	u8 *src;
	u8 *dst;
	int num_pack = 0;
	int i = 0;
        int ret= 1;
        int jpeglength= 0;
	int status;
	/*  processFrame is not a kernel thread anymore
	 *  Just a regular blocking function invoked from process context
	 *  Since process context can sleep, we don't see any need for a separate
	 *  kernel thread to carry-out JPEG processing */

	/* While(1) and break is required as the STV sensor operates that way */
	while (1) {
				HalcamTraceDbg("JPEG size before is %d",c->sCaptureJpegSize);
	/* Block waiting to be signalled by either VSYNC ISR or camera disable */
		if (0 != wait_event_interruptible_timeout(gDataReadyQ, c->still_ready,msecs_to_jiffies(6000))) 
		{
			c->still_ready = 0;
/* A Valid wake-up by VSYNC ISR has gProcessFrameRunning = 1.
 * The disable method wakes up with gProcessFrameRunning = 0 which means exit */
			if (!c->gProcessFrameRunning)
				break;
			atomic_set(&c->captured, 1);
#if 0 //haipeng
			tp = c->sens_m->DRV_GetJpegSize(sensor,NULL);
			if(c->sCaptureJpegSize == 0){
				HalcamTraceErr("Major problem with JPEG capture .. size 0 returned");
			} else {
				HalcamTraceDbg("JPEG size = %d Sensor size %d",c->sCaptureJpegSize,tp);
			}
			c->sCaptureJpegSize = tp;
#endif 		
			if(c->main.format != CamDataFmtJPEG){
				HalcamTraceDbg("YUV Capture ....");
				/* No Thumbnail */
				c->sCaptureRawSize = (c->main.size_window.end_pixel*c->main.size_window.end_line*2);
			} 
			else {
#if defined(CONFIG_BCM_CAM_ISX005) 
//swsw_dual
			extern int CAMDRV_DecodeInterleaveData(unsigned char *pInterleaveData, 	// (IN) Pointer of Interleave Data
						 int interleaveDataSize, 			// (IN) Data Size of Interleave Data
						 int yuvWidth, 						// (IN) Width of YUV Thumbnail
						 int yuvHeight, 					// (IN) Height of YUV Thumbnail
						 unsigned char *pJpegData,			// (OUT) Pointer of Buffer for Receiving JPEG Data 
						 int *pJpegSize,  					// (OUT) Pointer of JPEG Data Size
						 unsigned char *pYuvData);			// (OUT) Pointer of Buffer for Receiving YUV Data 
            extern void CAMDRV_ResetMode(void);

			status = CAMDRV_DecodeInterleaveData(__va(capture_buffer_global),/*cam_g->cam_buf.virt, */	// (IN) Pointer of Interleave Data
						2506752, 			// (IN) Data Size of Interleave Data     //1632*1536
						320, 						// (IN) Width of YUV Thumbnail
						240, 					// (IN) Height of YUV Thumbnail
						jpegBuf,			// (OUT) Pointer of Buffer for Receiving JPEG Data 
						 &jpeglength,  					// (OUT) Pointer of JPEG Data Size
						thumBuf);		// (OUT) Pointer of Buffer for Receiving YUV Data 

                       if((status==FALSE)||(jpeglength>=3000000)||(jpeglength == NULL))
                       	{
                            ret = -1;//swsw_dual
							HalcamTraceErr("ERROR ret=%d, jpeglength= %d ",status,jpeglength);
                            jpeglength = 0;
                            CAMDRV_ResetMode();
                       	}
		        HalcamTraceErr("jpeglength= %d ",jpeglength);
	
		    c->sCaptureJpegSize  = jpeglength;

//temp_denis       
#elif defined(CONFIG_BCM_CAM_S5K4ECGX) ||defined(CONFIG_BCM_CAM_S5K5CCGX)
#if 0       // Albert 2011-07-06 : To check JPEG capture
                c->sCaptureJpegSize = 2064*2304;
				HalcamTraceDbg("haipeng capture_buffer_global = 0x%x",capture_buffer_global);
				memcpy(jpegBuf,__va(capture_buffer_global),c->sCaptureJpegSize);
#else
			extern int CAMDRV_DecodeInterleaveData(unsigned char *pInterleaveData, 	// (IN) Pointer of Interleave Data
						 int interleaveDataSize, 			// (IN) Data Size of Interleave Data
						 int yuvWidth, 						// (IN) Width of YUV Thumbnail
						 int yuvHeight, 					// (IN) Height of YUV Thumbnail
						 unsigned char *pJpegData,			// (OUT) Pointer of Buffer for Receiving JPEG Data 
						 int *pJpegSize,  					// (OUT) Pointer of JPEG Data Size
						 unsigned char *pYuvData);			// (OUT) Pointer of Buffer for Receiving YUV Data 

            status= CAMDRV_DecodeInterleaveData(__va(capture_buffer_global),/*cam_g->cam_buf.virt, */  // (IN) Pointer of Interleave Data
                        4755456,            // (IN) Data Size of Interleave Data     //2064*2304
                        640,                        // (IN) Width of YUV Thumbnail
                        480,                    // (IN) Height of YUV Thumbnail
                        jpegBuf,            // (OUT) Pointer of Buffer for Receiving JPEG Data 
                         &jpeglength,                   // (OUT) Pointer of JPEG Data Size
                        thumBuf);       // (OUT) Pointer of Buffer for Receiving YUV Data 

                       if((status==FALSE)||(jpeglength>=4000000)||(jpeglength == NULL))
                        {
                			HalcamTraceErr("ERROR status =%d, jpeglength= %d ",status,jpeglength);
							ret = -1;
                        }
                HalcamTraceErr("jpeglength= %d ",jpeglength);
    
                c->sCaptureJpegSize  = jpeglength;

//c->sCaptureJpegSize  = 4700000;
#endif
#endif

			}

			/* This is where the JPEG would be stored */
			atomic_set(&c->captured, 0);
			break;
		}
		else
		{
			HalcamTraceErr("still capture timeout");
			{	// Sreedhar's patch for debugging the timeout error in getting frame
				U32 r;

				HalcamTraceErr(" still =%d \n , buffer =%x",c->still_ready,capture_buffer_global);   
				r= readl(io_p2v(0x08440c00));
				HalcamTraceErr("CAM_CPIS = %ld",r);

				r = readl(io_p2v(0x08440c04));
				HalcamTraceErr("CAM_CPIR = %ld  ",r);

				r = readl(io_p2v(0x08440c08));
				HalcamTraceErr("CAM_CPIF = %ld  ",r);

				r = readl(io_p2v(0x08440c0c));
				HalcamTraceErr("CAM_CPIW = %ld  ",r);

				r = readl(io_p2v(0x08440c10));
				HalcamTraceErr("CAM_CPIW VC= %ld  ",r);

				r = readl(io_p2v(0x08440c14));
				HalcamTraceErr("CAM_CPIW VS= %ld  ",r);

				r = readl(io_p2v(0x08440c18));
				HalcamTraceErr("CAM_CPIW HC= %ld  ",r);

				r = readl(io_p2v(0x08440c1c));
				HalcamTraceErr("CAM_CPIW HS= %ld  ",r);

				r = readl(io_p2v(0x08440c20));
				HalcamTraceErr("CAM_CPIW M= %ld  ",r);

				r = readl(io_p2v(0x08440c24));
				HalcamTraceErr("CAM_CPIPIB= %ld  ",r);

                //panic("Forced assertion");
			}

			ret = -2;
			break;
		}
	}
	HalcamTraceDbg("Exit process frame (ret = %d)", ret);
	return ret;
}

static struct file_operations cam_fops = {
owner:  THIS_MODULE,
open :	cam_open,
release : cam_release,
unlocked_ioctl : cam_ioctl,
};

/* Queue handling functions begin */
static int64_t systemTime(void)
{
	struct timespec t;
	t.tv_sec = t.tv_nsec = 0;
	ktime_get_ts(&t);
	return (int64_t) (t.tv_sec) * 1000000000LL + t.tv_nsec;
}

static void init_queue(struct buf_q *queue)
{
	if (queue) {
		if (queue->isActive)
			return;
		queue->ReadIndex = 0;
		queue->WriteIndex = 0;
		sema_init(&queue->Sem, 0);
		spin_lock_init(&queue->lock);
		queue->isActive = true;
		queue->isWaitQueue = false;
		queue->Num = 0;
		HalcamTraceDbg("Queue RD and WR %d and %d",queue->ReadIndex,queue->WriteIndex);
	} else {
	}
	/*
	   g_CurrData.id =-1;
	   g_CurrData.busAddress =NULL;
	   g_CurrData.timestamp=0;
	 */
}

static void deinit_queue(struct buf_q *queue)
{
	unsigned long stat;
	if (queue) {
		if (!queue->isActive)
			return;

		spin_lock_irqsave(&queue->lock, stat);
		queue->ReadIndex = 0;
		queue->WriteIndex = 0;
		sema_init(&queue->Sem, 1);
		spin_unlock_irqrestore(&queue->lock, stat);
		queue->isActive = false;
		queue->isWaitQueue = false;
		queue->Num = 0;
	}
}

static void push_queue(struct buf_q *queue, CAM_BufData * buf)
{
	unsigned long stat;
	if (!queue)
		return;
	if (!queue->isActive)
		return;
	if (queue->isWaitQueue)
		return;

	spin_lock_irqsave(&queue->lock, stat);

	
	if (queue->Num == MAX_QUEUE_SIZE) {
		HalcamTraceErr("Push datat fail!!: Camera  Data Queue Full!");
	} else {
		queue->data[queue->WriteIndex].busAddress = buf->busAddress;
		queue->data[queue->WriteIndex].id = buf->id;
		queue->data[queue->WriteIndex].timestamp = buf->timestamp;
		queue->WriteIndex = (queue->WriteIndex + 1) % MAX_QUEUE_SIZE;
		queue->Num++;
	}

	spin_unlock_irqrestore(&queue->lock, stat);
}

static bool pull_queue(struct buf_q *queue, CAM_BufData * buf)
{
	unsigned long stat;
	if (!queue)
		return false;
	if (!queue->isActive)
		return false;
	if (queue->isWaitQueue)
		return false;
	spin_lock_irqsave(&queue->lock, stat);
	
	if (queue->Num == 0) {
		buf->busAddress = NULL;
		buf->id = -1;
	} else {
		buf->busAddress = queue->data[queue->ReadIndex].busAddress;
		buf->id = queue->data[queue->ReadIndex].id;
		buf->timestamp = queue->data[queue->ReadIndex].timestamp;
		queue->ReadIndex = (queue->ReadIndex + 1) % MAX_QUEUE_SIZE;//& MAX_QUEUE_SIZE_MASK;
		queue->Num--;
	//	HalcamTraceDbg("%s ID is %d",__FUNCTION__,buf->id);
	}
	
	spin_unlock_irqrestore(&queue->lock, stat);
	return true;
}
ktime_t new;
static void wakeup_push_queue(struct buf_q *queue, CAM_BufData * buf) //wr_Q
{
	unsigned int NextIndex;
	unsigned long stat;
	if (!queue)
		return;
	if (!queue->isActive)
		return;
	if (!buf)
		return;
	if (!buf->busAddress)
		return;

	spin_lock_irqsave(&queue->lock, stat);
	
	NextIndex = (queue->WriteIndex + 1) % MAX_QUEUE_SIZE_MASK; //& MAX_QUEUE_SIZE_MASK;

	if (queue->Num == MAX_QUEUE_SIZE) {
		 HalcamTraceDbg( "Wake Push datat failed!!l Camera  Data Queue Full!"); 
	} else {
		queue->data[queue->WriteIndex].busAddress = buf->busAddress;
		queue->data[queue->WriteIndex].id = buf->id;
		queue->data[queue->WriteIndex].timestamp = systemTime();
		queue->WriteIndex = (queue->WriteIndex + 1) % MAX_QUEUE_SIZE;
                queue->Num++;
		new = ktime_get();	
		//HalcamTraceDbg("Sec %d nsec %d id %d",new.tv.sec,new.tv.nsec,buf->id);
	}
	
	queue->isWaitQueue = true;
	spin_unlock_irqrestore(&queue->lock, stat);
	//HalcamTraceDbg("%s id is %d",__FUNCTION__,buf->id);
	up(&queue->Sem);

}

static bool wait_pull_queue(struct buf_q *queue, CAM_BufData * buf) //rd-Q
{
	unsigned long stat;
	if (!queue)
		return false;
	if (!queue->isActive)
		return false;

	if (down_interruptible(&queue->Sem)) {
		return false;
	}

	spin_lock_irqsave(&queue->lock, stat);
	if (queue->Num == 0) {
		buf->busAddress = NULL;
		buf->id = -1;
		HalcamTraceErr("Wait WaitPullQueue failed !! Camera  Data Queue Empty!");
	} else {
		buf->busAddress = queue->data[queue->ReadIndex].busAddress;
		buf->id = queue->data[queue->ReadIndex].id;
		buf->timestamp = queue->data[queue->ReadIndex].timestamp;
		queue->ReadIndex = (queue->ReadIndex + 1) % MAX_QUEUE_SIZE;//& MAX_QUEUE_SIZE_MASK;
		queue->Num--;
	}
	
	queue->isWaitQueue = true;
	spin_unlock_irqrestore(&queue->lock, stat);
	return true;
}

/* Queue handling functions end*/

static int cam_i2c_probe(struct i2c_client *client,
			 const struct i2c_device_id *dev_id)
{
	int s = 0; /* For primary sensor */
	HalcamTraceDbg("In I2C probe **** ");
	cam_g->sens[s].cam_i2c_datap =
	    kmalloc(sizeof(struct cam_i2c_info), GFP_KERNEL);
	memset(cam_g->sens[s].cam_i2c_datap, 0, sizeof(struct cam_i2c_info));
	i2c_set_clientdata(client, cam_g->sens[s].cam_i2c_datap);
	cam_g->sens[s].cam_i2c_datap->client = client;
	cam_g->sens[0].cam_irq = client->irq;
	return 0;
}

static int cam_i2c_remove(struct i2c_client *client)
{
	int s = 0;
	kfree(cam_g->sens[s].cam_i2c_datap);
	cam_g->sens[s].cam_i2c_datap = NULL;
	return 0;
}

static int cam_i2c_command(struct i2c_client *device, unsigned int cmd,
			   void *arg)
{
	return 0;
}

static void msleep_SS(UInt32 msec) //CYK_TEST
{ 
	long start = 0;
	long end = 0;

	do_gettimeofday( &g_stDbgTimeval);

	start = g_stDbgTimeval.tv_usec;
	end = g_stDbgTimeval.tv_usec;
	
	while( (end - start) < msec*1000)
	{
		do_gettimeofday( &g_stDbgTimeval);
		end = g_stDbgTimeval.tv_usec;
	};
}

struct i2c_device_id cam_i2c_id_table[] = {
	{"cami2c", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cam_i2c_id_table);



static HAL_CAM_Result_en_t cam_sensor_cntrl_seq(CamSensorIntfCntrl_st_t *seq,
						UInt32 length)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	UInt32 i;
	CSL_CAM_CLOCK_OUT_st_t cslCamClock;
	HalcamTraceDbg("%s(): ", __FUNCTION__);
	HalcamTraceDbg("Sequence Length=%d: ",
		 (int)length / sizeof(CamSensorIntfCntrl_st_t));

	for (i = 0; i < length / sizeof(CamSensorIntfCntrl_st_t); i++) {
		switch (seq[i].cntrl_sel) {
		case GPIO_CNTRL:
			gpio_request(seq[i].value, "hal_cam_gpio_cntrl");
			if (seq[i].cmd == GPIO_SetHigh) {
				HalcamTraceDbg("GPIO_CNTRL(%d), GPIO_SetHigh",
					 (int)seq[i].value);
				if (gpio_is_valid(seq[i].value))
					gpio_direction_output(seq[i].value, 1);
			} else {
				HalcamTraceDbg("GPIO_CNTRL(%d), GPIO_SetLow",
					 (int)seq[i].value);
				if (gpio_is_valid(seq[i].value))
					gpio_direction_output(seq[i].value, 0);
			}
			gpio_free(seq[i].value);
			break;

		case MCLK_CNTRL:
			if (seq[i].cmd == CLK_TurnOn){
				switch(seq[i].value)
				{
					case CamDrv_12MHz :
		            case CamDrv_24MHz :
                	case CamDrv_48MHz :
					// PWRMGMT_48MPLL_Enable("CAM"); FIXME
						cslCamClock.clock_select = 0x00;          
                        cslCamClock.enable = TRUE;          
	                    cslCamClock.speed = (CSL_CAM_CLK_SPEED_T)seq[i].value;          
						//HalcamTraceDbg("Trying to set 12/24/48");
            		    if ( csl_cam_set_clk ( &cslCamClock) )
                   		{
							HalcamTraceErr("Failed to set MCLK CTRL 12/24/48");
                	    }
					break;
					case CamDrv_13MHz :
		                        case CamDrv_26MHz :
					HalcamTraceDbg("Trying to set clock 13Mhz %d %d",(int)CamDrv_13MHz,(int)seq[i].value);
                		        cslCamClock.clock_select = 0x00;          
                           		cslCamClock.enable = TRUE;          
	                                cslCamClock.speed = (CSL_CAM_CLK_SPEED_T)seq[i].value;          
            		                if ( csl_cam_set_clk ( &cslCamClock) )
                        		{
						 HalcamTraceErr("Failed to set MCLK CTRL 13/26");
                            		}
					break;
					case CamDrv_NO_CLK:
                            		cslCamClock.clock_select = 0x00;          
		                            cslCamClock.enable = FALSE;          
                		            cslCamClock.speed = (CSL_CAM_CLK_SPEED_T)seq[i].value;          
		                            if ( csl_cam_set_clk ( &cslCamClock) )
                		            {
						 HalcamTraceErr("Failed to set MCLK CTRL NO CLOCK");
                		            }
					break;
					default:
						 HalcamTraceErr("Default MCLK CTRL NO CLOCK");
						break;
				}
			} else {
				cslCamClock.clock_select = 0x00;          
		                    cslCamClock.enable = FALSE;          
                		    cslCamClock.speed = (CSL_CAM_CLK_SPEED_T)seq[i].value;          
	                    if ( csl_cam_set_clk ( &cslCamClock) )
        	            {
				HalcamTraceErr("Failed to disable clock");
                    		}
			}
			break;

		case PAUSE:
			if (seq[i].value != 0) {
#if 1 //CYK_TEST
				//HalcamTraceErr("[CYK] +++ TEST PAUSE - %d ms", (int)seq[i].value);
				msleep_SS(seq[i].value);
				//HalcamTraceErr("[CYK] --- TEST PAUSE - %d ms", (int)seq[i].value);

#else
				HalcamTraceDbg("PAUSE - %d ms", (int)seq[i].value);
				msleep(seq[i].value);
#endif
			}
			break;
#if 0			
     	case REGULATOR_CNTRL:
			switch(seq[i].port_id)
			{
			
			case PORT_A :
				if(seq[i].value > 0)
				{
					cam_g->cam_regulator_a = regulator_get(NULL, "cam_vdda");
					if (!cam_g->cam_regulator_a || IS_ERR(cam_g->cam_regulator_a)) {
						printk(KERN_ERR "[cam_vdda]No Regulator_A available\n");
						result = HAL_CAM_ERROR_ACTION_NOT_SUPPORTED;
						}
						
					regulator_set_voltage(cam_g->cam_regulator_a,seq[i].value,seq[i].value);
					if (cam_g->cam_regulator_a)
						result += regulator_enable(cam_g->cam_regulator_a);
				}
				else
				{
					if (cam_g->cam_regulator_a)
						regulator_disable(cam_g->cam_regulator_a);
					//To_do : need adding return value
				}
				break;
				
			case PORT_I :
				
				if(seq[i].value > 0)
				{
					cam_g->cam_regulator_i = regulator_get(NULL, "cam_vddi");
					if (!cam_g->cam_regulator_i || IS_ERR(cam_g->cam_regulator_i)) {
						printk(KERN_ERR "[cam_vddi]No Regulator_I available\n");
						result = HAL_CAM_ERROR_ACTION_NOT_SUPPORTED;
						}

					regulator_set_voltage(cam_g->cam_regulator_i,seq[i].value,seq[i].value);
					if (cam_g->cam_regulator_i)
						result += regulator_enable(cam_g->cam_regulator_i);
				}
				else
				{
					if (cam_g->cam_regulator_i)
						regulator_disable(cam_g->cam_regulator_i);
					//To_do : need adding return value
				}
				break;

			case PORT_C :
				if(seq[i].value > 0)
				{
					cam_g->cam_regulator_c = regulator_get(NULL, "cam_vddc");
					if (!cam_g->cam_regulator_c|| IS_ERR(cam_g->cam_regulator_c)) {
						printk(KERN_ERR "[cam_vddc]No Regulator_C available\n");
						result = HAL_CAM_ERROR_ACTION_NOT_SUPPORTED;
						}

					regulator_set_voltage(cam_g->cam_regulator_c,seq[i].value,seq[i].value);
						if (cam_g->cam_regulator_c)
							result += regulator_enable(cam_g->cam_regulator_c);
				}
				else
				{
					if (cam_g->cam_regulator_c)
						regulator_disable(cam_g->cam_regulator_c);
					//To_do : need adding return value
				}
				break;

		default:
				printk(KERN_INFO"Default MCLK CTRL NO CLOCK\n");

			}

			break;
#endif
		default:
			HalcamTraceErr("CNTRL - Not Supported");
			result = HAL_CAM_ERROR_ACTION_NOT_SUPPORTED;
			break;
		}
	}
	return result;
}

int cam_get_bpp_from_fmt(CamDataFmt_t fmt)
{
	/* For now */
	return 2;
}

static HAL_CAM_Result_en_t cam_sensor_intf_seqsel(CamSensorSelect_t nSensor,
						  CamSensorSeqSel_t nSeqSel)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	UInt32 len = 0;
	CamSensorIntfCntrl_st_t *power_seq = NULL;
	struct camera_sensor_t *c = &cam_g->sens[nSensor];
	power_seq = c->sens_m->DRV_GetIntfSeqSel(nSensor, nSeqSel, &len);
		/* get power-up/power-down sequence */
	if ((len != 0) && (power_seq != NULL)) {
		result = cam_sensor_cntrl_seq(power_seq, len);
	} else {
		HalcamTraceDbg("%s(): No Sequence", __FUNCTION__);
	}
	return result;
	
}

static int cam_power_up(CamSensorSelect_t sensor)
{
	int rc = -1;
	struct camera_sensor_t *c; 
	HalcamTraceDbg("%s called ",__FUNCTION__);

	c = &cam_g->sens[sensor];

HalcamTraceDbg("before DRV_GetIntfConfig, sensor = %d ", sensor);

	c->sensor_intf = c->sens_m->DRV_GetIntfConfig(sensor);
	if (!c->sensor_intf) {
		HalcamTraceErr("Unable to get sensor interface config ");
		rc = -EFAULT;
	}
#if 1 //CYK_TEST
HalcamTraceDbg("before DRV_TurnOnRegulator");
    
//swsw_dual
	c->sens_m->DRV_TurnOnRegulator();
#endif	
	/* Config CMI controller over here */
	if (cam_sensor_intf_seqsel(sensor, SensorPwrUp) != 0) {
		HalcamTraceErr("Unable to Set power seq at Open");
		rc = -EFAULT;
	}
	return 0;
}

static int cam_power_down(CamSensorSelect_t sensor)
{
	int rc = -1;
	struct camera_sensor_t *c;
	c = &cam_g->sens[sensor];
	c->sensor_intf = c->sens_m->DRV_GetIntfConfig(sensor);
	if (c->sensor_intf == NULL) {
		HalcamTraceErr( "Cam power down unable to get intf config");
		return -1;
	}
	if (c->sens_m->DRV_SetCamSleep(sensor) != HAL_CAM_SUCCESS) {
		HalcamTraceErr( "Cam power down unable to get intf config");
		return -1;
	}
	if (cam_sensor_intf_seqsel(sensor, SensorPwrDn) != 0) {
		HalcamTraceErr( "Unable to Set power seq at power down");
		rc = -EFAULT;
	}
#if 1 //CYK_TEST
//swsw_dual
	c->sens_m->DRV_TurnOffRegulator();
#endif
	return 0;

}
ktime_t t1;
static irqreturn_t vsync_isr(int irq, void *arg)
{
	cslCamFrameLisr();
	return IRQ_HANDLED;
}

/* Invoke these functions directly from IOCTL based on arg */
int camera_enable(CamSensorSelect_t sensor)
{
	struct camera_sensor_t *c = &cam_g->sens[sensor];
	u32 size = 0;
	u32 still_pingpang=0;
	int i;
	int ydelta = 0;
	CSL_CAM_PIPELINE_st_t   cslCamPipeline;
	CSL_CAM_FRAME_st_t      cslCamFrame;
	/* SetParm IOCTL would have populated the CAM_PARM_t structure */
	HAL_CAM_ResolutionSize_st_t sensor_size;
	CSL_CAM_INPUT_st_t      cslCamInput;
	CSL_CAM_BUFFER_st_t     cslCamBuffer1, cslCamBuffer2;
	CSL_CAM_IMAGE_ID_st_t   cslCamImageCtrl;
	CSL_CAM_WINDOW_st_t     cslCamWindow;
	HalcamTraceDbg("%s called ",__FUNCTION__);
	
    if (c->main.format == CamDataFmtJPEG)
    {
        gNumOfStillPingPongBuf = STILL_PING_PANG;
    }
    else
    {
        gNumOfStillPingPongBuf = 1;         // Not to use PingPong buffer in case of YUV capture.
    }
    
	if (c->main.format == CamDataFmtJPEG) {
		sensor_size.resX =
		    c->sensor_intf->sensor_config_jpeg->
		    jpeg_packet_size_bytes;
		sensor_size.resY =
		    c->sensor_intf->sensor_config_jpeg->jpeg_max_packets;
	} else if (c->main.format == CamDataFmtYCbCr) {
		sensor_size.resX = c->main.size_window.end_pixel;
		sensor_size.resY = c->main.size_window.end_line;		
		
	}
	else
	{
		HalcamTraceDbg(" other format than YCbCr and JPEG");
		sensor_size.resX = c->main.size_window.end_pixel;
		sensor_size.resY = c->main.size_window.end_line;
	}
	/* Config DMA with dma_x and dma_y */
	/* Config DMA over here
	Sensor driver settings */
	if ((c->main.mode == CamStillnThumb) || (c->main.mode == CamStill)) {

		c->still_ready = 0;			
		cslCamInput.input_mode = c->cslCamIntfCfg.input_mode; 
		cslCamInput.frame_time_out = c->cslCamIntfCfg.frame_time_out;
		cslCamInput.p_cpi_intf_st = NULL;
		if(c->cslCamIntfCfg.intf == CSL_CAM_INTF_CPI)
			cslCamInput.p_cpi_intf_st = c->cslCamIntfCfg.p_cpi_intf_st;
		if ( csl_cam_set_input_mode( c->hdl, &cslCamInput) ) {
			HalcamTraceErr("Unable to set CSI input mode");
		}
/* for post view time
		CSL_CAM_CLOCK_OUT_st_t cslCamClock;
		cslCamClock.clock_select = 0x00;          
        cslCamClock.enable = TRUE;          
	    cslCamClock.speed = CamDrv_24MHz;        //Use fast clock for still image capture to make it is more stable  
		if ( csl_cam_set_clk ( &cslCamClock) )
        {
			HalcamTraceDbg("Failed to set MCLK CTRL 12/24/48");
        }

*/
		//Configure the bottom of 8M reserve buffer for still image capture case

		if(sensor_size.resX*sensor_size.resY*gNumOfStillPingPongBuf>SZ_12M)
		{
			HalcamTraceErr("Reserved buffer is too small to fit Ping-Pang buffer for still image capture, change it to one buffer");
			still_pingpang=1;
		}
		if(c->main.format == CamDataFmtJPEG)
		{
			cslCamBuffer1.start_addr = cam_g->cam_buf.phy+SZ_12M-(sensor_size.resX*sensor_size.resY*gNumOfStillPingPongBuf)-(0x400*6);		
			cslCamBuffer1.start_addr &= ~0x3FF;
			cslCamBuffer1.line_stride = sensor_size.resX;
			cslCamBuffer1.size = sensor_size.resX*sensor_size.resY;
			HalcamTraceDbg("Start address for still JPEG image capture is 0x%x",cslCamBuffer1.start_addr);
		}
		else
		{
			cslCamBuffer1.start_addr = cam_g->cam_buf.phy+SZ_12M-(sensor_size.resX*sensor_size.resY*gNumOfStillPingPongBuf*2)-(0x400*2);	
			cslCamBuffer1.start_addr &= ~0x3FF;
			cslCamBuffer1.line_stride = sensor_size.resX * 2;
			cslCamBuffer1.size = sensor_size.resX*sensor_size.resY* 2;
			HalcamTraceDbg("Start address for still YUV image capture = 0x%x, base address = 0x%x ",cslCamBuffer1.start_addr,cam_g->cam_buf.phy);
		}	
		
		cslCamBuffer1.buffer_wrap_en = FALSE;	
		if((gNumOfStillPingPongBuf>1)&&(still_pingpang == 0))
		{
			HalcamTraceDbg(" configure second buffer");
			cslCamBuffer2.start_addr = ((cslCamBuffer1.start_addr + cslCamBuffer1.size)+0x3FF)&~0x3FF;			
			cslCamBuffer2.size = cslCamBuffer1.size;
			cslCamBuffer2.line_stride = cslCamBuffer1.line_stride;
			cslCamBuffer2.buffer_wrap_en = FALSE;	
			if (csl_cam_set_input_addr(c->hdl, &cslCamBuffer1,&cslCamBuffer2, NULL)){
				HalcamTraceErr("Unable to set CSI buffer control");
    		}
		}		
		else
		{
			if (csl_cam_set_input_addr(c->hdl, &cslCamBuffer1,NULL, NULL)){
				HalcamTraceErr("Unable to set CSI buffer control");
    		}
		}

		if(c->cslCamIntfCfg.intf == CSL_CAM_INTF_CSI){
			cslCamImageCtrl.image_data_id0 = 0x30;//pCdiState_st->csi2ImageDataType; //only id0 is effective in Athena.
			cslCamImageCtrl.image_data_id1 = 0x00;
			cslCamImageCtrl.image_data_id2 = 0x00;
			cslCamImageCtrl.image_data_id3 = 0x00;
			if (csl_cam_set_image_type_control(c->hdl, &cslCamImageCtrl ) ){
				HalcamTraceErr("Unable to set image type control");
			}
		}
		/* PIPELINE CONTROL */
		cslCamPipeline.decode = CSL_CAM_DEC_NONE;
		
		cslCamPipeline.unpack = CSL_CAM_PIXEL_8BIT;
	    cslCamPipeline.dec_adv_predictor = FALSE;
		cslCamPipeline.encode               = CSL_CAM_ENC_NONE;
		cslCamPipeline.pack                 = CSL_CAM_PIXEL_8BIT;
	    cslCamPipeline.enc_adv_predictor    = FALSE;
	    cslCamPipeline.encode_blk_size      = 0x0000;
		if ( csl_cam_set_pipeline_control(c->hdl, &cslCamPipeline) ) {
			HalcamTraceErr("Unable to set CSI pipeline control");
    	}

		cslCamWindow.enable = FALSE;
        cslCamWindow.horizontal_start_byte = 0;
		cslCamWindow.horizontal_size_bytes = sensor_size.resX * 2;//(pcam_window_config->win_end_X - pcam_window_config->win_start_X);
		cslCamWindow.vertical_start_line = 0;//pcam_window_config->win_start_Y;
		cslCamWindow.vertical_size_lines = sensor_size.resY;//pcam_window_config->win_start_Y;

		if ( csl_cam_set_image_window(c->hdl, &cslCamWindow ) ) {
			HalcamTraceErr("Cam_CfgWindowScale(): csl_cam_set_image_window() ERROR: ");
		}
		
	    cslCamFrame.int_enable      = c->sensor_intf->sensor_config_ccp_csi->pkt_intr_enable | CSL_CAM_INT_FRAME_ERROR | CSL_CAM_INT_FRAME_START;
	    cslCamFrame.capture_mode    = CSL_CAM_CAPTURE_MODE_NORMAL;
	    cslCamFrame.int_line_count  = sensor_size.resY;
	    cslCamFrame.capture_size    = size;
		
		if (csl_cam_set_frame_control( c->hdl, &cslCamFrame) ) {
			HalcamTraceErr("Unable to set CSI frame control");
    		}
		unsigned int temp = readl(io_p2v(0x08880020));
		writel(temp|=0x7<<12,io_p2v(0x08880020));
		

		c->sens_m->DRV_CfgStillnThumbCapture(c->main.size_window.size,
                            c->main.format, c->th.size_window.size, c->th.format, sensor);
		writel(0x0100200f, io_p2v(BCM21553_MLARB_BASE + 0x100)); //BMARBL_MACONF0
        writel(0x44444 , io_p2v(BCM21553_MLARB_BASE + 0x108)); //BMARBL_MACONF2	
		udelay(5);	
		
		if(csl_cam_rx_start(c->hdl))
			HalcamTraceErr("Unable to start RX");
		
		c->mode = CAM_STILL;
		c->prev.tv.sec = 0;
		c->prev.tv.nsec = 0;
		/*
		cam_config_dma_buffers(sensor_size.resX, sensor_size.resY,
				       c->main.format, sensor); */
		c->gProcessFrameRunning = 1;
		c->state = CAM_INIT;
		c->sCaptureJpegSize = 0;
	} else{
		/* Video and/or VF mode */
		/*
		cam_config_dma_buffers(sensor_size.resX, sensor_size.resY,
				       c->main.format, sensor); */
		//c->main.format = CAMDRV_IMAGE_YUV422;	
//		init_waitqueue_head(&gVideoReadyQ);
		CSL_CAM_CLOCK_OUT_st_t cslCamClock;
		cslCamClock.clock_select = 0x00;          
        cslCamClock.enable = TRUE;          
	    cslCamClock.speed = CamDrv_24MHz;        //Use slow clock for still image capture to make it is more stable  
		if ( csl_cam_set_clk ( &cslCamClock) )
        {
			HalcamTraceErr("Failed to set MCLK CTRL 12/24/48");
        }

	
		cslCamInput.input_mode = c->cslCamIntfCfg.input_mode; 
		cslCamInput.frame_time_out = c->cslCamIntfCfg.frame_time_out;
		cslCamInput.p_cpi_intf_st = NULL;
		if(c->cslCamIntfCfg.intf == CSL_CAM_INTF_CPI)
			cslCamInput.p_cpi_intf_st = c->cslCamIntfCfg.p_cpi_intf_st;
		
		if ( csl_cam_set_input_mode( c->hdl, &cslCamInput) ) {
			HalcamTraceErr("Unable to set CSI input mode");
    		}
		cslCamBuffer1.start_addr = cam_g->cam_buf.phy + SZ_12M - (2 * sensor_size.resX * sensor_size.resY * VIDEO_PING_PONG)-(0x400*6);
		cslCamBuffer1.start_addr &= ~0x3FF;
		//cam_g->sens[sensor].camb[0].phy = cslCamBuffer1.start_addr;
		/* 2 bpp */
		size = (sensor_size.resX * sensor_size.resY * 2);
		/* Align all addresses to 10 bit address boundary*/
	
		HalcamTraceDbg("Buf 1 addr 0x%x and fps %d, size %d, sensor_size.resX = %d, sensor_size.resY = %d",(u32)cslCamBuffer1.start_addr,(int)c->main.fps,size,sensor_size.resX,sensor_size.resY);
		cslCamBuffer1.size = size;
		cslCamBuffer1.line_stride = sensor_size.resX * 2;
		cslCamBuffer1.buffer_wrap_en = FALSE;	
		cslCamBuffer2.start_addr = ((cslCamBuffer1.start_addr + size)+0x3FF*4)&~0x3FF;	
		
		
		//cam_g->sens[sensor].camb[1].phy = cslCamBuffer2.start_addr;
		/* 2 bpp */
		cslCamBuffer2.size = size;
		cslCamBuffer2.line_stride = sensor_size.resX * 2;
		cslCamBuffer2.buffer_wrap_en = FALSE;	
		HalcamTraceDbg("Buf 2 addr 0x%x",(u32)cslCamBuffer2.start_addr);
		if (csl_cam_set_input_addr(c->hdl, &cslCamBuffer1,&cslCamBuffer2, NULL)){
			HalcamTraceErr("Unable to set CSI buffer control");
    		}
		/* PIPELINE CONTROL */
		cslCamPipeline.decode = CSL_CAM_DEC_NONE;
		cslCamPipeline.unpack = CSL_CAM_PIXEL_8BIT;
	    	cslCamPipeline.dec_adv_predictor = FALSE;
		cslCamPipeline.encode               = CSL_CAM_ENC_NONE;
		cslCamPipeline.pack                 = CSL_CAM_PIXEL_8BIT;
	    	cslCamPipeline.enc_adv_predictor    = FALSE;
	    	cslCamPipeline.encode_blk_size      = 0x0000;
		if ( csl_cam_set_pipeline_control(c->hdl, &cslCamPipeline) ) {
			HalcamTraceErr("Unable to set CSI pipeline control");
    		}
		cslCamWindow.enable = FALSE;
		cslCamWindow.horizontal_start_byte = 0;
		cslCamWindow.horizontal_size_bytes = sensor_size.resX*2;//(pcam_window_config->win_end_X - pcam_window_config->win_start_X);
		cslCamWindow.vertical_start_line = 0;//pcam_window_config->win_start_Y;
		cslCamWindow.vertical_size_lines = sensor_size.resY;//pcam_window_config->win_start_Y;
		if ( csl_cam_set_image_window(c->hdl, &cslCamWindow ) ) {
			HalcamTraceErr("Cam_CfgWindowScale(): csl_cam_set_image_window() ERROR: ");
		}

		cslCamFrame.int_enable      = c->sensor_intf->sensor_config_ccp_csi->data_intr_enable | CSL_CAM_INT_FRAME_START; // | CSL_CAM_INT_FRAME_ERROR;
	    cslCamFrame.capture_mode    = CSL_CAM_CAPTURE_MODE_NORMAL;
	  	cslCamFrame.int_line_count  = sensor_size.resY;
	    cslCamFrame.capture_size    = size;
		if (csl_cam_set_frame_control( c->hdl, &cslCamFrame) ) {
			HalcamTraceErr("Unable to set CSI frame control");
		}
		c->sens_m->
			  DRV_SetVideoCaptureMode(c->main.size_window.size,
						  c->main.format, sensor,c->main.mode);
		c->sens_m->DRV_SetFrameRate(c->main.fps, sensor);
		c->sens_m->DRV_EnableVideoCapture(sensor);
		/* Sensor driver methods to enable video modes */
		if(csl_cam_rx_start(c->hdl))
			HalcamTraceErr("Unable to start RX");
		c->mode = CAM_STREAM;
		c->state = CAM_INIT;
	}
	/* Sequence from app is MEM_REGISTER -- ENABLE -- MEM_BUFFERS */
	
	enable_irq(c->cam_irq);
	
   
        c->sCaptureFrameCountdown = 0;
    

	wake_lock(&cam_g->camera_wake_lock);
	c->rd_Q.isActive = 0;
	c->wr_Q.isActive = 0;
	init_queue(&c->wr_Q);
	init_queue(&c->rd_Q);
	c->drop_fps = 0;
	c->state = CAM_ON;
	return 0;
}

int camera_disable(CamSensorSelect_t sensor)
{
	struct camera_sensor_t *c = &cam_g->sens[sensor];
/* SetParm IOCTL would have populated the CAM_PARM_t structure */
	unsigned long stat;
	int rc = 0;

	HalcamTraceDbg( "camera_disable!!!");
	spin_lock_irqsave(&c->c_lock, stat);
	c->mode = CAM_NONE;
	c->state = CAM_STOPPING;
	spin_unlock_irqrestore(&c->c_lock, stat);
	c->gProcessFrameRunning = 0;
	wake_up_interruptible(&gDataReadyQ);
	wake_up_interruptible(&gVideoReadyQ);
	if ((c->main.mode == CamStillnThumb) || (c->main.mode == CamStill)) {
		HalcamTraceDbg( "Disabling capture");
		/* taskcallback would have already disabled csl_cam_rx*/
		c->sens_m->DRV_DisableCapture(sensor);
	} else {
		HalcamTraceDbg( "Disabling stream");
		csl_cam_rx_stop(c->hdl);
		c->sens_m->DRV_DisablePreview(sensor);
	}
	writel(0x3f0f, io_p2v(BCM21553_MLARB_BASE + 0x100)); //BMARBL_MACONF0 
	csl_cam_reset(c->hdl, (CSL_CAM_RESET_t)(CSL_CAM_RESET_SWR | CSL_CAM_RESET_ARST ));
	disable_irq(c->cam_irq);
	wake_unlock(&cam_g->camera_wake_lock);
	deinit_queue(&c->wr_Q);
	deinit_queue(&c->rd_Q);
	c->gCurrData.id = -1;
	c->gCurrData.busAddress = NULL;
	c->gCurrData.timestamp = 0;
	c->state = CAM_OFF;
	return rc;
}

static int cam_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct camera_sensor_t *c;


	/* Choose between primary and sec camera */
	HalcamTraceDbg("%s called ",__FUNCTION__);
#if defined (CONFIG_CPU_FREQ_GOV_BCM21553)
	cpufreq_bcm_dvfs_disable(cam_g->cam_dvfs);
#endif
	down(&cam_g->cam_sem);

	HalcamTraceDbg("inode->i_rdev = %d ",MAJOR(inode->i_rdev));
	if (MAJOR(inode->i_rdev)==BCM_CAM_MAJOR) {//swsw_dual
		//sensor_open=CamSensorPrimary;
		cam_g->curr = CamSensorPrimary;
		if(cam_g->sens[cam_g->curr].devbusy){
			up(&cam_g->cam_sem);
			HalcamTraceErr("Error,cam_open CamSensorPrimary_CYK !");
			return -EBUSY;
		}
		cam_g->sens[cam_g->curr].devbusy= 1;
		cam_g->sens[CamSensorPrimary].sens_m = CAMDRV_primary_get();
	} else {
		cam_g->curr = CamSensorSecondary;
                 if(cam_g->sens[cam_g->curr].devbusy){
			up(&cam_g->cam_sem);
			HalcamTraceErr("Error,cam_open CamSensorSecondary_CYK !");
			return -EBUSY;
		}
		cam_g->sens[cam_g->curr].devbusy= 1;
	#if (CAM_SENSOR_NUM==2)
		cam_g->sens[CamSensorSecondary].sens_m = CAMDRV_secondary_get();
	#else
		HalcamTraceErr("Error, there are no second camera exists!");
	#endif
//swsw_dual
	}

	//board_sysconfig(SYSCFG_CAMERA,SYSCFG_INIT);
	board_sysconfig(SYSCFG_CAMERA,SYSCFG_ENABLE);
	c = &cam_g->sens[cam_g->curr];
	if(csl_cam_init()) {
		HalcamTraceErr("Unable to init the CAMINTF ");
		rc = -EBUSY;
		goto oerr;
	}
	/* Need to init I2C but our probe would have already done that */

HalcamTraceDbg("before cam_power_up : curr = %d", cam_g->curr);

	cam_power_up(cam_g->curr);	
	/* Camera CSL open */	
	c->sensor_intf = c->sens_m->DRV_GetIntfConfig(cam_g->curr);
	
	c->cslCamIntfCfg.intf = (CSL_CAM_INTF_T)c->sensor_intf->sensor_config_caps->intf_mode;
    c->cslCamIntfCfg.afe_port = (CSL_CAM_PORT_AFE_T)c->sensor_intf->sensor_config_caps->intf_port;
    c->cslCamIntfCfg.frame_time_out = 100; /* 100ms or 10 fps*/
    //c->cslCamIntfCfg.capture_mode = CSL_CAM_CAPTURE_MODE_NORMAL;
	HalcamTraceDbg("Out interface %d and CSL_ %d",c->cslCamIntfCfg.intf,CSL_CAM_INTF_CSI);
	if(c->cslCamIntfCfg.intf == CSL_CAM_INTF_CPI){
		HalcamTraceDbg("CPI being opened");
		c->cslCamIntfCfg.p_cpi_intf_st  = kmalloc(sizeof(CSL_CAM_CPI_INTF_st_t),GFP_KERNEL);
		c->cslCamIntfCfg.p_cpi_intf_st->sync_mode = CSL_CAM_NO_MODE_SELECT;
		c->cslCamIntfCfg.p_cpi_intf_st->sync_mode = c->sensor_intf->sensor_config_ccir656->sync_mode;
		c->cslCamIntfCfg.p_cpi_intf_st->hsync_signal = c->sensor_intf->sensor_config_ccir656->hsync_control;
		c->cslCamIntfCfg.p_cpi_intf_st->hsync_trigger = c->sensor_intf->sensor_config_ccir656->hsync_polarity;
		c->cslCamIntfCfg.p_cpi_intf_st->vsync_signal = c->sensor_intf->sensor_config_ccir656->vsync_control;
		c->cslCamIntfCfg.p_cpi_intf_st->vsync_trigger = c->sensor_intf->sensor_config_ccir656->vsync_polarity;
		c->cslCamIntfCfg.p_cpi_intf_st->clock_edge = c->sensor_intf->sensor_config_ccir656->data_clock_sample;
		c->cslCamIntfCfg.p_cpi_intf_st->bit_width = c->sensor_intf->sensor_config_ccir656->bus_width;
		c->cslCamIntfCfg.p_cpi_intf_st->data_shift = c->sensor_intf->sensor_config_ccir656->data_shift;
		c->cslCamIntfCfg.p_cpi_intf_st->fmode = c->sensor_intf->sensor_config_ccir656->field_mode;
		c->cslCamIntfCfg.p_cpi_intf_st->smode = CSL_CAM_SCOPE_DISABLED;
	} else if(c->cslCamIntfCfg.intf == CSL_CAM_INTF_CSI){
		HalcamTraceDbg("CSI2 being opened");
		c->cslCamIntfCfg.input_mode = (CSL_CAM_INPUT_MODE_t)c->sensor_intf->sensor_config_ccp_csi->input_mode;
        c->cslCamIntfCfg.p_cpi_intf_st  = NULL;
	}
	if(0 != request_irq(c->cam_irq,vsync_isr,IRQF_DISABLED,"vsync",&cam_g->curr)){
			HalcamTraceErr("Major problem .. no IRQ available for CSI2****");
			rc = -EBUSY;
			goto oerr;
	}
	disable_irq(c->cam_irq); /* Is it needed ??*/
	if ( csl_cam_open( &c->cslCamIntfCfg, &c->hdl ) ){
		HalcamTraceErr("Failed to open CSL interface");
		rc = -EBUSY;
		goto oerr;
    }
	HalcamTraceDbg("Sensor communication over I2C start");
	if (c->sens_m->DRV_Wakeup(cam_g->curr)) {
    	HalcamTraceErr( "Failed to init the sensor");
		rc = -EFAULT;
		goto oerr;
    } else {
	HalcamTraceDbg("Sensor communication over I2C success");
	}
	HalcamTraceDbg("Done DRV_Wakeup ");
	/*TODO interrupt processing request callback */
	/* Avoid LISR callback as it's only status storage
 	* Register task callback instead */	
	if( csl_cam_register_event_callback( c->hdl, CSL_CAM_TASK, (cslCamCB_t)taskcallback, NULL ) ) {
		/* This is to be seen :)*/
		HalcamTraceErr("Failed to register task callback");
		rc = -EBUSY;
		goto oerr;
	}
#ifdef REG_DISP
	csl_cam_register_display(c->hdl );
#endif
	// CAMDRV_InitSensor
	/* c->sensor_intf would be valid after successful return of power up */
	/* Init the working buffer queue */
	c->gCurrData.id = -1;
	c->gCurrData.busAddress = NULL;
	c->gCurrData.timestamp = 0;
	cam_g->sens[cam_g->curr].state = CAM_OFF;
	c->mode = CAM_NONE;
	memset(&c->th,0,sizeof(c->th));
	c->bufsw = 0;
	c->zoom = CamZoom_1_0;
	up(&cam_g->cam_sem);
	// disable_irq(IRQ_VSYNC);
	return 0;
oerr:
	HalcamTraceErr( "Failed in cam_open() : Reset some ");

	if (c->state != CAM_OFF) {
		camera_disable(cam_g->curr);
	}

	cam_power_down(cam_g->curr);

	if (c->hdl != NULL)
	{
		csl_cam_reset(c->hdl, (CSL_CAM_RESET_t)(CSL_CAM_RESET_SWR | CSL_CAM_RESET_ARST ));
		csl_cam_close(c->hdl);
	}
	csl_cam_exit();

	if (c->cslCamIntfCfg.p_cpi_intf_st != NULL)
		kfree(c->cslCamIntfCfg.p_cpi_intf_st);
	free_irq(c->cam_irq,&cam_g->curr);
	HalcamTraceDbg( "IRQ VSYNC freed in cam_open()");

	board_sysconfig(SYSCFG_CAMERA,SYSCFG_DISABLE);

	cpufreq_bcm_dvfs_enable(cam_g->cam_dvfs);

	cam_g->sens[cam_g->curr].devbusy= 0;
	HalcamTraceDbg( "cam_open E");

	up(&cam_g->cam_sem);
	return rc;
}
/*  Very important need to see if reset is enough in camera_disable 
 *  We release the csl_interface only on powerdown VVVV */
static int cam_release(struct inode *inode, struct file *file)
{
	struct camera_sensor_t *c;
	down(&cam_g->cam_sem);
	c = &cam_g->sens[cam_g->curr];
    HalcamTraceDbg( "cam_release");
		
	if (c->state != CAM_OFF) {
		camera_disable(cam_g->curr);
	}
	cam_power_down(cam_g->curr);
	csl_cam_reset(c->hdl, (CSL_CAM_RESET_t)(CSL_CAM_RESET_SWR | CSL_CAM_RESET_ARST ));
	csl_cam_close(c->hdl);
	csl_cam_exit();
	if(c->cslCamIntfCfg.intf == CSL_CAM_INTF_CPI)
		kfree(c->cslCamIntfCfg.p_cpi_intf_st);
	free_irq(c->cam_irq,&cam_g->curr);
	board_sysconfig(SYSCFG_CAMERA,SYSCFG_DISABLE);
	HalcamTraceDbg( "IRQ VSYNC freed");
	wake_unlock(&cam_g->camera_wake_lock);
#if defined (CONFIG_CPU_FREQ_GOV_BCM21553)
	cpufreq_bcm_dvfs_enable(cam_g->cam_dvfs);
#endif
	cam_g->sens[cam_g->curr].devbusy= 0;
	HalcamTraceDbg( "cam_release E");
	up(&cam_g->cam_sem);
	return 0;
}

/* CAM I2C functions begin */
unsigned char msgbuf[MAX_I2C_DATA_COUNT];
HAL_CAM_Result_en_t CAM_WriteI2c(UInt16 camRegID, UInt16 DataCnt, UInt8 *Data)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	struct i2c_adapter *adap;
	int s = cam_g->curr;
#if 0 //BYKIM_PREVENT
	if (!DataCnt || (DataCnt > MAX_I2C_DATA_COUNT)) {
		result = HAL_CAM_ERROR_INTERNAL_ERROR;
		HalcamTraceErr("%s() - DataCnt = %d out of range", __FUNCTION__,
			 DataCnt);
		goto done;
	}

	if ((cam_g->sens[s].cam_i2c_datap != NULL)
	    && (adap = cam_g->sens[s].cam_i2c_datap->client->adapter)) {
		int ret;
		struct i2c_msg msg = {
			cam_g->sens[s].cam_i2c_datap->client->addr,
			cam_g->sens[s].cam_i2c_datap->client->flags,
			sizeof(camRegID) + DataCnt, msgbuf };
		/* Check the swapping of sensor ID */
		msgbuf[0] = (u8) ((camRegID & 0xFF00) >> 8);
		msgbuf[1] = (u8) (camRegID & 0x00FF);
		memcpy(&msgbuf[sizeof(camRegID)], Data, DataCnt);

		ret = i2c_transfer(adap, &msg, 1);
		if (ret != 1)
			result = HAL_CAM_ERROR_INTERNAL_ERROR;

	} else {
		HalcamTraceErr("%s() - Camera I2C adapter null", __FUNCTION__);
		result = HAL_CAM_ERROR_INTERNAL_ERROR;
	}
done:
	return result;
}

HAL_CAM_Result_en_t CAM_ReadI2c(UInt16 camRegID, UInt16 DataCnt, UInt8 *Data)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	struct i2c_adapter *adap;
	int s = cam_g->curr;

	if (!DataCnt) {
		result = HAL_CAM_ERROR_INTERNAL_ERROR;
		HalcamTraceErr("%s() - DataCnt = %d out of range", __FUNCTION__,
			 DataCnt);
		goto done;
	}

	if ((cam_g->sens[s].cam_i2c_datap)
	    && (adap = cam_g->sens[s].cam_i2c_datap->client->adapter)) {
		int ret;
		unsigned char msgbuf0[sizeof(camRegID)];
		struct i2c_msg msg[2] = {
			{cam_g->sens[s].cam_i2c_datap->client->addr,
			 cam_g->sens[s].cam_i2c_datap->client->flags,
			 sizeof(camRegID), msgbuf0}
			,
			{cam_g->sens[s].cam_i2c_datap->client->addr,
			 cam_g->sens[s].cam_i2c_datap->client->flags | I2C_M_RD,
			 DataCnt, Data}
		};

		msgbuf0[0] = (camRegID & 0xFF00) >> 8;
		msgbuf0[1] = (camRegID & 0x00FF);
		ret = i2c_transfer(adap, msg, 2);
		if (ret != 2)
			result |= HAL_CAM_ERROR_INTERNAL_ERROR;

	} else {
		HalcamTraceErr("%s() - Camera I2C adapter null", __FUNCTION__);
		result = HAL_CAM_ERROR_INTERNAL_ERROR;
	}
done:
#endif	
	return result;
}
#if 0 //BYKIM_CAMACQ
HAL_CAM_Result_en_t CamacqExtWriteI2c(UInt8 *pucValue, UInt8 ucSize )
{

    HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
    struct i2c_adapter *adap;
    int s = cam_g->curr;
	
    if (!ucSize || (ucSize > MAX_I2C_DATA_COUNT)) {
    	result = HAL_CAM_ERROR_INTERNAL_ERROR;
    	HalcamTraceErr("%s() - DataCnt = %d out of range", __FUNCTION__,ucSize);
    	goto done;
    }

	if ((cam_g->sens[s].cam_i2c_datap != NULL)
	    && (adap = cam_g->sens[s].cam_i2c_datap->client->adapter)) {
		int ret;
		struct i2c_msg msg = {
			cam_g->sens[s].cam_i2c_datap->client->addr,
			cam_g->sens[s].cam_i2c_datap->client->flags,
			ucSize, pucValue };

		ret = i2c_transfer(adap, &msg, 1);
		if (ret != 1){	
		       HalcamTraceErr("HAL_CAM_ERROR_INTERNAL_ERROR: ret = %d",ret);
			result = HAL_CAM_ERROR_INTERNAL_ERROR;
		}

	} else {
		HalcamTraceErr("%s() - Camera I2C adapter null", __FUNCTION__);
		result = HAL_CAM_ERROR_INTERNAL_ERROR;
	}
done:
	return result;
    }
#endif

#if 0
/* CAM I2C functions end */
struct i2c_driver i2c_driver_cam = {
	.driver = {
		   .name = IF_NAME,
		   },
	.id_table = cam_i2c_id_table,
	.probe = cam_i2c_probe,
	.remove = cam_i2c_remove,
	.command = cam_i2c_command,
};
#endif
#if 1 //Dual_Cam

// [[ mhoon.chae@LTN_MM support anti-banding
#if defined(CONFIG_LTN_COMMON)
cam_antibanding_setting camera_antibanding = CAM_ANTIBANDING_60HZ; //default

int camera_antibanding_get (void)
{
	printk(KERN_ERR"[LTN] camera_antibanding_get %d\n", camera_antibanding);
	return camera_antibanding;
}
ssize_t camera_antibanding_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;

	count = sprintf(buf, "%d", camera_antibanding);
	printk(KERN_ERR"[LTN] The value of antibanding is %d\n", camera_antibanding);

	return count;
}

ssize_t camera_antibanding_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int tmp = 0;

	sscanf(buf, "%d", &tmp);
	if ((CAM_ANTIBANDING_50HZ == (cam_antibanding_setting) tmp)
			|| (CAM_ANTIBANDING_60HZ == (cam_antibanding_setting) tmp)) {
		camera_antibanding = (cam_antibanding_setting) tmp;
		printk(KERN_ERR"[LTN] The antibanding is set to %d\n", camera_antibanding);
	}

	return count;
}

struct device *cam_dev = NULL;
static struct device_attribute camera_antibanding_attr = {
	.attr = {
		.name = "anti-banding",
		.mode = (S_IRUSR|S_IRGRP | S_IWUSR|S_IWGRP)}, // Juliano.b <-- Removing OTHERS from write access2011.06.01 cause : anti-banding file open fail
	.show = camera_antibanding_show,
	.store = camera_antibanding_store
};
#endif //ltn_cam : Support anti-banding hmin84.park 2011.12.08

static int __init cam_init(void)
{
	int rc;
	struct camera_sensor_t *c;
	ktime_t in = ktime_get();
	HalcamTraceDbg( "%s sec %d nsec %d", banner,in.tv.sec,in.tv.nsec);
	cam_g =
	(struct cam_generic_t *)kmalloc(sizeof(struct cam_generic_t), GFP_KERNEL);
	if (!cam_g) {
		HalcamTraceErr( "No memory for camera driver");
		return -ENOMEM;
	}
	rc = register_chrdev(BCM_CAM_MAJOR, "camera", &cam_fops);
	if (rc < 0) {
		HalcamTraceErr( "Camera: register_chrdev failed for major %d",
		       BCM_CAM_MAJOR);
		return rc;
	}
	cam_g->cam_class = class_create(THIS_MODULE, "camera");
	if (IS_ERR(cam_g->cam_class)) {
		unregister_chrdev(BCM_CAM_MAJOR, "camera");
		rc = PTR_ERR(cam_g->cam_class);
		goto err;
	}
// [[ mhoon.chae@LTN_MM support anti-banding
#if defined(CONFIG_LTN_COMMON)
	cam_dev = device_create(cam_g->cam_class, NULL, MKDEV(BCM_CAM_MAJOR, 0), NULL, "camera");
#else
	device_create(cam_g->cam_class, NULL, MKDEV(BCM_CAM_MAJOR, 0), NULL,
		      "camera");
#endif
	
	
//swsw_dual
#if 0 //CYK
	rc = register_chrdev(BCM_CAM2_MAJOR, "camera2", &cam_fops);
	if (rc < 0) {
		HalcamTraceErr( "Camera: register_chrdev failed for major %d",
			   BCM_CAM2_MAJOR);
		return rc;
	}
#if 0	
	cam_g->cam_class = class_create(THIS_MODULE, "camera2");
	if (IS_ERR(cam_g->cam_class)) {
		unregister_chrdev(BCM_CAM2_MAJOR, "camera2");
		rc = PTR_ERR(cam_g->cam_class);
		goto err;
	}
#endif

#endif

	//device_create(cam_g->cam_class, NULL, MKDEV(BCM_CAM2_MAJOR, 0), NULL, "camera2");

board_sysconfig(SYSCFG_CAMERA,SYSCFG_INIT);


#if defined (CONFIG_CPU_FREQ_GOV_BCM21553)
	cam_g->cam_dvfs = cpufreq_bcm_client_get("cam_dvfs");
	if(!cam_g->cam_dvfs)
	{
		HalcamTraceErr("Unable to get a DVFS client for camera driver !! ");
	}
	else
		cpufreq_bcm_dvfs_enable(cam_g->cam_dvfs);
#endif	

#if 1 //CYK
	wake_lock_init(&cam_g->camera_wake_lock, WAKE_LOCK_SUSPEND, "camera");
	cam_g->gSysCtlHeader = register_sysctl_table(gSysCtl);
#else
	wake_lock_init(&cam_g->camera_wake_lock, WAKE_LOCK_SUSPEND, "camera");
	cam_g->gSysCtlHeader = register_sysctl_table(gSysCtl);

	cam_g->gSysCtlHeader = register_sysctl_table(gSysCtl_sub);
#endif

	cam_g->sens[CamSensorPrimary].sens_m	= CAMDRV_primary_get();//swsw_dual
#if (CAM_SENSOR_NUM==2)
	cam_g->sens[CamSensorSecondary].sens_m	= CAMDRV_secondary_get();
#endif
	cam_g->curr = CamSensorPrimary;
		
	//	cam_power_up(cam_g->curr);
	/* Alloc separately for primary and secondary cameras. For now only primary
	 cam_buf is the operating buffer
	 cam_buf_main always holds the addresses of the allocated boot time buffer
	 Stub 1 */
	c = &cam_g->sens[CamSensorPrimary];//swsw_dual
#if 1
cam_g->sens[CamSensorPrimary].devbusy =0;
cam_g->sens[CamSensorSecondary].devbusy =0;

#else
	c->devbusy = 0;
#endif
//	c->sens_m->DRV_GetRegulator();
	c->sensor_intf = c->sens_m->DRV_GetIntfConfig(CamSensorPrimary);//swsw_dual
	if (!c->sensor_intf) {
		HalcamTraceErr( "Unable to get sensor interface config ");
		rc = -EFAULT;
	}
	cam_g->sens[CamSensorPrimary].cam_i2c_datap = NULL;//swsw_dual
	cam_g->sens[CamSensorSecondary].cam_i2c_datap = NULL;
	/* For now only for primary camera */
//BYKIM_CAMACQ
#if 0 // work in camacq
	HalcamTraceDbg("Adding I2C driver **** ");
	rc = i2c_add_driver(&i2c_driver_cam);
#endif
	sema_init(&cam_g->cam_sem, 1);
	mdelay(2);
	/*
	if (cam_sensor_intf_seqsel(sensor, SensorPwrDn) != 0) {
		HalcamTraceErr( "Unable to Set power seq at Open");
		rc = -EFAULT;
	}*/
// [[ mhoon.chae@LTN_MM support anti-banding
#if defined(CONFIG_LTN_COMMON)
	printk(KERN_ERR"[LTN] create anti-banding device file!\n");
	if (device_create_file(cam_dev, &camera_antibanding_attr) < 0) {
		printk(KERN_ERR"[LTN] Failed to create anti-banding device file!\n");
		return -1;
	}
#endif //ltn_cam : support anti-banding hmin84.park 2011.12.08

	//cam_power_down(cam_g->curr);
	in = ktime_get();
	HalcamTraceDbg("Cam_init end sec %d nsec %d",in.tv.sec,in.tv.nsec);
	return rc;
#if 0 	//BYKIM_PREVENT
err3:	wake_lock_destroy(&cam_g->camera_wake_lock);
	device_destroy(cam_g->cam_class, MKDEV(BCM_CAM_MAJOR, 0));
	class_destroy(cam_g->cam_class);
#endif	
err:	
unregister_chrdev(BCM_CAM_MAJOR, "camera");
//unregister_chrdev(BCM_CAM2_MAJOR, "camera2");

	return rc;
}



#else // init_ori


static int __init cam_init(void)
{
	int rc;
	struct camera_sensor_t *c;
	ktime_t in = ktime_get();
	HalcamTraceDbg( "%s sec %d nsec %d", banner,in.tv.sec,in.tv.nsec);
	cam_g =
	(struct cam_generic_t *)kmalloc(sizeof(struct cam_generic_t), GFP_KERNEL);
	if (!cam_g) {
		HalcamTraceErr( "No memory for camera driver");
		return -ENOMEM;
	}
	rc = register_chrdev(BCM_CAM_MAJOR, "camera", &cam_fops);
	if (rc < 0) {
		HalcamTraceErr( "Camera: register_chrdev failed for major %d",
		       BCM_CAM_MAJOR);
		return rc;
	}
	cam_g->cam_class = class_create(THIS_MODULE, "camera");
	if (IS_ERR(cam_g->cam_class)) {
		unregister_chrdev(BCM_CAM_MAJOR, "camera");
		rc = PTR_ERR(cam_g->cam_class);
		goto err;
	}
	device_create(cam_g->cam_class, NULL, MKDEV(BCM_CAM_MAJOR, 0), NULL,
		      "camera");
	
	
//swsw_dual


	device_create(cam_g->cam_class, NULL, MKDEV(BCM_CAM_MAJOR, 1), NULL,
		      "camera2");

board_sysconfig(SYSCFG_CAMERA,SYSCFG_INIT);

#if 0//swsw_dual



#if 1//defined(CONFIG_BOARD_THUNDERBIRD_EDN31)
	cam_g->cam_regulator_a = regulator_get(NULL, "cam_vdda");
	if (!cam_g->cam_regulator_a || IS_ERR(cam_g->cam_regulator_a)) {
		HalcamTraceErr( "[cam_vdda]No Regulator available");
		rc = -EFAULT;
		goto err;
	}
	cam_g->cam_regulator_c = regulator_get(NULL, "cam_vddc");
	if (!cam_g->cam_regulator_c|| IS_ERR(cam_g->cam_regulator_c)) {
		HalcamTraceErr( "[cam_vddc]No Regulator available");
		rc = -EFAULT;
		goto err;
	}
#else
	cam_g->cam_regulator_i = regulator_get(NULL, "cam_vddi");
	if (!cam_g->cam_regulator_i || IS_ERR(cam_g->cam_regulator_i)) {
		HalcamTraceErr( "[cam_vddi]No Regulator available");
		rc = -EFAULT;
		goto err;
	}
#endif
#endif//swsw_dual
#if defined (CONFIG_CPU_FREQ_GOV_BCM21553)
	cam_g->cam_dvfs = cpufreq_bcm_client_get("cam_dvfs");
	if(!cam_g->cam_dvfs)
		HalcamTraceErr("Unable to get a DVFS client for camera driver !! ");
	else
		cpufreq_bcm_dvfs_enable(cam_g->cam_dvfs);
#endif	
	
#if 1 //CYK
	wake_lock_init(&cam_g->camera_wake_lock, WAKE_LOCK_SUSPEND, "camera");
	cam_g->gSysCtlHeader = register_sysctl_table(gSysCtl);
#else
	wake_lock_init(&cam_g->camera_wake_lock, WAKE_LOCK_SUSPEND, "camera");
	cam_g->gSysCtlHeader = register_sysctl_table(gSysCtl);

	cam_g->gSysCtlHeader = register_sysctl_table(gSysCtl_sub);
#endif

	cam_g->sens[CamSensorPrimary].sens_m	= CAMDRV_primary_get();//swsw_dual
#if (CAM_SENSOR_NUM==2)
	cam_g->sens[CamSensorSecondary].sens_m	= CAMDRV_secondary_get();
#endif
	cam_g->curr = CamSensorPrimary;
		
	//	cam_power_up(cam_g->curr);
	/* Alloc separately for primary and secondary cameras. For now only primary
	 cam_buf is the operating buffer
	 cam_buf_main always holds the addresses of the allocated boot time buffer
	 Stub 1 */
	c = &cam_g->sens[CamSensorPrimary];//swsw_dual
	c->devbusy = 0;
//	c->sens_m->DRV_GetRegulator();
	c->sensor_intf = c->sens_m->DRV_GetIntfConfig(CamSensorPrimary);//swsw_dual
	if (!c->sensor_intf) {
		HalcamTraceErr( "Unable to get sensor interface config ");
		rc = -EFAULT;
	}
	cam_g->sens[CamSensorPrimary].cam_i2c_datap = NULL;//swsw_dual
	cam_g->sens[CamSensorSecondary].cam_i2c_datap = NULL;
	/* For now only for primary camera */
//BYKIM_CAMACQ
#if 0 // work in camacq
	HalcamTraceDbg("Adding I2C driver **** ");
	rc = i2c_add_driver(&i2c_driver_cam);
#endif
	sema_init(&cam_g->cam_sem, 1);
	mdelay(2);
	/*
	if (cam_sensor_intf_seqsel(sensor, SensorPwrDn) != 0) {
		HalcamTraceErr( "Unable to Set power seq at Open");
		rc = -EFAULT;
	}*/

	//cam_power_down(cam_g->curr);
	in = ktime_get();
	HalcamTraceDbg("Cam_init end sec %d nsec %d",in.tv.sec,in.tv.nsec);
	return rc;
#if 0 	//BYKIM_PREVENT
err3:	wake_lock_destroy(&cam_g->camera_wake_lock);
	device_destroy(cam_g->cam_class, MKDEV(BCM_CAM_MAJOR, 0));
	class_destroy(cam_g->cam_class);
#endif	
err:	unregister_chrdev(BCM_CAM_MAJOR, "camera");
	return rc;
}
#endif
static void __exit cam_exit(void)
{
	if (cam_g->gSysCtlHeader != NULL) {
		unregister_sysctl_table(cam_g->gSysCtlHeader);
	}
	/*
	if (cam_g->cam_regulator)
		regulator_put(cam_g->cam_regulator);
	*/
	wake_lock_destroy(&cam_g->camera_wake_lock);
	device_destroy(cam_g->cam_class, MKDEV(BCM_CAM_MAJOR, 0));
	#if (CAM_SENSOR_NUM==2)
	device_destroy(cam_g->cam_class, MKDEV(BCM_CAM2_MAJOR, 0));
	#endif

	class_destroy(cam_g->cam_class);
	unregister_chrdev(BCM_CAM_MAJOR, "camera");
	//unregister_chrdev(BCM_CAM2_MAJOR, "camera2");
#if defined (CONFIG_CPU_FREQ_GOV_BCM21553)
	cpufreq_bcm_client_put(cam_g->cam_dvfs);
#endif
	kfree(cam_g);
}

module_init(cam_init);
module_exit(cam_exit);
MODULE_AUTHOR("Broadcom");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Camera Driver");

