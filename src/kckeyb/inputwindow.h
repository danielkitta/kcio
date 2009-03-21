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

#ifndef KCMILL_KCKEYB_INPUTWINDOW_H_INCLUDED
#define KCMILL_KCKEYB_INPUTWINDOW_H_INCLUDED

#include <gtkmm/window.h>

namespace KC
{

class InputWindow : public Gtk::Window
{
public:
  InputWindow();
  virtual ~InputWindow();

protected:
  virtual bool on_button_press_event(GdkEventButton* event);
  virtual bool on_key_press_event(GdkEventKey* event);
  virtual bool on_key_release_event(GdkEventKey* event);
};

} // namespace KC

#endif /* !KCMILL_KCKEYB_INPUTWINDOW_H_INCLUDED */
