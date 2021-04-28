#ifndef _LOOPCHECK_H_
#define _LOOPCHECK_H_

void start_loopcheck_timer(void);
void stop_loopcheck_timer(void);
int loopcheck_init(void);
int loopcheck_deinit(void);
void complete_kernel_loopcheck(void);
void complete_kernel_atcmd(void);
unsigned int loopcheck_register_cb(unsigned int type, void *func);

#endif
