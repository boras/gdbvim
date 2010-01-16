#ifndef __MI_CMD_LIST__
#define __MI_CMD_LIST__

typedef struct gdb_mi_cmd {
	char *name;
	int code;
} gdb_mi_cmd_t;

typedef enum gdb_mi_cmd_code {
	GDB_MI_EXEC_BEGIN,
	GDB_MI_EXEC_START,
	GDB_MI_EXEC_RUN,
	GDB_MI_EXEC_CONTINUE,
	GDB_MI_EXEC_UNTIL,
	GDB_MI_EXEC_NEXT,
	GDB_MI_EXEC_NEXT_INS,
	GDB_MI_EXEC_STEP,
	GDB_MI_EXEC_STEP_INS,
	GDB_MI_EXEC_FINISH,
	GDB_MI_EXEC_RETURN,
	GDB_MI_EXEC_JUMP,
	GDB_MI_EXEC_INTERRUPT,
	GDB_MI_EXEC_END
} gdb_mi_cmd_code_t;

#endif /* __MI_CMD_LIST__ */
