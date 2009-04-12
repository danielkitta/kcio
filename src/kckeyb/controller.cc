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
#include "controller.h"
#include <glibmm.h>
#include <cerrno>
#include <termios.h>
#include <unistd.h>

namespace
{

enum KeyboardCommand
{
  CMD_RESET_SYSTEM   = 0x00,
  CMD_RESET_KEYBOARD = 0x01,
  CMD_MODE_SCANCODES = 0x02,
  CMD_IDENTIFY       = 0x03,
  CMD_PROGRAM_KEYS   = 0x04,
  CMD_PROGRAMMABLE   = 0x05,
  CMD_MODE_CAOS      = 0x06,
  CMD_MODE_CPM       = 0x07,
  CMD_TYPE_RATE      = 0x08,
  CMD_CONFIGURE_ALL  = 0x09,
  CMD_CONFIGURE_KEY  = 0x0A,
  CMD_SWITCH_LEDS    = 0x0B,
  CMD_START_SCANNING = 0x0C,
  CMD_ECHO           = 0x0D
};

static void command_not_implemented(const char* command, unsigned int argument)
{
  g_message("Command not implemented: %s (0x%.2X)", command, argument);
}

} // anonymous namespace

namespace KC
{

Controller::Controller(const std::string& portname)
:
  outbox_          (),
  port_            (SerialPort::create(portname)),
  command_buffer_  (),
  command_handler_ (),
  state_           (STATE_IDLE),
  mode_            (KEYBOARD_CPM),
  break_disabled_  ()
{
  port_->connect(sigc::mem_fun(*this, &Controller::on_io_event));
  port_->attach();
  port_->enable_events(Glib::IO_IN);
}

Controller::~Controller()
{}

bool Controller::send_codes(const std::string& sequence)
{
  const OutputQueue::size_type length = outbox_.size();

  if (length >= 8)
    return false;

  if (length == 0)
  {
    // Try to send as much as possible immediately in order to reduce latency.
    const std::string::size_type n = port_->write_bytes(sequence);

    if (n < sequence.size())
    {
      // Queue what's left and wait for the output to become writable again.
      outbox_.push_back(sequence.substr(n));
      port_->enable_events(Glib::IO_OUT);
    }
  }
  else
    outbox_.push_back(sequence);

  return true;
}

bool Controller::send_break(const std::string& sequence)
{
  g_return_val_if_fail(!sequence.empty(), false);

  const unsigned char code = *sequence.begin();

  if (!break_disabled_.test(code))
    return send_codes('\xF0' + sequence);

  return true;
}

void Controller::shutdown()
{
  port_->close();
  outbox_.clear();
}

void Controller::reset()
{
  g_warn_if_fail(state_ == STATE_IDLE);
  command_handler_ = 0;

  outbox_.clear();
  port_->discard();

  send_codes(std::string(1, '\x0D'));
}

void Controller::set_mode(KeyboardMode mode)
{
  mode_ = mode;
}

void Controller::on_io_event(Glib::IOCondition condition)
{
  if (condition == 0 && state_ != STATE_IDLE)
  {
    state_ = STATE_IDLE;
    command_handler_ = 0;
    g_message("Time-out while waiting for command code");
  }

  if ((condition & Glib::IO_IN) != 0)
  {
    int byte;

    while ((byte = port_->read_byte()) >= 0)
      switch (state_)
      {
        case STATE_IDLE:
          if (byte == 0x00)
          {
            state_ = STATE_EXPECT_COMMAND;
            port_->reset_timeout(250); // ms
          }
          break;

        case STATE_EXPECT_COMMAND:
          state_ = STATE_IDLE; // in case of exceptions
          on_command(byte);
          break;

        case STATE_PROCESS_COMMAND:
          g_return_if_fail(command_handler_ != 0);
          state_ = (this->*command_handler_)(byte);
          break;
      }
  }

  if ((condition & Glib::IO_OUT) != 0)
  {
    if (!outbox_.empty())
    {
      const std::string::size_type n = port_->write_bytes(outbox_.front());

      if (n == outbox_.front().size())
        outbox_.pop_front();
      else
        outbox_.front().erase(0, n);
    }

    if (outbox_.empty())
      port_->disable_events(Glib::IO_OUT);
  }
}

void Controller::on_command(unsigned int word)
{
  command_buffer_.clear();

  switch (word)
  {
    case CMD_RESET_SYSTEM:
    case CMD_RESET_KEYBOARD:
      reset();
      break;
    case CMD_MODE_SCANCODES:
      reset();
      mode_ = KEYBOARD_RAW;
      break;
    case CMD_IDENTIFY:
      send_codes(std::string("\xFA\x83\xAB", 3));
      break;
    case CMD_PROGRAM_KEYS:
      command_handler_ = &Controller::command_program_keys;
      state_ = STATE_PROCESS_COMMAND;
      break;
    case CMD_PROGRAMMABLE:
      send_codes(std::string(1, '\xFF')); // programming not available
      break;
    case CMD_MODE_CAOS:
      reset();
      mode_ = KEYBOARD_CAOS;
      break;
    case CMD_MODE_CPM:
      reset();
      mode_ = KEYBOARD_CPM;
      break;
    case CMD_TYPE_RATE:
      command_handler_ = &Controller::command_type_rate;
      state_ = STATE_PROCESS_COMMAND;
      break;
    case CMD_CONFIGURE_ALL:
      command_handler_ = &Controller::command_configure_all;
      state_ = STATE_PROCESS_COMMAND;
      break;
    case CMD_CONFIGURE_KEY:
      command_handler_ = &Controller::command_configure_key;
      state_ = STATE_PROCESS_COMMAND;
      break;
    case CMD_SWITCH_LEDS:
      command_handler_ = &Controller::command_switch_leds;
      state_ = STATE_PROCESS_COMMAND;
      break;
    case CMD_START_SCANNING:
      if (mode_ != KEYBOARD_RAW)
        g_message("Command start scanning: not in scancode mode");
      break;
    case CMD_ECHO:
      send_codes(std::string(1, '\xEE'));
      break;
  }
}

Controller::State Controller::command_program_keys(unsigned char byte)
{
  if (byte != 0xFE)
    return STATE_PROCESS_COMMAND;

  command_not_implemented("program user keys", byte);
  return STATE_IDLE;
}

Controller::State Controller::command_type_rate(unsigned char byte)
{
  command_not_implemented("set typematic rate", byte);
  return STATE_IDLE;
}

Controller::State Controller::command_configure_all(unsigned char byte)
{
  if (byte >= 0xF7 && byte <= 0xFA)
  {
    if ((byte & 0x04u) != 0) // 0xF7 or 0xFA
      command_not_implemented("enable autorepeat", byte);

    if ((byte & 0x01u) != 0) // 0xF7 or 0xF9
      break_disabled_.set();
    else // 0xF8 or 0xFA
      break_disabled_.reset();
  }
  else
    command_not_implemented("configure all keys", byte);

  return STATE_IDLE;
}

Controller::State Controller::command_configure_key(unsigned char byte)
{
  if (command_buffer_.empty())
  {
    command_buffer_.push_back(byte);
    return STATE_PROCESS_COMMAND;
  }

  switch (command_buffer_.front())
  {
    case 0xFB: command_not_implemented("enable autorepeat", byte); break;
    case 0xFC: break_disabled_.reset(byte); break;
    case 0xFD: break_disabled_.set(byte);   break;
    default:   command_not_implemented("configure key", byte); break;
  }
  command_buffer_.clear();
  return STATE_IDLE;
}

Controller::State Controller::command_switch_leds(unsigned char byte)
{
  command_not_implemented("switch LEDs", byte);
  return STATE_IDLE;
}

} // namespace KC
