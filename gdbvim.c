#include <stdio.h>
#include <pty.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <termios.h>
#include "parser.h"

#define BUFFER_SIZE	1024

/* Extern declarations */
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *yy_str);
extern void yy_delete_buffer(YY_BUFFER_STATE b);
extern void yy_flush_buffer(YY_BUFFER_STATE b);

void print_mi_output(void)
{
	async_record_t *async_rec_ptr;
	frame_info_t *finfo_ptr;
	char *str;

	/*
	 * FIXME: DONE should be handled. Others; EXIT, CONNECTED,
	 * and RUNNING do not have any value(s).
	 */
	/* Check if there is error result record */
	if (str = mi_get_error_result_record(gdbmi_out_ptr)) {
		printf("%s\n", str);
		logger(str, strlen(str), 0);
		logger("\n", 1, 0);
		free(str);
	}
	else {
		/* Print console stream messages */
		mi_print_console_stream(gdbmi_out_ptr);
		/* Frame information is retrieved from exec async record */
		if (async_rec_ptr = mi_get_exec_async_record(gdbmi_out_ptr)) {
			finfo_ptr = mi_get_frame(async_rec_ptr);
			mi_print_frame_info(finfo_ptr);
			free_frame_info(finfo_ptr);
		}
	}
}

void parse_mi_output(char *str)
{
	YY_BUFFER_STATE bufstate;

	bufstate = yy_scan_string(str);

	/* Start parsing */
	yyparse();

	/* Check if there is a valid gdb/mi output */
	if (gdbmi_out_ptr) {
		print_mi_output();
		destroy_gdbmi_output();
		gdbmi_out_ptr = NULL;
	}
	else
		printf("Partial or wrong gdbmi output. Syntax or "
		       "grammar problem?\n");

	yy_delete_buffer(bufstate);
}

int main_loop(int ptym)
{
	char buf[BUFFER_SIZE];
	char *buf_ptr = buf;
	struct pollfd fds[2];
	int nread, nread_total = 0;
	int ret;

	/* The descriptors to be listened */
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	fds[1].fd = ptym;
	fds[1].events = POLLIN;

	/* The main loop */
	while (1) {
		/* Wait for indefinetely */
		if ((ret = poll(fds, 2, -1)) <= 0) {
			fprintf(stderr, "Poll error\n");
			perror(__FUNCTION__);
			return ret;
		}
		if (fds[0].revents == POLLIN) {
			/*
			 * pseudo gdb/cli interface. Commands are read by
			 * readline library. Tab completion should be there.
			 * Afterwards they are converted to gdb/mi input
			 * commands.
			 */
			nread = read(fds[0].fd, buf, BUFFER_SIZE);
			write(ptym, buf, nread);
		}
		if (fds[1].revents == POLLIN) {
			/*
			 * gdb/mi output. Two things should be done.
			 * 1. gdb/mi output is converted to the gdb/cli form.
			 * 2. file and line information is obtained to have
			 * Vim show the file and line in question by using
			 * signs.
			 */
			nread = read(fds[1].fd, buf_ptr, BUFFER_SIZE);
			/*
			 * Before giving the buffer for parsing, we must
			 * ensure that it contains valid gdb/mi output.
			 * If there ara some data and we pass it immediately
			 * there is a high chance that it has not been
			 * complete yet. So for every read we should compare
			 * its last 7 characters to (gdb) \n. (gdb) \n
			 * notation represents the end of gdb/mi output.
			 * The space between ')' and '\n' is not mentioned
			 * in the documentation but we verified that it is
			 * placed.
			 */
			if (!strncmp(&buf_ptr[nread - 7], "(gdb) \n", 7)) {
				/*
				 * It covers two situation: If the last or
				 * the only block is (gdb) \n.
				 */
				nread_total += nread;
				/* Output is logged for debugging purposes */
				if (logger(buf, nread_total, 1) < 0)
					return -1;
				/* Scanner expects a null terminated string */
				buf[nread_total] = '\0';
				parse_mi_output(buf);
				/* gdb prompt */
				write(STDIN_FILENO, "(gdb) ", 6);
				/* Reset the buffer head */
				buf_ptr = buf;
				nread_total = 0;
			}
			else {
				/* gdb/mi output are accumulated */
				buf_ptr += nread;
				nread_total += nread;
			}
		}
	}

	return 0;
}

static char *prog_name = "gdbvim";
static char *gdb_bin_name;

static void show_help(void)
{
	printf("Usage: %s -x gdb_bin_name\n", prog_name);
	printf("for help, type -h\n");
}

static char *get_prog_name(char *str)
{
	char *name = str + strlen(str);

	do {
		--name;
	} while (*name != '/');

	return ++name;
}

static int parse_args(int argc, char *argv[])
{
	int c;
	char *path;

	gdb_bin_name = getenv("GDB_BIN_NAME");
	prog_name = get_prog_name(argv[0]);

	/* Option processing */
	opterr = 0;
	while (1) {
		c = getopt(argc, argv, "hx:");
		if (c == -1)
			break;

		switch (c) {
		case 'x':
			gdb_bin_name = optarg;
			//FIXME: check if gdb_bin_name is in the path
			break;
		case 'h':
			show_help();
			return -1;
		case '?':
			fprintf(stderr, "%s: Missing option argument or "
				"unknown option\n", prog_name);
			return -1;
		default:
			fprintf(stderr, "%s: ?? getopt returned character "
				"code 0x%x ??\n", prog_name, c);
			return -1;
		}
	}
	if (optind < argc) { /* Non-option encountered */
		fprintf(stderr, "%s: Missing option argument or "
			"unknown option\n", prog_name);
		return -1;
	}

	if (argc == 1 && !gdb_bin_name) {
		printf("Warning: No gdb executable specified, assuming "
		       "\"gdb\" as the default back-end\n");
		gdb_bin_name = "gdb";
		return 0;
	}


	return 0;
}

int main(int argc, char *argv[])
{
	int ptym, pid;
	struct termios stermios;
	int ret = 0;

	/* Before going further, parse arguments */
	ret = parse_args(argc, argv);
	if (ret < 0)
		return -1;

	/* Child is created with a pseudo controlling terminal */
	pid = forkpty(&ptym, NULL, NULL, NULL);
	if (pid < 0) {
		fprintf(stderr, "Cannot fork\n");
		perror(__FUNCTION__);
		return -1;
	}
	else if (pid == 0) {	/* Child */
		tcgetattr(STDIN_FILENO, &stermios);
		/* Turn echo'ing off */
		stermios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
		/* Turn NL -> CR/NL output mapping off */
		stermios.c_oflag &= ~(ONLCR);
		tcsetattr(STDIN_FILENO, TCSANOW, &stermios);

		execlp(gdb_bin_name, gdb_bin_name, "--interpreter=mi", NULL);
	}

	/* Parent */
	main_loop(ptym); /* Copies stdin -> ptym, ptym -> stdin */

	return 0;
}
