#ifndef _WCN_PROCFS
#define _WCN_PROCFS

#define MDBG_SNAP_SHOOT_SIZE		(32*1024)
#define MDBG_WRITE_SIZE			(64)
#define MDBG_ASSERT_SIZE		(1024)
#define MDBG_LOOPCHECK_SIZE		(128)
#define MDBG_AT_CMD_SIZE		(128)

void mdbg_fs_channel_init(void);
void mdbg_fs_channel_destroy(void);
int proc_fs_init(void);
int mdbg_memory_alloc(void);
void proc_fs_exit(void);
int get_loopcheck_status(void);
void wakeup_loopcheck_int(void);
void loopcheck_ready_clear(void);
void loopcheck_ready_set(void);
void mdbg_assert_interface(char *str);
#endif
