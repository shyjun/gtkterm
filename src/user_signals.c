#include <glib-unix.h>
#include <gtk/gtk.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include "interface.h"

static gint signal_fd = -1;
static GIOChannel *signal_channel = NULL;

static void clear_display_handler(int sig)
{
    char c = sig;
    if (signal_fd >= 0) {
        write(signal_fd, &c, 1);
    }
}

static gboolean handle_signal_io(GIOChannel *channel, GIOCondition condition, gpointer data)
{
    gchar buf[64];
    gsize bytes_read = 0;
    GError *error = NULL;

    while (TRUE) {
        GIOStatus status = g_io_channel_read_chars(channel, buf, sizeof(buf), &bytes_read, &error);
        if (status != G_IO_STATUS_NORMAL || bytes_read == 0) {
            break;
        }
        for (gsize i = 0; i < bytes_read; i++) {
            int sig = (unsigned char)buf[i];
            if (sig == SIGUSR1) {
                interface_open_port();
            } else if (sig == SIGUSR2) {
                interface_close_port();
            } else if (sig == SIGRTMIN) {
                clear_display();
            }
        }
    }

    return G_SOURCE_CONTINUE;
}

void user_signals_catch(void)
{
    struct sigaction sa;
    int fds[2];

    if (pipe(fds) == -1) {
        return;
    }

    signal_fd = fds[1];

    signal_channel = g_io_channel_unix_new(fds[0]);
    g_io_channel_set_encoding(signal_channel, NULL, NULL);
    g_io_channel_set_flags(signal_channel, G_IO_FLAG_NONBLOCK, NULL);
    g_io_add_watch(signal_channel, G_IO_IN, (GIOFunc)handle_signal_io, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = clear_display_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGRTMIN, &sa, NULL);
}
