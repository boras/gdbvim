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
#define GDB_ARGS_SIZE	64

/* Extern declarations */
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *yy_str);
extern void yy_delete_buffer(YY_BUFFER_STATE b);
extern void yy_flush_buffer(YY_BUFFER_STATE b);

gdb_mi_cmd_state_t parse_mi_parsetree(void)
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
		return GDB_MI_CMD_COMPLETED;
	}
	else {
		/* Print console stream messages */
		mi_print_console_stream(gdbmi_out_ptr);
		/* Frame information is retrieved from exec async record */
		if (async_rec_ptr = mi_get_exec_async_record(gdbmi_out_ptr)) {
			/* Found */
			finfo_ptr = mi_get_frame(async_rec_ptr);
			mi_print_frame_info(finfo_ptr);
			free_frame_info(finfo_ptr);
			return GDB_MI_CMD_COMPLETED;
		}
		else /* We have not got it yet */
			return GDB_MI_CMD_INCOMPLETED;
	}

	return GDB_MI_CMD_COMPLETED;
}

int create_mi_parsetree(char *str)
{
	YY_BUFFER_STATE bufstate;
	int ret;

	bufstate = yy_scan_string(str);

	/* Start creating a parse tree */
	yyparse();

	/* Check if there is a valid gdb/mi output */
	if (!gdbmi_out_ptr) {
		printf("Partial or wrong gdbmi output. Syntax or "
		       "grammar problem?\n");
		ret = -1;
	}
	else
		ret = 0;

	yy_delete_buffer(bufstate);

	return ret;
}

gdb_mi_cmd_state_t handle_mi_output(char *gdbbuf)
{
	gdb_mi_cmd_state_t mi_cmd_status;

	if (!create_mi_parsetree(gdbbuf)) {
		/* There is a valid parse tree */
		mi_cmd_status = parse_mi_parsetree();
		destroy_gdbmi_output();
		gdbmi_out_ptr = NULL;

		return mi_cmd_status;
	}
	/* Parse tree could not be created */

	return GDB_MI_CMD_COMPLETED;
}

int get_mi_output(int ptym, char *gdbbuf)
{
	static int nread_total = 0;
	int nread;
	char *gdbbuf_ptr;

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
	gdbbuf_ptr = gdbbuf + nread_total;
	nread = read(ptym, gdbbuf_ptr, GDB_BUF_SIZE);
	if (!strncmp(&gdbbuf_ptr[nread - 7], "(gdb) \n", 7)) {
		/*
		 * It covers two situation: If the last or
		 * the only block is (gdb) \n.
		 */
		nread_total += nread;
		/* Scanner expects a null terminated string */
		gdbbuf[nread_total] = '\0';
		/* Output is logged for debugging purposes */
		if (logger(gdbbuf, nread_total, 1) < 0)
			return -1;
		/* Reset the buffer head */
		nread_total = 0;

		return 0;
	}
	else {
		/* gdb/mi output are accumulated */
		nread_total += nread;
	}

	return nread;
}

int main_loop(gdbvim_t *gv_h)
{
	char inbuf[IN_BUF_SIZE];
	char gdbbuf[GDB_BUF_SIZE];
	char progbuf[PROG_BUF_SIZE];
	char gdb_cmd_buf[GDB_CMD_SIZE];
	struct pollfd fds[3];
	gdb_state_t gdbstatus = GDB_STATE_CLI;
	gdb_cmd_type_t prev_cmd_type = GDB_CMD_CLI;
	int nread;
	int ret;

	/* The descriptors to be listened */
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	fds[1].fd = gv_h->gdb_ptym;
	fds[1].events = POLLIN;
	fds[2].fd = gv_h->prog_ptym;
	fds[2].events = POLLIN;

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
			if (inbuf[0] == '-') { /* gdb/mi command */
				inbuf[nread] = '\0';
				sprintf(gdb_cmd_buf, "interpreter mi %s", inbuf);
				prev_cmd_type = GDB_CMD_MI;
				gdbstatus = GDB_STATE_MI;
				write(gv_h->gdb_ptym,
				      gdb_cmd_buf,
				      strlen(gdb_cmd_buf));
			}
			else if (inbuf[0] == '\n') { /* previous command */
				if (prev_cmd_type == GDB_CMD_MI)
					gdbstatus = GDB_STATE_MI;
				else
					gdbstatus = GDB_STATE_CLI;
				write(gv_h->gdb_ptym, inbuf, nread);
			}
			else { /* gdb/cli command */
				prev_cmd_type = GDB_CMD_CLI;
				gdbstatus = GDB_STATE_CLI;
				write(gv_h->gdb_ptym, inbuf, nread);
			}
		}
		if (fds[1].revents == POLLIN) {
			/*
			 * gdb/mi output. Two things should be done.
			 * 1. gdb/mi output is converted to the gdb/cli form.
			 * 2. file and line information is obtained to have
			 * Vim show the file and line in question by using
			 * signs.
			 */
			if (gdbstatus == GDB_STATE_CLI) {
				nread = read(fds[1].fd, gdbbuf, GDB_BUF_SIZE);
				write(STDOUT_FILENO, gdbbuf, nread);
			}
			else { /* GDB_MI_STATE */
				/* Get it as a block */
				if (!get_mi_output(fds[1].fd, gdbbuf)) {
					if (handle_mi_output(gdbbuf) ==
					    GDB_MI_CMD_COMPLETED)
						gdbstatus = GDB_STATE_CLI;
					else
						gdbstatus = GDB_STATE_MI;
				}
			}
		}
		if (fds[2].revents == POLLIN) {
			nread = read(fds[2].fd, progbuf, PROG_BUF_SIZE);
			write(STDOUT_FILENO, progbuf, nread);
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
	gdbvim_t *gv_h;
	struct termios stermios;
	char gdb_args[GDB_ARGS_SIZE];
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

	//FIXME: Do we have to close an open pseudo terminal?
	/* Pseudo terminal for the program being debugged */
	ret = openpty(&gv_h->prog_ptym, &gv_h->prog_ptys, NULL, NULL, NULL);
	if (ret < 0) {
		fprintf(stderr, "No pseudo tty left\n");
		goto err_out;
		return -1;
	}
	/*
	 * We pass prog_tty to gdb as an argument because it is
	 * easy to handle. If we give it as a command right after
	 * gdb is started, then gdb welcome msg and the answer to
	 * this command is intermixed. gdb command prompt shows
	 * this annoying output.
	 */
	sprintf(gdb_args, "--tty=%s", ptsname(gv_h->prog_ptys));

	/* Child is created with a pseudo controlling terminal */
	gv_h->gdb_pid = forkpty(&gv_h->gdb_ptym, NULL, NULL, NULL);
	if (gv_h->gdb_pid < 0) {
		fprintf(stderr, "Cannot fork\n");
		perror(__FUNCTION__);
		goto err_out;
		return -1;
	}
	else if (gv_h->gdb_pid == 0) {	/* Child */
		tcgetattr(STDIN_FILENO, &stermios);
		/* Turn echo'ing off */
		stermios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
		/* Turn NL -> CR/NL output mapping off */
		stermios.c_oflag &= ~(ONLCR);
		tcsetattr(STDIN_FILENO, TCSANOW, &stermios);

		/*execlp(gdb_bin_name, gdb_bin_name, "--interpreter=mi", NULL);*/
		execlp(gdb_bin_name, gdb_bin_name, gdb_args, NULL);
	}
	/* Parent */

	/*
	 * Direction of transfers:
	 *	stdin -> gdb_ptym or prog_ptym,
	 *	stdout <- gdb_ptym or prog_ptym
	 */
	main_loop(gv_h);

err_out:
	free(gv_h);

	return 0;
}
