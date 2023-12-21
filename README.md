
# [Timedate Daemon](https://cgit.radix.pro/radix/timedated.git/)

**TimeDate** Daemon is a system service that can be used to control the system time
and related settings.

This is the replacement of *systemd* service that control the **org.freedesktop.timedate1**
*D-Bus interface* for GNU Linux distributions which does not have a *systemd*.

You can find specification at: [**Freedesktop.org**](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.timedate1.html)

**TimeDate** Daemon does not support interactive parameter which can be used to control
whether PolKit should interactively ask the user for authentication credentials if required.
Instead of interactive way users permissions can be set by PolKit rules in the
*/usr/share/polkit-1/rules.d/org.freedesktop.timedate1.rules* file. For example,
a system administrator can add Desktop-users into **wheel** group to give them rights
to access the org.freedesktop.timedate1 D-Bus interface.

## Requirements:

 | Package           |      | min Version  |
 | :---              | :--: | :---         |
 | glib-2.0          |  >=  |  2.76.0      |
 | gobject-2.0       |  >=  |  2.76.0      |
 | gio-2.0           |  >=  |  2.76.0      |
 | polkit-gobject-1  |  >=  |  123         |
 | libpcre2-8        |  >=  |  10.36       |
 | dbus              |  >=  |  1.13.18     |

## How to Build:

```Bash
 meson setup --prefix=/usr . ..
 ninja
 ninja install
```

## Supported Distributions:

 - [Radix cross Linux](https://radix.pro)
 - [Slackware](http://www.slackware.com)
   (needed litle changes in timeconfig script)

For other systems the special implementation of NTP daemon control should be developed.


## TODO:


  - timedatectl (simply it can be writen in Bash).


# LICENSE:

  GNU GENERAL PUBLIC LICENSE Version 2, June 1991
