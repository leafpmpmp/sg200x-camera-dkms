// SPDX-License-Identifier: GPL-2.0-only
/*
 * Sophgo CV1800/SG2000 VI: the ISP path to YUV
 *
 * The front end writes each raw frame to an internal buffer. The back end
 * reads the frame back from memory and runs it through the raw, RGB and
 * YUV stages, which write 4:2:0 YUV to the capture buffer. The vendor
 * driver uses this "offline back end" mode for two sensors.
 *
 * Active stages: black level, white balance gains, demosaic, colour
 * matrix, gamma, and the BT.601 RGB to YUV matrix. Everything else is off:
 * lens shading, noise reduction, sharpening, tone mapping and all
 * statistics.
 *
 * The register sequence follows the vendor's GPL kernel driver (vi_drv.c,
 * vi_subsys_ctrl.c and the vi_*_ip_ctrl.c files of osdrv/interdrv/vi in
 * the Milk-V duo-buildroot-sdk-v2).
 *
 * Copyright (C) 2026 Lukasz Sobala <lsobala1@gmail.com>
 */

#include <linux/sizes.h>

#include "cv1800-vi.h"
#include "cv1800-vi-isp-regs.h"

#define CV1800_ISP_CID_BLACK_LEVEL	(V4L2_CID_USER_BASE | 0x1000)
#define CV1800_ISP_CID_CCM		(V4L2_CID_USER_BASE | 0x1001)

/* Front end frames to wait for a busy ISP before resetting the VI */
#define CV1800_ISP_TIMEOUT_FRAMES	3

/* sRGB transfer curve: 256 points over the 12-bit input, 12-bit output */
static const u16 cv1800_isp_srgb[256] = {
	   0,  203,  347,  452,  538,  613,  679,  739,  794,  845,  894,  939,
	 982, 1023, 1062, 1099, 1136, 1170, 1204, 1237, 1268, 1299, 1329, 1358,
	1386, 1414, 1441, 1467, 1493, 1518, 1543, 1567, 1591, 1615, 1638, 1660,
	1683, 1704, 1726, 1747, 1768, 1789, 1809, 1829, 1849, 1868, 1888, 1907,
	1926, 1944, 1962, 1981, 1998, 2016, 2034, 2051, 2068, 2085, 2102, 2119,
	2135, 2151, 2168, 2184, 2199, 2215, 2231, 2246, 2261, 2277, 2292, 2307,
	2321, 2336, 2351, 2365, 2379, 2394, 2408, 2422, 2436, 2449, 2463, 2477,
	2490, 2504, 2517, 2530, 2543, 2557, 2570, 2582, 2595, 2608, 2621, 2633,
	2646, 2658, 2670, 2683, 2695, 2707, 2719, 2731, 2743, 2755, 2767, 2778,
	2790, 2802, 2813, 2825, 2836, 2847, 2859, 2870, 2881, 2892, 2903, 2914,
	2925, 2936, 2947, 2958, 2969, 2979, 2990, 3001, 3011, 3022, 3032, 3043,
	3053, 3063, 3074, 3084, 3094, 3104, 3114, 3124, 3134, 3144, 3154, 3164,
	3174, 3184, 3194, 3203, 3213, 3223, 3232, 3242, 3252, 3261, 3271, 3280,
	3289, 3299, 3308, 3317, 3327, 3336, 3345, 3354, 3363, 3372, 3382, 3391,
	3400, 3409, 3418, 3426, 3435, 3444, 3453, 3462, 3471, 3479, 3488, 3497,
	3505, 3514, 3523, 3531, 3540, 3548, 3557, 3565, 3574, 3582, 3590, 3599,
	3607, 3615, 3624, 3632, 3640, 3648, 3656, 3665, 3673, 3681, 3689, 3697,
	3705, 3713, 3721, 3729, 3737, 3745, 3753, 3761, 3769, 3776, 3784, 3792,
	3800, 3807, 3815, 3823, 3831, 3838, 3846, 3854, 3861, 3869, 3876, 3884,
	3891, 3899, 3906, 3914, 3921, 3929, 3936, 3944, 3951, 3958, 3966, 3973,
	3980, 3988, 3995, 4002, 4009, 4017, 4024, 4031, 4038, 4045, 4053, 4060,
	4067, 4074, 4081, 4088,
};

struct cv1800_isp_format {
	u32 fourcc;
	bool swap_uv;
};

/*
 * The vendor stack outputs NV21 from this path. NV12 swaps the two chroma
 * rows of the RGB to YUV matrix.
 */
static const struct cv1800_isp_format cv1800_isp_formats[] = {
	{ .fourcc = V4L2_PIX_FMT_NV21, .swap_uv = false },
	{ .fourcc = V4L2_PIX_FMT_NV12, .swap_uv = true },
};

static const struct cv1800_isp_format *cv1800_isp_find_format(u32 fourcc)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(cv1800_isp_formats); i++)
		if (cv1800_isp_formats[i].fourcc == fourcc)
			return &cv1800_isp_formats[i];
	return NULL;
}

static inline struct cv1800_isp_buffer *to_isp_buffer(struct vb2_v4l2_buffer *vb)
{
	return container_of(vb, struct cv1800_isp_buffer, vb);
}

static void vi_set(struct cv1800_vi *vi, u32 reg, u32 bits)
{
	vi_update(vi, reg, bits, bits);
}

static void vi_clear(struct cv1800_vi *vi, u32 reg, u32 bits)
{
	vi_update(vi, reg, bits, 0);
}

/* -----------------------------------------------------------------------------
 * Hardware
 */

static void cv1800_isp_dma_setup(struct cv1800_vi *vi, u32 dma, u32 seglen,
				 u32 stride, u32 segnum)
{
	vi_write(vi, DMA_CTL_SEGLEN(dma), seglen);
	vi_write(vi, DMA_CTL_STRIDE(dma), stride);
	vi_write(vi, DMA_CTL_SEGNUM(dma), segnum);
	vi_set(vi, DMA_CTL_SYS_CONTROL(dma), DMA_CTL_SW_MODE);
}

static void cv1800_isp_dma_addr(struct cv1800_vi *vi, u32 dma, dma_addr_t addr)
{
	vi_write(vi, DMA_CTL_BASE_ADDR(dma), lower_32_bits(addr));
	vi_update(vi, DMA_CTL_SYS_CONTROL(dma), DMA_BASEH,
		  FIELD_PREP(DMA_BASEH, upper_32_bits(addr)));
}

/* RGB gamma table 0: 256 points and the value at full scale */
static void cv1800_isp_load_gamma(struct cv1800_vi *vi)
{
	unsigned int i;

	vi_update(vi, GAMMA_PROG_CTRL, GAMMA_WSEL | GAMMA_PROG_EN | GAMMA_PROG_1TO3_EN,
		  GAMMA_PROG_EN | GAMMA_PROG_1TO3_EN);
	vi_update(vi, GAMMA_PROG_ST_ADDR, GAMMA_ST_ADDR, 0);
	vi_set(vi, GAMMA_PROG_ST_ADDR, GAMMA_ST_W);
	vi_write(vi, GAMMA_PROG_MAX, 4095);
	for (i = 0; i < ARRAY_SIZE(cv1800_isp_srgb); i += 2)
		vi_write(vi, GAMMA_PROG_DATA,
			 FIELD_PREP(GAMMA_DATA_E, cv1800_isp_srgb[i]) |
			 FIELD_PREP(GAMMA_DATA_O, cv1800_isp_srgb[i + 1]) | GAMMA_W);
	vi_clear(vi, GAMMA_PROG_CTRL, GAMMA_RSEL);
	vi_clear(vi, GAMMA_PROG_CTRL, GAMMA_PROG_EN);
}

/*
 * BT.601 RGB to YUV, rows Y, Cb, Cr, 1024 = 1.0. The offsets are in 10-bit
 * units. The full-range matrix is the reset value of the stage.
 */
static const s16 cv1800_isp_csc_full[9] = {
	306, 601, 117, -173, -339, 512, 512, -429, -83,
};

static const s16 cv1800_isp_csc_lim[9] = {
	263, 516, 100, -152, -298, 450, 450, -377, -73,
};

/*
 * REG_4..REG_9 hold, in this order: c00 c01, c02 c10, c11 c12, c20 c21,
 * c22 offset0, offset1 offset2. The other chroma order swaps the chroma
 * rows.
 */
static void cv1800_isp_setup_csc(struct cv1800_vi *vi, bool swap_uv, bool full)
{
	const s16 *m = full ? cv1800_isp_csc_full : cv1800_isp_csc_lim;
	u32 c[9], o[3] = { full ? 0 : 64, 512, 512 };
	unsigned int i;

	for (i = 0; i < 9; i++)
		c[i] = (u16)m[i] & FIELD_MAX(CSC_LO14);

	if (swap_uv)
		for (i = 3; i < 6; i++)
			swap(c[i], c[i + 3]);

	for (i = 0; i < 4; i++)
		vi_write(vi, CSC_REG(i), FIELD_PREP(CSC_LO14, c[2 * i]) |
			 FIELD_PREP(CSC_HI14, c[2 * i + 1]));
	vi_write(vi, CSC_REG(4), FIELD_PREP(CSC_LO14, c[8]) |
		 FIELD_PREP(CSC_HI11, o[0]));
	vi_write(vi, CSC_REG(5), FIELD_PREP(CSC_LO11, o[1]) |
		 FIELD_PREP(CSC_HI11, o[2]));

	vi_set(vi, CSC_REG_0, CSC_ENABLE);
}

/* Called with qlock held, or before the stream runs, while the ISP is idle */
static void cv1800_isp_apply_params(struct cv1800_vi *vi)
{
	const struct cv1800_isp_params *p = &vi->isp.params;
	u32 bl = p->black_level, gain, v;
	unsigned int i;

	if (bl) {
		/* Stretch what is left above the black level to full scale */
		gain = DIV_ROUND_CLOSEST(4095 * 1024, 4095 - bl);
		v = FIELD_PREP(BLC_LO12, bl) | FIELD_PREP(BLC_HI12, bl);
		vi_write(vi, BLC_3(ISP_BLC5), v);
		vi_write(vi, BLC_4(ISP_BLC5), v);
		v = FIELD_PREP(BLC_LO16, gain) | FIELD_PREP(BLC_HI16, gain);
		vi_write(vi, BLC_5(ISP_BLC5), v);
		vi_write(vi, BLC_6(ISP_BLC5), v);
		vi_write(vi, BLC_9(ISP_BLC5), 0);
		vi_write(vi, BLC_A(ISP_BLC5), 0);
		vi_set(vi, BLC_2(ISP_BLC5), BLC_ENABLE);
	} else {
		vi_clear(vi, BLC_2(ISP_BLC5), BLC_ENABLE);
	}

	vi_write(vi, WBG_4(ISP_WBG0), FIELD_PREP(WBG_RGAIN, p->red_gain) |
		 FIELD_PREP(WBG_GGAIN, 1024));
	vi_update(vi, WBG_5(ISP_WBG0), WBG_BGAIN,
		  FIELD_PREP(WBG_BGAIN, p->blue_gain));

	/* s3.10, two's complement in 14 bits */
	for (i = 0; i < ARRAY_SIZE(p->ccm); i++)
		vi_write(vi, CCM_COEF(ISP_CCM0, i), (u16)p->ccm[i] & GENMASK(13, 0));

	vi_update(vi, GAMMA_CTRL, GAMMA_ENABLE, p->gamma ? GAMMA_ENABLE : 0);
}

/*
 * Called from cv1800_vi_hw_setup() after the VI reset and the front end
 * setup, before the bridge runs. Configures the back end and the post-raw
 * stages for one frame size; the per-frame work is only the DMA addresses
 * and the trigger.
 */
void cv1800_isp_hw_setup(struct cv1800_vi *vi)
{
	struct cv1800_isp *isp = &vi->isp;
	u32 w = vi->width, h = vi->height;
	u32 size = PAIR13(w - 1, h - 1);
	u32 bayer = vi->bayer;
	const struct cv1800_isp_format *fmt =
		cv1800_isp_find_format(isp->pix.pixelformat);
	u32 v;

	/* Routing: front end to memory, memory to back end, back end to raw */
	v = vi_read(vi, TOP_SCENARIOS_CTRL);
	v &= ~(SCN_PRE2BE_L_ENABLE | SCN_PRE2BE_S_ENABLE | SCN_PRE2YUV_422_ENABLE |
	       SCN_BE2RAW_S_ENABLE | SCN_BE_RDMA_S_ENABLE | SCN_BE_WDMA_L_ENABLE |
	       SCN_BE_WDMA_S_ENABLE | SCN_BE_SRC_SEL | SCN_AF_RAW0YUV1 |
	       SCN_RGBMP_ONLINE_L_ENABLE | SCN_RGBMP_ONLINE_S_ENABLE |
	       SCN_RAW2YUV_422_ENABLE | SCN_HDR_ENABLE | SCN_HW_AUTO_ENABLE |
	       SCN_DCI_RGB0YUV1);
	v |= SCN_BE2RAW_L_ENABLE | SCN_BE_RDMA_L_ENABLE | SCN_FE_DMA_SHARE_ENABLE;
	vi_write(vi, TOP_SCENARIOS_CTRL, v);

	/* Back end input: the raw frame in memory */
	vi_update(vi, VI_SEL_0, VI_SEL_DMA_LD_DPCM_MODE | VI_SEL_DPCM_RX_XSTR, 0);
	vi_write(vi, VI_SEL_1, size);
	cv1800_isp_dma_setup(vi, ISP_DMA_CTL4, cv1800_vi_stride(w), vi->raw_bpl, h);

	/* Back end */
	vi_update(vi, BE_TOP_CTRL, BE_BAYER_TYPE_LE | BE_BAYER_TYPE_SE | BE_CH_NUM,
		  FIELD_PREP(BE_BAYER_TYPE_LE, bayer) |
		  FIELD_PREP(BE_BAYER_TYPE_SE, bayer));
	vi_write(vi, BE_IMG_SIZE_LE, size);
	vi_write(vi, BE_UP_PQ_EN, 1);
	vi_write(vi, BE_DEBUG_ENABLE, 1);
	vi_clear(vi, PRE_WDMA_CTRL, PRE_WDMA_WDMI_EN_LE | PRE_WDMA_WDMI_EN_SE);
	vi_update(vi, PRE_WDMA_RDMI_DPCM, PRE_WDMA_DPCM_MODE | PRE_WDMA_DPCM_XSTR, 0);

	vi_clear(vi, BLC_0(ISP_BLC5), BLC_BYPASS);
	vi_clear(vi, BLC_2(ISP_BLC6), BLC_ENABLE);
	vi_clear(vi, DPC_2(ISP_DPC0), DPC_ENABLES);
	vi_clear(vi, DPC_2(ISP_DPC1), DPC_ENABLES);
	vi_clear(vi, AF_KICKOFF, AF_ENABLE);
	vi_clear(vi, AF_DMI_ENABLE, DMI_ENABLE);
	vi_write(vi, AF_BYPASS, 1);

	/* Raw top: fed by the back end, not by its own read DMA */
	vi_write(vi, RAWTOP_RAW_2, FIELD_PREP(GENMASK(13, 0), w - 1) |
		 FIELD_PREP(GENMASK(29, 16), h - 1));
	vi_update(vi, RAWTOP_BAYER_TYPE_TOPLEFT, RAWTOP_BAYER_TYPE,
		  FIELD_PREP(RAWTOP_BAYER_TYPE, bayer));
	vi_clear(vi, RAWTOP_RDMI_ENABLE, RAWTOP_RDMI_EN | RAWTOP_CH_NUM);
	vi_clear(vi, RAWTOP_PATGEN1, RAWTOP_PG_ENABLE);
	vi_write(vi, RAWTOP_CTRL, 0);
	vi_write(vi, RAWTOP_RAW_4, 0);
	vi_write(vi, RAWTOP_DPCM_MODE, 0);
	vi_write(vi, RAWTOP_RDMA_SIZE, FIELD_PREP(GENMASK(15, 0), w - 1) |
		 FIELD_PREP(GENMASK(31, 16), h - 1));
	vi_write(vi, RAW_RDMA_CONFIG, 0);
	vi_write(vi, RAW_RDMA_SIZE, FIELD_PREP(GENMASK(15, 0), w - 1) |
		 FIELD_PREP(GENMASK(31, 16), h - 1));
	vi_write(vi, RAW_RDMA_DPCM_MODE, 0);

	vi_write(vi, BNR_OUT_SEL, BNR_OUT_B_DELAY);
	vi_clear(vi, LSC_ENABLE_REG, LSC_ENABLE);
	vi_clear(vi, STS_DMI_ENABLE(ISP_LSC), DMI_ENABLE);
	vi_clear(vi, LSC_BLD, LSC_BLDRATIO_ENABLE);
	vi_set(vi, CFA_REG_00, CFA_SHDW_SEL | CFA_ENABLE);
	vi_clear(vi, RGBCAC_CTRL, RGBCAC_ENABLE);
	vi_clear(vi, LCAC_REG00, LCAC_ENABLE);
	vi_clear(vi, GMS_ENABLE_REG, GMS_ENABLE);
	vi_clear(vi, STS_DMI_ENABLE(ISP_GMS), DMI_ENABLE);

	vi_clear(vi, WBG_0(ISP_WBG0), WBG_BYPASS);
	vi_write(vi, WBG_34(ISP_WBG0), 0);
	vi_write(vi, WBG_38(ISP_WBG0), 0);
	vi_write(vi, WBG_3C(ISP_WBG0), 0);
	vi_set(vi, WBG_2(ISP_WBG0), WBG_ENABLE);
	vi_clear(vi, WBG_2(ISP_WBG1), WBG_ENABLE);

	vi_update(vi, LMAP_LMP_0(ISP_LMAP0), LMAP_ENABLE | LMAP_FORCE_DMA_DISABLE,
		  LMAP_FORCE_DMA_DISABLE);
	vi_update(vi, LMAP_LMP_0(ISP_LMAP1), LMAP_ENABLE | LMAP_FORCE_DMA_DISABLE,
		  LMAP_FORCE_DMA_DISABLE);
	vi_write(vi, AEHIST_ENABLE(ISP_AEHIST0), 0);
	vi_clear(vi, STS_DMI_ENABLE(ISP_AEHIST0), DMI_ENABLE);
	vi_write(vi, AEHIST_ENABLE(ISP_AEHIST1), 0);
	vi_clear(vi, STS_DMI_ENABLE(ISP_AEHIST1), DMI_ENABLE);

	/* RGB top */
	vi_update(vi, RGBTOP_REG_0, RGBTOP_BAYER_TYPE,
		  FIELD_PREP(RGBTOP_BAYER_TYPE, bayer));
	vi_write(vi, RGBTOP_REG_9, size);
	vi_set(vi, RGBTOP_DBG_IP_S_VLD, RGBTOP_IP_DBG_EN);

	vi_clear(vi, HIST_V_IP_CONFIG, HIST_EDGE_V_ENABLE);
	vi_update(vi, HIST_V_DMI_ENABLE, DMI_ENABLE | DMI_FORCE_DMA_DISABLE,
		  DMI_FORCE_DMA_DISABLE);

	/* CCM0 takes the colour matrix control, in cv1800_isp_apply_params() */
	vi_update(vi, CCM_CTRL(ISP_CCM0), CCM_SHDW_SEL | CCM_ENABLE,
		  CCM_SHDW_SEL | CCM_ENABLE);
	vi_clear(vi, CCM_CTRL(ISP_CCM1), CCM_ENABLE);

	vi_clear(vi, DHZ_BYPASS, DEHAZE_ENABLE);
	vi_clear(vi, YGAMMA_CTRL, YGAMMA_ENABLE);
	cv1800_isp_load_gamma(vi);
	vi_clear(vi, CLUT_CTRL, CLUT_ENABLE);
	vi_write(vi, RGBDITHER, RGB_DITHER_SHDW_SEL |
		 FIELD_PREP(RGB_DITHER_WIDTHM1, w - 1) |
		 FIELD_PREP(RGB_DITHER_HEIGHTM1, h - 1));
	cv1800_isp_setup_csc(vi, fmt && fmt->swap_uv,
			     isp->pix.quantization == V4L2_QUANTIZATION_FULL_RANGE);

	vi_update(vi, MMAP_REG_00, MMAP_0_ENABLE | MMAP_1_ENABLE | MMAP_BYPASS,
		  MMAP_BYPASS);
	vi_update(vi, MMAP_REG_6C, MMAP_FORCE_DMA_DISABLE, MMAP_FORCE_DMA_DISABLE);
	vi_clear(vi, MMAP_REG_D0, MMAP_CROP_ENABLE_SCALAR);
	vi_clear(vi, FUSION_FS_CTRL_0, FS_ENABLE | FS_MC_ENABLE);
	vi_update(vi, FUSION_FS_SE_GAIN, FS_OUT_SEL,
		  FIELD_PREP(FS_OUT_SEL, FS_OUT_LONG));
	vi_update(vi, LTM_REG_H00, LTM_ENABLE | LTM_DARK_ENH_ENABLE |
		  LTM_BRIT_ENH_ENABLE | LTM_FORCE_DMA_DISABLE,
		  LTM_FORCE_DMA_DISABLE);
	vi_clear(vi, LTM_REG_H64, LTM_EE_ENABLE);

	/* YUV top: 4:2:0 to memory, not to the scaler */
	vi_write(vi, YUVTOP_YUV_3, 0);
	vi_write(vi, YUVTOP_IMGW_M1, size);
	vi_clear(vi, YUVTOP_YUV_CTRL, YUV_CTRL_BYPASS_V);

	vi_clear(vi, TNR_REG_4, TNR_422_444 | TNR_SWAP);
	vi_update(vi, TNR_REG_8, TNR_FORCE_DMA_DISABLE, TNR_FORCE_DMA_DISABLE);
	vi_clear(vi, TNR_REG_5, TNR_TDNR_ENABLE | TNR_FORCE_MONO_ENABLE);
	vi_clear(vi, FBCE_REG_00, FBCE_EN);
	vi_clear(vi, FBCD_REG_00, FBCD_EN);
	vi_clear(vi, YUV_DITHER_Y, YUV_DITHER_ENABLE);
	vi_clear(vi, YUV_DITHER_UV, YUV_DITHER_ENABLE);
	vi_clear(vi, CA_REG_00, CACP_ENABLE);
	vi_clear(vi, CA_LITE_REG_00, CA_LITE_ENABLE);
	vi_write(vi, YNR_OUT_SEL, YNR_OUT_Y_DELAY);
	vi_clear(vi, CNR_ENABLE_REG, CNR_ENABLE | CNR_PFC_ENABLE);
	vi_clear(vi, EE_REG_00(ISP_PRE_EE), EE_ENABLE);
	vi_clear(vi, EE_REG_00(ISP_EE), EE_ENABLE);
	vi_clear(vi, YCUR_CTRL, YCUR_ENABLE);
	vi_clear(vi, DCI_ENABLE_REG, DCI_ENABLE | DCI_HIST_ENABLE);
	vi_clear(vi, STS_DMI_ENABLE(ISP_DCI), DMI_ENABLE);
	vi_clear(vi, LDCI_ENABLE_REG, LDCI_ENABLE);
	vi_clear(vi, LDCI_DMI_ENABLE, LDCI_DMI_ENABLES);

	/*
	 * Output DMA. The vendor keeps it force-disabled until the first
	 * trigger in this mode.
	 */
	vi_update(vi, CROP_REG_0(ISP_CROP4), CROP_ENABLE | CROP_DMA_ENABLE,
		  CROP_DMA_ENABLE);
	vi_update(vi, CROP_REG_0(ISP_CROP5), CROP_ENABLE | CROP_DMA_ENABLE,
		  CROP_DMA_ENABLE);
	vi_set(vi, CROP_DEBUG(ISP_CROP4), CROP_FORCE_DMA_DISABLE);
	vi_set(vi, CROP_DEBUG(ISP_CROP5), CROP_FORCE_DMA_DISABLE);
	cv1800_isp_dma_setup(vi, ISP_DMA_CTL46, ALIGN(w, 16), isp->pix.bytesperline, h);
	cv1800_isp_dma_setup(vi, ISP_DMA_CTL47, ALIGN(w, 16), isp->pix.bytesperline,
			     h / 2);

	/* No block uses the previous frame */
	vi_update(vi, TOP_FIRST_FRAME, FIRST_FRAME_SW | FIRST_FRAME_TOP,
		  FIELD_PREP(FIRST_FRAME_SW, 0xf) | FIRST_FRAME_TOP);

	cv1800_isp_apply_params(vi);
	isp->params_dirty = false;

	vi_set(vi, TOP_INT_EVENT0_EN, EV0_FRAME_DONE_POST);
}

static bool cv1800_isp_frame_done(struct cv1800_vi *vi)
{
	unsigned long flags;
	bool done;

	spin_lock_irqsave(&vi->qlock, flags);
	done = !vi->isp.busy;
	spin_unlock_irqrestore(&vi->qlock, flags);

	return done;
}

/*
 * Called at stream stop, after the front end went idle. The ISP must not
 * read or write the buffers that the stop gives back. The interrupt is
 * still enabled: the POST frame-done interrupt ends the frame in flight.
 * TOP_BLK_IDLE cannot show it: its BE bit also covers the second BE
 * channel, which goes busy at the first trigger and stays busy.
 */
void cv1800_isp_hw_stop(struct cv1800_vi *vi)
{
	bool done;

	if (!read_poll_timeout(cv1800_isp_frame_done, done, done, 1000, 100000,
			       false, vi))
		return;

	dev_warn(vi->dev, "ISP frame did not finish, resetting the ISP\n");
	vi_write(vi, TOP_SW_RST, SW_RST_AXI);
	vi_write(vi, TOP_SW_RST, SW_RST_ALL);
	vi_write(vi, TOP_SW_RST, SW_RST_AXI);
	vi_write(vi, TOP_SW_RST, 0);
}

/* Called with qlock held: run the back end and the post-raw stages once */
static void cv1800_isp_trigger(struct cv1800_vi *vi, dma_addr_t raw, dma_addr_t out)
{
	struct cv1800_isp *isp = &vi->isp;

	if (isp->params_dirty) {
		cv1800_isp_apply_params(vi);
		isp->params_dirty = false;
	}

	cv1800_isp_dma_addr(vi, ISP_DMA_CTL4, raw);
	cv1800_isp_dma_addr(vi, ISP_DMA_CTL46, out);
	cv1800_isp_dma_addr(vi, ISP_DMA_CTL47,
			    out + isp->pix.bytesperline * isp->pix.height);
	vi_clear(vi, CROP_DEBUG(ISP_CROP4), CROP_FORCE_DMA_DISABLE);
	vi_clear(vi, CROP_DEBUG(ISP_CROP5), CROP_FORCE_DMA_DISABLE);
	vi_update(vi, TOP_FIRST_FRAME, FIRST_FRAME_SW | FIRST_FRAME_TOP,
		  FIELD_PREP(FIRST_FRAME_SW, 0xf) | FIRST_FRAME_TOP);

	vi_write(vi, TOP_SW_CTRL_1, FIELD_PREP(SW1_PQ_UP_BE, 1) | SW1_PQ_UP_RAW |
		 SW1_PQ_UP_POST);
	vi_write(vi, TOP_SW_CTRL_0, FIELD_PREP(SW0_TRIG_STR_BE, 1) |
		 FIELD_PREP(SW0_SHAW_UP_BE, 1) | SW0_TRIG_STR_RAW |
		 SW0_SHAW_UP_RAW | SW0_TRIG_STR_POST | SW0_SHAW_UP_POST);
}

/* Called with qlock held */
static void cv1800_isp_drop(struct cv1800_vi *vi, struct cv1800_vi_frame *raw)
{
	vi->isp.dropped++;
	list_add_tail(&raw->list, &vi->isp.raw_free);
}

/* Called with qlock held. False when no output buffer is queued. */
static bool cv1800_isp_start(struct cv1800_vi *vi, struct cv1800_vi_frame *raw)
{
	struct cv1800_isp *isp = &vi->isp;
	struct cv1800_isp_buffer *buf;

	buf = list_first_entry_or_null(&isp->out, struct cv1800_isp_buffer, list);
	if (!buf)
		return false;
	list_del(&buf->list);

	isp->out_active = buf;
	isp->busy = raw;
	isp->busy_frames = 0;
	isp->triggers++;
	cv1800_isp_trigger(vi, raw->addr, buf->addr);

	return true;
}

/* Called with qlock held, for each raw frame the front end completes */
void cv1800_isp_raw_done(struct cv1800_vi *vi, struct cv1800_vi_frame *raw)
{
	struct cv1800_isp *isp = &vi->isp;

	if (isp->busy && ++isp->busy_frames > CV1800_ISP_TIMEOUT_FRAMES) {
		isp->timeouts++;
		dev_err_ratelimited(vi->dev, "ISP stalled: idle %08x, events seen %08x\n",
				    vi_read(vi, TOP_BLK_IDLE), isp->ev0_seen);
		cv1800_isp_drop(vi, raw);
		schedule_work(&vi->recover);
		return;
	}

	if (raw->error || !vi->running) {
		cv1800_isp_drop(vi, raw);
		return;
	}

	if (!isp->busy) {
		if (!cv1800_isp_start(vi, raw))
			cv1800_isp_drop(vi, raw);
		return;
	}

	/* Keep only the newest frame waiting */
	if (isp->ready)
		cv1800_isp_drop(vi, isp->ready);
	isp->ready = raw;
}

/* Called with qlock held: give everything in flight back, for a restart */
void cv1800_isp_requeue(struct cv1800_vi *vi)
{
	struct cv1800_isp *isp = &vi->isp;

	if (isp->busy)
		list_add_tail(&isp->busy->list, &isp->raw_free);
	if (isp->ready)
		list_add_tail(&isp->ready->list, &isp->raw_free);
	if (isp->out_active)
		list_add(&isp->out_active->list, &isp->out);
	isp->busy = NULL;
	isp->ready = NULL;
	isp->out_active = NULL;
	isp->busy_frames = 0;
}

void cv1800_isp_irq(struct cv1800_vi *vi, u32 ev0, u32 ev2)
{
	struct cv1800_isp *isp = &vi->isp;
	struct cv1800_isp_buffer *buf;
	struct cv1800_vi_frame *raw, *next;
	bool error = false;

	isp->ev0_seen |= ev0 & ~EV0_FRAME_DONE_FE0_CH0;
	if (!(ev0 & EV0_FRAME_DONE_POST))
		return;

	spin_lock(&vi->qlock);
	raw = isp->busy;
	buf = isp->out_active;
	isp->busy = NULL;
	isp->out_active = NULL;
	if (raw) {
		isp->done++;
		if (buf) {
			buf->vb.vb2_buf.timestamp = raw->timestamp;
			buf->vb.sequence = raw->sequence;
			buf->vb.field = V4L2_FIELD_NONE;
			error = raw->error;
		}
		list_add_tail(&raw->list, &isp->raw_free);
	}

	next = isp->ready;
	isp->ready = NULL;
	if (next && (!vi->running || !cv1800_isp_start(vi, next)))
		cv1800_isp_drop(vi, next);

	/* A raw frame came free: the front end may have had none to arm */
	cv1800_vi_arm_next(vi);
	spin_unlock(&vi->qlock);

	if (buf)
		vb2_buffer_done(&buf->vb.vb2_buf, error ? VB2_BUF_STATE_ERROR :
				VB2_BUF_STATE_DONE);
}

/* Called with vi->lock held, at stream start, before the hardware setup */
int cv1800_isp_prepare(struct cv1800_vi *vi)
{
	struct cv1800_isp *isp = &vi->isp;
	unsigned long flags;
	unsigned int i;

	if (vi->width > CV1800_ISP_MAX_WIDTH ||
	    vi->height > CV1800_ISP_MAX_HEIGHT) {
		dev_dbg(vi->dev, "%ux%u is too large for the ISP\n",
			vi->width, vi->height);
		return -EPIPE;
	}

	isp->raw_size = ALIGN(vi->raw_bpl * vi->height, SZ_4K);
	isp->raw_cookie = dma_alloc_attrs(vi->dev,
					  isp->raw_size * CV1800_ISP_NUM_RAW,
					  &isp->raw_dma, GFP_KERNEL | __GFP_NOWARN,
					  DMA_ATTR_NO_KERNEL_MAPPING);
	if (!isp->raw_cookie) {
		dev_err(vi->dev, "no memory for %u raw frames of %zu bytes\n",
			CV1800_ISP_NUM_RAW, isp->raw_size);
		return -ENOMEM;
	}

	spin_lock_irqsave(&vi->qlock, flags);
	INIT_LIST_HEAD(&isp->raw_free);
	for (i = 0; i < CV1800_ISP_NUM_RAW; i++) {
		isp->raw[i].addr = isp->raw_dma + i * isp->raw_size;
		list_add_tail(&isp->raw[i].list, &isp->raw_free);
	}
	vi->fe_free = &isp->raw_free;
	isp->busy = NULL;
	isp->ready = NULL;
	isp->out_active = NULL;
	isp->busy_frames = 0;
	isp->triggers = 0;
	isp->done = 0;
	isp->dropped = 0;
	isp->timeouts = 0;
	isp->ev0_seen = 0;
	isp->params_dirty = true;
	spin_unlock_irqrestore(&vi->qlock, flags);

	return 0;
}

/* Called with vi->lock held, after the stream stopped */
void cv1800_isp_unprepare(struct cv1800_vi *vi)
{
	struct cv1800_isp *isp = &vi->isp;
	unsigned long flags;

	spin_lock_irqsave(&vi->qlock, flags);
	INIT_LIST_HEAD(&isp->raw_free);
	vi->fe_free = &vi->buffers;
	spin_unlock_irqrestore(&vi->qlock, flags);

	dma_free_attrs(vi->dev, isp->raw_size * CV1800_ISP_NUM_RAW,
		       isp->raw_cookie, isp->raw_dma, DMA_ATTR_NO_KERNEL_MAPPING);
	isp->raw_cookie = NULL;
}

/* hw: the VI is powered, its registers can be read */
void cv1800_isp_status_show(struct cv1800_vi *vi, struct seq_file *s, bool hw)
{
	struct cv1800_isp *isp = &vi->isp;

	seq_printf(s, "isp triggers %u, done %u, dropped %u, stalls %u%s\n",
		   isp->triggers, isp->done, isp->dropped, isp->timeouts,
		   isp->busy ? ", busy" : "");
	seq_printf(s, "isp events seen    ev0 %08x\n", isp->ev0_seen);
	seq_printf(s, "isp output         %s range\n",
		   isp->pix.quantization == V4L2_QUANTIZATION_FULL_RANGE ?
		   "full" : "limited");
	if (!hw)
		return;
	seq_printf(s, "isp block idle     %08x\n", vi_read(vi, TOP_BLK_IDLE));
	seq_printf(s, "csc                %08x %08x %08x %08x %08x %08x\n",
		   vi_read(vi, CSC_REG(0)), vi_read(vi, CSC_REG(1)),
		   vi_read(vi, CSC_REG(2)), vi_read(vi, CSC_REG(3)),
		   vi_read(vi, CSC_REG(4)), vi_read(vi, CSC_REG(5)));
}

/* -----------------------------------------------------------------------------
 * Controls
 */

static int cv1800_isp_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct cv1800_vi *vi = container_of(ctrl->handler, struct cv1800_vi,
					    isp.ctrls);
	struct cv1800_isp_params *p = &vi->isp.params;
	unsigned long flags;
	unsigned int i;

	spin_lock_irqsave(&vi->qlock, flags);
	switch (ctrl->id) {
	case V4L2_CID_RED_BALANCE:
		p->red_gain = ctrl->val;
		break;
	case V4L2_CID_BLUE_BALANCE:
		p->blue_gain = ctrl->val;
		break;
	case V4L2_CID_GAMMA:
		p->gamma = ctrl->val;
		break;
	case CV1800_ISP_CID_BLACK_LEVEL:
		p->black_level = ctrl->val;
		break;
	case CV1800_ISP_CID_CCM:
		for (i = 0; i < ARRAY_SIZE(p->ccm); i++)
			p->ccm[i] = ctrl->p_new.p_s32[i];
		break;
	}
	/* The next trigger writes them, while the ISP is idle */
	vi->isp.params_dirty = true;
	spin_unlock_irqrestore(&vi->qlock, flags);

	return 0;
}

static const struct v4l2_ctrl_ops cv1800_isp_ctrl_ops = {
	.s_ctrl = cv1800_isp_s_ctrl,
};

/*
 * The colour matrix: linear RGB after white balance and demosaic, rows R G B,
 * 1024 = 1.0. The default is the identity, which an element-wide default
 * cannot give.
 */
static void cv1800_isp_ccm_init(const struct v4l2_ctrl *ctrl, u32 from_idx,
				union v4l2_ctrl_ptr ptr)
{
	u32 i;

	for (i = from_idx; i < ctrl->elems; i++)
		ptr.p_s32[i] = i % 4 ? 0 : 1024;
}

static void cv1800_isp_ccm_minimum(const struct v4l2_ctrl *ctrl, u32 idx,
				   union v4l2_ctrl_ptr ptr)
{
	for (; idx < ctrl->elems; idx++)
		ptr.p_s32[idx] = ctrl->minimum;
}

static void cv1800_isp_ccm_maximum(const struct v4l2_ctrl *ctrl, u32 idx,
				   union v4l2_ctrl_ptr ptr)
{
	for (; idx < ctrl->elems; idx++)
		ptr.p_s32[idx] = ctrl->maximum;
}

static const struct v4l2_ctrl_type_ops cv1800_isp_ccm_type_ops = {
	.equal = v4l2_ctrl_type_op_equal,
	.init = cv1800_isp_ccm_init,
	.minimum = cv1800_isp_ccm_minimum,
	.maximum = cv1800_isp_ccm_maximum,
	.log = v4l2_ctrl_type_op_log,
	.validate = v4l2_ctrl_type_op_validate,
};

static const struct v4l2_ctrl_config cv1800_isp_ccm = {
	.ops = &cv1800_isp_ctrl_ops,
	.type_ops = &cv1800_isp_ccm_type_ops,
	.id = CV1800_ISP_CID_CCM,
	.name = "Colour Correction Matrix",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = -8192,
	.max = 8191,
	.step = 1,
	.def = 0,
	.dims = { 3, 3 },
};

static const struct v4l2_ctrl_config cv1800_isp_black_level = {
	.ops = &cv1800_isp_ctrl_ops,
	.id = CV1800_ISP_CID_BLACK_LEVEL,
	.name = "Black Level",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 4000,
	.step = 1,
	.def = 0,
};

/* -----------------------------------------------------------------------------
 * Video node
 */

void cv1800_isp_init_state(struct v4l2_mbus_framefmt *yuv,
			   const struct v4l2_mbus_framefmt *sink)
{
	*yuv = (struct v4l2_mbus_framefmt) {
		.width = sink->width,
		.height = sink->height,
		.code = MEDIA_BUS_FMT_YUYV8_1_5X8,
		.field = V4L2_FIELD_NONE,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.ycbcr_enc = V4L2_YCBCR_ENC_601,
		.quantization = V4L2_QUANTIZATION_LIM_RANGE,
		.xfer_func = V4L2_XFER_FUNC_SRGB,
	};
}

static void cv1800_isp_try_pix(struct v4l2_pix_format *pix)
{
	u32 min_bpl;

	if (!cv1800_isp_find_format(pix->pixelformat))
		pix->pixelformat = cv1800_isp_formats[0].fourcc;
	pix->width = clamp_t(u32, ALIGN(pix->width, 2), CV1800_VI_MIN_SIZE,
			     CV1800_ISP_MAX_WIDTH);
	pix->height = clamp_t(u32, ALIGN(pix->height, 2), CV1800_VI_MIN_SIZE,
			      CV1800_ISP_MAX_HEIGHT);
	min_bpl = ALIGN(pix->width, 16);
	pix->bytesperline = clamp_t(u32, ALIGN(pix->bytesperline, 16), min_bpl,
				    ALIGN(CV1800_ISP_MAX_WIDTH, 16));
	pix->sizeimage = pix->bytesperline * pix->height * 3 / 2;
	pix->field = V4L2_FIELD_NONE;
	pix->colorspace = V4L2_COLORSPACE_SRGB;
	pix->ycbcr_enc = V4L2_YCBCR_ENC_601;
	pix->xfer_func = V4L2_XFER_FUNC_SRGB;
	/* Limited range, unless the application asks for full range */
	if (!(pix->flags & V4L2_PIX_FMT_FLAG_SET_CSC) ||
	    pix->quantization != V4L2_QUANTIZATION_FULL_RANGE)
		pix->quantization = V4L2_QUANTIZATION_LIM_RANGE;
	pix->flags &= V4L2_PIX_FMT_FLAG_SET_CSC;
}

static int cv1800_isp_querycap(struct file *file, void *priv,
			       struct v4l2_capability *cap)
{
	strscpy(cap->driver, CV1800_VI_NAME, sizeof(cap->driver));
	strscpy(cap->card, "Sophgo CV1800 ISP", sizeof(cap->card));

	return 0;
}

static int cv1800_isp_enum_fmt(struct file *file, void *priv,
			       struct v4l2_fmtdesc *f)
{
	if (f->mbus_code && f->mbus_code != MEDIA_BUS_FMT_YUYV8_1_5X8)
		return -EINVAL;
	if (f->index >= ARRAY_SIZE(cv1800_isp_formats))
		return -EINVAL;
	f->pixelformat = cv1800_isp_formats[f->index].fourcc;
	f->flags = V4L2_FMT_FLAG_CSC_QUANTIZATION;

	return 0;
}

static int cv1800_isp_enum_framesizes(struct file *file, void *priv,
				      struct v4l2_frmsizeenum *fsize)
{
	if (fsize->index || !cv1800_isp_find_format(fsize->pixel_format))
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
	fsize->stepwise.min_width = CV1800_VI_MIN_SIZE;
	fsize->stepwise.max_width = CV1800_ISP_MAX_WIDTH;
	fsize->stepwise.step_width = 2;
	fsize->stepwise.min_height = CV1800_VI_MIN_SIZE;
	fsize->stepwise.max_height = CV1800_ISP_MAX_HEIGHT;
	fsize->stepwise.step_height = 2;

	return 0;
}

static int cv1800_isp_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct cv1800_vi *vi = video_drvdata(file);

	f->fmt.pix = vi->isp.pix;

	return 0;
}

static int cv1800_isp_try_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	cv1800_isp_try_pix(&f->fmt.pix);

	return 0;
}

static int cv1800_isp_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct cv1800_vi *vi = video_drvdata(file);

	if (vb2_is_busy(&vi->isp.queue))
		return -EBUSY;

	cv1800_isp_try_pix(&f->fmt.pix);
	vi->isp.pix = f->fmt.pix;

	return 0;
}

static const struct v4l2_ioctl_ops cv1800_isp_ioctl_ops = {
	.vidioc_querycap = cv1800_isp_querycap,
	.vidioc_enum_fmt_vid_cap = cv1800_isp_enum_fmt,
	.vidioc_enum_framesizes = cv1800_isp_enum_framesizes,
	.vidioc_g_fmt_vid_cap = cv1800_isp_g_fmt,
	.vidioc_s_fmt_vid_cap = cv1800_isp_s_fmt,
	.vidioc_try_fmt_vid_cap = cv1800_isp_try_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static const struct v4l2_file_operations cv1800_isp_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.unlocked_ioctl = video_ioctl2,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
	.read = vb2_fop_read,
};

static int cv1800_isp_link_validate(struct media_link *link)
{
	struct video_device *vdev = media_entity_to_video_device(link->sink->entity);
	struct cv1800_vi *vi = video_get_drvdata(vdev);
	struct v4l2_pix_format *pix = &vi->isp.pix;
	struct v4l2_subdev_state *state;
	struct v4l2_mbus_framefmt *src;
	int ret = 0;

	/* The pipeline holds both capture nodes; check only the one starting */
	if (vi->mode != CV1800_VI_MODE_YUV)
		return 0;

	state = v4l2_subdev_lock_and_get_active_state(&vi->sd);
	src = v4l2_subdev_state_get_format(state, CV1800_VI_PAD_YUV);
	if (src->width != pix->width || src->height != pix->height) {
		dev_dbg(vi->dev, "format mismatch: %ux%u vs %ux%u\n",
			src->width, src->height, pix->width, pix->height);
		ret = -EPIPE;
	}
	v4l2_subdev_unlock_state(state);

	return ret;
}

static const struct media_entity_operations cv1800_isp_entity_ops = {
	.link_validate = cv1800_isp_link_validate,
};

/* -----------------------------------------------------------------------------
 * Buffer queue
 */

static int cv1800_isp_queue_setup(struct vb2_queue *q, unsigned int *nbuffers,
				  unsigned int *nplanes, unsigned int sizes[],
				  struct device *alloc_devs[])
{
	struct cv1800_vi *vi = vb2_get_drv_priv(q);

	if (*nplanes)
		return sizes[0] < vi->isp.pix.sizeimage ? -EINVAL : 0;

	*nplanes = 1;
	sizes[0] = vi->isp.pix.sizeimage;

	return 0;
}

static int cv1800_isp_buf_prepare(struct vb2_buffer *vb)
{
	struct cv1800_vi *vi = vb2_get_drv_priv(vb->vb2_queue);

	if (vb2_plane_size(vb, 0) < vi->isp.pix.sizeimage)
		return -EINVAL;
	vb2_set_plane_payload(vb, 0, vi->isp.pix.sizeimage);

	return 0;
}

static void cv1800_isp_buf_queue(struct vb2_buffer *vb)
{
	struct cv1800_vi *vi = vb2_get_drv_priv(vb->vb2_queue);
	struct cv1800_isp_buffer *buf = to_isp_buffer(to_vb2_v4l2_buffer(vb));
	struct cv1800_isp *isp = &vi->isp;
	struct cv1800_vi_frame *raw;
	unsigned long flags;

	buf->addr = vb2_dma_contig_plane_dma_addr(vb, 0);
	spin_lock_irqsave(&vi->qlock, flags);
	list_add_tail(&buf->list, &isp->out);
	/*
	 * A raw frame may be waiting for an output buffer. Not while the VI
	 * resets: the recovery work sets skip.
	 */
	if (vi->yuv && vi->running && !vi->skip && !isp->busy && isp->ready) {
		raw = isp->ready;
		isp->ready = NULL;
		cv1800_isp_start(vi, raw);
	}
	spin_unlock_irqrestore(&vi->qlock, flags);
}

static void cv1800_isp_return_buffers(struct cv1800_vi *vi,
				      enum vb2_buffer_state state)
{
	struct cv1800_isp_buffer *buf, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&vi->qlock, flags);
	list_for_each_entry_safe(buf, tmp, &vi->isp.out, list) {
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb.vb2_buf, state);
	}
	spin_unlock_irqrestore(&vi->qlock, flags);
}

static int cv1800_isp_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct cv1800_vi *vi = vb2_get_drv_priv(q);
	int ret;

	ret = cv1800_vi_stream_start(vi, CV1800_VI_MODE_YUV);
	if (ret)
		cv1800_isp_return_buffers(vi, VB2_BUF_STATE_QUEUED);

	return ret;
}

static void cv1800_isp_stop_streaming(struct vb2_queue *q)
{
	struct cv1800_vi *vi = vb2_get_drv_priv(q);

	cv1800_vi_stream_stop(vi);
	cv1800_isp_return_buffers(vi, VB2_BUF_STATE_ERROR);
}

static const struct vb2_ops cv1800_isp_qops = {
	.queue_setup = cv1800_isp_queue_setup,
	.buf_prepare = cv1800_isp_buf_prepare,
	.buf_queue = cv1800_isp_buf_queue,
	.start_streaming = cv1800_isp_start_streaming,
	.stop_streaming = cv1800_isp_stop_streaming,
};

/* -----------------------------------------------------------------------------
 * Probe
 */

static int cv1800_isp_init_ctrls(struct cv1800_vi *vi)
{
	struct cv1800_isp *isp = &vi->isp;
	struct v4l2_ctrl_handler *hdl = &isp->ctrls;

	v4l2_ctrl_handler_init(hdl, 5);
	isp->red_gain = v4l2_ctrl_new_std(hdl, &cv1800_isp_ctrl_ops,
					  V4L2_CID_RED_BALANCE, 0, 16383, 1, 1024);
	isp->blue_gain = v4l2_ctrl_new_std(hdl, &cv1800_isp_ctrl_ops,
					   V4L2_CID_BLUE_BALANCE, 0, 16383, 1, 1024);
	/* 0: linear output, 1: sRGB curve */
	isp->gamma = v4l2_ctrl_new_std(hdl, &cv1800_isp_ctrl_ops,
				       V4L2_CID_GAMMA, 0, 1, 1, 1);
	isp->black_level = v4l2_ctrl_new_custom(hdl, &cv1800_isp_black_level, NULL);
	isp->ccm = v4l2_ctrl_new_custom(hdl, &cv1800_isp_ccm, NULL);
	if (hdl->error)
		return hdl->error;

	/* Fills isp->params */
	return v4l2_ctrl_handler_setup(hdl);
}

int cv1800_isp_init(struct cv1800_vi *vi)
{
	struct cv1800_isp *isp = &vi->isp;
	struct video_device *vdev = &isp->vdev;
	struct vb2_queue *q = &isp->queue;
	int ret;

	isp->pix.pixelformat = cv1800_isp_formats[0].fourcc;
	isp->pix.width = CV1800_VI_DEF_WIDTH;
	isp->pix.height = CV1800_VI_DEF_HEIGHT;
	cv1800_isp_try_pix(&isp->pix);

	ret = cv1800_isp_init_ctrls(vi);
	if (ret)
		goto err_ctrls;

	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_DMABUF | VB2_READ;
	q->drv_priv = vi;
	q->buf_struct_size = sizeof(struct cv1800_isp_buffer);
	q->ops = &cv1800_isp_qops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->gfp_flags = __GFP_NOWARN;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC |
			     V4L2_BUF_FLAG_TSTAMP_SRC_SOE;
	q->lock = &vi->lock;
	q->min_queued_buffers = 1;
	q->dev = vi->dev;
	q->allow_cache_hints = 1;
	ret = vb2_queue_init(q);
	if (ret)
		goto err_ctrls;

	strscpy(vdev->name, "cv1800-vi-yuv", sizeof(vdev->name));
	vdev->release = video_device_release_empty;
	vdev->fops = &cv1800_isp_fops;
	vdev->ioctl_ops = &cv1800_isp_ioctl_ops;
	vdev->v4l2_dev = &vi->v4l2_dev;
	vdev->queue = q;
	vdev->lock = &vi->lock;
	vdev->ctrl_handler = &isp->ctrls;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING |
			    V4L2_CAP_READWRITE | V4L2_CAP_IO_MC;
	vdev->vfl_dir = VFL_DIR_RX;
	vdev->entity.ops = &cv1800_isp_entity_ops;
	video_set_drvdata(vdev, vi);

	isp->vdev_pad.flags = MEDIA_PAD_FL_SINK | MEDIA_PAD_FL_MUST_CONNECT;
	ret = media_entity_pads_init(&vdev->entity, 1, &isp->vdev_pad);
	if (ret)
		goto err_ctrls;

	ret = video_register_device(vdev, VFL_TYPE_VIDEO, -1);
	if (ret)
		goto err_entity;

	ret = media_create_pad_link(&vi->sd.entity, CV1800_VI_PAD_YUV,
				    &vdev->entity, 0,
				    MEDIA_LNK_FL_ENABLED | MEDIA_LNK_FL_IMMUTABLE);
	if (ret)
		goto err_video;

	return 0;

err_video:
	vb2_video_unregister_device(vdev);
err_entity:
	media_entity_cleanup(&vdev->entity);
err_ctrls:
	v4l2_ctrl_handler_free(&isp->ctrls);
	return ret;
}

void cv1800_isp_cleanup(struct cv1800_vi *vi)
{
	vb2_video_unregister_device(&vi->isp.vdev);
	media_entity_cleanup(&vi->isp.vdev.entity);
	v4l2_ctrl_handler_free(&vi->isp.ctrls);
}
