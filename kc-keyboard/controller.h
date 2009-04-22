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

#ifndef KC_KEYBOARD_CONTROLLER_H_INCLUDED
#define KC_KEYBOARD_CONTROLLER_H_INCLUDED

#include "serialport.h"
#include <sigc++/sigc++.h>
#include <glibmm.h>
#include <bitset>
#include <list>
#include <string>
#include <vector>

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

  void set_mode(KeyboardMode mode);
  KeyboardMode get_mode() const { return mode_; }

  bool break_enabled_for_key(unsigned char scancode) const;
  bool send_key_codes(const std::string& sequence);

  void reset();
  void shutdown();

private:
  enum State
  {
    STATE_IDLE,
    STATE_SUSPENDED,
    STATE_EXPECT_COMMAND,
    STATE_PROCESS_COMMAND
  };
  typedef std::list<std::string> OutputQueue;
  typedef State (Controller::* CommandHandler)(unsigned char);

  OutputQueue                outbox_;
  Glib::RefPtr<SerialPort>   port_;
  std::vector<unsigned char> command_buffer_;
  CommandHandler             command_handler_;
  State                      state_;
  KeyboardMode               mode_;
  std::bitset<256>           break_disabled_;

  void on_io_event(Glib::IOCondition condition);
  void on_command(unsigned int word);

  State command_program_keys(unsigned char byte);
  State command_type_rate(unsigned char byte);
  State command_configure_all(unsigned char byte);
  State command_configure_key(unsigned char byte);
  State command_switch_leds(unsigned char byte);
};

} // namespace KC

#endif /* !KC_KEYBOARD_CONTROLLER_H_INCLUDED */
