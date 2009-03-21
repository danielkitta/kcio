## Copyright (c) 2008  Daniel Elstner <daniel.kitta@gmail.com>
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

## This project does not use recursive make, but a single toplevel Makefile
## to build the entire tree.  Read Peter Miller's excellent paper to learn
## why recursive make invocations are both slow and error-prone:
## http://members.pcug.org.au/~millerp/rmch/recu-make-cons-harm.html

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS  = -I m4 $(ACLOCAL_FLAGS)
DISTCHECK_CONFIGURE_FLAGS = --enable-warnings=fatal

SUBDIRS = po

noinst_LIBRARIES = src/libkc/libkc.a

src_libkc_libkc_a_SOURCES =	\
	src/libkc/charset.c	\
	src/libkc/libkc.h

bin_PROGRAMS =		\
	src/cli/kcplay	\
	src/cli/kcrec	\
	src/cli/kcsend	\
	src/cli/kcterm	\
	src/kckeyb	\
	src/kcmill

src_cli_kcplay_SOURCES = src/cli/kcplay.c
src_cli_kcrec_SOURCES  = src/cli/kcrec.c
src_cli_kcsend_SOURCES = src/cli/kcsend.c
src_cli_kcterm_SOURCES = src/cli/kcterm.c

src_kckeyb_SOURCES =	\
	src/kckeyb.cc

src_kcmill_SOURCES =	\
	src/main.cc

# The location of the gettext catalogs as defined by intltool.
kclocaledir = $(prefix)/$(DATADIRNAME)/locale
global_defs = -DKCMILL_LOCALEDIR=\""$(kclocaledir)"\" -DKCMILL_PKGDATADIR=\""$(pkgdatadir)"\"
AM_CPPFLAGS = $(global_defs) -I$(top_builddir) $(CURSES_CFLAGS) $(KCMILL_MODULES_CFLAGS)
AM_CFLAGS   = $(LIBKC_WFLAGS)
AM_CXXFLAGS = $(KCMILL_WXXFLAGS)

src_cli_kcplay_LDADD = $(noinst_LIBRARIES) $(KCREC_MODULES_LIBS)
src_cli_kcrec_LDADD  = $(noinst_LIBRARIES) $(KCREC_MODULES_LIBS)
src_cli_kcsend_LDADD = $(noinst_LIBRARIES)
src_cli_kcterm_LDADD = $(noinst_LIBRARIES) $(CURSES_LIBS)
src_kckeyb_LDADD     = $(noinst_LIBRARIES) $(KCMILL_MODULES_LIBS) $(INTLLIBS)
src_kcmill_LDADD     = $(noinst_LIBRARIES) $(KCMILL_MODULES_LIBS) $(INTLLIBS)

iconthemedir	    = $(datadir)/icons/hicolor
appicondir	    = $(iconthemedir)/48x48/apps
dist_appicon_DATA   = ui/kcmill.png

desktop_in_files    = ui/kcmill.desktop.in
desktopdir          = $(datadir)/applications
desktop_DATA        = ui/kcmill.desktop

dist_intltool       = intltool-extract.in intltool-merge.in intltool-update.in
dist_noinst_DATA    = $(dist_intltool) $(desktop_in_files)
dist_noinst_SCRIPTS = autogen.sh

CLEANFILES          = $(desktop_DATA)
DISTCLEANFILES      = intltool-extract intltool-merge intltool-update

update_icon_cache   = $(GTK_UPDATE_ICON_CACHE) --ignore-theme-index --force

@INTLTOOL_DESKTOP_RULE@

install-data-hook: install-update-icon-cache
uninstall-hook: uninstall-update-icon-cache

install-update-icon-cache:
	@$(POST_INSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

uninstall-update-icon-cache:
	@$(POST_UNINSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

dist-deb: distdir
	cd "$(distdir)" && dpkg-buildpackage -nc -rfakeroot -uc -us
	rm -rf "$(distdir)"

.PHONY: dist-deb install-update-icon-cache uninstall-update-icon-cache
