
DIGIT			[0-9]+
IDENTIFIER		[a-zA-Z_][a-zA-Z0-9_-]*
C_STRING		\"([^"\\]|\\.)*\"
SKIP_WS			[ \t\n]
%%
"(gdb)"			{printf("gdb_prompt = %s\n", yytext);}

"^"			{printf("caret = %s\n", yytext);}
","			{printf("comma = %s\n", yytext);}

"*"			{printf("star = %s\n", yytext);}
"+"			{printf("plus = %s\n", yytext);}
"="			{printf("equal = %s\n", yytext);}
"~"			{printf("tilde = %s\n", yytext);}
"@"			{printf("at = %s\n", yytext);}
"&"			{printf("ampersand = %s\n", yytext);}

"done"			{printf("done = %s\n", yytext);}
"running"		{printf("running = %s\n", yytext);}
"connected"		{printf("connected = %s\n", yytext);}
"error"			{printf("error = %s\n", yytext);}
"exit"			{printf("exit = %s\n", yytext);}
"stopped"		{printf("stopped = %s\n", yytext);}

"{}"			{printf("brace = %s\n", yytext);}
"{" 			{printf("opening brace = %s\n", yytext);}
"}" 			{printf("closing brace = %s\n", yytext);}
"[]"			{printf("square bracket = %s\n", yytext);}
"[" 			{printf("opening square bracket = %s\n", yytext);}
"]" 			{printf("closing square bracket = %s\n", yytext);}

{DIGIT}			{printf("digit = %s\n", yytext);}
{C_STRING}		{printf("c-string = %s\n", yytext);}
{IDENTIFIER}		{printf("identifier = %s\n", yytext);}
{SKIP_WS}		/* Skip */
%%

int yywrap(void)
{
	return 1;
}

int main(void)
{
	if (!(yyin = fopen("out", "r"))) {
		fprintf(stderr, "Cannot open the file called \"out\"");
		return -1;
	}

	yylex();

	fclose(yyin);

	return 0;
}

