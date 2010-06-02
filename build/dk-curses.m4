## Copyright (c) 2008-2009  Daniel Elstner  <daniel.kitta@gmail.com>
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
## with this program.  If not, see <http://www.gnu.org/licenses/>.

#serial 20091107

## _DK_IF_OPTION(list, word, action-if-found, [action-if-not-found])
##
m4_define([_DK_IF_OPTION],
          [m4_if([m4_index([ ]m4_quote(m4_normalize([$1]))[ ], [ $2 ])], [-1], [$4], [$3])])

## _DK_MODULE_CURSES_WCHAR
##
## Internal helper macro, checking the configured curses implementation
## for wide character support.
##
m4_define([_DK_MODULE_CURSES_WCHAR],
[dnl
AC_PROVIDE([$0])[]dnl
AC_MSG_CHECKING([for wide character support in curses])
DK_SH_VAR_PUSH([CPPFLAGS], ["$CPPFLAGS $CURSES_CFLAGS"])
DK_SH_VAR_PUSH([LIBS], ["$LIBS $CURSES_LIBS"])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <curses.h>]], [[(void) wadd_wch(0, 0);]])],
               [dk_have_cursesw=yes], [dk_have_cursesw=no])
DK_SH_VAR_POP([LIBS])
DK_SH_VAR_POP([CPPFLAGS])
AC_MSG_RESULT([$dk_have_cursesw])
])

## _DK_MODULE_CURSES_FLAGS
##
## Internal helper macro for determining the compiler and linker flags
## needed to use the system's curses library.
##
m4_define([_DK_MODULE_CURSES_FLAGS],
[dnl
AC_PROVIDE([$0])[]dnl
AC_PATH_PROGS([CURSES_CONFIG], [ncursesw5-config ncurses5-config])
AS_IF([test "x$CURSES_CONFIG" = x], [AC_MSG_ERROR([[
No ncurses configuration script could be found.  Please install the
development package of the ncurses library.  Choose the version with
wide character support if it is packaged separately.
]])])
AC_MSG_CHECKING([for curses library])
AS_IF([$CURSES_CONFIG --version >/dev/null 2>&AS_MESSAGE_LOG_FD],
[
  CURSES_CFLAGS=`$CURSES_CONFIG --cflags 2>&AS_MESSAGE_LOG_FD`
  CURSES_LIBS=`$CURSES_CONFIG --libs 2>&AS_MESSAGE_LOG_FD`
])
AS_IF([test "x$CURSES_CFLAGS$CURSES_LIBS" = x],
      [AC_MSG_FAILURE([[error running ncurses configuration script]])])
AC_MSG_RESULT([$CURSES_CFLAGS $CURSES_LIBS])
AC_SUBST([CURSES_CFLAGS])
AC_SUBST([CURSES_LIBS])
])

## DK_MODULE_CURSES([option-list])
##
## Determine the compiler and linker flags required to use the system's
## curses library, preferably with support for wide characters enabled.
## The output variables CURSES_CFLAGS and CURSES_LIBS are substituted.
##
## Options can be passed in <option-list> to adjust the behavior.  The
## only option implemented at this time is "require-wchar", to indicate
## that wide character support is mandatory rather than optional.
##
## Currently, only the ncurses implementation is checked for.
##
AC_DEFUN([DK_MODULE_CURSES],
[dnl
AC_REQUIRE([_DK_MODULE_CURSES_FLAGS])[]dnl
_DK_IF_OPTION([$1], [require-wchar], [AC_REQUIRE([_DK_MODULE_CURSES_WCHAR])])[]dnl
])
