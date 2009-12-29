#ifndef __GDBVIM_H__
#define __GDBVIM_H__

#include "parser.h"

typedef struct gdbvim {
	pid_t gdb_pid;
	int gdb_ptym;
	int prog_ptym;
	int prog_ptys;
} gdbvim_t;

#endif /* __GDBVIM_H__ */
