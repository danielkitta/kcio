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
#include "serialport.h"
#include <glibmm.h>
#include <cerrno>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace
{

static void throw_file_error(const std::string& filename, int err_no) G_GNUC_NORETURN;
static void throw_file_error(const std::string& filename, int err_no)
{
  g_assert(err_no != 0);

  throw Glib::FileError(Glib::FileError::Code(g_file_error_from_errno(err_no)),
                        Glib::ustring::compose("\"%1\": %2",
                                               Glib::filename_display_name(filename),
                                               Glib::strerror(err_no)));
}

static void init_serial_port(int portfd, const std::string& portname)
{
  const int fdflags = fcntl(portfd, F_GETFD, 0);

  if (fdflags < 0 || fcntl(portfd, F_SETFD, fdflags | FD_CLOEXEC) < 0)
    throw_file_error(portname, errno);

  struct termios portattr;

  if (tcgetattr(portfd, &portattr) < 0)
    throw_file_error(portname, errno);

  portattr.c_iflag &= ~(BRKINT | INPCK | ISTRIP | INLCR | ICRNL | IXON | IXOFF);
  portattr.c_iflag |= IGNBRK | IGNCR;

  portattr.c_oflag &= ~(OPOST | OCRNL | OFILL);

  portattr.c_cflag &= ~(CLOCAL | CSIZE | CSTOPB | PARENB);
  portattr.c_cflag |= CREAD | CS8 | HUPCL | CRTSCTS; /* not in POSIX */

  portattr.c_lflag &= ~(ICANON | IEXTEN | ISIG | ECHO | NOFLSH | TOSTOP);

  portattr.c_cc[VINTR] = 3;
  portattr.c_cc[VMIN]  = 1;
  portattr.c_cc[VTIME] = 0;

  enum { BAUDRATE_KEYBOARD = B1200 };

  cfsetispeed(&portattr, BAUDRATE_KEYBOARD);
  cfsetospeed(&portattr, BAUDRATE_KEYBOARD);

  if (tcsetattr(portfd, TCSAFLUSH, &portattr) < 0
      || tcgetattr(portfd, &portattr) < 0)
    throw_file_error(portname, errno);

  if ((portattr.c_cflag & (CSIZE | CSTOPB | PARENB | CRTSCTS)) != (CS8 | CRTSCTS)
      || cfgetispeed(&portattr) != BAUDRATE_KEYBOARD
      || cfgetospeed(&portattr) != BAUDRATE_KEYBOARD)
  {
    throw Glib::FileError(Glib::FileError::NO_SUCH_DEVICE,
                          Glib::ustring::compose("\"%1\": serial port configuration not supported",
                                                 Glib::filename_display_name(portname)));
  }

  const int flags = fcntl(portfd, F_GETFL, 0);

  if (flags < 0 || fcntl(portfd, F_SETFL, flags & ~O_NONBLOCK) < 0)
    throw_file_error(portname, errno);
}

} // anonymous namespace

namespace KC
{

ScopedUnixFile::~ScopedUnixFile()
{
  if (fd_ >= 0)
  {
    int rc;
    do
      rc = close(fd_);
    while (rc < 0 && errno == EINTR);

    g_return_if_fail(rc == 0);
  }
}

SerialPort::SerialPort(const std::string& portname)
:
  portname_ (portname),
  portfd_   (open(portname_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK))
{
  if (portfd_.get() < 0)
    throw_file_error(portname_, errno);

  init_serial_port(portfd_.get(), portname_);
}

SerialPort::~SerialPort()
{}

void SerialPort::close()
{
  if (portfd_.get() >= 0)
  {
    while (::close(portfd_.get()) < 0)
    {
      const int err_no = errno;
      if (err_no != EINTR)
        throw_file_error(portname_, err_no);
    }
    portfd_.reset();
  }
}

} // namespace KC
