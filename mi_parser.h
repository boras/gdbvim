#ifndef __MI_PARSER_H__
#define __MI_PARSER_H__

#include "mi_parsetree.h"

/* Frame information */
typedef struct frame_info {
	int level;
	char *addr;
	char *func;
	char *args;
	char *file;
	char *fullname;
	char *line;
	char *from;
} frame_info_t;

/* Function prototypes */
char *mi_get_error_result_record(gdbmi_output_t *gdbmi_out_ptr);

void mi_print_console_stream(gdbmi_output_t *gdbmi_out_ptr);

async_record_t *mi_get_exec_async_record(gdbmi_output_t *gdbmi_out_ptr);
frame_info_t *mi_get_frame(async_record_t *async_rec_ptr);
void mi_print_frame_info(frame_info_t *finfo_ptr);
frame_info_t *alloc_frame_info(void);
void free_frame_info(frame_info_t *finfo_ptr);

#endif /* __MI_PARSER_H__ */
