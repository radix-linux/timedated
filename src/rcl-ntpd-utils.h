
/*
 * Copyright (C) 2023 Andrey V.Kosteltsev <kx@radix.pro>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __RCL_NTPD_UTILS_H__
#define __RCL_NTPD_UTILS_H__

#include "config.h"

#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <linux/rtc.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

#include <glib.h>
#include <glib-unix.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <locale.h>


#if !defined( NTPD_CONF )
#define NTPD_CONF "/etc/ntp.conf"
#endif

#if !defined( NTPD_RC )
#define NTPD_RC "/etc/rc.d/rc.ntpd"
#endif

extern gboolean  ntp_daemon_installed ( void );
extern gboolean  ntp_daemon_enabled   ( void );
extern gboolean  ntp_daemon_status    ( void );
extern gboolean  stop_ntp_daemon      ( void );
extern gboolean  start_ntp_daemon     ( void );
extern gboolean  disable_ntp_daemon   ( void );
extern gboolean  enable_ntp_daemon    ( void );


#endif /* __RCL_NTPD_UTILS_H__ */
