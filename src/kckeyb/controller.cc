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

namespace KC
{

Controller::Controller(const std::string& portname)
:
  outbox_ (),
  port_   (portname)
{
  port_.set_input_handler(sigc::mem_fun(*this, &Controller::on_serial_input));
}

Controller::~Controller()
{}

bool Controller::send_key_sequence(const std::string& sequence)
{
  if (outbox_.size() < 10)
  {
    outbox_.push(sequence);
    return true;
  }
  return false;
}

void Controller::shutdown()
{
  port_.close();
}

bool Controller::on_serial_input(Glib::IOCondition)
{
  return true; // keep handler
}

} // namespace KC
