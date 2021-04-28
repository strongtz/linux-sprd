#ifndef _MIPI_DSI_API_H_
#define _MIPI_DSI_API_H_

#include "sprd_dsi.h"

int sprd_dsi_init(struct sprd_dsi *dsi);
int sprd_dsi_uninit(struct sprd_dsi *dsi);
int sprd_dsi_dpi_video(struct sprd_dsi *dsi);
int sprd_dsi_edpi_video(struct sprd_dsi *dsi);
int sprd_dsi_wr_pkt(struct sprd_dsi *dsi, u8 vc, u8 type,
			const u8 *param, u16 len);
int sprd_dsi_rd_pkt(struct sprd_dsi *dsi, u8 vc, u8 type,
			u8 msb_byte, u8 lsb_byte,
			u8 *buffer, u8 bytes_to_read);
void sprd_dsi_set_work_mode(struct sprd_dsi *dsi, u8 mode);
int sprd_dsi_get_work_mode(struct sprd_dsi *dsi);
void sprd_dsi_lp_cmd_enable(struct sprd_dsi *dsi, bool enable);
void sprd_dsi_nc_clk_en(struct sprd_dsi *dsi, bool enable);
void sprd_dsi_state_reset(struct sprd_dsi *dsi);
u32 sprd_dsi_int_status(struct sprd_dsi *dsi, int index);
void sprd_dsi_int_mask(struct sprd_dsi *dsi, int index);

#endif /* _MIPI_DSI_API_H_ */
