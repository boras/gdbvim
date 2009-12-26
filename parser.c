#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "parsetree.h"

frame_info_t *alloc_frame_info(void)
{
	frame_info_t *f;

	if (!(f = (frame_info_t *)calloc(1, sizeof(frame_info_t)))) {
		fprintf(stderr, "Cannot allocate memory\n");
		return NULL;
	}

	return f;
}

void free_frame_info(frame_info_t *finfo_ptr)
{
	if (finfo_ptr->addr)
		free(finfo_ptr->addr);
	if (finfo_ptr->func)
		free(finfo_ptr->func);
	if (finfo_ptr->args)
		free(finfo_ptr->args);
	if (finfo_ptr->file)
		free(finfo_ptr->file);
	if (finfo_ptr->fullname)
		free(finfo_ptr->fullname);
	if (finfo_ptr->line)
		free(finfo_ptr->line);
	if (finfo_ptr->from)
		free(finfo_ptr->from);
	free(finfo_ptr);
}

/*
 * A cstring has the following form:
 * ~"This GDB was configured as \"i486-linux-gnu\".\n"
 * &"warning: Source file is more recent than executable.\n"
 *
 * Things to be done:
 *	1. Initial character(~ etc.) is omitted
 *	2. The beginning and ending double quotes are omitted
 *	3. \" is converted to "
 *	4. \n is converted to newline
 *
 * In our situation, the initial character is already removed.
 * However, the last three still holds true. In addition, \t
 * is also converted to a tab.
 */
static char *convert_cstr_to_str(const char *cstr)
{
	char *str;
	int i, j;

	/*
	 * str will never be longer than cstr. Even if it
	 * includes a null character.
	 */
	if (!(str = (char *)malloc(strlen(cstr)))) {
		fprintf(stderr, "Cannot allocate memory\n");
		return NULL;
	}

	/* Skip the first character of cstring */
	for (i = 1, j = 0; cstr[i] != '\0'; i++) {
		/* \" is converted to " */
		if (cstr[i] == '\\' && cstr[i + 1] == '\"')
			continue;
		/* \n is converted to newline */
		if (cstr[i] == '\\' && cstr[i + 1] == 'n') {
			str[j++] = '\n';
			cstr++;
			continue;
		}
		/* \t is converted to tab */
		if (cstr[i] == '\\' && cstr[i + 1] == 't') {
			str[j++] = '\t';
			cstr++;
			continue;
		}
		/* The ending double quote is omitted */
		if (cstr[i] == '\"' && cstr[i + 1] == '\0')
			continue;
		str[j++] = cstr[i];
	}
	str[j] = '\0';

	return str;
}

/*
 * For a given variable, it searches through the result_list
 * and then returns the value_ptr associated with it.
 */
static value_t *mi_lookup_var(result_t *rlist_ptr, const char *var)
{
	result_t *cur = rlist_ptr;

	while (cur) {
		if (!strcmp(cur->identifier, var))
			return cur->val_ptr;
		cur = cur->next;
	}

	return NULL;
}

static value_t *mi_get_val_list_by_value(value_t *val_ptr)
{
	return val_ptr->data.list_ptr->data.value_ptr;
}

static result_t *mi_get_val_list_by_result(value_t *val_ptr)
{
	return val_ptr->data.list_ptr->data.result_ptr;
}

static result_t *mi_get_val_tuple(value_t *val_ptr)
{
	return val_ptr->data.tuple_ptr->result_ptr;
}

static char *mi_get_val_cstr(value_t *val_ptr)
{
	return convert_cstr_to_str(val_ptr->data.cstr);
}

/* For debugging purposes */
void mi_print_frame_info(frame_info_t *finfo_ptr)
{
	printf("addr: %s\n", finfo_ptr->addr);
	printf("func: %s\n", finfo_ptr->func);
	printf("file: %s\n", finfo_ptr->file);
	printf("fullname: %s\n", finfo_ptr->fullname);
	printf("line: %s\n", finfo_ptr->line);
}

/*
 * For a given result_list, it finds the frame variable and after getting
 * its value, fills in the frame structure.
 */
frame_info_t *mi_parse_frame(result_t *rlist)
{
	value_t *v;
	result_t *r;
	frame_info_t *finfo_ptr;

	/* Argument validity */
	if (!rlist)
		return NULL;

	/* "frame" variable is looked up. Its value is returned if found */
	v = mi_lookup_var(rlist, "frame");
	if (!v)
		return NULL;
	/*
	 * Frame information, value in mi parlance, is kept in a tuple.
	 * In the below line, we are getting the head of the result list
	 * in that tuple.
	 */
	r = mi_get_val_tuple(v);
	if (!(finfo_ptr = alloc_frame_info()))
		return NULL;

	while (r) {
		if (!strcmp(r->identifier, "addr"))
			finfo_ptr->addr = mi_get_val_cstr(r->val_ptr);
		if (!strcmp(r->identifier, "func"))
			finfo_ptr->func = mi_get_val_cstr(r->val_ptr);
		if (!strcmp(r->identifier, "file"))
			finfo_ptr->file = mi_get_val_cstr(r->val_ptr);
		if (!strcmp(r->identifier, "fullname"))
			finfo_ptr->fullname = mi_get_val_cstr(r->val_ptr);
		if (!strcmp(r->identifier, "line"))
			finfo_ptr->line = mi_get_val_cstr(r->val_ptr);
		r = r->next;
	}

	return finfo_ptr;
}

/*
 * Some commands bring in asynchronous responses. For example most
 * execution commands like next, step, finish etc. belong to this
 * category. They are type of exec async record.
 */
frame_info_t *mi_get_frame(async_record_t *async_rec_ptr)
{
	async_output_t *aout = async_rec_ptr->async_out_ptr;

	if (aout->aclass == ASYNC_STOPPED)
		return mi_parse_frame(aout->result_ptr);
	else
		fprintf(stderr, "Unknown async class\n");

	return NULL;
}

async_record_t *mi_get_exec_async_record(gdbmi_output_t *gdbmi_out_ptr)
{
	oob_record_t *cur = gdbmi_out_ptr->oob_rec_ptr;

	while (cur) {
		if (cur->rtype == ASYNC_RECORD &&
		    cur->r.async_rec_ptr->atype == EXEC_ASYNC)
			return cur->r.async_rec_ptr;
		cur = cur->next;
	}

	return NULL;
}

/*
 * Console stream messages are replies to cli commands.
 */
void mi_print_console_stream(gdbmi_output_t *gdbmi_out_ptr)
{
	oob_record_t *cur = gdbmi_out_ptr->oob_rec_ptr;
	char *str;

	while (cur) {
		if (cur->rtype == STREAM_RECORD &&
		    cur->r.stream_rec_ptr->stype == CONSOLE_STREAM) {
			str = convert_cstr_to_str(cur->r.stream_rec_ptr->cstr);
			printf("%s", str);
			free(str);
		}
		cur = cur->next;
	}
}

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

void yyerror(const char *str)
{
	printf("%s: %s\n", __FUNCTION__, str);
}

int main(void)
{
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

	return 0;
}
