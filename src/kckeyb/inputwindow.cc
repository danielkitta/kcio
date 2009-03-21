/*
 * Copyright (c) 2009  Daniel Elstner <daniel.kitta@gmail.com>
 *
 * This file is part of KC-Mill.
 *
 * KC-Mill is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * KC-Mill is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "inputwindow.h"

namespace KC
{

InputWindow::InputWindow()
:
  Gtk::Window(Gtk::WINDOW_POPUP)
{
  add_events(Gdk::BUTTON_PRESS_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
  set_keep_above(true);
  set_position(Gtk::WIN_POS_CENTER);
}

InputWindow::~InputWindow()
{}

bool InputWindow::on_button_press_event(GdkEventButton*)
{
  hide();
  return true;
}

bool InputWindow::on_key_press_event(GdkEventKey* event)
{
  return !event;
}

bool InputWindow::on_key_release_event(GdkEventKey* event)
{
  return !event;
}

} // namespace KC
