## Copyright (c) 2008  Daniel Elstner  <daniel.kitta@gmail.com>
##
## This file is part of danielk's Autostuff.
##
## danielk's Autostuff is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License as published
## by the Free Software Foundation; either version 2 of the License, or (at
## your option) any later version.
##
## danielk's Autostuff is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
## or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
## for more details.
##
## You should have received a copy of the GNU General Public License along
## with danielk's Autostuff; if not, write to the Free Software Foundation,
## Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#serial 20081109

## DK_MODULE_CURSES()
##
## Determine the compiler and linker flags required to use the system's
## curses library, preferably with support for wide characters enabled.
## The output variables CURSES_CFLAGS and CURSES_LIBS are substituted.
##
## Currently, only the ncurses implementation is checked for.
##
AC_DEFUN_ONCE([DK_MODULE_CURSES],
[dnl
AC_PATH_PROGS([CURSES_CONFIG], [ncursesw5-config ncurses5-config])
AS_IF([test "x$CURSES_CONFIG" = x], [AC_MSG_ERROR([[
No ncurses configuration script could be found.  Please install
your distribution's development package of the ncurses library.
Choose the version with wide character support if it is packaged
separately.
]])])
CURSES_CFLAGS=`"$CURSES_CONFIG" --cflags 2>&AS_MESSAGE_LOG_FD` && \
CURSES_LIBS=`"$CURSES_CONFIG" --libs 2>&AS_MESSAGE_LOG_FD` || {
AC_MSG_FAILURE([[
Execution of the ncurses configuration script failed.
]])
}
AC_SUBST([CURSES_CFLAGS])
AC_SUBST([CURSES_LIBS])
])
