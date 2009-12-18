%{
#include "parser.tab.h"
%}

DIGITS			[0-9]+
IDENTIFIER		[a-zA-Z_][a-zA-Z0-9_-]*
C_STRING		\"([^"\\]|\\.)*\"
SKIP_WS			[ \t]
%%
"(gdb)"			{printf("gdb_prompt\n"); return TOKEN_GDB_PROMPT;}

"^"			{printf("caret\n"); return *yytext;}
","			{printf("comma\n"); return *yytext;}

"*"			{printf("star\n"); return *yytext;}
"+"			{printf("plus\n"); return *yytext;}
"="			{printf("equal\n"); return *yytext;}
"~"			{printf("tilde\n"); return *yytext;}
"@"			{printf("at_sign\n"); return *yytext;}
"&"			{printf("ampersand\n"); return *yytext;}

"done"			{printf("done\n"); return TOKEN_RESULT_DONE;}
"running"		{printf("running\n"); return TOKEN_RESULT_RUNNING;}
"connected"		{printf("connected\n"); return TOKEN_RESULT_CONNECTED;}
"error"			{printf("error\n"); return TOKEN_RESULT_ERROR;}
"exit"			{printf("exit\n"); return TOKEN_RESULT_EXIT;}
"stopped"		{printf("stopped\n"); return TOKEN_ASYNC_STOPPED;}

"{" 			{printf("opening_brace\n"); return *yytext;}
"}" 			{printf("closing_brace\n"); return *yytext;}
"[" 			{printf("opening_bracket\n"); return *yytext;}
"]" 			{printf("closing_bracket\n"); return *yytext;}

"\n"			{printf("newline\n"); return TOKEN_NEWLINE;}
"\r\n"			{printf("newline\n"); return TOKEN_NEWLINE;}
"\r"			{printf("newline\n"); return TOKEN_NEWLINE;}

{DIGITS}		{printf("digits\n"); return TOKEN_DIGITS;}
{C_STRING}		{printf("cstring\n"); return TOKEN_CSTRING;}
{IDENTIFIER}		{printf("identifier\n"); return TOKEN_IDENTIFIER;}
{SKIP_WS}		/* Skip */
%%

int yywrap(void)
{
	return 1;
}

