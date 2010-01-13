#ifndef __GDBVIM_H__
#define __GDBVIM_H__

#include "parser.h"

typedef enum key_type {
	KEY_TAB,
	KEY_OTHER
} key_type_t;

typedef enum gdb_out_type {
	GDB_OUT_ECHO,
	GDB_OUT_ANS
} gdb_out_type_t;

typedef enum gdb_cmd_type {
	GDB_CMD_CLI,
	GDB_CMD_MI
} gdb_cmd_type_t;

typedef enum gdb_cmd_mi_state {
	GDB_CMD_MI_COMPLETED,
	GDB_CMD_MI_INCOMPLETED,
} gdb_cmd_mi_state_t;

typedef enum gdb_state {
	GDB_STATE_CLI,
	GDB_STATE_CLI_COMPLETION,
	GDB_STATE_MI
} gdb_state_t;

typedef struct gdbvim {
	pid_t gdb_pid;
} gdbvim_t;

#endif /* __GDBVIM_H__ */
