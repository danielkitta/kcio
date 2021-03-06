## Copyright (c) 2008-2010  Daniel Elstner <daniel.kitta@gmail.com>
##
## This file is part of KC-I/O.
##
## KC-I/O is free software: you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by the
## Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## KC-I/O is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
## See the GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License along
## with this program.  If not, see <http://www.gnu.org/licenses/>.

AC_INIT([KC-I/O], [0.1], [daniel.kitta@gmail.com], [kcio], [http://github.com/danielkitta/kcio])
AC_PREREQ([2.60])

AC_CONFIG_SRCDIR([libkc/libkc.h])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build])
AC_CONFIG_HEADERS([build/config.h])

AM_INIT_AUTOMAKE([1.9 -Wall check-news no-define nostdinc])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AC_ARG_VAR([ACLOCAL_FLAGS], [aclocal flags, e.g. -I <macro dir>])

AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_RANLIB

AC_LANG_ASSERT([C])
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_SYS_POSIX_TERMIOS
AS_IF([test "x$ac_cv_sys_posix_termios" != xyes], [AC_MSG_ERROR([[
The system does not support the POSIX termios interface.
]])])

IT_PROG_INTLTOOL([0.35])
AC_SUBST([GETTEXT_PACKAGE], [kcio])
AM_GNU_GETTEXT_VERSION([0.11])
AM_GLIB_GNU_GETTEXT

DK_MODULE_CURSES([require-wchar])
PKG_CHECK_MODULES([KCIO_MODULES],
  [alsa >= 1.0 libudev++1 librsvgmm-2.0 >= 2.14 gtkhotkey-1.0 >= 0.2 gtkmm-2.4 >= 2.12])
PKG_CHECK_MODULES([KC_CONTROL_MODULES], [alsa gtkmm-2.4])
PKG_CHECK_MODULES([KC_KEYBOARD_MODULES], [libudev++1 librsvgmm-2.0 gtkhotkey-1.0 gtkmm-2.4])
PKG_CHECK_MODULES([KCREC_MODULES], [alsa])
DK_PKG_PATH_PROG([GTK_UPDATE_ICON_CACHE], [gtk+-2.0], [gtk-update-icon-cache])

AC_TYPE_SSIZE_T
AC_TYPE_INT16_T
AC_TYPE_UINT8_T

DK_ARG_ENABLE_WARNINGS([KCIO_WFLAGS],
                       [-Wall],
                       [-Wall -Wextra])
AC_LANG([C++])
DK_ARG_ENABLE_WARNINGS([KCIO_WXXFLAGS],
                       [-Wall],
                       [-DGDK_MULTIHEAD_SAFE -Wall -Wextra],
                       [LIBSIGC PANGO G GDK GDK_PIXBUF GLIBMM GDKMM GTKMM])

AC_CONFIG_FILES([Makefile po/Makefile.in])
AC_OUTPUT
