#ifndef __SPOOL_H
#define __SPOOL_H

struct spool_init_data {
	char	*name;
	u8	dst;
	u8	channel;
	u8	nodev;
	u32	txblocknum;
	u32	txblocksize;
	u32	rxblocknum;
	u32	rxblocksize;
};

#endif
