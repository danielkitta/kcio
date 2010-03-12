/*
 * Copyright (c) 2010  Daniel Elstner <daniel.kitta@gmail.com>
 *
 * This file is part of KC-I/O.
 *
 * KC-I/O is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * KC-I/O is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <build/config.h>
#include "devicepool.h"
#include <glib.h>

namespace
{

/* Check whether a particular device in the udev database could be a serial
 * port device.  Filter out virtual terminals and other non-hardware devices.
 * In particular, ignore unpopulated COM[1-4] ports on the PC platform.
 */
bool match_serial_device(const Udev::Device& device)
{
  if (device.get_subsystem() == "tty")
    if (const Udev::Device parent = device.get_parent())
      return (parent.get_subsystem() != "platform");

  return false;
}

KC::DeviceInfo make_device_info(const Udev::Device& device)
{
  std::string name = device.get_property("ID_MODEL_FROM_DATABASE");
  if (name.empty())
  {
    name = device.get_property("ID_MODEL");
    if (name.empty())
      name = "Unknown Device";
    else
      std::replace(name.begin(), name.end(), '_', ' ');
  }
  std::string path = device.get_property("ID_PATH");
  if (path.empty())
    path = "unknown";

  const std::string port = device.get_property("ID_PORT");
  if (!port.empty())
  {
    path += "-port";
    path += port;
  }
  return KC::DeviceInfo(name, path, device.get_dev_node());
}

} // anonymous namespace

namespace KC
{

DeviceInfo::DeviceInfo()
:
  name_ (),
  path_ (),
  file_ ()
{}

DeviceInfo::DeviceInfo(const std::string& devname, const std::string& devpath,
                       const std::string& devfile)
:
  name_ (devname),
  path_ (devpath),
  file_ (devfile)
{
  g_print("%s [%s] (%s)\n", name_.c_str(), path_.c_str(), file_.c_str());
}

DeviceInfo::~DeviceInfo()
{}

void DeviceInfo::swap(DeviceInfo& other)
{
  name_.swap(other.name_);
  path_.swap(other.path_);
  file_.swap(other.file_);
}

DeviceInfo::DeviceInfo(const DeviceInfo& other)
:
  name_ (other.name_),
  path_ (other.path_),
  file_ (other.file_)
{}

int DeviceInfo::compare(const DeviceInfo& other) const
{
  if (const int result = name_.compare(other.name_))
    return result;

  if (const int result = path_.compare(other.path_))
    return result;

  return file_.compare(other.file_);
}

DevicePool::DevicePool()
:
  pool_ ()
{}

DevicePool::~DevicePool()
{}

void DevicePool::uevent(Udev::Action action, const Udev::Device& device)
{
  switch (action)
  {
    case Udev::ACTION_ENUMERATE:
    case Udev::ACTION_ADD:
    case Udev::ACTION_CHANGE:
      if (match_serial_device(device))
      {
        pool_[device.get_sys_path()] = make_device_info(device);
        break;
      }
      // fallthrough
    case Udev::ACTION_REMOVE:
      pool_.erase(device.get_sys_path());
      break;
    default:
      break;
  }
}

} // namespace KC
