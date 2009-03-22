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

class ScopedUnixFile
{
private:
  int fd_;
  // noncopyable
  ScopedUnixFile(const ScopedUnixFile&);
  ScopedUnixFile& operator=(const ScopedUnixFile&);

public:
  explicit ScopedUnixFile(int fd) : fd_ (fd) {}
  ~ScopedUnixFile();

  int get() const { return fd_; }
  void reset(int fd = -1) { fd_ = fd; }
};

class SerialPort
{
public:
  explicit SerialPort(const std::string& filename);
  ~SerialPort();

  void close();
  void set_input_handler(const sigc::slot<bool, Glib::IOCondition>& slot);

private:
  std::string      portname_;
  ScopedUnixFile   portfd_;
  sigc::connection input_handler_;

  // noncopyable
  SerialPort(const SerialPort&);
  SerialPort& operator=(const SerialPort&);
};

} // namespace KC

#endif /* !KCMILL_KCKEYB_SERIALPORT_H_INCLUDED */
