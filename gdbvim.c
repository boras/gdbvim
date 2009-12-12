#include <stdio.h>
#include <pty.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>

#define BUFF_SIZE		1024

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

int main(void)
{
	int ptym, pid;

	/* Child is created with a pseudo controlling terminal */
	pid = forkpty(&ptym, NULL, NULL, NULL);
	if (pid < 0) {
		fprintf(stderr, "Cannot fork\n");
		perror(__FUNCTION__);
		return -1;
	}
	else if (pid == 0) {	/* Child */
		execlp("gdb", "gdb", "--interpreter=mi", NULL);
	}

	/* Parent */
	loop(ptym); /* Copies stdin -> ptym, ptym -> stdin */

	return 0;
}
