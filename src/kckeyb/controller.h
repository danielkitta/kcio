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

#ifndef KCMILL_KCKEYB_CONTROLLER_H_INCLUDED
#define KCMILL_KCKEYB_CONTROLLER_H_INCLUDED

#include "serialport.h"
#include <sigc++/sigc++.h>
#include <glibmm.h>
#include <queue>
#include <string>

namespace KC
{

enum KeyboardMode
{
  KEYBOARD_RAW  = 0,
  KEYBOARD_CAOS = 1,
  KEYBOARD_CPM  = 2,
  KEYBOARD_TPKC = 3, // meta-mode used by InputWindow
  KEYBOARD_COUNT
};

class Controller : public sigc::trackable
{
public:
  explicit Controller(const std::string& portname);
  virtual ~Controller();

  sigc::signal<void, KeyboardMode>& signal_mode_switch() { return signal_mode_switch_; }

  bool send_key_sequence(const std::string& sequence);
  void shutdown();

private:
  typedef std::queue<std::string> OutputQueue;

  OutputQueue outbox_;
  SerialPort  port_;
  sigc::signal<void, KeyboardMode> signal_mode_switch_;

  void on_serial_io(Glib::IOCondition condition);
};

} // namespace KC

#endif /* !KCMILL_KCKEYB_CONTROLLER_H_INCLUDED */
