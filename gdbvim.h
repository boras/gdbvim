#ifndef __GDBVIM_H__
#define __GDBVIM_H__

#include "mi_parser.h"

typedef enum key_type {
	KEY_TAB,
	KEY_OTHER
} key_type_t;

typedef enum gdb_out_type {
	GDB_OUT_ECHO_INCLUDED,
	GDB_OUT_ECHO_TRIMMED
} gdb_out_type_t;

typedef enum gdb_cmd_type {
	GDB_CMD_CLI,
	GDB_CMD_MI
} gdb_cmd_type_t;

typedef enum gdb_mi_cmd_state {
	GDB_MI_CMD_COMPLETED,
	GDB_MI_CMD_INCOMPLETED,
} gdb_mi_cmd_state_t;

typedef enum gdb_state {
	GDB_STATE_CHECK_CMD,
	GDB_STATE_CLI,
	GDB_STATE_MI,
	GDB_STATE_COMPLETION
} gdb_state_t;

typedef struct gdbvim {
	pid_t gdb_pid;
} gdbvim_t;

#endif /* __GDBVIM_H__ */
