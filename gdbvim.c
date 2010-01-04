#include <stdio.h>
#include <pty.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <termios.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
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

/* Function definitions */
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

static gdb_state_t gdbstatus = GDB_STATE_CLI;
static int gdb_ptym;

/* Strip whitespace(s) from the start and end of a str */
char *stripws(char *str)
{
	char *head = str;
	char *tail = &str[strlen(str)];

	/* If tab completion is active, str will not contain '\t' */
	/* "  \t\t return  \t" */
	while (*head == ' ' || *head == '\t')
		head++;
	if (*head == '\0') { /* head reached the end of the list */
		/* Consists of only whitespace(s) */
		return head;
	}
	while (*--tail == ' ' || *tail == '\t')
		;

	/* head points to 'r', tail points to 'n' */
	*++tail = '\0';

	return head;
}

/* Called when EOF or newline is encountered */
void handle_gdb_input(char *line)
{
	static gdb_cmd_type_t prev_cmd_type = GDB_CMD_CLI;
	char gdb_cmd_buf[GDB_CMD_SIZE];
	char *cmd;

	/* Check if it is EOF: C-d */
	if (line) {
		/*
		 * Remove leading and trailing whitespace(s) from
		 * the line. If there is anything left, add it to
		 * the history list. It is also checked if only
		 * newline is entered to prevent it from ending up
		 * in the list.
		 */
		if (*line && *(cmd = stripws(line)))
			add_history(cmd);

		/*
		 * When readline library gives the line to the application,
		 * it strips newline. However, gdb commands should be ended
		 * with a newline so we set it again.
		 */
		if (!*line || !*cmd) { /* previous command */
			/* readline gives: line = "" */
			if (prev_cmd_type == GDB_CMD_MI)
				gdbstatus = GDB_STATE_MI;
			else
				gdbstatus = GDB_STATE_CLI;
			write(gdb_ptym, "\n", 1);
		}
		else if (*cmd == '-') { /* gdb/mi command */
			/* readline gives: -exec-next'\0' */
			prev_cmd_type = GDB_CMD_MI;
			gdbstatus = GDB_STATE_MI;
			sprintf(gdb_cmd_buf, "interpreter mi %s\n", cmd);
			write(gdb_ptym, gdb_cmd_buf, strlen(gdb_cmd_buf));
		}
		else { /* gdb/cli command */
			/* readline gives: line = next'\0' */
			prev_cmd_type = GDB_CMD_CLI;
			gdbstatus = GDB_STATE_CLI;
			sprintf(gdb_cmd_buf, "%s\n", cmd);
			write(gdb_ptym, gdb_cmd_buf, strlen(gdb_cmd_buf));
		}

		free(line);
	}
	else { /* EOF */
		//FIXME: Handle EOF
	}
}

void handle_user_input(gdbvim_t *gv_h, char *inbuf)
{
	int nread;

	/* gdb or prog input */
	if (gdbstatus == GDB_STATE_CLI) /* input for gdb */
		rl_callback_read_char();
	else { /* input for prog */
		/*
		 * The only possibility for a program to get input is the
		 * state of already being run, and a program is
		 * in that condition only if execution control commands
		 * are being executed. That means that gdb must be operating
		 * in the mi state.
		 */
		nread = read(STDIN_FILENO, inbuf, IN_BUF_SIZE);
		write(gv_h->prog_ptym, inbuf, nread);
	}
}

int main_loop(gdbvim_t *gv_h)
{
	char inbuf[IN_BUF_SIZE];
	char gdbbuf[GDB_BUF_SIZE];
	char progbuf[PROG_BUF_SIZE];
	struct pollfd fds[3];
	int nread;
	int ret;

	/* The descriptors to be listened */
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	fds[1].fd = gdb_ptym;
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
			 * gdb/cli or prog input. Commands are read by
			 * readline library. Tab completion should be there.
			 * Afterwards they are converted to gdb/mi input
			 * commands.
			 */
			handle_user_input(gv_h, inbuf);
		}
		if (fds[1].revents == POLLIN) {
			/*
			 * gdb output. Two things should be done.
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
		if (fds[2].revents == POLLIN) { /* prog output */
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

void turn_echo_off(int fd)
{
	struct termios stermios;

	/* Turn echo'ing off */
	tcgetattr(fd, &stermios);
	stermios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	tcsetattr(fd, TCSANOW, &stermios);
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

	/* Initialize readline */
	rl_bind_key('\t', rl_insert);
	rl_already_prompted = 1;
	rl_callback_handler_install("(gdb) ", handle_gdb_input);

	//FIXME: Close an open pseudo terminal
	/* Pseudo terminal for the program being debugged */
	ret = openpty(&gv_h->prog_ptym, &gv_h->prog_ptys, NULL, NULL, NULL);
	if (ret < 0) {
		fprintf(stderr, "No pseudo tty left\n");
		goto err_out;
		return -1;
	}
	/*
	 * The below line is disabled because readline turns off gdbvim's
	 * echo'ing. If the char goes to readline, there is no problem it
	 * outputs. However, if it goes to program, unless the below line
	 * is commented out, there is no way to see the entered characters
	 * in the gdbvim display. We may change this in the future if a
	 * different pseudo terminal is assigned to readline.
	 */
	/*turn_echo_off(gv_h->prog_ptym);*/
	/*
	 * We pass prog_tty to gdb as an argument because it is
	 * easy to handle. If we give it as a command right after
	 * gdb is started, then gdb welcome msg and the answer to
	 * this command is intermixed. gdb command prompt shows
	 * this annoying output.
	 */
	sprintf(gdb_args, "--tty=%s", ptsname(gv_h->prog_ptys));

	/* Child is created with a pseudo controlling terminal */
	gv_h->gdb_pid = forkpty(&gdb_ptym, NULL, NULL, NULL);
	if (gv_h->gdb_pid < 0) {
		fprintf(stderr, "Cannot fork\n");
		perror(__FUNCTION__);
		goto err_out;
		return -1;
	}
	else if (gv_h->gdb_pid == 0) {	/* Child */
		turn_echo_off(STDIN_FILENO);
		/* Turn NL -> CR/NL output mapping off */
		tcgetattr(STDIN_FILENO, &stermios);
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
