/*
 * Copyright (c) 2009  Daniel Elstner <daniel.kitta@gmail.com>
 *
 * This file is part of KC-IO.
 *
 * KC-IO is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * KC-IO is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "libkcui.h"
#include <gtkmm/aboutdialog.h>

std::auto_ptr<Gtk::AboutDialog> Util::create_about_dialog()
{
  std::auto_ptr<Gtk::AboutDialog> dialog (new Gtk::AboutDialog());

  dialog->set_version(PACKAGE_VERSION);
  dialog->set_copyright("Copyright \302\251 2009 Daniel Elstner");
  dialog->set_website("http://danielkitta.org/");

  const char *const program_authors[] = { "Daniel Elstner <daniel.kitta@gmail.com>", 0 };

  dialog->set_authors(program_authors);
  dialog->set_license("KC-IO is free software: you can redistribute it and/or modify it "
                      "under the terms of the GNU General Public License as published by the "
                      "Free Software Foundation, either version 3 of the License, or "
                      "(at your option) any later version.\n"
                      "\n"
                      "KC-IO is distributed in the hope that it will be useful, but "
                      "WITHOUT ANY WARRANTY; without even the implied warranty of "
                      "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. "
                      "See the GNU General Public License for more details.\n"
                      "\n"
                      "You should have received a copy of the GNU General Public License along "
                      "with this program.  If not, see <http://www.gnu.org/licenses/>.\n");
  dialog->set_wrap_license(true);

  return dialog;
}
