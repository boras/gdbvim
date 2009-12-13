#include <stdio.h>
#include <pty.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <termios.h>

#define BUFF_SIZE	1024

int loop(int ptym)
{
	char buff[BUFF_SIZE];
	struct pollfd fds[2];
	int nread;
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
			nread = read(fds[0].fd, buff, BUFF_SIZE);
			write(ptym, buff, nread);
		}
		if (fds[1].revents == POLLIN) {
			nread = read(fds[1].fd, buff, BUFF_SIZE);
			write(STDIN_FILENO, buff, nread);
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
	loop(ptym); /* Copies stdin -> ptym, ptym -> stdin */

	return 0;
}
