%{
#include <stdio.h>
extern int yylex(void);
extern void yyerror(const char *str);
extern FILE *yyin;
%}

%token TOKEN_GDB_PROMPT		"(gdb)"

%token TOKEN_RESULT_DONE	"done"
%token TOKEN_RESULT_RUNNING	"running"
%token TOKEN_RESULT_CONNECTED	"connected"
%token TOKEN_RESULT_ERROR	"error"
%token TOKEN_RESULT_EXIT	"exit"
%token TOKEN_ASYNC_STOPPED	"stopped"

%token TOKEN_DIGITS
%token TOKEN_NEWLINE		/* '\n' '\r\n' '\r' */
%token TOKEN_CSTRING
%token TOKEN_IDENTIFIER
%%
output: oob_record_list result_record_list TOKEN_GDB_PROMPT TOKEN_NEWLINE
;
result_record_list: /* empty */
	|      digits '^' result_class result_list TOKEN_NEWLINE {
	printf("result_record>\n")
}
;
oob_record_list: /* empty */
	|   oob_record_list oob_record {printf("oob_record>\n")}
;
oob_record:	async_record {printf("async_record> ");}
	|	stream_record {printf("stream_record> ");}
;
async_record:	exec_async_output
	|	status_async_output
	|	notify_async_output
;
exec_async_output: digits '*' async_output {
	printf("exec_async_output> ");
}
;
status_async_output: digits '+' async_output {
	printf("status_async_output> ");
}
;
notify_async_output: digits '=' async_output {
	printf("notify_async_output> ");
}
;
async_output: async_class result_list TOKEN_NEWLINE {
	printf("async_output> ");
}
;
result_class:	"done" | "running" | "connected" | "error" | "exit"
;
async_class:	"stopped"
;
stream_record:	console_stream_output
	|	target_stream_output
	|	log_stream_output
;
console_stream_output: '~' TOKEN_CSTRING TOKEN_NEWLINE {
	printf("console_stream_output> ");
}
;
target_stream_output: '@' TOKEN_CSTRING TOKEN_NEWLINE {
	printf("target_stream_output> ");
}
;
log_stream_output: '&' TOKEN_CSTRING TOKEN_NEWLINE {
	printf("log_stream_output> ");
}
;
result_list:	/* empty */
	|	result_list ',' result
;
result:		TOKEN_IDENTIFIER '=' value
;
value_list:	/* empty */
	|	value_list ',' value
;
value:		TOKEN_CSTRING
	|	tuple
	|	list
;
tuple:		'{''}'
	|	'{' result result_list '}'
;
list:		'['']'
	|	'[' result result_list ']'
	|	'[' value value_list ']'
;
digits:		/* empty */
	|	TOKEN_DIGITS
;
%%

void yyerror(const char *str)
{
	printf("Fatal error: %s\n", str);
}

int main(void)
{
	/*if (!(yyin = fopen("out", "r"))) {*/
		/*fprintf(stderr, "Cannot open the file called \"out\"");*/
		/*return -1;*/
	/*}*/

	yyparse();

	/*fclose(yyin);*/

	return 0;
}

