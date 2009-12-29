#include <stdio.h>
#include <pty.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <termios.h>
#include <signal.h>
#include "gdbvim.h"

/* Symbolic constants */
#define IN_BUF_SIZE	256
#define GDB_BUF_SIZE	1024
#define PROG_BUF_SIZE	1024
#define GDB_CMD_SIZE	256

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

int main_loop(gdbvim_t *gv_h)
{
	char inbuf[IN_BUF_SIZE];
	char gdbbuf[GDB_BUF_SIZE];
	char progbuf[PROG_BUF_SIZE];
	char gdb_cmd[GDB_CMD_SIZE];
	struct pollfd fds[3];
	char *gdbbuf_ptr = gdbbuf;
	int nread, nread_total = 0;
	int ret;

	/* The descriptors to be listened */
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	fds[1].fd = gv_h->gdb_ptym;
	fds[1].events = POLLIN;
	fds[2].fd = gv_h->prog_ptym;
	fds[2].events = POLLIN;

	/* Tell gdb to use prog_ptys for program output */
	sprintf(gdb_cmd, "set inferior-tty %s\n", ptsname(gv_h->prog_ptys));
	/*printf("gdb_cmd = %s", gdb_cmd);*/
	write(gv_h->gdb_ptym, gdb_cmd, strlen(gdb_cmd));

	/* The main loop */
	while (1) {
		/* Wait for indefinetely */
		if ((ret = poll(fds, 3, -1)) <= 0) {
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
			nread = read(fds[0].fd, inbuf, IN_BUF_SIZE);
			write(gv_h->gdb_ptym, inbuf, nread);
		}
		if (fds[1].revents == POLLIN) {
			/*
			 * gdb/mi output. Two things should be done.
			 * 1. gdb/mi output is converted to the gdb/cli form.
			 * 2. file and line information is obtained to have
			 * Vim show the file and line in question by using
			 * signs.
			 */
			nread = read(fds[1].fd, gdbbuf_ptr, GDB_BUF_SIZE);
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
			if (!strncmp(&gdbbuf_ptr[nread - 7], "(gdb) \n", 7)) {
				/*
				 * It covers two situation: If the last or
				 * the only block is (gdb) \n.
				 */
				nread_total += nread;
				/* Output is logged for debugging purposes */
				if (logger(gdbbuf, nread_total, 1) < 0)
					return -1;
				/* Scanner expects a null terminated string */
				gdbbuf[nread_total] = '\0';
				parse_mi_output(gdbbuf);
				/*[> gdb prompt <]*/
				/*write(STDIN_FILENO, "(gdb) ", 6);*/
				/* Reset the buffer head */
				gdbbuf_ptr = gdbbuf;
				nread_total = 0;
			}
			else {
				/* gdb/mi output are accumulated */
				gdbbuf_ptr += nread;
				nread_total += nread;
			}
		}
		if (fds[2].revents == POLLIN) {
			nread = read(fds[2].fd, progbuf, PROG_BUF_SIZE);
			write(STDOUT_FILENO, progbuf, nread);
		}
		/* gdb prompt */
		write(STDIN_FILENO, "(gdb) ", 6);
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
	gdbvim_t *gv_h;
	struct termios stermios;
	int ret = 0;

	/* Before going further, parse arguments */
	ret = parse_args(argc, argv);
	if (ret < 0)
		return -1;

	/* Allocate the handle */
	if (!(gv_h = (gdbvim_t *)malloc(sizeof(gdbvim_t)))) {
		fprintf(stderr, "Cannot allocate memory\n");
		return -1;
	}

	/* Child is created with a pseudo controlling terminal */
	gv_h->gdb_pid = forkpty(&gv_h->gdb_ptym, NULL, NULL, NULL);
	if (gv_h->gdb_pid < 0) {
		fprintf(stderr, "Cannot fork\n");
		perror(__FUNCTION__);
		goto err_out1;
		return -1;
	}
	else if (gv_h->gdb_pid == 0) {	/* Child */
		tcgetattr(STDIN_FILENO, &stermios);
		/* Turn echo'ing off */
		stermios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
		/* Turn NL -> CR/NL output mapping off */
		stermios.c_oflag &= ~(ONLCR);
		tcsetattr(STDIN_FILENO, TCSANOW, &stermios);

		execlp(gdb_bin_name, gdb_bin_name, "--interpreter=mi", NULL);
	}
	/* Parent */

	/* Pseudo terminal for the program being debugged */
	ret = openpty(&gv_h->prog_ptym, &gv_h->prog_ptys, NULL, NULL, NULL);
	if (ret < 0) {
		fprintf(stderr, "No pseudo tty left\n");
		goto err_out2;
		return -1;
	}
	/*
	 * Copies
	 *	stdin -> gdb_ptym or prog_ptym,
	 *	gdb_ptym or prog_ptym -> stdout
	 */
	main_loop(gv_h);

err_out2:
	kill(gv_h->gdb_pid, SIGTERM);
err_out1:
	free(gv_h);

	return 0;
}
