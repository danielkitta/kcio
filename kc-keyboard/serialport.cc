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

#include <config.h>
#include "serialport.h"
#include <glibmm.h>
#include <cerrno>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace
{

/* Last-ditch effort to deal with runtime errors during destructor execution,
 * when throwing an exception would be unwise.
 */
static void destruction_error_message(const char* what, int err_no)
{
  g_warning("Error during destruction (%s): %s", what, g_strerror(err_no));
}

} // anonymous namespace

namespace KC
{

ScopedPollFD::~ScopedPollFD()
{
  const int fd = get_fd();

  if (fd >= 0)
    while (close(fd) < 0)
      if (errno != EINTR)
      {
        destruction_error_message("close", errno);
        break;
      }
}

SerialPort::SerialPort(const std::string& portname)
:
  expiration_ (0),
  portname_   (portname),
  poll_fd_    (open(portname_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK)),
  remaining_  (-1),
  inlen_      (0),
  inpos_      (0)
{
  if (poll_fd_.get_fd() < 0)
    throw_file_error(errno);
  setup_interface();

  set_can_recurse(false);
  add_poll(poll_fd_);
}

SerialPort::~SerialPort()
{
  const int fd = poll_fd_.get_fd();

  if (fd >= 0 && tcflush(fd, TCIOFLUSH) < 0)
    destruction_error_message("tcflush", errno);
}

// static
Glib::RefPtr<SerialPort> SerialPort::create(const std::string& portname)
{
  return Glib::RefPtr<SerialPort>(new SerialPort(portname));
}

int SerialPort::read_byte()
{
  if (inpos_ >= inlen_)
  {
    inpos_ = 0;
    // When reaching the end of the buffer, don't attempt to read more
    // data right away, even though there might be more already in the
    // queue.  Instead, return -1 and wait until the next turn to avoid
    // starving the UI.
    if (inlen_ != 0)
    {
      inlen_ = 0;
      return -1;
    }
#if 0
    g_printerr("\n<read()>");
#endif
    const ssize_t nread = read(poll_fd_.get_fd(), inbuf_, sizeof inbuf_);

    if (nread <= 0)
    {
      if (nread < 0)
        check_file_error();

      return -1;
    }
    inlen_ = nread;
  }
#if 0
  g_printerr("<%.2X>", static_cast<unsigned int>(inbuf_[inpos_]));
#endif
  return inbuf_[inpos_++];
}

void SerialPort::reset_timeout(int milliseconds)
{
  g_return_if_fail(milliseconds >= 0);

  expiration_ = get_current_time() + milliseconds;
  remaining_  = milliseconds;
}

void SerialPort::enable_events(Glib::IOCondition events)
{
  poll_fd_.set_events(poll_fd_.get_events() | events);
  get_context()->wakeup();
}

void SerialPort::disable_events(Glib::IOCondition events)
{
  poll_fd_.set_events(poll_fd_.get_events() & ~events);
  get_context()->wakeup();
}

std::string::size_type SerialPort::write_bytes(const std::string& sequence)
{
  const ssize_t rc = write(poll_fd_.get_fd(), sequence.data(), sequence.size());

  if (rc >= 0)
    return rc;

  check_file_error();
  return 0;
}

void SerialPort::discard()
{
  if (tcflush(poll_fd_.get_fd(), TCOFLUSH) < 0)
    throw_file_error(errno);
}

void SerialPort::close()
{
  remove_poll(poll_fd_);

  while (::close(poll_fd_.get_fd()) < 0)
  {
    if (errno != EINTR)
      throw_file_error(errno);
  }
  poll_fd_.set_fd(-1);
  poll_fd_.set_revents(Glib::IOCondition());
}

Glib::ustring SerialPort::display_portname() const
{
  return Glib::filename_display_name(portname_);
}

void SerialPort::check_file_error()
{
  const int err_no = errno;

  switch (err_no)
  {
    case EINTR:
    case EAGAIN:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
    case EWOULDBLOCK:
#endif
      break;

    default:
      throw_file_error(err_no);
  }
}

void SerialPort::throw_file_error(int err_no)
{
  throw Glib::FileError(Glib::FileError::Code(g_file_error_from_errno(err_no)),
                        Glib::ustring::compose("\"%1\": %2",
                                               Glib::filename_display_name(portname_),
                                               Glib::strerror(err_no)));
}

/*
 * Get the current time in milliseconds since 1970.  Y287K-unsafe.
 */
long long SerialPort::get_current_time()
{
  Glib::TimeVal now;
  Glib::Source::get_current_time(now);
  return 1000LL * now.tv_sec + now.tv_usec / 1000;
}

bool SerialPort::prepare(int& timeout)
{
  if (inpos_ < inlen_)
    return true;

  if (remaining_ > 0)
  {
    const long long now  = get_current_time();
    const long long diff = expiration_ - now;

    if (diff < 0)
      remaining_ = 0;
    else if (diff <= remaining_)
      remaining_ = diff;
    else // system time changed backwards
      expiration_ = now + remaining_;
  }

  timeout = remaining_;
  return (timeout == 0);
}

bool SerialPort::check()
{
  if (inpos_ < inlen_ || (poll_fd_.get_revents() & poll_fd_.get_events()) != 0)
    return true;

  if (remaining_ > 0 && get_current_time() >= expiration_)
    remaining_ = 0;

  return (remaining_ == 0);
}

bool SerialPort::dispatch(sigc::slot_base* slot)
{
  Glib::IOCondition revents = poll_fd_.get_revents();

  if (inpos_ < inlen_)
    revents |= Glib::IO_IN;

  if ((revents & poll_fd_.get_events()) == 0 && remaining_ == 0)
    remaining_ = -1;

  (*static_cast<sigc::slot<void, Glib::IOCondition>*>(slot))(revents);

  return true;
}

void SerialPort::setup_interface()
{
  const int flags = fcntl(poll_fd_.get_fd(), F_GETFD, 0);

  if (flags < 0 || fcntl(poll_fd_.get_fd(), F_SETFD, flags | FD_CLOEXEC) < 0)
    throw_file_error(errno);

  struct termios portattr;

  if (tcgetattr(poll_fd_.get_fd(), &portattr) < 0)
    throw_file_error(errno);

  portattr.c_iflag &= ~(BRKINT | IGNCR | ISTRIP | INLCR | ICRNL | IXON | IXOFF);
  portattr.c_iflag |= INPCK | IGNBRK | IGNPAR | PARMRK;

  portattr.c_oflag &= ~(OPOST | OCRNL | OFILL);

  portattr.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
  portattr.c_cflag |= CREAD | CS8 | HUPCL | CLOCAL | CRTSCTS; // not in POSIX

  portattr.c_lflag &= ~(ICANON | IEXTEN | ISIG | ECHO | TOSTOP);

  portattr.c_cc[VMIN]  = 1;
  portattr.c_cc[VTIME] = 0;

  enum { BAUDRATE_KEYBOARD = B1200 };

  cfsetispeed(&portattr, BAUDRATE_KEYBOARD);
  cfsetospeed(&portattr, BAUDRATE_KEYBOARD);

  if (tcsetattr(poll_fd_.get_fd(), TCSAFLUSH, &portattr) < 0
      || tcgetattr(poll_fd_.get_fd(), &portattr) < 0)
    throw_file_error(errno);

  if ((portattr.c_cflag & (CSIZE | CSTOPB | PARENB | CRTSCTS)) != (CS8 | CRTSCTS)
      || cfgetispeed(&portattr) != BAUDRATE_KEYBOARD
      || cfgetospeed(&portattr) != BAUDRATE_KEYBOARD)
  {
    throw Glib::FileError(Glib::FileError::NO_SUCH_DEVICE,
                          Glib::ustring::compose("\"%1\": serial port configuration not supported",
                                                 Glib::filename_display_name(portname_)));
  }
}

} // namespace KC
