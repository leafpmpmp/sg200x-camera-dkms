/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Sophgo CV1800/SG2000 ISP registers used by the YUV path
 *
 * Offsets are from the start of the VI register window. The names follow
 * the vendor's register description in its GPL kernel driver.
 *
 * Copyright (C) 2026 Lukasz Sobala <lsobala1@gmail.com>
 */

#ifndef _CV1800_VI_ISP_REGS_H_
#define _CV1800_VI_ISP_REGS_H_

#include <linux/bits.h>

/* ISP top, beyond what the front end uses */
#define   EV0_FRAME_DONE_BE		GENMASK(9, 8)
#define   EV0_FRAME_DONE_POST		BIT(13)
#define   SW0_TRIG_STR_BE		GENMASK(9, 8)
#define   SW0_TRIG_STR_RAW		BIT(10)
#define   SW0_TRIG_STR_POST		BIT(11)
#define   SW0_SHAW_UP_BE		GENMASK(25, 24)
#define   SW0_SHAW_UP_RAW		BIT(26)
#define   SW0_SHAW_UP_POST		BIT(27)
#define   SW1_PQ_UP_BE			GENMASK(9, 8)
#define   SW1_PQ_UP_RAW			BIT(10)
#define   SW1_PQ_UP_POST		BIT(11)
#define   SCN_PRE2BE_L_ENABLE		BIT(2)
#define   SCN_PRE2BE_S_ENABLE		BIT(3)
#define   SCN_PRE2YUV_422_ENABLE	BIT(4)
#define   SCN_BE2RAW_L_ENABLE		BIT(5)
#define   SCN_BE2RAW_S_ENABLE		BIT(6)
#define   SCN_BE_RDMA_L_ENABLE		BIT(7)
#define   SCN_BE_RDMA_S_ENABLE		BIT(8)
#define   SCN_BE_WDMA_L_ENABLE		BIT(9)
#define   SCN_BE_WDMA_S_ENABLE		BIT(10)
#define   SCN_BE_SRC_SEL		GENMASK(12, 11)
#define   SCN_AF_RAW0YUV1		BIT(13)
#define   SCN_RGBMP_ONLINE_L_ENABLE	BIT(14)
#define   SCN_RGBMP_ONLINE_S_ENABLE	BIT(15)
#define   SCN_RAW2YUV_422_ENABLE	BIT(16)
#define   SCN_HDR_ENABLE		BIT(17)
#define   SCN_HW_AUTO_ENABLE		BIT(18)
#define   SCN_DCI_RGB0YUV1		BIT(21)
#define   BLK_IDLE_BE_BLK		BIT(3)
#define   BLK_IDLE_RAW_BLK		BIT(4)
#define   BLK_IDLE_RGB_BLK		BIT(5)
#define   BLK_IDLE_YUV_BLK		BIT(6)
#define   BLK_IDLE_RDMA			BIT(7)
#define TOP_FIRST_FRAME			(VI_ISPTOP + 0xa0)
#define   FIRST_FRAME_SW		GENMASK(4, 0)
#define   FIRST_FRAME_TOP		BIT(5)

/* A DMA channel: the same layout as DMA_CTL6 of the front end */
#define DMA_CTL_SYS_CONTROL(b)		((b) + 0x00)
#define DMA_CTL_BASE_ADDR(b)		((b) + 0x04)
#define DMA_CTL_SEGLEN(b)		((b) + 0x08)
#define DMA_CTL_STRIDE(b)		((b) + 0x0c)
#define DMA_CTL_SEGNUM(b)		((b) + 0x10)
#define   DMA_CTL_SW_MODE		(DMA_BASE_SEL | DMA_STRIDE_SEL | \
					 DMA_SEGLEN_SEL | DMA_SEGNUM_SEL)

/* Block offsets */
#define ISP_PRE_RAW_BE			0x18000
#define ISP_BLC5			0x19800	/* back end, long exposure */
#define ISP_BLC6			0x1a000	/* back end, short exposure */
#define ISP_AF				0x1a800
#define ISP_DPC0			0x1b000
#define ISP_DPC1			0x1b100
#define ISP_PRE_WDMA			0x1b900
#define ISP_RAWTOP			0x30000
#define ISP_CFA				0x31000
#define ISP_LSC				0x32000
#define ISP_GMS				0x33000
#define ISP_AEHIST0			0x34000
#define ISP_AEHIST1			0x35000
#define ISP_RAW_RDMA			0x36100
#define ISP_BNR				0x3c000
#define ISP_LMAP0			0x3f000
#define ISP_LMAP1			0x3f200
#define ISP_WBG0			0x40000	/* raw top, long exposure */
#define ISP_WBG1			0x41000	/* raw top, short exposure */
#define ISP_LCAC			0x44000
#define ISP_RGBCAC			0x45000
#define ISP_RGBTOP			0x50000
#define ISP_CCM0			0x52000
#define ISP_CCM1			0x52100
#define ISP_RGBGAMMA			0x52200
#define ISP_YGAMMA			0x52300
#define ISP_MMAP			0x53000
#define ISP_CLUT			0x54000
#define ISP_DHZ				0x55000
#define ISP_CSC				0x56000
#define ISP_RGBDITHER			0x57000
#define ISP_HIST_V			0x5c000
#define ISP_HDRFUSION			0x5d000
#define ISP_HDRLTM			0x5e000
#define ISP_YUVTOP			0x60000
#define ISP_TNR				0x61000
#define ISP_FBCE			0x61a00
#define ISP_FBCD			0x61d00
#define ISP_YUVDITHER			0x61e00
#define ISP_CA				0x62000
#define ISP_CA_LITE			0x63000
#define ISP_YNR				0x64000
#define ISP_CNR				0x65000
#define ISP_EE				0x66000
#define ISP_YCURVE			0x67000
#define ISP_DCI				0x68000
#define ISP_CROP4			0x69000	/* Y output */
#define ISP_DMA_CTL46			0x69100
#define ISP_CROP5			0x6a000	/* UV output */
#define ISP_DMA_CTL47			0x6a100
#define ISP_LDCI			0x6b000
#define ISP_PRE_EE			0x6c000
#define ISP_PRE_RAW_VI_SEL		0x7f400
#define ISP_DMA_CTL4			0x7f500	/* back end raw read */

/* Pre-raw VI select: the back end input from memory */
#define VI_SEL_0			(ISP_PRE_RAW_VI_SEL + 0x00)
#define   VI_SEL_DMA_LD_DPCM_MODE	GENMASK(4, 2)
#define   VI_SEL_DPCM_RX_XSTR		GENMASK(28, 16)
#define VI_SEL_1			(ISP_PRE_RAW_VI_SEL + 0x04)

/* Pre-raw back end */
#define BE_TOP_CTRL			(ISP_PRE_RAW_BE + 0x00)
#define   BE_BAYER_TYPE_LE		GENMASK(3, 0)
#define   BE_BAYER_TYPE_SE		GENMASK(7, 4)
#define   BE_CH_NUM			BIT(9)
#define BE_UP_PQ_EN			(ISP_PRE_RAW_BE + 0x04)
#define BE_IMG_SIZE_LE			(ISP_PRE_RAW_BE + 0x08)
#define BE_DEBUG_ENABLE			(ISP_PRE_RAW_BE + 0x24)

#define PRE_WDMA_CTRL			(ISP_PRE_WDMA + 0x00)
#define   PRE_WDMA_WDMI_EN_LE		BIT(0)
#define   PRE_WDMA_WDMI_EN_SE		BIT(1)
#define PRE_WDMA_RDMI_DPCM		(ISP_PRE_WDMA + 0x04)
#define   PRE_WDMA_DPCM_MODE		GENMASK(2, 0)
#define   PRE_WDMA_DPCM_XSTR		GENMASK(28, 16)

/* Black level: out = (in - offset) * gain, gain 1024 = 1.0 */
#define BLC_0(b)			((b) + 0x00)
#define   BLC_BYPASS			BIT(0)
#define BLC_2(b)			((b) + 0x08)
#define   BLC_ENABLE			BIT(0)
#define BLC_3(b)			((b) + 0x0c)	/* R, Gr offsets */
#define BLC_4(b)			((b) + 0x10)	/* Gb, B offsets */
#define BLC_5(b)			((b) + 0x14)	/* R, Gr gains */
#define BLC_6(b)			((b) + 0x18)	/* Gb, B gains */
#define BLC_9(b)			((b) + 0x34)	/* R, Gr offsets after gain */
#define BLC_A(b)			((b) + 0x38)	/* Gb, B offsets after gain */
#define   BLC_LO12			GENMASK(11, 0)
#define   BLC_HI12			GENMASK(27, 16)
#define   BLC_LO16			GENMASK(15, 0)
#define   BLC_HI16			GENMASK(31, 16)

#define DPC_2(b)			((b) + 0x08)
#define   DPC_ENABLES			GENMASK(3, 0)

#define AF_BYPASS			(ISP_AF + 0x0c)
#define AF_KICKOFF			(ISP_AF + 0x10)
#define   AF_ENABLE			BIT(0)
#define AF_DMI_ENABLE			(ISP_AF + 0xa0)

/* Most statistics blocks: DMA enable at 0xa0 */
#define STS_DMI_ENABLE(b)		((b) + 0xa0)
#define   DMI_ENABLE			BIT(0)
#define   DMI_FORCE_DMA_DISABLE		BIT(8)

/* Raw top */
#define RAWTOP_CTRL			(ISP_RAWTOP + 0x10)
#define RAWTOP_RAW_2			(ISP_RAWTOP + 0x18)
#define RAWTOP_RAW_4			(ISP_RAWTOP + 0x20)
#define RAWTOP_BAYER_TYPE_TOPLEFT	(ISP_RAWTOP + 0x30)
#define   RAWTOP_BAYER_TYPE		GENMASK(3, 0)
#define RAWTOP_RDMI_ENABLE		(ISP_RAWTOP + 0x34)
#define   RAWTOP_RDMI_EN		BIT(0)
#define   RAWTOP_CH_NUM			BIT(16)
#define RAWTOP_RDMA_SIZE		(ISP_RAWTOP + 0x38)
#define RAWTOP_DPCM_MODE		(ISP_RAWTOP + 0x3c)
#define RAWTOP_PATGEN1			(ISP_RAWTOP + 0x48)
#define   RAWTOP_PG_ENABLE		BIT(31)

#define RAW_RDMA_CONFIG			(ISP_RAW_RDMA + 0x04)
#define RAW_RDMA_SIZE			(ISP_RAW_RDMA + 0x08)
#define RAW_RDMA_DPCM_MODE		(ISP_RAW_RDMA + 0x0c)

#define BNR_OUT_SEL			(ISP_BNR + 0x04)
#define   BNR_OUT_B_DELAY		1

#define LSC_ENABLE_REG			(ISP_LSC + 0x0c)
#define   LSC_ENABLE			BIT(0)
#define LSC_BLD				(ISP_LSC + 0xa4)
#define   LSC_BLDRATIO_ENABLE		BIT(0)

#define CFA_REG_00			(ISP_CFA + 0x00)
#define   CFA_SHDW_SEL			BIT(0)
#define   CFA_ENABLE			BIT(1)

#define RGBCAC_CTRL			(ISP_RGBCAC + 0x00)
#define   RGBCAC_ENABLE			BIT(0)
#define LCAC_REG00			(ISP_LCAC + 0x00)
#define   LCAC_ENABLE			BIT(0)

#define GMS_ENABLE_REG			(ISP_GMS + 0x0c)
#define   GMS_ENABLE			BIT(0)

/* White balance gains, 1024 = 1.0, 14 bits */
#define WBG_0(b)			((b) + 0x00)
#define   WBG_BYPASS			BIT(0)
#define WBG_2(b)			((b) + 0x08)
#define   WBG_ENABLE			BIT(0)
#define WBG_4(b)			((b) + 0x10)
#define   WBG_RGAIN			GENMASK(13, 0)
#define   WBG_GGAIN			GENMASK(29, 16)
#define WBG_5(b)			((b) + 0x14)
#define   WBG_BGAIN			GENMASK(13, 0)
#define WBG_34(b)			((b) + 0x34)	/* gain fractions */
#define WBG_38(b)			((b) + 0x38)
#define WBG_3C(b)			((b) + 0x3c)

#define LMAP_LMP_0(b)			((b) + 0x00)
#define   LMAP_ENABLE			BIT(0)
#define   LMAP_FORCE_DMA_DISABLE	BIT(31)

#define AEHIST_ENABLE(b)		((b) + 0x14)

/* RGB top */
#define RGBTOP_REG_0			(ISP_RGBTOP + 0x00)
#define   RGBTOP_BAYER_TYPE		GENMASK(3, 0)
#define RGBTOP_REG_9			(ISP_RGBTOP + 0x24)
#define RGBTOP_DBG_IP_S_VLD		(ISP_RGBTOP + 0x50)
#define   RGBTOP_IP_DBG_EN		BIT(31)

/* Colour matrix, 1024 = 1.0, 14-bit two's complement */
#define CCM_COEF(b, i)			((b) + 4 * (i))	/* 00 01 02 10 ... 22 */
#define CCM_CTRL(b)			((b) + 0x24)
#define   CCM_SHDW_SEL			BIT(0)
#define   CCM_ENABLE			BIT(2)

#define GAMMA_CTRL			(ISP_RGBGAMMA + 0x00)
#define   GAMMA_ENABLE			BIT(0)
#define GAMMA_PROG_CTRL			(ISP_RGBGAMMA + 0x04)
#define   GAMMA_WSEL			BIT(0)
#define   GAMMA_RSEL			BIT(4)
#define   GAMMA_PROG_EN			BIT(8)
#define   GAMMA_PROG_1TO3_EN		BIT(12)
#define GAMMA_PROG_ST_ADDR		(ISP_RGBGAMMA + 0x08)
#define   GAMMA_ST_ADDR			GENMASK(7, 0)
#define   GAMMA_ST_W			BIT(31)
#define GAMMA_PROG_DATA			(ISP_RGBGAMMA + 0x0c)
#define   GAMMA_DATA_E			GENMASK(11, 0)
#define   GAMMA_DATA_O			GENMASK(27, 16)
#define   GAMMA_W			BIT(31)
#define GAMMA_PROG_MAX			(ISP_RGBGAMMA + 0x10)

#define YGAMMA_CTRL			(ISP_YGAMMA + 0x00)
#define   YGAMMA_ENABLE			BIT(0)

#define CLUT_CTRL			(ISP_CLUT + 0x00)
#define   CLUT_ENABLE			BIT(0)

#define DHZ_BYPASS			(ISP_DHZ + 0x14)
#define   DEHAZE_ENABLE			BIT(0)

/* RGB to YUV: 3x3 coefficients and 3 offsets in REG_4..REG_9 */
#define CSC_REG_0			(ISP_CSC + 0x00)
#define   CSC_ENABLE			BIT(0)
#define CSC_REG(i)			(ISP_CSC + 0x10 + 4 * (i))
#define   CSC_LO14			GENMASK(13, 0)
#define   CSC_HI14			GENMASK(29, 16)
#define   CSC_LO11			GENMASK(10, 0)
#define   CSC_HI11			GENMASK(26, 16)

#define RGBDITHER			(ISP_RGBDITHER + 0x00)
#define   RGB_DITHER_SHDW_SEL		BIT(4)
#define   RGB_DITHER_WIDTHM1		GENMASK(19, 8)
#define   RGB_DITHER_HEIGHTM1		GENMASK(31, 20)

#define HIST_V_IP_CONFIG		(ISP_HIST_V + 0x0c)
#define   HIST_EDGE_V_ENABLE		BIT(0)
#define HIST_V_DMI_ENABLE		(ISP_HIST_V + 0x28)

/* Motion map of the 3D noise reduction */
#define MMAP_REG_00			(ISP_MMAP + 0x00)
#define   MMAP_0_ENABLE			BIT(0)
#define   MMAP_1_ENABLE			BIT(1)
#define   MMAP_BYPASS			BIT(24)
#define MMAP_REG_6C			(ISP_MMAP + 0x6c)
#define   MMAP_FORCE_DMA_DISABLE	GENMASK(15, 8)
#define MMAP_REG_D0			(ISP_MMAP + 0xd0)
#define   MMAP_CROP_ENABLE_SCALAR	BIT(31)

#define FUSION_FS_CTRL_0		(ISP_HDRFUSION + 0x00)
#define   FS_ENABLE			BIT(0)
#define   FS_MC_ENABLE			BIT(4)
#define FUSION_FS_SE_GAIN		(ISP_HDRFUSION + 0x08)
#define   FS_OUT_SEL			GENMASK(19, 16)
#define     FS_OUT_LONG			1

#define LTM_REG_H00			(ISP_HDRLTM + 0x00)
#define   LTM_ENABLE			BIT(0)
#define   LTM_DARK_ENH_ENABLE		BIT(1)
#define   LTM_BRIT_ENH_ENABLE		BIT(2)
#define   LTM_FORCE_DMA_DISABLE		GENMASK(17, 16)
#define LTM_REG_H64			(ISP_HDRLTM + 0x64)
#define   LTM_EE_ENABLE			BIT(0)

/* YUV top */
#define YUVTOP_YUV_3			(ISP_YUVTOP + 0x0c)
#define YUVTOP_YUV_CTRL			(ISP_YUVTOP + 0x60)
#define   YUV_CTRL_BYPASS_V		BIT(4)
#define YUVTOP_IMGW_M1			(ISP_YUVTOP + 0x64)

/* RGB to 4:2:2 conversion and 3D noise reduction */
#define TNR_REG_4			(ISP_TNR + 0x10)
#define   TNR_422_444			BIT(0)
#define   TNR_SWAP			GENMASK(2, 1)
#define TNR_REG_5			(ISP_TNR + 0x14)
#define   TNR_TDNR_ENABLE		BIT(1)
#define   TNR_FORCE_MONO_ENABLE		BIT(3)
#define TNR_REG_8			(ISP_TNR + 0x20)
#define   TNR_FORCE_DMA_DISABLE		GENMASK(13, 8)

#define FBCE_REG_00			(ISP_FBCE + 0x00)
#define   FBCE_EN			BIT(0)
#define FBCD_REG_00			(ISP_FBCD + 0x00)
#define   FBCD_EN			BIT(0)

#define YUV_DITHER_Y			(ISP_YUVDITHER + 0x00)
#define YUV_DITHER_UV			(ISP_YUVDITHER + 0x04)
#define   YUV_DITHER_ENABLE		BIT(0)

#define CA_REG_00			(ISP_CA + 0x00)
#define   CACP_ENABLE			BIT(0)
#define CA_LITE_REG_00			(ISP_CA_LITE + 0x00)
#define   CA_LITE_ENABLE		BIT(0)

#define YNR_OUT_SEL			(ISP_YNR + 0x04)
#define   YNR_OUT_Y_DELAY		1

#define CNR_ENABLE_REG			(ISP_CNR + 0x00)
#define   CNR_ENABLE			BIT(0)
#define   CNR_PFC_ENABLE		BIT(4)

/* Edge enhancement: EE and PRE_EE share the layout */
#define EE_REG_00(b)			((b) + 0x00)
#define   EE_ENABLE			BIT(0)

#define YCUR_CTRL			(ISP_YCURVE + 0x00)
#define   YCUR_ENABLE			BIT(0)

#define DCI_ENABLE_REG			(ISP_DCI + 0x0c)
#define   DCI_ENABLE			BIT(0)
#define   DCI_HIST_ENABLE		BIT(4)

#define LDCI_ENABLE_REG			(ISP_LDCI + 0x00)
#define   LDCI_ENABLE			BIT(0)
#define LDCI_DMI_ENABLE			(ISP_LDCI + 0xa0)
#define   LDCI_DMI_ENABLES		GENMASK(1, 0)

/* Output crops: they also gate the output DMA */
#define CROP_REG_0(b)			((b) + 0x00)
#define   CROP_ENABLE			BIT(0)
#define   CROP_DMA_ENABLE		BIT(1)
#define CROP_DEBUG(b)			((b) + 0x14)
#define   CROP_FORCE_DMA_DISABLE	BIT(1)

#endif /* _CV1800_VI_ISP_REGS_H_ */
