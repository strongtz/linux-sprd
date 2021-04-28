/*
 * File:shub_protocol.c
 * Author:Sensor Hub Team
 *
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include "shub_common.h"
#include "shub_protocol.h"

struct shub_data_processor shub_stream_processor;
struct shub_data_processor shub_stream_processor_nwu;

/**
 * Function :  shub_search_flag
 * Description:
 * it find the matic word form the data buffer
 * Parameters:
 * stream : point the current  parse  data
 * data : point the uart buffer data
 * len       : the receive data length
 * processed_len : it deal with data len
 * Return : void
 */
static void shub_search_flag(struct shub_data_processor *stream,
			     u8 *data, u16 len,
			     u16 *processed_len)
{
	u16 headsize = stream->head_size;
	u8 *start_data = data;
	int i = 0;

	/* the magic number is 4 '~' */
	for (i = 0; i < len; i++) {
		if (*start_data == 0x7E) {
			headsize++;
			/* we got the 4 magic '~'  */
			if (headsize == SHUB_MAGIC_NUMBER_LEN) {
				start_data++;
				memset(stream->cur_header, SHUB_MAGIC_NUMBER,
				       SHUB_MAGIC_NUMBER_LEN);
				stream->state = SHUB_RECV_COLLECT_HEAD;
				break;
			}
		} else {
			headsize = 0;
		}
		start_data++;
	}
	stream->head_size = headsize;
	*processed_len = start_data - data;
}

/**
 * Function :  shub_checksum
 * Description :
 * it Calculate the CRC  for the 8 bytes in head buffer
 * Parameters :
 * pHeadData: point the head  data
 * Return : void
 */
static u16 shub_checksum(u8 *head_data)
{
	/* The first 4 octet is 0x7e
	 * 0x7e7e + 0x7e7e = 0xfcfc
	 */
	u32 sum = 0xfcfc;
	u16 n_add;

	head_data += 4;
	n_add = *head_data++;
	n_add <<= 8;
	n_add += *head_data++;
	sum += n_add;

	n_add = *head_data++;
	n_add <<= 8;
	n_add += *head_data++;
	sum += n_add;

	/* The carry is 2 at most, so we need 2 additions at most. */
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

/**
 * Function :  shub_data_checksum
 * Description :
 * it auto fill the encode head context in one packet
 * Parameters:
 * in_data : point the send data context
 * out_data : the one packet head address
 */
static u16 shub_data_checksum(u8 *data, u16 out_len)
{
	unsigned int sum = 0;
	u16 n_add;

	if (!data || out_len == 0)
		return 0;

	while (out_len > 2) {
		n_add = *data++;
		n_add <<= 8;
		n_add += *data++;
		sum += n_add;
		out_len -= 2;
	}
	if (out_len == 2) {
		n_add = *data++;
		n_add <<= 8;
		n_add += *data++;
		sum += n_add;
	} else {
		n_add = *data++;
		n_add <<= 8;
		sum += n_add;
	}
	/*The carry is 2 at most, so we need 2 additions at most. */
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);

	return (u16)(~sum);
}

static void research_flag(struct shub_data_processor *stream)
{
	u8 *start = stream->cur_header;

	/* slip the first '~'  */
	if (start[4] == 0x7e) {
		memmove(start + 4, start + 5, 5);
		stream->head_size = 9;
		stream->state = SHUB_RECV_COLLECT_HEAD;
	} else {
		unsigned int value;

		value = start[5];
		value <<= 8;
		value |= start[6];
		value <<= 8;
		value |= start[7];
		value <<= 8;
		value |= start[8];
		 /* get the [5--8] is magic head  */
		if (value == 0x7e7e7e7e) {
			start[4] = start[9];
			stream->head_size = 5;
			stream->state = SHUB_RECV_COLLECT_HEAD;
		} else {	/* get [6--9] */
			value <<= 8;
			value |= start[9];
			if (value == 0x7e7e7e7e) {
				stream->head_size = 4;
				stream->state = SHUB_RECV_COLLECT_HEAD;
			} else if (0x7e7e7e == (0xffffff & value)) {
				stream->head_size = 3;
				stream->state = SHUB_RECV_SEARCH_FLAG;
			} else if (0x7e7e == (0xffff & value)) {
				stream->head_size = 2;
				stream->state = SHUB_RECV_SEARCH_FLAG;
			} else if (0x7e == (0xff & value)) {
				stream->head_size = 1;
				stream->state = SHUB_RECV_SEARCH_FLAG;
			} else {
				stream->head_size = 0;
				stream->state = SHUB_RECV_SEARCH_FLAG;
			}
		}
	}
}

static void shub_collect_header(struct shub_data_processor *stream,
				u8 *data, u16 len,
				u16 *processed_len)
{
	u16 headsize = stream->head_size;
	u16 remain_len = SHUB_MAX_HEAD_LEN - headsize;
	u16 processed_length;
	u16 crc;
	u16 crc_inframe;

	processed_length = remain_len > len ? len : remain_len;
	memcpy(stream->cur_header + headsize, data, processed_length);
	headsize += processed_length;
	*processed_len = processed_length;
	stream->head_size = headsize;

	if (headsize != SHUB_MAX_HEAD_LEN) /* We have not got 10 bytes*/
		return;

	/* We have got 10 bytes
	 * Calculate the checksum (only 8 bytes in head buffer)
	 */
	crc = shub_checksum(stream->cur_header);
	crc_inframe = stream->cur_header[8];
	crc_inframe <<= 8;
	crc_inframe |= stream->cur_header[9];
	if (crc == crc_inframe)	{	/* We have got a right header*/
		u16 data_len;

		/* Set the frame length here*/
		data_len = stream->cur_header[6];
		data_len <<= 8;
		data_len |= stream->cur_header[7];
		stream->data_len = data_len;
		stream->cmd_data.type = stream->cur_header[4];
		stream->cmd_data.subtype = stream->cur_header[5];
		stream->cmd_data.length = data_len;
		if (data_len == 0) {
			shub_dispatch(&stream->cmd_data);
			stream->state = SHUB_RECV_SEARCH_FLAG;
			stream->head_size = 0;
			stream->received_data_len = 0;
			stream->data_len = 0;
		} else {
			if (data_len <
			    (MAX_MSG_BUFF_SIZE - SHUB_MAX_HEAD_LEN -
			     SHUB_MAX_DATA_CRC)) {
				stream->state = SHUB_RECV_DATA;
			} else {
				stream->error_num++;
				research_flag(stream);
				pr_err("error=%d dataLen=%d\n",
				       stream->error_num, data_len);
			}
		}

	} else {
		stream->error_num++;
		pr_err("crc_inframe=0x%x crc=0x%x\n", crc_inframe, crc);
		research_flag(stream);
		pr_err("error = %d\n", stream->error_num);
	}
}

static int shub_collect_data(struct shub_data_processor *stream,
			     u8 *data, u16 len,
			     u16 *processed_len)
{
	u16 n_frame_remain =
	    stream->data_len - stream->received_data_len + SHUB_MAX_DATA_CRC;
	struct cmd_data *p_packet = &stream->cmd_data;
	u16 n_copy = n_frame_remain > len ? len : n_frame_remain;
	u16 data_crc;
	u16 crc_inframe;

	memcpy(p_packet->buff + stream->received_data_len, data, n_copy);
	stream->received_data_len += n_copy;

	*processed_len = n_copy;
	/*  Have we got the whole frame? */
	if (stream->received_data_len ==
		(stream->data_len + SHUB_MAX_DATA_CRC)) {
		data_crc = shub_data_checksum(p_packet->buff, p_packet->length);
		crc_inframe = p_packet->buff[p_packet->length];
		crc_inframe <<= 8;
		crc_inframe |= p_packet->buff[(p_packet->length + 1)];
		if (data_crc == crc_inframe) {
			shub_dispatch(&stream->cmd_data);
		} else {
			if (p_packet->subtype == SHUB_MEMDUMP_DATA_SUBTYPE ||
			    p_packet->subtype == SHUB_MEMDUMP_CODE_SUBTYPE) {
				/* gps_dispatch(&stream->cmd_data); */
			} else {
				pr_info
					("err type=%d,subtype=%d len=%d\n",
				     p_packet->type, p_packet->subtype,
				     p_packet->length);
				pr_info
				    ("err CRC=%d crc_inframe=%d\n",
				     data_crc, crc_inframe);
			}
		}
		stream->state = SHUB_RECV_SEARCH_FLAG;
		stream->head_size = 0;
		stream->received_data_len = 0;
		stream->data_len = 0;
	}

	return 0;
}

/**
 * Function:  shub_InitOnePacket
 * Description :
 * the init SHUB parse data
 * Parameters :
 * stream : point the current  parse  data
 * Return :
 * TRUE   One  frame completed
 * FALSE  One  frame not completed
 *  Negetive Error
 */
void shub_init_parse_packet(struct shub_data_processor *stream)
{
	stream->state = SHUB_RECV_SEARCH_FLAG;
	stream->head_size = 0;
	stream->received_data_len = 0;
	stream->data_len = 0;
	stream->error_num = 0;
}

/**
 * Function:  shub_parse_one_packet
 * Description :
 * Parse the input uart  data
 * Parameters :
 * stream : point the current  parse  data
 * UseData : point the uart buffer data
 * len       : the receive data length
 * Return :
 * TRUE     One  frame completed
 * FALSE    One  frame not completed
 * Negetive  Error
 */
int shub_parse_one_packet(struct shub_data_processor *stream,
			  u8 *data, u16 len)
{
	u8 *input;
	u16 remain_len = 0;
	u16 processed_len = 0;

	remain_len = len;
	input = data;

	if (!stream || !data)
		return -EINVAL;

	while (remain_len) {
		switch (stream->state) {
		case SHUB_RECV_SEARCH_FLAG:
			shub_search_flag(stream, input, remain_len,
					 &processed_len);
			break;
		case SHUB_RECV_COLLECT_HEAD:
			shub_collect_header(stream, input, remain_len,
					    &processed_len);
			break;
		case SHUB_RECV_DATA:
			shub_collect_data(stream, input, remain_len,
					  &processed_len);
			break;
		default:
			break;
		}
		remain_len -= processed_len;
		input += processed_len;
	}

	return 0;
}

void shub_fill_head(struct cmd_data *in_data, u8 *out_data)
{
	u8 *data = out_data;
	u16 crc = 0;

	*data++ = SHUB_MAGIC_NUMBER;
	*data++ = SHUB_MAGIC_NUMBER;
	*data++ = SHUB_MAGIC_NUMBER;
	*data++ = SHUB_MAGIC_NUMBER;
	*data++ = in_data->type;
	*data++ = in_data->subtype;
	*data++ = SHUB_GET_HIGH_BYTE(in_data->length);
	*data++ = SHUB_GET_LOW_BYTE(in_data->length);
	/*calc crc */
	crc = shub_checksum(out_data);
	*data++ = SHUB_GET_HIGH_BYTE(crc);
	*data++ = SHUB_GET_LOW_BYTE(crc);
}

int shub_encode_one_packet(struct cmd_data *in_data,
			   u8 *out_data,
			   u16 out_len)
{
	int len = 0;
	u8 *crc_data = NULL;
	u16 data_checksum;

	if (!in_data) {
		pr_info("NULL == in_data");
		return -EINVAL;
	}

	if (out_len <= SHUB_MAX_HEAD_LEN) {
		pr_info("  out_len == %d", out_len);
		return -EINVAL;
	}

	len = in_data->length;
	/* First fill the SHUB head context */
	shub_fill_head(in_data, out_data);
	if (len) {
		memcpy((out_data + SHUB_MAX_HEAD_LEN), in_data->buff, len);
		data_checksum = shub_data_checksum(in_data->buff, len);
		crc_data = out_data + SHUB_MAX_HEAD_LEN + len;
		*crc_data++ = SHUB_GET_HIGH_BYTE(data_checksum);
		*crc_data++ = SHUB_GET_LOW_BYTE(data_checksum);
		len += SHUB_MAX_HEAD_LEN;
		len += SHUB_MAX_DATA_CRC;
	}

	return len;
}
