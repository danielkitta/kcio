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

#ifndef LIBKC_LIBKCUI_H_INCLUDED
#define LIBKC_LIBKCUI_H_INCLUDED

#include <memory>
#include <string>

namespace Gtk { class AboutDialog; }

namespace Util
{

std::string locate_data_file(const std::string& basename);
std::string locate_config_dir();
std::string make_config_dir();

std::auto_ptr<Gtk::AboutDialog> create_about_dialog();

} // namespace Util

#endif /* !LIBKC_LIBKCUI_H_INCLUDED */
