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

#ifndef KCMILL_KCKEYB_SERIALPORT_H_INCLUDED
#define KCMILL_KCKEYB_SERIALPORT_H_INCLUDED

#include <sigc++/sigc++.h>
#include <glibmm.h>
#include <string>

namespace KC
{

class PortSource;

class SerialPort
{
public:
  explicit SerialPort(const std::string& filename);
  ~SerialPort();

  void close();
  void set_io_handler(const sigc::slot<void, Glib::IOCondition>& slot);

  void enable_events(Glib::IOCondition events);
  void disable_events(Glib::IOCondition events);

private:
  std::string              portname_;
  Glib::RefPtr<PortSource> port_;
  sigc::connection         io_handler_;

  // noncopyable
  SerialPort(const SerialPort&);
  SerialPort& operator=(const SerialPort&);
};

} // namespace KC

#endif /* !KCMILL_KCKEYB_SERIALPORT_H_INCLUDED */
