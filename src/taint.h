#ifndef TAINT_H
#define TAINT_H

enum
{
	TAINT_ON,
	TAINT_OFF,
};

extern int taint_flag;

void taint_mem(void *mem, unsigned long size, int type);
void do_taint(long ret, long call, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6);

#endif /* TAINT_H */
