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

class ScopedPollFD : public Glib::PollFD
{
public:
  ScopedPollFD(int fd, Glib::IOCondition events) : Glib::PollFD(fd, events) {}
  ~ScopedPollFD();

private:
  // noncopyable
  ScopedPollFD(const ScopedPollFD&);
  ScopedPollFD& operator=(const ScopedPollFD&);
};

ScopedPollFD::~ScopedPollFD()
{
  if (get_fd() >= 0)
  {
    int rc;
    do
      rc = close(get_fd());
    while (rc < 0 && errno == EINTR);

    g_return_if_fail(rc == 0);
  }
}

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
  const int flags = fcntl(portfd, F_GETFD, 0);

  if (flags < 0 || fcntl(portfd, F_SETFD, flags | FD_CLOEXEC) < 0)
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
}

} // anonymous namespace

namespace KC
{

class PortSource : public Glib::Source
{
public:
  typedef PortSource CppObjectType;

  static Glib::RefPtr<PortSource> create(int fd);
  sigc::connection connect(const sigc::slot<void, Glib::IOCondition>& slot);

  int get_fd() { return poll_fd_.get_fd(); }
  int close();

  void enable_events(Glib::IOCondition events);
  void disable_events(Glib::IOCondition events);

protected:
  explicit PortSource(int fd);
  virtual ~PortSource();

  virtual bool prepare(int& timeout);
  virtual bool check();
  virtual bool dispatch(sigc::slot_base* slot);

private:
  ScopedPollFD poll_fd_;
};

// static
Glib::RefPtr<PortSource> PortSource::create(int fd)
{
  return Glib::RefPtr<PortSource>(new PortSource(fd));
}

sigc::connection PortSource::connect(const sigc::slot<void, Glib::IOCondition>& slot)
{
  return connect_generic(slot);
}

int PortSource::close()
{
  const int fd = poll_fd_.get_fd();

  if (fd >= 0)
  {
    remove_poll(poll_fd_);

    while (::close(fd) < 0)
    {
      const int err_no = errno;

      if (err_no != EINTR)
        return err_no;
    }

    poll_fd_.set_fd(-1);
    poll_fd_.set_revents(Glib::IOCondition());
  }

  return 0;
}

void PortSource::enable_events(Glib::IOCondition events)
{
  poll_fd_.set_events(poll_fd_.get_events() | events);
  get_context()->wakeup();
}

void PortSource::disable_events(Glib::IOCondition events)
{
  poll_fd_.set_events(poll_fd_.get_events() & ~events);
  get_context()->wakeup();
}

PortSource::PortSource(int fd)
:
  poll_fd_ (fd, Glib::IO_IN)
{
  add_poll(poll_fd_);
}

PortSource::~PortSource()
{}

bool PortSource::prepare(int& timeout)
{
  timeout = -1;
  return false;
}

bool PortSource::check()
{
  return ((poll_fd_.get_revents() & poll_fd_.get_events()) != 0);
}

bool PortSource::dispatch(sigc::slot_base* slot)
{
  (*static_cast<sigc::slot<void, Glib::IOCondition>*>(slot))(poll_fd_.get_revents());
  return true;
}

SerialPort::SerialPort(const std::string& portname)
:
  portname_ (portname),
  port_     (PortSource::create(open(portname_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK)))
{
  if (port_->get_fd() < 0)
    throw_file_error(portname_, errno);

  init_serial_port(port_->get_fd(), portname_);
}

SerialPort::~SerialPort()
{
  io_handler_.disconnect();
}

void SerialPort::close()
{
  if (const int err_no = port_->close())
    throw_file_error(portname_, err_no);
}

void SerialPort::set_io_handler(const sigc::slot<void, Glib::IOCondition>& slot)
{
  // FIXME: disconnect old one?
  io_handler_ = port_->connect(slot);
}

void SerialPort::enable_events(Glib::IOCondition events)
{
  port_->enable_events(events);
}

void SerialPort::disable_events(Glib::IOCondition events)
{
  port_->disable_events(events);
}

} // namespace KC
