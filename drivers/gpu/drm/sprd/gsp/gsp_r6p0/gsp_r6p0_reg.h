/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _GSP_R6P0_REG_H
#define _GSP_R6P0_REG_H

#include "gsp_debug.h"

#define R6P0_IMGL_NUM   2
#define R6P0_OSDL_NUM   2
#define R6P0_IMGSEC_NUM 0
#define R6P0_OSDSEC_NUM 1

/* Global config reg */
#define R6P0_GSP_GLB_CFG(base)		(base + 0x1000)
#define R6P0_GSP_INT(base)		(base + 0x004 + 0x1000)
#define R6P0_GSP_MOD_CFG(base)		(base + 0x008 + 0x1000)
#define R6P0_GSP_SECURE_CFG(base)	(base + 0x00C + 0x1000)

/* Destination reg 1 */
#define R6P0_DES_DATA_CFG(base)		   (base + 0x010 + 0x1000)
#define R6P0_DES_Y_ADDR(base)		   (base + 0x014 + 0x1000)
#define R6P0_DES_U_ADDR(base)		   (base + 0x018 + 0x1000)
#define R6P0_DES_V_ADDR(base)		   (base + 0x01C + 0x1000)
#define R6P0_DES_PITCH(base)		   (base + 0x020 + 0x1000)
#define R6P0_BACK_RGB(base)		   (base + 0x024 + 0x1000)
#define R6P0_WORK_AREA_SIZE(base)	   (base + 0x028 + 0x1000)
#define R6P0_WORK_AREA_XY(base)		   (base + 0x02C + 0x1000)

/* LAYERIMG */
#define R6P0_LIMG_CFG(base)		   (base + 0x030 + 0x1000)
#define R6P0_LIMG_Y_ADDR(base)		   (base + 0x034 + 0x1000)
#define R6P0_LIMG_U_ADDR(base)		   (base + 0x038 + 0x1000)
#define R6P0_LIMG_V_ADDR(base)		   (base + 0x03C + 0x1000)
#define R6P0_LIMG_PITCH(base)		   (base + 0x040 + 0x1000)
#define R6P0_LIMG_CLIP_START(base)	   (base + 0x044 + 0x1000)
#define R6P0_LIMG_CLIP_SIZE(base)	   (base + 0x048 + 0x1000)
#define R6P0_LIMG_DES_START(base)	   (base + 0x04C + 0x1000)
#define R6P0_LIMG_PALLET_RGB(base)	   (base + 0x050 + 0x1000)
#define R6P0_LIMG_CK(base)		   (base + 0x054 + 0x1000)
#define R6P0_Y2Y_Y_PARAM(base)		   (base + 0x058 + 0x1000)
#define R6P0_Y2Y_U_PARAM(base)		   (base + 0x05C + 0x1000)
#define R6P0_Y2Y_V_PARAM(base)		   (base + 0x060 + 0x1000)
#define R6P0_LIMG_DES_SIZE(base)	   (base + 0x064 + 0x1000)

#define R6P0_LIMG_BASE_ADDR(base)	   (base + 0x030 + 0x1000)
#define R6P0_LIMG_OFFSET		   0x040

/* LAYEROSD */
#define R6P0_LOSD_CFG(base)		   (base + 0x0B0 + 0x1000)
#define R6P0_LOSD_R_ADDR(base)		   (base + 0x0B4 + 0x1000)
#define R6P0_LOSD_PITCH(base)		   (base + 0x0B8 + 0x1000)
#define R6P0_LOSD_CLIP_START(base)	   (base + 0x0BC + 0x1000)
#define R6P0_LOSD_CLIP_SIZE(base)	   (base + 0x0C0 + 0x1000)
#define R6P0_LOSD_DES_START(base)	   (base + 0x0C4 + 0x1000)
#define R6P0_LOSD_PALLET_RGB(base)	   (base + 0x0C8 + 0x1000)
#define R6P0_LOSD_CK(base)		   (base + 0x0CC + 0x1000)

#define R6P0_LOSD_BASE_ADDR(base)	   (base + 0x0B0 + 0x1000)
#define R6P0_LOSD_OFFSET		   0x020
#define R6P0_LOSD_SEC_ADDR(base)	   (base + 0x0D0 + 0x1000)

#define R6P0_DES_DATA_CFG1(base)	   (base + 0x0F0 + 0x1000)
#define R6P0_DES_Y_ADDR1(base)		   (base + 0x0F4 + 0x1000)
#define R6P0_DES_U_ADDR1(base)		   (base + 0x0F8 + 0x1000)
#define R6P0_DES_V_ADDR1(base)		   (base + 0x0FC + 0x1000)
#define R6P0_DES_PITCH1(base)		   (base + 0x100 + 0x1000)

#define R6P0_GSP_IP_REV(base)		   (base + 0x204 + 0x1000)
#define R6P0_GSP_DEBUG_CFG(base)	   (base + 0x208 + 0x1000)
#define R6P0_GSP_DEBUG1(base)		   (base + 0x20C + 0x1000)
#define R6P0_GSP_DEBUG2(base)		   (base + 0x210 + 0x1000)

#define R6P0_SCALE_COEF_ADDR(base)	   (base + 0x300 + 0x1000)
#define R6P0_SCALE_COEF_OFFSET		   0x200

struct R6P0_GSP_GLB_CFG_REG {
	union {
		struct {
			uint32_t GSP_RUN0		:   1;
			uint32_t GSP_RUN1		:   1;
			uint32_t GSP_BUSY0		:   1;
			uint32_t GSP_BUSY1		:   1;
			uint32_t REG_BUSY		:   1;
			uint32_t Reserved1		:   3;
			uint32_t ERR_FLG		:   1;
			uint32_t ERR_CODE		:   7;
			uint32_t Reserved2		:  16;
		};
		uint32_t	value;
	};
};

struct R6P0_GSP_INT_REG {
	union {
		struct {
			uint32_t INT_GSP_RAW	   :	1;
			uint32_t INT_CORE1_RAW	   :	1;
			uint32_t INT_GERR_RAW	   :	1;
			uint32_t INT_CERR1_RAW	   :	1;
			uint32_t Reserved1	   :	4;
			uint32_t INT_GSP_EN	   :	1;
			uint32_t INT_CORE1_EN	   :	1;
			uint32_t INT_GERR_EN	   :	1;
			uint32_t INT_CERR1_EN	   :	1;
			uint32_t Reserved2	   :	4;
			uint32_t INT_GSP_CLR	   :	1;
			uint32_t INT_CORE1_CLR	   :	1;
			uint32_t INT_GERR_CLR	   :	1;
			uint32_t INT_CERR1_CLR	   :	1;
			uint32_t Reserved3	   :	4;
			uint32_t INT_GSP_STS	   :	1;
			uint32_t INT_CORE1_STS	   :	1;
			uint32_t INT_GERR_STS	   :	1;
			uint32_t INT_CERR1_STS	   :	1;
			uint32_t Reserved4	   :	4;
		};
		uint32_t	value;
	};
};

struct R6P0_GSP_MOD_CFG_REG {
	union {
		struct {
			uint32_t WORK_MOD	  :   1;
			uint32_t CORE_NUM	  :   1;
			uint32_t CO_WORK0	  :   1;
			uint32_t CO_WORK1	  :   1;
			uint32_t PMARGB_EN	  :   1;
			uint32_t ARLEN_MOD	  :   1;
			uint32_t IFBCE_AWLEN_MOD  :   2;
			uint32_t Reserved1	  :   24;
		};
		uint32_t	value;
	};
};

struct R6P0_GSP_SECURE_CFG_REG {
	union {
		struct {
			uint32_t SECURE_MOD	   :   1;
			uint32_t NONSEC_AWPROT	   :   3;
			uint32_t NONSEC_ARPROT	   :   3;
			uint32_t SECURE_AWPROT	   :   3;
			uint32_t SECURE_ARPROT	   :   3;
			uint32_t Reserved1	   :   19;
		};
		uint32_t	value;
	};
};

struct R6P0_DES_DATA_CFG_REG {
	union {
		struct {
			uint32_t Y_ENDIAN_MOD		 :	4;
			uint32_t UV_ENDIAN_MOD		 :	4;
			uint32_t Reserved1		 :	1;
			uint32_t A_SWAP_MOD		 :	1;
			uint32_t ROT_MOD		 :	3;
			uint32_t R2Y_MOD		 :	3;
			uint32_t DES_IMG_FORMAT		 :	3;
			uint32_t Reserved2		 :	1;
			uint32_t RSWAP_MOD		 :	3;
			uint32_t Reserved3		 :	3;
			uint32_t FBCE_MOD		 :	2;
			uint32_t DITHER_EN		 :	1;
			uint32_t BK_EN			 :	1;
			uint32_t BK_BLD			 :	1;
			uint32_t Reserved4		 :	1;
		};
		uint32_t	value;
	};
};

struct R6P0_DES_Y_ADDR_REG {
	union {
		struct {
			uint32_t Reserved	         :   4;
			uint32_t DES_Y_BASE_ADDR1	 :   28;
		};
		uint32_t	value;
	};
};

struct R6P0_DES_U_ADDR_REG {
	union {
		struct {
			uint32_t Reserved			:   4;
			uint32_t DES_U_BASE_ADDR1	:   28;
		};
		uint32_t	value;
	};
};

struct R6P0_DES_V_ADDR_REG {
	union {
		struct {
			uint32_t DES_V_BASE_ADDR1	:	32;
		};
		uint32_t	value;
	};
};

struct R6P0_DES_PITCH_REG {
	union {
		struct {
			uint32_t DES_PITCH	 :   13;
			uint32_t Reserved1	 :   3;
			uint32_t DES_HEIGHT	 :   13;
			uint32_t Reserved2	 :   3;
		};
		uint32_t	value;
	};
};

struct R6P0_DES_DATA_CFG1_REG {
	union {
		struct {
			uint32_t Y_ENDIAN_MOD		 :   4;
			uint32_t UV_ENDIAN_MOD		 :   4;
			uint32_t Reserved1		 :   1;
			uint32_t A_SWAP_MOD		 :   1;
			uint32_t ROT_MOD		 :   3;
			uint32_t R2Y_MOD		 :   3;
			uint32_t DES_IMG_FORMAT		 :   3;
			uint32_t Reserved2		 :   1;
			uint32_t RSWAP_MOD		 :   3;
			uint32_t Reserved3		 :   3;
			uint32_t FBCE_MOD		 :   2;
			uint32_t DITHER_EN		 :   1;
			uint32_t Reserved4		 :   3;
		};
		uint32_t	value;
	};
};

struct R6P0_DES_Y_ADDR1_REG {
	union {
		struct {
			uint32_t Reserved			:   4;
			uint32_t DES_Y_BASE_ADDR1	:   28;
		};
		uint32_t	value;
	};
};

struct R6P0_DES_U_ADDR1_REG {
	union {
		struct {
			uint32_t Reserved	       :   4;
			uint32_t DES_U_BASE_ADDR1      :   28;
		};
		uint32_t	value;
	};
};

struct R6P0_DES_V_ADDR1_REG {
	union {
		struct {
			uint32_t DES_V_BASE_ADDR1      :   32;
		};
		uint32_t	value;
	};
};

struct R6P0_DES_PITCH1_REG {
	union {
		struct {
			uint32_t DES_PITCH	      :    13;
			uint32_t Reserved1	      :    3;
			uint32_t DES_HEIGHT	      :    13;
			uint32_t Reserved2	      :    3;
		};
		uint32_t	value;
	};
};

struct R6P0_BACK_RGB_REG {
	union {
		struct {
			uint32_t   BACKGROUND_B			:	8;
			uint32_t   BACKGROUND_G			:	8;
			uint32_t   BACKGROUND_R			:	8;
			uint32_t   BACKGROUND_A			:	8;
		};
		uint32_t	   value;
	};
};

struct R6P0_WORK_AREA_SIZE_REG {
	union {
		struct {
			uint32_t WORK_AREA_W	   :   13;
			uint32_t Reserved1	   :   3;
			uint32_t WORK_AREA_H	   :   13;
			uint32_t Reserved2	   :   3;
		};
		uint32_t	value;
	};
};

struct R6P0_WORK_AREA_XY_REG {
	union {
		struct {
			uint32_t WORK_AREA_X	   :   13;
			uint32_t Reserved1	   :   3;
			uint32_t WORK_AREA_Y	   :   13;
			uint32_t Reserved2	   :   3;
		};
		uint32_t	value;
	};
};

struct R6P0_GSP_IP_REV_REG {
	union {
		struct {
			uint32_t PATCH_NUM	   :    4;
			uint32_t GSP_IP_REV	   :    12;
			uint32_t Reserved1	   :    16;
		};
		uint32_t	   value;
	};
};

struct R6P0_GSP_DEBUG_CFG_REG {
	union {
		struct {
			uint32_t SCL_CLR1	   :    1;
			uint32_t SCL_CLR2	   :	1;
			uint32_t CACHE_DIS	   :    1;
			uint32_t Reserved1	   :    29;
		};
		uint32_t	   value;
	};
};

struct R6P0_GSP_DEBUG1_REG {
	union {
		struct {
			uint32_t   Reserved1		 :   24;
			uint32_t   SCL_OUT_EMP0		 :   1;
			uint32_t   SCL_OUT_FULL0	 :   1;
			uint32_t   SCL_OUT_EMP1		 :   1;
			uint32_t   SCL_OUT_FULL1	 :   1;
			uint32_t   BLD_OUT_EMP		 :   1;
			uint32_t   BLD_OUT_FULL		 :   1;
			uint32_t   Reserved2		 :   2;
		};
		uint32_t	   value;
	};
};

struct R6P0_GSP_DEBUG2_REG {
	union {
		struct {
			uint32_t   LAYER0_DBG_STS		 :   8;
			uint32_t   LAYER1_DBG_STS		 :   8;
			uint32_t   LAYER2_DBG_STS		 :   8;
			uint32_t   LAYER3_DBG_STS		 :   8;
		};
		uint32_t	   value;
	};
};

/* LAYERIMG */
struct R6P0_LAYERIMG_CFG_REG {
	union {
		struct {
			uint32_t   Y_ENDIAN_MOD		  :   4;
			uint32_t   UV_ENDIAN_MOD	  :   4;
			uint32_t   RGB_SWAP_MOD		  :   3;
			uint32_t   A_SWAP_MOD		  :   1;
			uint32_t   PMARGB_MOD		  :   1;
			uint32_t   ROT_SRC		  :   3;
			uint32_t   IMG_FORMAT		  :   3;
			uint32_t   CK_EN		  :   1;
			uint32_t   PALLET_EN		  :   1;
			uint32_t   FBCD_MOD		  :   2;
			uint32_t   Y2R_MOD		  :   3;
			uint32_t   Y2Y_MOD		  :   1;
			uint32_t   ZNUM_L		  :   2;
			uint32_t   Reserved1		  :   1;
			uint32_t   SCALE_EN		  :   1;
			uint32_t   Limg_en		  :   1;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYERIMG_Y_ADDR_REG {
	union {
		struct {
			uint32_t   Reserved1		 :   4;
			uint32_t   Y_BASE_ADDR		 :   28;
		};
		uint32_t	   value;
	};
};


struct R6P0_LAYERIMG_U_ADDR_REG {
	union {
		struct {
			uint32_t   Reserved1		:   4;
			uint32_t   U_BASE_ADDR		:   28;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYERIMG_V_ADDR_REG {
	union {
		struct {
			uint32_t   Reserved1		:   4;
			uint32_t   V_BASE_ADDR		:   28;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYERIMG_PITCH_REG {
	union {
		struct {
			uint32_t   PITCH		 :   13;
			uint32_t   Reserved1		 :   3;
			uint32_t   HEIGHT		 :   13;
			uint32_t   Reserved2		 :   3;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYERIMG_CLIP_START_REG {
	union {
		struct {
			uint32_t   CLIP_START_X		:   13;
			uint32_t   Reserved1		:   3;
			uint32_t   CLIP_START_Y		:   13;
			uint32_t   Reserved2		:   3;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYERIMG_CLIP_SIZE_REG {
	union {
		struct {
			uint32_t   CLIP_SIZE_X				:   13;
			uint32_t   Reserved1				:   3;
			uint32_t   CLIP_SIZE_Y				:   13;
			uint32_t   Reserved2				:   3;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYERIMG_DES_START_REG {
	union {
		struct {
			uint32_t   DES_START_X				:   13;
			uint32_t   Reserved1				:   3;
			uint32_t   DES_START_Y				:   13;
			uint32_t   Reserved2				:   3;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYERIMG_PALLET_RGB_REG {
	union {
		struct {
			uint32_t   PALLET_B			:   8;
			uint32_t   PALLET_G			:   8;
			uint32_t   PALLET_R			:   8;
			uint32_t   PALLET_A			:   8;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYERIMG_CK_REG {
	union {
		struct {
			uint32_t   CK_B				:   8;
			uint32_t   CK_G				:   8;
			uint32_t   CK_R				:   8;
			uint32_t   BLOCK_ALPHA		        :   8;
		};
		uint32_t	   value;
	};
};

struct R6P0_Y2Y_Y_PARAM_REG {
	union {
		struct {
			uint32_t   Y_CONTRAST			:   10;
			uint32_t   Reserved1			:   6;
			uint32_t   Y_BRIGHTNESS			:   9;
			uint32_t   Reserved2			:   7;
		};
		uint32_t	   value;
	};
};

struct R6P0_Y2Y_U_PARAM_REG {
	union {
		struct {
			uint32_t   U_SATURATION			:    10;
			uint32_t   Reserved1			:    6;
			uint32_t   U_OFFSET			:    8;
			uint32_t   Reserved2			:    8;
		};
		uint32_t	   value;
	};
};

struct R6P0_Y2Y_V_PARAM_REG {
	union {
		struct {
			uint32_t   V_SATURATION			:   10;
			uint32_t   Reserved1			:   6;
			uint32_t   V_OFFSET			:   8;
			uint32_t   Reserved2			:   8;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYERIMG_DES_SCL_SIZE_REG {
	union {
		struct {
			uint32_t DES_SCL_W	  :   13;
			uint32_t HTAP_MOD	  :   2;
			uint32_t Reserved1	  :   1;
			uint32_t DES_SCL_H	  :   13;
			uint32_t VTAP_MOD	  :   2;
			uint32_t Reserved2	  :   1;
		};
		uint32_t	value;
	};
};


/* LAYEROSD */
struct R6P0_LAYEROSD_CFG_REG {
	union {
		struct {
			uint32_t   ENDIAN			  :   4;
			uint32_t   RGB_SWAP			  :   3;
			uint32_t   A_SWAP			  :   1;
			uint32_t   PMARGB_MOD			  :   1;
			uint32_t   Reserved1			  :   7;
			uint32_t   IMG_FORMAT			  :   2;
			uint32_t   CK_EN			  :   1;
			uint32_t   PALLET_EN			  :   1;
			uint32_t   FBCD_MOD			  :   1;
			uint32_t   ZNUM_L			  :   2;
			uint32_t   Reserved2			  :   8;
			uint32_t   Losd_en			  :   1;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYEROSD_R_ADDR_REG {
	union {
		struct {
			uint32_t   Reserved1			  :   4;
			uint32_t   R_BASE_ADDR			  :   28;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYEROSD_PITCH_REG {
	union {
		struct {
			uint32_t   PITCH			   :    13;
			uint32_t   Reserved1			   :    3;
			uint32_t   HEIGHT			   :    13;
			uint32_t   Reserved2			   :    3;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYEROSD_CLIP_START_REG {
	union {
		struct {
			uint32_t   CLIP_START_X		          :    13;
			uint32_t   Reserved1			  :    3;
			uint32_t   CLIP_START_Y			  :    13;
			uint32_t   Reserved2			  :    3;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYEROSD_CLIP_SIZE_REG {
	union {
		struct {
			uint32_t   CLIP_SIZE_X				:   13;
			uint32_t   Reserved1				:   3;
			uint32_t   CLIP_SIZE_Y				:   13;
			uint32_t   Reserved2				:   3;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYEROSD_DES_START_REG {
	union {
		struct {
			uint32_t   DES_START_X				:    13;
			uint32_t   Reserved1				:    3;
			uint32_t   DES_START_Y				:    13;
			uint32_t   Reserved2				:    3;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYEROSD_PALLET_RGB_REG {
	union {
		struct {
			uint32_t   PALLET_B				:    8;
			uint32_t   PALLET_G				:    8;
			uint32_t   PALLET_R				:    8;
			uint32_t   PALLET_A				:    8;
		};
		uint32_t	   value;
	};
};

struct R6P0_LAYEROSD_CK_REG {
	union {
		struct {
			uint32_t   CK_B				:    8;
			uint32_t   CK_G				:    8;
			uint32_t   CK_R				:    8;
			uint32_t   BLOCK_ALPHA		        :    8;
		};
		uint32_t	   value;
	};
};

struct R6P0_GSP_CTL_REG_T {
	struct R6P0_GSP_GLB_CFG_REG glb_cfg;
	struct R6P0_GSP_INT_REG int_cfg;
	struct R6P0_GSP_MOD_CFG_REG mod_cfg;
	struct R6P0_GSP_SECURE_CFG_REG secure_cfg;

	struct R6P0_DES_DATA_CFG_REG des_data_cfg;
	struct R6P0_DES_Y_ADDR_REG des_y_addr;
	struct R6P0_DES_U_ADDR_REG  des_u_addr;
	struct R6P0_DES_V_ADDR_REG  des_v_addr;
	struct R6P0_DES_PITCH_REG  des_pitch;
	struct R6P0_BACK_RGB_REG  back_rgb;
	struct R6P0_WORK_AREA_SIZE_REG  work_area_size;
	struct R6P0_WORK_AREA_XY_REG  work_area_xy;

	struct R6P0_LAYERIMG_CFG_REG  limg_cfg[R6P0_IMGL_NUM];
	struct R6P0_LAYERIMG_Y_ADDR_REG  limg_y_addr[R6P0_IMGL_NUM];
	struct R6P0_LAYERIMG_U_ADDR_REG  limg_u_addr[R6P0_IMGL_NUM];
	struct R6P0_LAYERIMG_V_ADDR_REG  limg_v_addr[R6P0_IMGL_NUM];
	struct R6P0_LAYERIMG_PITCH_REG  limg_pitch[R6P0_IMGL_NUM];
	struct R6P0_LAYERIMG_CLIP_START_REG  limg_clip_start[R6P0_IMGL_NUM];
	struct R6P0_LAYERIMG_CLIP_SIZE_REG  limg_clip_size[R6P0_IMGL_NUM];
	struct R6P0_LAYERIMG_DES_START_REG  limg_des_start[R6P0_IMGL_NUM];
	struct R6P0_LAYERIMG_PALLET_RGB_REG  limg_pallet_rgb[R6P0_IMGL_NUM];
	struct R6P0_LAYERIMG_CK_REG  limg_ck[R6P0_IMGL_NUM];
	struct R6P0_Y2Y_Y_PARAM_REG  y2y_y_param[R6P0_IMGL_NUM];
	struct R6P0_Y2Y_U_PARAM_REG  y2y_u_param[R6P0_IMGL_NUM];
	struct R6P0_Y2Y_V_PARAM_REG  y2y_v_param[R6P0_IMGL_NUM];
	struct R6P0_LAYERIMG_DES_SCL_SIZE_REG  limg_des_scl_size[R6P0_IMGL_NUM];

	struct R6P0_LAYEROSD_CFG_REG  losd_cfg[R6P0_OSDL_NUM];
	struct R6P0_LAYEROSD_R_ADDR_REG  losd_r_addr[R6P0_OSDL_NUM];
	struct R6P0_LAYEROSD_PITCH_REG  losd_pitch[R6P0_OSDL_NUM];
	struct R6P0_LAYEROSD_CLIP_START_REG  losd_clip_start[R6P0_OSDL_NUM];
	struct R6P0_LAYEROSD_CLIP_SIZE_REG  losd_clip_size[R6P0_OSDL_NUM];
	struct R6P0_LAYEROSD_DES_START_REG  losd_des_start[R6P0_OSDL_NUM];
	struct R6P0_LAYEROSD_PALLET_RGB_REG  losd_pallet_rgb[R6P0_OSDL_NUM];
	struct R6P0_LAYEROSD_CK_REG  losd_ck[R6P0_OSDL_NUM];

	struct R6P0_LAYEROSD_CFG_REG  osdsec_cfg[R6P0_OSDSEC_NUM];
	struct R6P0_LAYEROSD_R_ADDR_REG  osdsec_r_addr[R6P0_OSDSEC_NUM];
	struct R6P0_LAYEROSD_PITCH_REG  osdsec_pitch[R6P0_OSDSEC_NUM];
	struct R6P0_LAYEROSD_CLIP_START_REG  osdsec_clip_start[R6P0_OSDSEC_NUM];
	struct R6P0_LAYEROSD_CLIP_SIZE_REG  osdsec_clip_size[R6P0_OSDSEC_NUM];
	struct R6P0_LAYEROSD_DES_START_REG  osdsec_des_start[R6P0_OSDSEC_NUM];
	struct R6P0_LAYEROSD_PALLET_RGB_REG  osdsec_pallet_rgb[R6P0_OSDSEC_NUM];
	struct R6P0_LAYEROSD_CK_REG  osdsec_ck[R6P0_OSDSEC_NUM];

	struct R6P0_GSP_IP_REV_REG ip_rev;
	struct R6P0_GSP_DEBUG_CFG_REG debug_cfg;
	struct R6P0_GSP_DEBUG1_REG debug1_cfg;
	struct R6P0_GSP_DEBUG2_REG debug2_cfg;
};

#endif
