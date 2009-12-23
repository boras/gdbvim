#include <stdio.h>
#include "parsetree.h"

void yyerror(const char *str)
{
	printf("%s: %s\n", __FUNCTION__, str);
}

int main(void)
{
	/*if (!(yyin = fopen("out", "r"))) {*/
		/*fprintf(stderr, "Cannot open the file called \"out\"");*/
		/*return -1;*/
	/*}*/

	yyparse();

	if (gdbmi_out_ptr) {
		print_gdbmi_output();
		destroy_gdbmi_output();
		gdbmi_out_ptr = NULL;
	}
	else
		printf("Partial or wrong gdbmi output\n");


	/*fclose(yyin);*/

	return 0;
}
