%{
#include <stdio.h>
#include <string.h>
#include "parser.h"
extern char *yytext;
%}

%union {
	char *char_ptr;
	gdbmi_output_t *gdbmi_output_ptr;
	oob_record_t *oob_record_ptr;
	stream_record_t *stream_record_ptr;
	async_record_t *async_record_ptr;
	async_output_t *async_output_ptr;
	async_class_t aclass;
	result_record_t *result_record_ptr;
	result_class_t rclass;
	result_t *result_ptr;
	value_t *value_ptr;
	tuple_t *tuple_ptr;
	list_t *list_ptr;
}

%type <gdbmi_output_ptr> output
%type <oob_record_ptr> oob_record_list
%type <oob_record_ptr> oob_record
%type <stream_record_ptr> stream_record
%type <char_ptr> console_stream_output
%type <char_ptr> target_stream_output
%type <char_ptr> log_stream_output
%type <async_record_ptr> async_record
%type <async_record_ptr> exec_async_output
%type <async_record_ptr> status_async_output
%type <async_record_ptr> notify_async_output
%type <async_output_ptr> async_output
%type <aclass> async_class

%type <result_record_ptr> result_record_list
%type <rclass> result_class
%type <result_ptr> result
%type <result_ptr> result_list
%type <value_ptr> value
%type <value_ptr> value_list
%type <tuple_ptr> tuple
%type <list_ptr> list

%type <char_ptr> identifier
%type <char_ptr> cstring
%type <char_ptr> digits

%token TOKEN_GDB_PROMPT		"(gdb)"

%token TOKEN_RESULT_DONE	"done"
%token TOKEN_RESULT_RUNNING	"running"
%token TOKEN_RESULT_CONNECTED	"connected"
%token TOKEN_RESULT_ERROR	"error"
%token TOKEN_RESULT_EXIT	"exit"
%token TOKEN_ASYNC_STOPPED	"stopped"

%token <char_ptr> TOKEN_DIGITS
%token TOKEN_NEWLINE		/* '\n' '\r\n' '\r' */
%token <char_ptr> TOKEN_CSTRING
%token TOKEN_IDENTIFIER
%%
output: oob_record_list result_record_list TOKEN_GDB_PROMPT TOKEN_NEWLINE {
	gdbmi_out_ptr = create_gdbmi_output($1, $2);
}
;
result_record_list: {$$ = NULL;}
	|      digits '^' result_class result_list TOKEN_NEWLINE {
	$$ = create_result_record($1, $3, $4);
}
;
oob_record_list: {$$ = NULL;}
	|	oob_record_list oob_record {$$ = append_oob_record($1, $2);}
;
oob_record:	async_record {$$ = create_oob_record(ASYNC_RECORD, $1);}
	|	stream_record {$$ = create_oob_record(STREAM_RECORD, $1);}
;
async_record:	exec_async_output {$$ = $1;}
	|	status_async_output {$$ = $1;}
	|	notify_async_output {$$ = $1;}
;
exec_async_output: digits '*' async_output {
	$$ = create_async_record(EXEC_ASYNC, $1, $3);
}
;
status_async_output: digits '+' async_output {
	$$ = create_async_record(STATUS_ASYNC, $1, $3);
}
;
notify_async_output: digits '=' async_output {
	$$ = create_async_record(NOTIFY_ASYNC, $1, $3);
}
;
async_output: async_class result_list TOKEN_NEWLINE {
	$$ = create_async_output($1, $2);
}
;
result_class:	"done" {$$ = RESULT_DONE;}
	 |	"running" {$$ = RESULT_RUNNING;}
	 |	"connected" {$$ = RESULT_CONNECTED;}
	 |	"error" {$$ = RESULT_ERROR;}
	 |	"exit" {$$ = RESULT_EXIT;}
;
async_class:	"stopped" {$$ = ASYNC_STOPPED;}
;
stream_record:	console_stream_output {$$ = create_stream_record(CONSOLE_STREAM, $1);}
	|	target_stream_output {$$ = create_stream_record(TARGET_STREAM, $1);}
	|	log_stream_output {$$ = create_stream_record(LOG_STREAM, $1);}
;
console_stream_output: '~' cstring TOKEN_NEWLINE {$$ = $2;}
;
target_stream_output: '@' cstring TOKEN_NEWLINE { $$ = $2;}
;
log_stream_output: '&' cstring TOKEN_NEWLINE {$$ = $2;}
;
result_list:	{$$ = NULL;} /* empty */
	|	result {$$ = $1;} /* result_list_head */
	|	result_list ',' result {$$ = append_result($1, $3);}
;
result:		identifier '=' value {$$ = create_result($1, $3);}
;
/* We do not include empty match because result_list already provides it */
value_list: 	value {$$ = $1;} /* value_list_head */
	|	value_list ',' value {$$ = append_value($1, $3);}
;
value:		cstring {$$ = create_value(CSTRING, $1);}
	|	tuple {$$ = create_value(TUPLE, $1);}
	|	list {$$ = create_value(LIST, $1);}
;
tuple: 		'{' result_list '}' {$$ = $2 ? create_tuple($2) : NULL;}
;
list:		'[' result_list ']' {$$ = $2 ? create_list(RESULT, $2) : NULL;}
	|	'[' value_list ']' {$$ = $2 ? create_list(VALUE, $2) : NULL;} 
;
identifier:	TOKEN_IDENTIFIER {$$ = strdup(yytext);}
;
cstring:	TOKEN_CSTRING {$$ = strdup(yytext);}
;
digits:		{$$ = NULL;}
	|	TOKEN_DIGITS {$$ = strdup(yytext);}
;
%%
