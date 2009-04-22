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

#ifndef KC_KEYBOARD_SERIALPORT_H_INCLUDED
#define KC_KEYBOARD_SERIALPORT_H_INCLUDED

#include <glibmm.h>
#include <string>

namespace KC
{

class ScopedPollFD : public Glib::PollFD
{
public:
  explicit ScopedPollFD(int fd) : Glib::PollFD(fd) {}
  ~ScopedPollFD();

private:
  // noncopyable
  ScopedPollFD(const ScopedPollFD&);
  ScopedPollFD& operator=(const ScopedPollFD&);
};

class SerialPort : public Glib::Source
{
public:
  typedef SerialPort CppObjectType;

  static Glib::RefPtr<SerialPort> create(const std::string& portname);

  sigc::connection connect(const sigc::slot<void, Glib::IOCondition>& slot)
    { return connect_generic(slot); }

  void reset_timeout(int milliseconds);
  void enable_events(Glib::IOCondition events);
  void disable_events(Glib::IOCondition events);

  int read_byte();
  std::string::size_type write_bytes(const std::string& sequence);
  void discard();
  void close();

  Glib::ustring display_portname() const;

protected:
  explicit SerialPort(const std::string& portname);
  virtual ~SerialPort();

  virtual bool prepare(int& timeout);
  virtual bool check();
  virtual bool dispatch(sigc::slot_base* slot);

  long long get_current_time();

private:
  long long     expiration_;
  std::string   portname_;
  ScopedPollFD  poll_fd_;
  int           remaining_;
  unsigned int  inlen_;
  unsigned int  inpos_;
  unsigned char inbuf_[32];

  void setup_interface();
  void check_file_error();
  void throw_file_error(int err_no) G_GNUC_NORETURN;
};

} // namespace KC

#endif /* !KC_KEYBOARD_SERIALPORT_H_INCLUDED */
