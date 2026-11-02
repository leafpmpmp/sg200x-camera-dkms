/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Sophgo CV1800/SG2000 video input (VI): definitions shared by the raw
 * capture and the ISP code
 *
 * Copyright (C) 2026 morikon <leafpmpmp@gmail.com>
 */

#ifndef _CV1800_VI_H_
#define _CV1800_VI_H_

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/seq_file.h>

#include <media/media-device.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-dma-contig.h>

#define CV1800_VI_NAME			"cv1800-vi"

/* vip_sys, shared with the CSI-2 receiver */
#define VIP_SYS_RESETS			0x00
#define   VIP_RST_ISP_TOP		BIT(1)
#define   VIP_RST_ISP_TOP_APB		BIT(17)
#define VIP_SYS_RESERVE			0x04
#define   VIP_AXI_ISP_TOP_CLK_EN	BIT(0)
#define VIP_SYS_AXI_SW			0x10
#define   VIP_AXI_SW_SC_TOP		BIT(0)
#define   VIP_AXI_SW_ISP_RAW		BIT(1)
#define   VIP_AXI_SW_ISP_YUV		BIT(2)
#define VIP_SYS_CLK_CTRL1		0x1c
#define   VIP_CLK_MAC0_SRC		GENMASK(5, 4)
#define     VIP_CLK_SRC_VIP_SYS_2	2
#define VIP_SYS_DIV_CSI_MAC0		0x38
#define   VIP_DIV_EN			BIT(0)
#define   VIP_DIV_UPDATE		BIT(2)
#define   VIP_DIV_RATIO			GENMASK(20, 16)

/* Block offsets in the VI register window */
#define VI_FE0				0x00000
#define VI_CSIBDG0			0x00800
#define VI_DMA_CTL6			0x00b00
#define VI_ISPTOP			0x70000

/* ISP top */
#define TOP_INT_EVENT0			(VI_ISPTOP + 0x00)
#define   EV0_FRAME_DONE_FE0_CH0	BIT(0)
#define TOP_INT_EVENT1			(VI_ISPTOP + 0x04)
#define TOP_INT_EVENT2			(VI_ISPTOP + 0x08)
#define   EV2_FRAME_START_FE0_CH0	BIT(0)
#define   EV2_FRAME_ERR			BIT(8)
#define   EV2_INT_DMA_ERR		BIT(16)
#define TOP_INT_EVENT0_EN		(VI_ISPTOP + 0x10)
#define TOP_INT_EVENT1_EN		(VI_ISPTOP + 0x14)
#define TOP_INT_EVENT2_EN		(VI_ISPTOP + 0x18)
#define TOP_SW_CTRL_0			(VI_ISPTOP + 0x20)
#define   SW0_SHAW_UP_FE0		GENMASK(19, 16)
#define TOP_SW_CTRL_1			(VI_ISPTOP + 0x24)
#define   SW1_PQ_UP_FE0			GENMASK(3, 0)
#define TOP_CTRL_MODE_SEL0		(VI_ISPTOP + 0x28)
#define   SEL0_TRIG_STR_SEL_FE0		GENMASK(3, 0)
#define   SEL0_SHAW_UP_SEL_FE0		GENMASK(19, 16)
#define TOP_CTRL_MODE_SEL1		(VI_ISPTOP + 0x2c)
#define   SEL1_PQ_UP_SEL_FE0		GENMASK(3, 0)
#define TOP_SCENARIOS_CTRL		(VI_ISPTOP + 0x30)
#define   SCN_ROUTING			GENMASK(21, 0)
#define   SCN_FE_DMA_SHARE_ENABLE	BIT(22)
#define TOP_SW_RST			(VI_ISPTOP + 0x34)
#define   SW_RST_AXI			BIT(6)
#define   SW_RST_ALL			0x37f
#define TOP_BLK_IDLE			(VI_ISPTOP + 0x38)
#define   BLK_IDLE_FE0			BIT(0)
#define   BLK_IDLE_WDMA0		BIT(8)
#define   BLK_IDLE_WDMA1		BIT(9)
#define   BLK_IDLE_RAW			(BLK_IDLE_FE0 | BLK_IDLE_WDMA0 | BLK_IDLE_WDMA1)
#define TOP_SVN_VERSION			(VI_ISPTOP + 0x74)

/* Pre-raw front end 0 */
#define FE_CTRL				(VI_FE0 + 0x00)
#define   FE_BAYER_TYPE_LE		GENMASK(3, 0)
#define   FE_BAYER_TYPE_SE		GENMASK(7, 4)
#define FE_FRAME_SIZE			(VI_FE0 + 0x04)
#define FE_FRAME_VLD			(VI_FE0 + 0x28)
#define   FE_FRAME_VLD_CH0		BIT(0)
#define   FE_PQ_VLD_CH0			BIT(4)

/* CSI bridge 0 */
#define BDG_TOP_CTRL			(VI_CSIBDG0 + 0x00)
#define   BDG_CSI_MODE			GENMASK(1, 0)
#define     BDG_CSI_MODE_SENSOR		1
#define     BDG_CSI_MODE_PATGEN		3
#define   BDG_CSI_IN_FORMAT		BIT(2)
#define   BDG_CH_NUM			GENMASK(5, 4)
#define   BDG_CH0_DMA_WR_ENABLE		BIT(6)
#define   BDG_PXL_DATA_SEL		BIT(10)
#define   BDG_RESET_MODE		BIT(13)
#define   BDG_VS_MODE			BIT(14)
#define   BDG_ABORT_MODE		BIT(15)
#define   BDG_ABORT			BIT(17)
#define   BDG_CSI_ENABLE		BIT(24)
#define   BDG_TGEN_ENABLE		BIT(25)
#define   BDG_CSI_UP_REG		BIT(31)
#define BDG_INT_CTRL			(VI_CSIBDG0 + 0x04)
#define   BDG_CH0_VS_INT_EN		BIT(0)
#define   BDG_DMA_ERROR_INT_EN		BIT(29)
#define   BDG_FIFO_OVERFLOW_INT_EN	BIT(31)
#define BDG_DMA_DPCM_MODE		(VI_CSIBDG0 + 0x08)
#define BDG_CH_SIZE(n)			(VI_CSIBDG0 + 0x10 + 4 * (n))
#define BDG_CH0_CROP_EN			(VI_CSIBDG0 + 0x20)
#define BDG_PAT_GEN_CTRL		(VI_CSIBDG0 + 0x60)
#define   BDG_PAT_EN			BIT(0)
#define   BDG_PAT_BAYER_ID		GENMASK(13, 12)
#define BDG_PAT_IDX_CTRL		(VI_CSIBDG0 + 0x64)
#define   BDG_PAT_IDX			GENMASK(12, 8)
#define BDG_PAT_COLOR_0			(VI_CSIBDG0 + 0x68)
#define BDG_PAT_COLOR_1			(VI_CSIBDG0 + 0x6c)
#define BDG_BACKGROUND_COLOR_0		(VI_CSIBDG0 + 0x70)
#define BDG_BACKGROUND_COLOR_1		(VI_CSIBDG0 + 0x74)
#define BDG_MDE_V_SIZE			(VI_CSIBDG0 + 0x80)
#define BDG_MDE_H_SIZE			(VI_CSIBDG0 + 0x84)
#define BDG_FDE_V_SIZE			(VI_CSIBDG0 + 0x88)
#define BDG_FDE_H_SIZE			(VI_CSIBDG0 + 0x8c)
#define BDG_HSYNC_CTRL			(VI_CSIBDG0 + 0x90)
#define BDG_VSYNC_CTRL			(VI_CSIBDG0 + 0x94)
#define BDG_TGEN_TT_SIZE		(VI_CSIBDG0 + 0x98)
#define BDG_CH0_DEBUG_0			(VI_CSIBDG0 + 0xa0)	/* pixel count */
#define BDG_CH0_DEBUG_1			(VI_CSIBDG0 + 0xa4)	/* line count */
#define BDG_CH0_DEBUG_2			(VI_CSIBDG0 + 0xa8)
#define   BDG_CH0_VS_CNT		GENMASK(15, 0)
#define   BDG_CH0_TRIG_CNT		GENMASK(31, 16)
#define BDG_CH0_DEBUG_3			(VI_CSIBDG0 + 0xac)	/* idle flags */
#define BDG_INT_STATUS_0		(VI_CSIBDG0 + 0xe0)
#define   BDG_CH0_FRAME_DROP_INT	BIT(0)
#define   BDG_CH0_VS_INT		BIT(1)
#define   BDG_CH0_TRIG_INT		BIT(2)
#define   BDG_CH0_WIDTH_GT_INT		BIT(4)
#define   BDG_CH0_WIDTH_LS_INT		BIT(5)
#define   BDG_CH0_HEIGHT_GT_INT		BIT(6)
#define   BDG_CH0_HEIGHT_LS_INT		BIT(7)
#define   BDG_CH0_SIZE_ERR		(BDG_CH0_WIDTH_GT_INT | BDG_CH0_WIDTH_LS_INT | \
					 BDG_CH0_HEIGHT_GT_INT | BDG_CH0_HEIGHT_LS_INT)
#define BDG_INT_STATUS_1		(VI_CSIBDG0 + 0xe4)
#define   BDG_FIFO_OVERFLOW_INT		BIT(0)
#define   BDG_RES_OVER_MAX_INT		BIT(1)
#define   BDG_DMA_ERROR_INT		BIT(4)
#define BDG_WDMA_CH0_CROP_EN		(VI_CSIBDG0 + 0x110)

/* Raw write DMA 6, fed by CSIBDG0 channel 0 */
#define DMA_SYS_CONTROL			(VI_DMA_CTL6 + 0x00)
#define   DMA_BASEH			GENMASK(15, 8)
#define   DMA_BASE_SEL			BIT(16)
#define   DMA_STRIDE_SEL		BIT(17)
#define   DMA_SEGLEN_SEL		BIT(18)
#define   DMA_SEGNUM_SEL		BIT(19)
#define DMA_BASE_ADDR			(VI_DMA_CTL6 + 0x04)
#define DMA_SEGLEN			(VI_DMA_CTL6 + 0x08)
#define DMA_STRIDE			(VI_DMA_CTL6 + 0x0c)
#define DMA_SEGNUM			(VI_DMA_CTL6 + 0x10)

/* Most sizes are a start/end or width/height pair of 13-bit fields */
#define PAIR13(lo, hi)			(FIELD_PREP(GENMASK(12, 0), lo) | \
					 FIELD_PREP(GENMASK(28, 16), hi))
#define PAIR12(lo, hi)			(FIELD_PREP(GENMASK(11, 0), lo) | \
					 FIELD_PREP(GENMASK(27, 16), hi))

#define CV1800_VI_MIN_SIZE		32
#define CV1800_VI_MAX_SIZE		8064
/* Widest frame for the ISP: frames 2896 or more wide stall on every frame */
#define CV1800_ISP_MAX_WIDTH		2880
/* Tallest frame for the ISP: frames 4092 or more tall stall */
#define CV1800_ISP_MAX_HEIGHT		4090
#define CV1800_VI_DEF_WIDTH		1920
#define CV1800_VI_DEF_HEIGHT		1080

/* Pattern generator active window offset inside its frame */
#define PATGEN_OFFSET			0x10
#define PATGEN_FPS			30

/* CSI MAC 0 clock: vip_sys_2 * (ratio + 1) / 32 */
#define CV1800_VI_MAC_RATIO		15

enum {
	CV1800_VI_PAD_SINK,
	CV1800_VI_PAD_SOURCE,		/* raw Bayer, to the raw capture node */
	CV1800_VI_PAD_YUV,		/* ISP output, to the YUV capture node */
	CV1800_VI_NUM_PADS
};

enum cv1800_vi_bayer {
	CV1800_VI_BAYER_BG,
	CV1800_VI_BAYER_GB,
	CV1800_VI_BAYER_GR,
	CV1800_VI_BAYER_RG,
};

/* Which capture node owns the front end */
enum cv1800_vi_mode {
	CV1800_VI_MODE_IDLE,
	CV1800_VI_MODE_RAW,
	CV1800_VI_MODE_YUV,
};

struct cv1800_vi_format {
	u32 fourcc;
	u32 code;			/* source pad, always 12 bit */
	u32 sink_codes[3];		/* 8, 10 and 12 bit with the same order */
	enum cv1800_vi_bayer bayer;
};

/*
 * A destination of the front end write DMA: a buffer of the raw capture
 * node, or an internal raw buffer that the ISP reads back.
 */
struct cv1800_vi_frame {
	struct list_head list;
	dma_addr_t addr;
	u64 timestamp;
	u32 sequence;
	bool error;
};

struct cv1800_vi_buffer {
	struct vb2_v4l2_buffer vb;
	struct cv1800_vi_frame frame;
};

struct cv1800_isp_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
	dma_addr_t addr;
};

/* Raw frames between the front end and the ISP: written, armed, read */
#define CV1800_ISP_NUM_RAW		3

struct cv1800_isp_params {
	u16 red_gain;			/* white balance gains, 1024 = 1.0 */
	u16 blue_gain;
	u16 black_level;		/* in 12-bit units */
	bool gamma;
	s16 ccm[9];			/* colour matrix, rows R G B, 1024 = 1.0 */
	u8 nr_raw;			/* noise reduction strengths, 0 = off */
	u8 nr_luma;
	u8 nr_chroma;
};

struct cv1800_isp {
	struct video_device vdev;
	struct media_pad vdev_pad;
	struct vb2_queue queue;
	struct v4l2_pix_format pix;
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_ctrl *red_gain;
	struct v4l2_ctrl *blue_gain;
	struct v4l2_ctrl *black_level;
	struct v4l2_ctrl *gamma;
	struct v4l2_ctrl *ccm;
	struct v4l2_ctrl *nr_raw;
	struct v4l2_ctrl *nr_luma;
	struct v4l2_ctrl *nr_chroma;

	struct cv1800_vi_frame raw[CV1800_ISP_NUM_RAW];
	struct list_head raw_free;	/* protected by cv1800_vi.qlock */
	void *raw_cookie;
	dma_addr_t raw_dma;
	size_t raw_size;

	/* Protected by cv1800_vi.qlock */
	struct list_head out;		/* queued output buffers */
	struct cv1800_isp_buffer *out_active;
	struct cv1800_vi_frame *busy;	/* raw frame the ISP reads */
	struct cv1800_vi_frame *ready;	/* raw frame waiting for the ISP */
	unsigned int busy_frames;	/* frames done by the front end since */
	struct cv1800_isp_params params;
	bool params_dirty;
	unsigned int triggers;
	unsigned int done;
	unsigned int dropped;
	unsigned int timeouts;
	u32 ev0_seen;
};

struct cv1800_vi {
	struct device *dev;
	void __iomem *base;
	struct regmap *vip_sys;
	struct clk_bulk_data *clks;
	int num_clks;
	struct clk *mac_src;		/* vip_sys_2, the CSI MAC clock source */
	struct reset_control *rst;
	int irq;

	struct media_device mdev;
	struct v4l2_device v4l2_dev;
	struct v4l2_async_notifier notifier;
	struct v4l2_subdev *source;
	u32 source_pad;

	struct v4l2_subdev sd;
	struct media_pad pads[CV1800_VI_NUM_PADS];
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_ctrl *test_pattern;

	struct video_device vdev;
	struct media_pad vdev_pad;
	struct vb2_queue queue;
	struct mutex lock;		/* both video nodes and queues, mode */
	struct v4l2_pix_format pix;
	const struct cv1800_vi_format *fmt;
	enum cv1800_vi_mode mode;

	/* Frame geometry latched at stream start */
	u32 width;
	u32 height;
	u32 raw_bpl;
	enum cv1800_vi_bayer bayer;

	spinlock_t qlock;		/* buffer lists and DMA arming */
	struct list_head buffers;	/* queued raw capture buffers */
	struct list_head *fe_free;	/* where the front end takes frames */
	struct cv1800_vi_frame *active;	/* frame in progress or armed */
	struct cv1800_vi_frame *next;	/* armed for the frame after it */
	bool running;
	bool yuv;			/* raw frames go to the ISP */
	u16 arm_trig;			/* bridge trigger count at the last arm */
	unsigned int pattern;		/* test pattern latched at stream start */
	u16 last_vs;
	u32 frames;
	u64 sof_ts;
	unsigned int errors;
	unsigned int skip;		/* frames to let pass before arming */
	struct work_struct recover;	/* resets the bridge after a bad frame */
	unsigned int recoveries;

	struct cv1800_isp isp;

	/* Every status bit seen since stream start, for debugfs */
	u32 ev0_seen, ev2_seen, s0_seen, s1_seen;
	unsigned int irq_log;		/* interrupts left to log */
	struct dentry *debugfs;
};

static inline struct cv1800_vi *sd_to_vi(struct v4l2_subdev *sd)
{
	return container_of(sd, struct cv1800_vi, sd);
}

static inline u32 vi_read(struct cv1800_vi *vi, u32 reg)
{
	return readl(vi->base + reg);
}

static inline void vi_write(struct cv1800_vi *vi, u32 reg, u32 val)
{
	writel(val, vi->base + reg);
}

static inline void vi_update(struct cv1800_vi *vi, u32 reg, u32 mask, u32 val)
{
	vi_write(vi, reg, (vi_read(vi, reg) & ~mask) | (val & mask));
}

/* Packed 12-bit Bayer line, padded to 16 bytes */
static inline u32 cv1800_vi_stride(u32 width)
{
	return ALIGN(3 * DIV_ROUND_UP(width, 2), 16);
}

/* cv1800-vi-core.c */
const struct cv1800_vi_format *cv1800_vi_find_code(u32 code);
void cv1800_vi_arm_next(struct cv1800_vi *vi);
int cv1800_vi_stream_start(struct cv1800_vi *vi, enum cv1800_vi_mode mode);
void cv1800_vi_stream_stop(struct cv1800_vi *vi);

/* cv1800-vi-isp.c */
int cv1800_isp_init(struct cv1800_vi *vi);
void cv1800_isp_cleanup(struct cv1800_vi *vi);
int cv1800_isp_prepare(struct cv1800_vi *vi);
void cv1800_isp_unprepare(struct cv1800_vi *vi);
void cv1800_isp_hw_setup(struct cv1800_vi *vi);
void cv1800_isp_hw_stop(struct cv1800_vi *vi);
void cv1800_isp_raw_done(struct cv1800_vi *vi, struct cv1800_vi_frame *raw);
void cv1800_isp_requeue(struct cv1800_vi *vi);
void cv1800_isp_irq(struct cv1800_vi *vi, u32 ev0, u32 ev2);
void cv1800_isp_status_show(struct cv1800_vi *vi, struct seq_file *s, bool hw);
void cv1800_isp_init_state(struct v4l2_mbus_framefmt *yuv,
			   const struct v4l2_mbus_framefmt *sink);

#endif /* _CV1800_VI_H_ */
