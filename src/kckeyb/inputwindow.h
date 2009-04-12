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

#ifndef KCMILL_KCKEYB_INPUTWINDOW_H_INCLUDED
#define KCMILL_KCKEYB_INPUTWINDOW_H_INCLUDED

#include "controller.h"
#include <gdkmm.h>
#include <gtkmm/statusicon.h>
#include <gtkmm/window.h>
#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>

extern "C" { typedef struct _GtkHotkeyInfo GtkHotkeyInfo; }

namespace KC
{

template <class T>
class GObjectPtr
{
private:
  T* obj_;

public:
  GObjectPtr() : obj_ (0) {}
  explicit GObjectPtr(T* obj) : obj_ (obj) {}
  ~GObjectPtr() { if (obj_) g_object_unref(obj_); }

  GObjectPtr(const GObjectPtr<T>& b)
    : obj_ (b.obj_) { if (obj_) g_object_ref(obj_); }
  GObjectPtr<T>& operator=(const GObjectPtr<T>& b)
    { GObjectPtr<T> temp (b); std::swap(obj_, temp.obj_); return *this; }

  void swap(GObjectPtr<T>& b) { std::swap(obj_, b.obj_); }

  T* get() const { return obj_; }
  operator const void*() const { return obj_; }
};

typedef GObjectPtr<GtkHotkeyInfo> HotkeyInfoPtr;

struct MappedKey
{
  unsigned int      keyval;
  Gdk::ModifierType state;
  std::string       sequence;

  MappedKey() : keyval (0), state (), sequence () {}
  MappedKey(unsigned int k, Gdk::ModifierType s) : keyval (k), state (s), sequence () {}
  explicit MappedKey(const std::string& seq) : keyval (0), state (), sequence (seq) {}

  struct SortPredicate;
};

struct MappedKey::SortPredicate : std::binary_function<bool, MappedKey, MappedKey>
{
  bool operator()(const MappedKey& a, const MappedKey& b) const
    { return (a.keyval < b.keyval || (a.keyval == b.keyval && a.state < b.state)); }
};

void swap(MappedKey& a, MappedKey& b);

class InputWindow : public Gtk::Window
{
public:
  explicit InputWindow(Controller& controller);
  virtual ~InputWindow();

protected:
  virtual bool on_button_press_event(GdkEventButton* event);
  virtual bool on_key_press_event(GdkEventKey* event);
  virtual bool on_key_release_event(GdkEventKey* event);
  virtual bool on_map_event(GdkEventAny* event);

private:
  typedef std::set<MappedKey, MappedKey::SortPredicate> KeyMap;

  Controller&                   controller_;
  std::vector<KeyMap>           keymaps_;
  Glib::RefPtr<Gtk::StatusIcon> status_icon_;
  HotkeyInfoPtr                 hotkey_;
  unsigned int                  hotkey_val_;
  Gdk::ModifierType             hotkey_mod_;

  void read_keymap_config();
  void bind_hotkey(const Glib::ustring& signature);
  std::string translate_scancode(unsigned int keycode);
  std::string translate_keyval(unsigned int keyval, Gdk::ModifierType state) const;
  void on_status_icon_activate();
};

} // namespace KC

#endif /* !KCMILL_KCKEYB_INPUTWINDOW_H_INCLUDED */
