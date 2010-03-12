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

#ifndef KC_KEYBOARD_DEVICEPOOL_H_INCLUDED
#define KC_KEYBOARD_DEVICEPOOL_H_INCLUDED

#include <libudev++.h>
#include <functional>
#include <map>
#include <string>

namespace KC
{

class DeviceInfo
{
public:
  class Less;

  DeviceInfo();
  DeviceInfo(const std::string& devname, const std::string& devpath, const std::string& devfile);
  ~DeviceInfo();

  void swap(DeviceInfo& other);

  DeviceInfo(const DeviceInfo& other);
  DeviceInfo& operator=(const DeviceInfo& other) { DeviceInfo(other).swap(*this); return *this; }

  const std::string& name() const { return name_; }
  const std::string& path() const { return path_; }
  const std::string& file() const { return file_; }

  int compare(const DeviceInfo& other) const;

private:
  std::string name_;
  std::string path_;
  std::string file_;
};

class DeviceInfo::Less : public std::binary_function<DeviceInfo, DeviceInfo, bool>
{
public:
  bool operator()(const DeviceInfo& a, const DeviceInfo& b) const { return (a.compare(b) < 0); }
};

inline void swap(DeviceInfo& a, DeviceInfo& b) { a.swap(b); }

class DevicePool
{
public:
  DevicePool();
  ~DevicePool();

  void uevent(Udev::Action action, const Udev::Device& device);

private:
  typedef std::map<std::string, DeviceInfo> DeviceMap;

  DeviceMap pool_;

  // noncopyable
  DevicePool(const DevicePool&);
  DevicePool& operator=(const DevicePool&);
};

} // namespace KC

#endif /* !KC_KEYBOARD_DEVICEPOOL_H_INCLUDED */
