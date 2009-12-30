#ifndef __GDBVIM_H__
#define __GDBVIM_H__

#include "parser.h"

typedef enum gdb_cmd_type {
	GDB_CMD_CLI,
	GDB_CMD_MI
} gdb_cmd_type_t;

typedef enum gdb_mi_cmd_state {
	GDB_MI_CMD_COMPLETED,
	GDB_MI_CMD_INCOMPLETED,
} gdb_mi_cmd_state_t;

typedef enum gdb_state {
	GDB_STATE_CLI,
	GDB_STATE_MI
} gdb_state_t;

typedef struct gdbvim {
	pid_t gdb_pid;
	int gdb_ptym;
	int prog_ptym;	/* master */
	int prog_ptys;	/* slave */
} gdbvim_t;

#endif /* __GDBVIM_H__ */
