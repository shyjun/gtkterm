/***********************************************************************/
/* baud.c                                                              */
/* -------                                                             */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Serial port baud rate setting                                  */
/*      In a separate file as <linux/termios.h> conflicts with	       */
/*	<termios.h>. A struct termios is not valid across a call to    */
/*      set_port_baudrate().                                           */
/*                                                                     */
/*      Returns 0 on error, otherwise the updated baud rate if the     */
/*      kernel reports it.                                             */
/*                                                                     */
/***********************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>

#include <config.h>

#define CHK(x) do { if (x) return 0; } while (0)

#ifdef HAVE_CFSETOBAUD

#include "serial.h"

unsigned int set_port_baudrate(unsigned int baud, int port_fd)
{
	struct termios tio;

	CHK(tcgetattr(port_fd, &tio));
	CHK(cfsetobaud(&tio, baud));
	CHK(cfsetibaud(&tio, baud));
	CHK(tcsetattr(port_fd, TCSANOW, &tio));
	CHK(tcgetattr(port_fd, &tio));
	return cfgetobaud(&tio);
}

#elif defined(HAVE_LINUX_TERMIOS_H)

#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#ifndef __struct_termios2_defined
#define __struct_termios2_defined
struct termios2 {
	unsigned int c_iflag;
	unsigned int c_oflag;
	unsigned int c_cflag;
	unsigned int c_lflag;
	unsigned char c_line;
	unsigned char c_cc[19];
	speed_t c_ispeed;
	speed_t c_ospeed;
};
#endif

#ifndef TCGETS2
# define TCGETS2 _IOR('t', 19, struct termios2)
#endif
#ifndef TCSETS2
# define TCSETS2 _IOW('t', 20, struct termios2)
#endif
#ifndef BOTHER
# ifdef __BOTHER
#  define BOTHER __BOTHER
# else
#  define BOTHER 0x80000000U
# endif
#endif
#ifndef CBAUD
# define CBAUD 0x100fU
#endif
#ifndef CIBAUD
# define CIBAUD 0x100f0000U
#endif

#include "serial.h"

#ifndef TCSETS2
# define termios2 termios
# ifdef __powerpc__
#  define TCGETS2 _IOR('t', 19, struct termios2)
#  define TCSETS2 _IOW('t', 20, struct termios2)
# else
#  define TCSETS2 TCSETS
#  define TCGETS2 TCGETS
# endif
#endif

unsigned int set_port_baudrate(unsigned int baud, int port_fd)
{
	struct termios2 tio;
	if (ioctl(port_fd, TCGETS2, &tio) < 0) {
		return 0;
	}

	tio.c_ispeed = tio.c_ospeed = baud;
	tio.c_cflag &= ~(CBAUD | CIBAUD);
	tio.c_cflag |= BOTHER;

	if (ioctl(port_fd, TCSETS2, &tio) < 0) {
		return 0;
	}
	if (ioctl(port_fd, TCGETS2, &tio) < 0) {
		return 0;
	}

	if (tio.c_ospeed == 0) {
		return baud;
	}
	return tio.c_ospeed;
}

#else  /* No baud_t interface, not Linux */

#include "serial.h"

int set_port_baudrate(unsigned int baud, int port_fd)
{
	struct termios tio;
	speed_t speed;

	if (speed_t_is_sane)
	{
		speed = baud;

		/* Check for type conversion errors */
		if (speed != baud)
			speed = B0;
	} else {
		speed = find_standard_baudrate(baud);
	}

	if (speed == B0)
	{
		errno = EINVAL;
		return -1;
	}

	CHK(tcgetattr(port_fd, &tio));
	CHK(cfsetospeed(&tio, speed));
	CHK(cfsetispeed(&tio, speed));
	CHK(tcsetattr(port_fd, TCSANOW, &tio));
	CHK(tcgetattr(port_fd, &tio));
	return speed_t_to_baud(cfgetospeed(&tio));
}

#endif
