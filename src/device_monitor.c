/***********************************************************************/
/* device_mintor.h													 */
/* ---------														   */
/*		   GTKTerm Software										  */
/*					  (c) Julien Schmitt							 */
/*																	 */
/* ------------------------------------------------------------------- */
/*																	 */
/*   Purpose														   */
/*	  Monitor device to autoreconnect								*/
/*   Written by Kevin Picot - picotk27@gmail.com					   */
/*																	 */
/***********************************************************************/

#include <device_monitor.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <locale.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <interface.h>
#include <term_config.h>
#include <gudev/gudev.h>

#include "serial.h"
#include "interface.h"

extern struct configuration_port config;

#include <fcntl.h>

extern int serial_port_fd;

static guint reconnect_timer_id = 0;
static int reconnect_attempts = 0;

static gboolean reconnect_timer_cb(gpointer user_data)
{
	reconnect_attempts++;

	if (!config.autoreconnect_enabled) {
		reconnect_timer_id = 0;
		return G_SOURCE_REMOVE;
	}

	if (serial_port_fd != -1) {
		reconnect_timer_id = 0;
		return G_SOURCE_REMOVE;
	}

	if (config.port && config.port[0]) {
		int fd = open(config.port, O_RDWR | O_NOCTTY | O_NDELAY);
		if (fd != -1) {
			close(fd);
			interface_open_port_quiet();
			if (serial_port_fd != -1) {
				reconnect_timer_id = 0;
				return G_SOURCE_REMOVE;
			}
		}
	}

	if (reconnect_attempts >= 15) {
		reconnect_timer_id = 0;
		return G_SOURCE_REMOVE;
	}

	return G_SOURCE_CONTINUE;
}

static void dbg_log(const char *fmt, ...)
{
	FILE *f = fopen("/tmp/gtkterm_debug.log", "a");
	if (!f) return;
	va_list args;
	va_start(args, fmt);
	vfprintf(f, fmt, args);
	va_end(args);
	fclose(f);
}

static inline void device_monitor_status(const bool connected)
{
	dbg_log("device_monitor_status: connected=%d, config.port=%s\n",
	        connected, config.port);
	if (connected) {
		if (reconnect_timer_id != 0) {
			g_source_remove(reconnect_timer_id);
			reconnect_timer_id = 0;
		}
		reconnect_attempts = 0;
		if (!config.autoreconnect_enabled) {
			return;
		}
		int fd = open(config.port, O_RDWR | O_NOCTTY | O_NDELAY);
		dbg_log("device_monitor_status: initial open test on %s returned fd=%d\n", config.port, fd);
		if (fd != -1) {
			close(fd);
			interface_open_port_quiet();
			dbg_log("device_monitor_status: interface_open_port_quiet called, serial_port_fd=%d\n", serial_port_fd);
		} else {
			dbg_log("device_monitor_status: open failed, starting retry timer\n");
			reconnect_timer_id = g_timeout_add(200, reconnect_timer_cb, NULL);
		}
	} else {
		if (reconnect_timer_id != 0) {
			g_source_remove(reconnect_timer_id);
			reconnect_timer_id = 0;
		}
		interface_close_port();
		dbg_log("device_monitor_status: interface_close_port called\n");
	}
}

static inline void device_monitor_handle(const char *action)
{
	dbg_log("device_monitor_handle: action=%s\n", action);
	if (strcmp(action, "remove") == 0)
		device_monitor_status(false);
	else if (strcmp(action, "add") == 0)
		device_monitor_status(true);
}

static const gchar *get_devnode(GUdevDevice *device)
{
	const gchar *node = g_udev_device_get_device_file(device);
	if (!node)
		node = g_udev_device_get_property(device, "DEVNAME");
	return node;
}

void event_udev(GUdevClient *client, const gchar *action, GUdevDevice *device)
{
	if (!device || !action)
		return;

	const gchar *devfile = g_udev_device_get_device_file(device);
	const gchar *devprop = g_udev_device_get_property(device, "DEVNAME");
	const gchar *subsys = g_udev_device_get_subsystem(device);
	const gchar *name = g_udev_device_get_name(device);

	dbg_log("event_udev: action=%s, devfile=%s, devprop=%s, subsys=%s, name=%s, config.port=%s\n",
	        action ? action : "null",
	        devfile ? devfile : "null",
	        devprop ? devprop : "null",
	        subsys ? subsys : "null",
	        name ? name : "null",
	        config.port ? config.port : "null");

	const gchar *devnode = get_devnode(device);
	if (!devnode)
		return;

	if (strcmp(devnode, config.port) == 0)
		device_monitor_handle(action);
	else if (devprop && strcmp(devprop, config.port) == 0)
		device_monitor_handle(action);
	else if (name && config.port && strstr(config.port, name) != NULL)
		device_monitor_handle(action);
}

extern void device_monitor_start(void)
{
	dbg_log("device_monitor_start called for config.port=%s\n", config.port);
	const gchar *const subsystems[] = {NULL, NULL};

	/* Initial check */
	GUdevClient *udev_client = g_udev_client_new(subsystems);

	if (g_udev_client_query_by_device_file(udev_client, config.port) == NULL) {
		dbg_log("device_monitor_start: query_by_device_file NULL\n");
		device_monitor_status(false);
	} else {
		dbg_log("device_monitor_start: query_by_device_file FOUND\n");
		device_monitor_status(true);
	}

	/* Monitor device */
	g_signal_connect(G_OBJECT(udev_client), "uevent",
	                 G_CALLBACK(event_udev), NULL);
}
