#include <stdio.h>

int logger(const char *buf, int nread, int raw_io)
{
	FILE *fptr;
	char *filename = "/home/bora/gdb-frontends/gdbvim/miout.log";

	if (!(fptr = fopen(filename, "a"))) {
		fprintf(stderr, "Cannot open %s\n", filename);
		return -1;
	}
	if (raw_io)
		fprintf(fptr, "\n<raw_begin, nread = %d>\n", nread);
	fwrite(buf, nread, 1, fptr);
	if (raw_io)
		fprintf(fptr, "\n</raw_end, nread = %d>\n\n", nread);

	fclose(fptr);

	return 0;
}

