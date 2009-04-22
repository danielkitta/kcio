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
#include <cairomm/cairomm.h>
#include <gdkmm.h>
#include <gtkmm/actiongroup.h>
#include <gtkmm/statusicon.h>
#include <gtkmm/toggleaction.h>
#include <gtkmm/uimanager.h>
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

template <class T>
inline void swap(GObjectPtr<T>& a, GObjectPtr<T>& b) { a.swap(b); }

class AutoConnection
{
private:
  void*         instance_;
  unsigned long id_;
  // noncopyable
  AutoConnection(const AutoConnection&);
  AutoConnection& operator=(const AutoConnection&);

public:
  AutoConnection() : instance_ (0), id_ (0) {}
  AutoConnection(void* instance, unsigned long id) : instance_ (instance), id_ (id) {}
  ~AutoConnection();
  void swap(AutoConnection& b) { std::swap(instance_, b.instance_); std::swap(id_, b.id_); }
};

inline void swap(AutoConnection& a, AutoConnection& b) { a.swap(b); }

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

  void set_toggle_hotkey(unsigned int accel_key, Gdk::ModifierType accel_mods);

protected:
  virtual void on_realize();
  virtual void on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen);
  virtual bool on_button_press_event(GdkEventButton* event);
  virtual bool on_key_press_event(GdkEventKey* event);
  virtual bool on_key_release_event(GdkEventKey* event);
  virtual bool on_map_event(GdkEventAny* event);
  virtual bool on_expose_event(GdkEventExpose* event);
  virtual bool on_configure_event(GdkEventConfigure* event);
  virtual bool on_delete_event(GdkEventAny* event);

private:
  typedef std::set<MappedKey, MappedKey::SortPredicate> KeyMap;

  Controller&                         controller_;
  std::vector<KeyMap>                 keymaps_;
  Glib::RefPtr<Gtk::StatusIcon>       status_icon_;
  Glib::RefPtr<Gtk::UIManager>        ui_manager_;
  Glib::RefPtr<Gtk::ToggleAction>     action_toggle_;
  GObjectPtr<GtkHotkeyInfo>           hotkey_;
  Cairo::RefPtr<Cairo::ImageSurface>  key_image_;
  Cairo::RefPtr<Cairo::ImageSurface>  logo_image_;
  AutoConnection                      accel_connection_;

  void init_ui_actions();
  void read_keymap_config();
  void bind_hotkey(const Glib::ustring& signature);
  std::string translate_scancode(unsigned int keycode);
  std::string translate_keyval(const GdkEventKey* event);

  void set_rgba_colormap();
  void update_window_shape();
  void on_composited_changed();
  bool on_grab_broken_event(GdkEventGrabBroken* event);
  void on_status_popup_menu(unsigned int button, guint32 activate_time);
  void on_action_toggle();
  void on_action_about();
};

} // namespace KC

#endif /* !KCMILL_KCKEYB_INPUTWINDOW_H_INCLUDED */
