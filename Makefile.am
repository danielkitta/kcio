## Copyright (c) 2008-2010  Daniel Elstner <daniel.kitta@gmail.com>
##
## This file is part of KC-I/O.
##
## KC-IO is free software: you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by the
## Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## KC-IO is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
## See the GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License along
## with this program.  If not, see <http://www.gnu.org/licenses/>.

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS  = -I build ${ACLOCAL_FLAGS}
DISTCHECK_CONFIGURE_FLAGS = --enable-warnings=fatal

SUBDIRS = po

noinst_LIBRARIES = libkc/libkc.a libkcui/libkcui.a

libkc_libkc_a_SOURCES =	\
	libkc/charset.c	\
	libkc/libkc.h

libkcui_libkcui_a_SOURCES =	\
	libkcui/aboutdialog.cc	\
	libkcui/appdata.cc	\
	libkcui/libkcui.h

bin_PROGRAMS =			\
	cli/kcplay		\
	cli/kcrec		\
	cli/kcsend		\
	cli/kcterm		\
	kc-control/kc-control	\
	kc-keyboard/kc-keyboard

cli_kcplay_SOURCES = cli/kcplay.c
cli_kcrec_SOURCES  = cli/kcrec.c
cli_kcsend_SOURCES = cli/kcsend.c
cli_kcterm_SOURCES = cli/kcterm.c

kc_keyboard_kc_keyboard_SOURCES =	\
	kc-keyboard/controller.cc	\
	kc-keyboard/controller.h	\
	kc-keyboard/inputwindow.cc	\
	kc-keyboard/inputwindow.h	\
	kc-keyboard/main.cc		\
	kc-keyboard/serialport.cc	\
	kc-keyboard/serialport.h

kc_control_kc_control_SOURCES =	\
	kc-control/main.cc

# The location of the gettext catalogs as defined by intltool.
kclocaledir = $(prefix)/$(DATADIRNAME)/locale
global_defs = -DKCIO_LOCALEDIR=\""$(kclocaledir)"\" -DKCIO_PKGDATADIR=\""$(pkgdatadir)"\"
AM_CPPFLAGS = -I. -I$(top_srcdir) $(global_defs) $(CURSES_CFLAGS) $(KCIO_MODULES_CFLAGS)
AM_CFLAGS   = $(KCIO_WFLAGS)
AM_CXXFLAGS = $(KCIO_WXXFLAGS)

cli_kcplay_LDADD		= libkc/libkc.a $(KCREC_MODULES_LIBS)
cli_kcrec_LDADD			= libkc/libkc.a $(KCREC_MODULES_LIBS)
cli_kcsend_LDADD		= libkc/libkc.a
cli_kcterm_LDADD		= libkc/libkc.a $(CURSES_LIBS)
kc_control_kc_control_LDADD	= libkc/libkc.a libkcui/libkcui.a $(KC_CONTROL_MODULES_LIBS) $(INTLLIBS)
kc_keyboard_kc_keyboard_LDADD	= libkc/libkc.a libkcui/libkcui.a $(KC_KEYBOARD_MODULES_LIBS) $(INTLLIBS)

iconthemedir = $(datadir)/icons/hicolor
appicon16dir = $(iconthemedir)/16x16/apps
appicon22dir = $(iconthemedir)/22x22/apps
appicon24dir = $(iconthemedir)/24x24/apps
appicon32dir = $(iconthemedir)/32x32/apps
appicon48dir = $(iconthemedir)/48x48/apps
appiconscdir = $(iconthemedir)/scalable/apps

dist_appicon16_DATA = ui/16x16/kc-control.png ui/16x16/kc-keyboard.png
dist_appicon22_DATA = ui/22x22/kc-control.png ui/22x22/kc-keyboard.png
dist_appicon24_DATA = ui/24x24/kc-control.png ui/24x24/kc-keyboard.png
dist_appicon32_DATA = ui/32x32/kc-control.png ui/32x32/kc-keyboard.png
dist_appicon48_DATA = ui/48x48/kc-control.png ui/48x48/kc-keyboard.png
dist_appiconsc_DATA = ui/scalable/kc-control.svg ui/scalable/kc-keyboard.svg

desktop_in_files    = ui/kc-control.desktop.in ui/kc-keyboard.desktop.in
desktopdir          = $(datadir)/applications
desktop_DATA        = ui/kc-control.desktop ui/kc-keyboard.desktop

dist_pkgdata_DATA   = ui/enter-key.svg ui/keyboard-logo.svg ui/keymap.conf

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
