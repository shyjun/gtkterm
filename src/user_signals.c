#include <glib-unix.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include "interface.h"
#include "logging.h"

static int rtmin_pipe[2] = { -1, -1 };

static gboolean handle_usr1(gpointer user_data)
{
	interface_open_port();
	return G_SOURCE_CONTINUE;
}

static gboolean handle_usr2(gpointer user_data)
{
	interface_close_port();
	return G_SOURCE_CONTINUE;
}

static gboolean handle_rtmin(gpointer user_data)
{
	clear_display();
	logging_clear();
	return G_SOURCE_CONTINUE;
}

static void posix_rtmin_handler(int sig)
{
	int saved_errno = errno;
	unsigned char byte = 0;
	if (rtmin_pipe[1] != -1) {
		(void)write(rtmin_pipe[1], &byte, 1);
	}
	errno = saved_errno;
}

static gboolean handle_pipe_read(gint fd, GIOCondition condition, gpointer user_data)
{
	unsigned char byte;
	while (read(fd, &byte, 1) > 0);
	handle_rtmin(NULL);
	return G_SOURCE_CONTINUE;
}

void user_signals_catch(void)
{
	struct sigaction sa;

	/* Ignore SIGHUP so GTKTerm continues running if its launching terminal window is closed */
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	g_unix_signal_add(SIGUSR1, (GSourceFunc) handle_usr1, NULL);
	g_unix_signal_add(SIGUSR2, (GSourceFunc) handle_usr2, NULL);

	/* SIGRTMIN is not supported by g_unix_signal_add in older GLib versions.
	 * We use the self-pipe pattern instead. */
	if (pipe2(rtmin_pipe, O_CLOEXEC | O_NONBLOCK) == 0) {
		g_unix_fd_add(rtmin_pipe[0], G_IO_IN, handle_pipe_read, NULL);

		sa.sa_handler = posix_rtmin_handler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART;
		sigaction(SIGRTMIN, &sa, NULL);
	}
}
