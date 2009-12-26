#include <stdio.h>
#include "parser.h"

/* Extern declarations */
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *yy_str);
extern void yy_delete_buffer(YY_BUFFER_STATE b);
extern void yy_flush_buffer(YY_BUFFER_STATE b);

int main_loop(void)
{
	async_record_t *async_rec_ptr;
	frame_info_t *finfo_ptr;

	/* Print console stream messages */
	mi_print_console_stream(gdbmi_out_ptr);
	/* Frame information is retrieved from exec async record */
	if (async_rec_ptr = mi_get_exec_async_record(gdbmi_out_ptr)) {
		finfo_ptr = mi_get_frame(async_rec_ptr);
		mi_print_frame_info(finfo_ptr);
		free_frame_info(finfo_ptr);
	}
	else
		printf("There is no exec async record\n");
	/*
	 * FIXME: ERROR should be handled. It has an associated
	 * var=cstring. Others, EXIT,CONNECTED, and RUNNING do
	 * not have var=value pairs.
	 */

	return 0;
}

int main(void)
{
	YY_BUFFER_STATE scanner_state;
	const char *str_array[] = {
		"~\"This GDB was configured as \\\"i486-linux-gnu\\\".\\n\"\n(gdb)\n",
		"*stopped,reason=\"breakpoint-hit\",bkptno=\"1\",thread-id=\"1\",frame={addr=\"0x080485a0\",func=\"main\",args=[],file=\"zero.c\",fullname=\"/home/bora/zero.c\",line=\"42\"}\n(gdb)\n",
		NULL
	};
	int i;
	const char *str;

	for (i = 0; str_array[i]; i++) {
		str = str_array[i];
		printf("str_raw[%d]: %s\n", i, str);
		scanner_state = yy_scan_string(str);

		/* Start parsing */
		yyparse();

		/* Check if there is a valid gdb/mi output */
		if (gdbmi_out_ptr) {
			main_loop();
			destroy_gdbmi_output();
			gdbmi_out_ptr = NULL;
		}
		else
			printf("Partial or wrong gdbmi output. Syntax or "
			       "grammar problem?\n");

		yy_flush_buffer(scanner_state);
		putchar('\n');
	}

	yy_delete_buffer(scanner_state);

	return 0;
}
