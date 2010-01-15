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
#define OUT_BUF_SIZE	256
#define GDB_BUF_SIZE	1024
#define PROG_BUF_SIZE	1024
#define GDB_CMD_SIZE	256
#define GDB_ARGS_SIZE	64

/* Extern declarations */
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *yy_str);
extern void yy_delete_buffer(YY_BUFFER_STATE b);
extern void yy_flush_buffer(YY_BUFFER_STATE b);

/* static global variable defitions */
static gdb_state_t gdbstatus = GDB_STATE_CLI;
static key_type_t prev_key = KEY_OTHER;
static int gdb_out = GDB_OUT_ECHO_TRIMMED;

static int gdb_ptym;
static int readline_ptym, readline_ptys;
static int prog_ptym, prog_ptys;

static struct termios save_termios;
static int gdb_cmd_len;

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

void turn_echo_off(int fd)
{
	struct termios stermios;

	/* Turn echo'ing off */
	tcgetattr(fd, &stermios);
	stermios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	tcsetattr(fd, TCSANOW, &stermios);
}

void turn_echo_on(int fd)
{
	struct termios stermios;

	/* Turn echo'ing on */
	tcgetattr(fd, &stermios);
	stermios.c_lflag |= ECHO | ECHOE | ECHOK | ECHONL;
	tcsetattr(fd, TCSANOW, &stermios);
}

/*
 * There might be four different situation:
 *	1. "erase line echo" coming from tab completion request.
 *	2. "erase line + cmd echo" coming from nearly every gdb cmd be it
 *	cli or mi.
 *	3. "messages" from gdb without associated gdb cmd. Afaik, the only
 *	one is gdb version output which shows up at the start of gdb.
 *	4. "cmd echo" coming from answers to gdb querying cmds. This may
 *	change in the future.
 */
static char *kill_echo(char *char_ptr, int cmd_echo)
{
	char *ans_ptr = char_ptr;

	/*
	 * erase_line brings two responses: \a (bell ring) if there is no
	 * input or an escape sequence if there are some input. Escape
	 * sequence has two forms: if the command is short, then some \b
	 * (backspace) which equals to the length of the cmd and a erase
	 * line esc sequence; otherwise a carriage return and some movement
	 * then an erase line escape sequence. Some examples can make a long
	 * story shorter.
	 *
	 * prompt = (gdb) next
	 * req = erase line
	 * ans = \b\b\b\b\033[K
	 *
	 * prompt = (gdb) file[space]
	 * req = erase line
	 * ans = \b\b\b\b\b\033[K
	 *
	 * prompt = (gdb) add-file-symbol
	 * req = erase line
	 * ans = \r\033[C\033[C\033[C\033[C\033[C\033[C\033[K

	 * \b -> backspace
	 * \r -> move to beginning of line
	 * \033[C -> forward a character. Six times equal to (gdb)[space]
	 * \033[K -> clear from cursor to end of screen in this case
	 *
	 * As it seems they all share a common suffix \033[K.
	 */
	if (*ans_ptr == '\a')
		ans_ptr++;
	else {
		do {
			ans_ptr = strchr(ans_ptr, '\033');
			if (!ans_ptr)
				break;
			ans_ptr++;
			if (!strncmp(ans_ptr, "[K", 2)) {
				ans_ptr += 2;
				break;
			}
			ans_ptr += 2;
		} while (*ans_ptr);
	}

	/* Buffer does not contain any erase line output */
	if (!ans_ptr || !*ans_ptr)
		ans_ptr = char_ptr;

	if (cmd_echo && gdb_out == GDB_OUT_ECHO_INCLUDED)
		ans_ptr += gdb_cmd_len;

	if (ans_ptr != char_ptr)
		return ans_ptr;
	return char_ptr;
}

static void erase_line(int fd)
{
	char c = 0x15;

	write(fd, &c, 1);
}

int tab_completion(int count, int key)
{
	char gdb_cmd_buf[GDB_CMD_SIZE];
	int nread;

	if (prev_key == KEY_TAB)
		write(gdb_ptym, "\t", 1);
	else {
		erase_line(gdb_ptym);
		sprintf(gdb_cmd_buf, "%s\t", rl_line_buffer);
		write(gdb_ptym, gdb_cmd_buf, strlen(gdb_cmd_buf));
	}
	gdbstatus = GDB_STATE_COMPLETION;
	prev_key = KEY_TAB;

	return 0;
}

gdb_mi_cmd_state_t handle_mi_output(char *gdbbuf)
{
	gdb_mi_cmd_state_t mi_cmd_status;
	char *ans_ptr;

	if (gdb_out == GDB_OUT_ECHO_INCLUDED) {
		/* This means echo'ing will be cut */
		ans_ptr = kill_echo(gdbbuf, 1);
		gdb_out = GDB_OUT_ECHO_TRIMMED;
	}
	else
		ans_ptr = gdbbuf;

	if (!create_mi_parsetree(ans_ptr)) {
		/* There is a valid parse tree */
		mi_cmd_status = parse_mi_parsetree();
		destroy_gdbmi_output();
		gdbmi_out_ptr = NULL;

		return mi_cmd_status;
	}
	/* Parse tree could not be created */

	return GDB_MI_CMD_COMPLETED;
}

void handle_cli_output(char *gdbbuf)
{
	char *ans_ptr = kill_echo(gdbbuf, 1);

	write(STDOUT_FILENO, ans_ptr, strlen(ans_ptr));
	gdb_out = GDB_OUT_ECHO_TRIMMED;
}

int parse_pre_cmd_output(char *cmd_list_buf, char **cmd)
{
	char *cmd_iter;
	int cmd_len;

	write(STDOUT_FILENO, cmd_list_buf, strlen(cmd_list_buf));

	/* not a valid cmd: (gdb)[space] */
	if (!(cmd_iter = strchr(cmd_list_buf, '\n')))
		return 2;

	cmd_iter++;

	/* unambiguous cmd: break\n(gdb)[space] */
	if (!strncmp(cmd_iter, "(gdb) ", 6)) {
		cmd_len = cmd_iter - cmd_list_buf;
		if (!(*cmd = (char *)malloc(cmd_len))) {
			fprintf(stderr, "Cannot allocate memory\n");
			return 3;
		}
		strncpy(*cmd, cmd_list_buf, cmd_len);
		(*cmd)[cmd_len - 1] = '\0';
		return 0;
	}
	/* ambiguous cmd: backtrace\nbreak\nbt\n(gdb)[space] */

	return 1;
}

void handle_pre_cmd_output(char *gdbbuf)
{
	char *ans_ptr = kill_echo(gdbbuf, 1);
	char *cmd;

	if (!parse_pre_cmd_output(ans_ptr, &cmd))
		free(cmd);

	gdbstatus = GDB_STATE_CLI;
	gdb_out = GDB_OUT_ECHO_TRIMMED;
}

void handle_completion_output(char *gdbbuf)
{
	char *char_ptr, *ans_ptr;
	int nread;

	nread = read(gdb_ptym, gdbbuf, GDB_BUF_SIZE);
	gdbbuf[nread] = '\0';
	ans_ptr = kill_echo(gdbbuf, 0);
	if (ans_ptr == (gdbbuf + nread)) {
		/* Answer does not contain echo */
		ans_ptr = gdbbuf;
	}

	if (*ans_ptr == '\a')
		write(STDOUT_FILENO, "\a", 1);
	else if ((char_ptr = strchr(ans_ptr, '\a')) != NULL) {
		if (!*++char_ptr) /* \a is at the end. no change */
			write(STDOUT_FILENO, "\a", 1);
		else {
			rl_insert_text(char_ptr);
			rl_redisplay();
			write(STDOUT_FILENO, "\a", 1);
		}
	}
	else if (*ans_ptr != '\n') { /* completion is successful */
			rl_delete_text(0, rl_end);
			rl_point = 0;
			rl_insert_text(ans_ptr);
			rl_redisplay();
	}
	else
		write(STDOUT_FILENO, ans_ptr, strlen(ans_ptr));

	gdbstatus = GDB_STATE_CLI;
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
		// FIXME: Why we need this?
		if (!*line || !*cmd) { /* previous command */
			/* readline gives: line = "" */
			if (prev_cmd_type == GDB_CMD_MI)
				gdbstatus = GDB_STATE_MI;
			else
				gdbstatus = GDB_STATE_CLI;
			erase_line(gdb_ptym);
			/* newline produces n\n as echo */
			gdb_cmd_len = 1;
			write(gdb_ptym, "\n", 1);
		}
		else if (*cmd == '-') { /* gdb/mi command */
			/* readline gives: -exec-next'\0' */
			prev_cmd_type = GDB_CMD_MI;
			gdbstatus = GDB_STATE_MI;
			erase_line(gdb_ptym);
			sprintf(gdb_cmd_buf, "interpreter mi %s\n", cmd);
			gdb_cmd_len = strlen(gdb_cmd_buf);
			write(gdb_ptym, gdb_cmd_buf, gdb_cmd_len);
		}
		else { /* gdb/cli command */
			/* readline gives: line = next'\0' */
			prev_cmd_type = GDB_CMD_CLI;
			gdbstatus = GDB_STATE_CLI;
			erase_line(gdb_ptym);
			sprintf(gdb_cmd_buf, "%s\n", cmd);
			gdb_cmd_len = strlen(gdb_cmd_buf);
			write(gdb_ptym, gdb_cmd_buf, gdb_cmd_len);
		}
		gdb_out = GDB_OUT_ECHO_INCLUDED;

		free(line);
	}
	else { /* EOF */
		//FIXME: Handle EOF
	}
}

void handle_user_input(char *inbuf)
{
	int nread;

	/* gdb or prog input */
	if (gdbstatus == GDB_STATE_CLI) { /* input for gdb */
		/*rl_callback_read_char();*/
		nread = read(STDIN_FILENO, inbuf, IN_BUF_SIZE);
		if (*inbuf != '\t')
			prev_key = KEY_OTHER;
		write(readline_ptym, inbuf, nread);
	}
	else { /* input for prog */
		/*
		 * The only possibility for a program to get input is the
		 * state of already being run, and a program is
		 * in that condition only if execution control commands
		 * are being executed. That means that gdb must be operating
		 * in the mi state.
		 */
		nread = read(STDIN_FILENO, inbuf, IN_BUF_SIZE);
		write(prog_ptym, inbuf, nread);
	}
}

int get_gdb_output(char *gdbbuf, char *pattern)
{
	static int nread_total = 0;
	int nread;
	char *next_slice;
	int plen = strlen(pattern);

	next_slice = gdbbuf + nread_total;
	nread = read(gdb_ptym, next_slice, GDB_BUF_SIZE);
	if (!strncmp(&next_slice[nread - plen], pattern, plen)) {
		/*
		 * It covers two situation: If the last or
		 * the only block is pattern.
		 */
		nread_total += nread;
		/* null terminated */
		gdbbuf[nread_total] = '\0';
		/* logged for debugging purposes */
		if (logger(gdbbuf, nread_total, 1) < 0)
			return -1;
		/* Reset the buffer head */
		nread_total = 0;

		return 0;
	}
	else {
		/* gdb output is accumulated */
		nread_total += nread;
	}

	return nread;
}

int main_loop(void)
{
	char inbuf[IN_BUF_SIZE];
	char outbuf[OUT_BUF_SIZE];
	char gdbbuf[GDB_BUF_SIZE];
	char progbuf[PROG_BUF_SIZE];
	struct pollfd fds[5];
	int nread;
	int ret;

	/* The descriptors to be listened */
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	fds[1].fd = gdb_ptym;
	fds[1].events = POLLIN;
	fds[2].fd = prog_ptym;
	fds[2].events = POLLIN;

	fds[3].fd = readline_ptys;
	fds[3].events = POLLIN;
	fds[4].fd = readline_ptym;
	fds[4].events = POLLIN;

	/* The main loop */
	while (1) {
		/* Wait for indefinetely */
		if ((ret = poll(fds, 5, -1)) <= 0) {
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
			handle_user_input(inbuf);
		}

		if (fds[3].revents == POLLIN) /* readline input */
			rl_callback_read_char();
		if (fds[4].revents == POLLIN) { /* readline output */
			nread = read(fds[4].fd, outbuf, OUT_BUF_SIZE);
			write(STDOUT_FILENO, outbuf, nread);
		}

		if (fds[2].revents == POLLIN) { /* prog output */
			nread = read(fds[2].fd, progbuf, PROG_BUF_SIZE);
			write(STDOUT_FILENO, progbuf, nread);
		}

		if (fds[1].revents == POLLIN) { /* gdb output */
			/*
			 * gdb output. Two things should be done.
			 * 1. gdb/mi output is converted to the gdb/cli form.
			 * 2. file and line information is obtained to have
			 * Vim show the file and line in question by using
			 * signs.
			 */
			if (gdbstatus == GDB_STATE_CLI) {
				if (!get_gdb_output(gdbbuf, "(gdb) "))
					handle_cli_output(gdbbuf);
			}
			else if (gdbstatus == GDB_STATE_COMPLETION)
				handle_completion_output(gdbbuf);
			else if (gdbstatus == GDB_STATE_CHECK_CMD) {
				if (!get_gdb_output(gdbbuf, "(gdb) "))
					handle_pre_cmd_output(gdbbuf);
			}
			else { /* GDB_STATE_MI */
				/*
				 * Before giving the buffer for parsing, we must
				 * ensure that it contains valid gdb/mi output.
				 * If there ara some data and we pass it
				 * immediately there is a high chance that it
				 * has not been complete yet. So for every read
				 * we should compare its last 7 characters to
				 * (gdb) \n. (gdb) \n notation represents the
				 * end of gdb/mi output. The space between ')'
				 * and '\n' is not mentioned in the documentation
				 * but we verified that it is there.
				 */
				if (!get_gdb_output(gdbbuf, "(gdb) \n")) {
					if (handle_mi_output(gdbbuf) ==
					    GDB_MI_CMD_COMPLETED)
						gdbstatus = GDB_STATE_CLI;
					else
						gdbstatus = GDB_STATE_MI;
				}
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

int tty_cbreak(int fd)
{
	struct termios buf;

	if (tcgetattr(fd, &save_termios) < 0)
		return -1;

	buf = save_termios;
	buf.c_lflag &= ~(ECHO | ICANON);
	buf.c_iflag &= ~(ICRNL | INLCR);
	buf.c_cc[VMIN] = 1;
	buf.c_cc[VTIME] = 0;

	buf.c_cc[VLNEXT] = _POSIX_VDISABLE;

#if defined(VDSUSP) && defined(_POSIX_VDISABLE)
	buf.c_cc[VDSUSP] = _POSIX_VDISABLE;
#endif

	/* disable flow control; let ^S and ^Q through to pty */
	buf.c_iflag &= ~(IXON|IXOFF);
#ifdef IXANY
	buf.c_iflag &= ~IXANY;
#endif

	if (tcsetattr(fd, TCSAFLUSH, &buf) < 0)
		return -1;

	return 0;
}

/* Sets the terminal attributes back to their previous state */
void tty_reset(int fd)
{
	tcsetattr(fd, TCSAFLUSH, &save_termios);
}

void sigint (int s)
{
	tty_reset(STDIN_FILENO);
	close(readline_ptym);
	close(readline_ptys);
	close(gdb_ptym);

	exit(0);
}

static void custom_deprep_term_function(void) {}

static int init_readline (void)
{
	FILE *input, *output;
	int ret;

	ret = openpty(&readline_ptym, &readline_ptys, NULL, NULL, NULL);
	if (ret == -1)
		return -1;

	if (!(input = fdopen(readline_ptys, "r")))
		return -1;

	if (!(output = fdopen(readline_ptys, "w")))
		return -1;
	rl_instream = input;
	rl_outstream = output;

	/*rl_bind_key('\t', rl_insert);*/
	rl_bind_key('\t', tab_completion);

	rl_already_prompted = 1;
	rl_callback_handler_install("(gdb) ", handle_gdb_input);

	/* Set the terminal type to dumb so the output of readline can be
	* understood by tgdb */
	/*if (rl_reset_terminal("dumb") == -1)*/
		/*return -1;*/

	/*
	 * For some reason, readline can not deprep the terminal.
	 * However, it doesn't matter because no other application is
	 * working on the terminal besides readline.
	 */
	rl_deprep_term_function = custom_deprep_term_function;

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

	/* Initialize readline */
	if (init_readline() < 0)
		return -1;

	if (tty_cbreak(STDIN_FILENO) < 0)
		return -1;

	signal(SIGINT, sigint);

	//FIXME: Close an open pseudo terminal
	/* Pseudo terminal for the program being debugged */
	ret = openpty(&prog_ptym, &prog_ptys, NULL, NULL, NULL);
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
	sprintf(gdb_args, "--tty=%s", ptsname(prog_ptys));

	/* Child is created with a pseudo controlling terminal */
	gv_h->gdb_pid = forkpty(&gdb_ptym, NULL, NULL, NULL);
	if (gv_h->gdb_pid < 0) {
		fprintf(stderr, "Cannot fork\n");
		perror(__FUNCTION__);
		goto err_out;
		return -1;
	}
	else if (gv_h->gdb_pid == 0) {	/* Child */
		/*turn_echo_off(gdb_ptym);*/
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
	main_loop();

	tty_reset(STDIN_FILENO);
err_out:
	free(gv_h);

	return 0;
}
