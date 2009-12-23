%{
/*
 * We have to insert "parser.h" here, otherwise bison %union declaration
 * does not see typedef declarations.
 */
#include "parsetree.h"
#include "grammar.tab.h"
%}

DIGITS			[0-9]+
IDENTIFIER		[a-zA-Z_][a-zA-Z0-9_-]*
C_STRING		\"([^"\\]|\\.)*\"
SKIP_WS			[ \t]
%%
"(gdb)"			{return TOKEN_GDB_PROMPT;}

"^"			{return *yytext;}
","			{return *yytext;}

"*"			{return *yytext;}
"+"			{return *yytext;}
"="			{return *yytext;}
"~"			{return *yytext;}
"@"			{return *yytext;}
"&"			{return *yytext;}

"done"			{return TOKEN_RESULT_DONE;}
"running"		{return TOKEN_RESULT_RUNNING;}
"connected"		{return TOKEN_RESULT_CONNECTED;}
"error"			{return TOKEN_RESULT_ERROR;}
"exit"			{return TOKEN_RESULT_EXIT;}
"stopped"		{return TOKEN_ASYNC_STOPPED;}

"{" 			{return *yytext;}
"}" 			{return *yytext;}
"[" 			{return *yytext;}
"]" 			{return *yytext;}

"\n"			{return TOKEN_NEWLINE;}
"\r\n"			{return TOKEN_NEWLINE;}
"\r"			{return TOKEN_NEWLINE;}

{DIGITS}		{return TOKEN_DIGITS;}
{C_STRING}		{return TOKEN_CSTRING;}
{IDENTIFIER}		{return TOKEN_IDENTIFIER;}
{SKIP_WS}		/* Skip */
%%

int yywrap(void)
{
	return 1;
}
