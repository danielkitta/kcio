## Copyright (c) 2008-2009  Daniel Elstner <daniel.kitta@gmail.com>
##
## This file is part of KC-Mill.
##
## KC-Mill is free software: you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by the
## Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## KC-Mill is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
## See the GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License along
## with this program.  If not, see <http://www.gnu.org/licenses/>.

AC_INIT([KC-Mill], [0.1], [daniel.kitta@gmail.com], [kcmill])
AC_PREREQ([2.60])

AC_CONFIG_SRCDIR([ui/kcmill.desktop.in])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.8 -Wall gnu no-define nostdinc check-news])
AC_ARG_VAR([ACLOCAL_FLAGS], [aclocal flags, e.g. -I <macro dir>])

AC_PROG_CC()
AC_PROG_CC_C99()
AM_PROG_CC_C_O()
AC_PROG_CXX()
AC_PROG_RANLIB()

AC_LANG_ASSERT([C])
AC_USE_SYSTEM_EXTENSIONS()
AC_SYS_POSIX_TERMIOS()
AS_IF([test "x$ac_cv_sys_posix_termios" != xyes], [AC_MSG_ERROR([[
The system does not support the POSIX termios interface.
]])])

IT_PROG_INTLTOOL([0.35])
AC_SUBST([GETTEXT_PACKAGE], [kcmill])
AM_GLIB_GNU_GETTEXT()

DK_MODULE_CURSES()
PKG_CHECK_MODULES([KCMILL_MODULES], [alsa >= 1.0 gtkmm-2.4 >= 2.12])
PKG_CHECK_MODULES([KCKEYB_MODULES], [gtkmm-2.4 >= 2.12])
PKG_CHECK_MODULES([KCREC_MODULES], [alsa >= 1.0])
DK_PKG_PATH_PROG([GTK_UPDATE_ICON_CACHE], [gtk+-2.0], [gtk-update-icon-cache])

AC_TYPE_SSIZE_T()
AC_TYPE_INT16_T()
AC_TYPE_UINT8_T()

DK_ARG_ENABLE_WARNINGS([LIBKC_WFLAGS],
                       [-Wall],
                       [-pedantic -Wall -Wextra])
AC_LANG([C++])
DK_ARG_ENABLE_WARNINGS([KCMILL_WXXFLAGS],
                       [-Wall],
                       [-DGDK_MULTIHEAD_SAFE -pedantic -Wall -Wextra],
                       [LIBSIGC PANGO G GDK GDK_PIXBUF GTK GLIBMM GDKMM GTKMM])

AC_CONFIG_FILES([Makefile po/Makefile.in])
AC_OUTPUT()
