/*
 * Copyright (c) 2009  Daniel Elstner <daniel.kitta@gmail.com>
 *
 * LibKCUI is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LibKCUI is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <build/config.h>
#include "libkcui.h"

#include <glibmm.h>
#include <glib.h>
#include <cerrno>

std::string Util::locate_data_file(const std::string& basename)
{
#ifdef G_OS_WIN32
  if (char *const exedir = g_win32_get_package_installation_directory_of_module(0))
  {
    const std::string fullpath = Glib::build_filename(Glib::ScopedPtr<char>(exedir).get(), basename);

    if (Glib::file_test(fullpath, Glib::FILE_TEST_IS_REGULAR))
      return fullpath;
  }
  return basename;
#else
  return Glib::build_filename(KCIO_PKGDATADIR, basename);
#endif
}

std::string Util::locate_config_dir()
{
  return Glib::build_filename(Glib::get_user_config_dir(), PACKAGE_TARNAME);
}

std::string Util::make_config_dir()
{
  const std::string config_dir = Util::locate_config_dir();

  if (g_mkdir_with_parents(config_dir.c_str(), 0755) < 0)
  {
    const int err_no = errno;
    throw Glib::FileError(Glib::FileError::Code(g_file_error_from_errno(err_no)),
                          Glib::ustring::compose("Creating directory \"%1\" failed: %2",
                                                 Glib::filename_display_name(config_dir),
                                                 Glib::strerror(err_no)));
  }
  return config_dir;
}
