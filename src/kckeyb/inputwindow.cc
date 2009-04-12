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
#include "inputwindow.h"
#include <libkc.h>
#include <glibmm.h>
#include <gtkmm/accelgroup.h>
#include <gtkmm/main.h>
#include <algorithm>
#include <functional>
#include <iterator>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gtkhotkey.h>

namespace
{

class LookupMappedKey : public std::unary_function<KC::MappedKey, Glib::ustring>
{
  const Glib::KeyFile& keyfile_;
  const Glib::ustring  group_;

public:
  LookupMappedKey(const Glib::KeyFile& keyfile, const Glib::ustring& group)
    : keyfile_ (keyfile), group_ (group) {}

  KC::MappedKey operator()(const Glib::ustring& keyname) const;
};

KC::MappedKey LookupMappedKey::operator()(const Glib::ustring& keyname) const
{
  KC::MappedKey mapping (Glib::strcompress(keyfile_.get_locale_string(group_, keyname)));
  Gtk::AccelGroup::parse(keyname, mapping.keyval, mapping.state);
  return mapping;
}

struct MappedKeyEqual : public std::binary_function<bool, KC::MappedKey, KC::MappedKey>
{
  bool operator()(const KC::MappedKey& a, const KC::MappedKey& b) const
    { return (a.keyval == b.keyval && a.state == b.state); }
};

} // anonymous namespace

namespace KC
{

void swap(MappedKey& a, MappedKey& b)
{
  std::swap(a.keyval, b.keyval);
  std::swap(a.state, b.state);
  a.sequence.swap(b.sequence);
}

InputWindow::InputWindow(Controller& controller)
:
  Gtk::Window (Gtk::WINDOW_POPUP),
  controller_ (controller),
  keymaps_    (KEYBOARD_COUNT),
  hotkey_     (),
  hotkey_val_ (GDK_VoidSymbol),
  hotkey_mod_ ()
{
  add_events(Gdk::BUTTON_PRESS_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
  set_keep_above(true);
  set_position(Gtk::WIN_POS_CENTER);
  read_keymap_config();
}

InputWindow::~InputWindow()
{}

bool InputWindow::on_button_press_event(GdkEventButton* event)
{
  if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
  {
    Gtk::Main::quit();
    return true; // handled
  }
  return Gtk::Window::on_button_press_event(event);
}

bool InputWindow::on_key_press_event(GdkEventKey* event)
{
  const Gdk::ModifierType state = Gdk::ModifierType(event->state);

  if (event->keyval == hotkey_val_
      && (state & Gtk::AccelGroup::get_default_mod_mask()) == hotkey_mod_)
  {
    hide();
    return true; // handled
  }

  const std::string kcseq = (controller_.get_mode() == KEYBOARD_RAW)
                            ? translate_scancode(event->hardware_keycode)
                            : translate_keyval(event->keyval, state);
  if (!kcseq.empty())
  {
    if (!controller_.send_codes(kcseq))
      get_display()->beep();

    return true; // handled
  }
  return Gtk::Window::on_key_press_event(event);
}

bool InputWindow::on_key_release_event(GdkEventKey* event)
{
  if (controller_.get_mode() == KEYBOARD_RAW)
  {
    const std::string scancode = translate_scancode(event->hardware_keycode);

    if (!scancode.empty())
    {
      controller_.send_break(scancode);
      return true; // handled
    }
  }
  return Gtk::Window::on_key_release_event(event);
}

bool InputWindow::on_map_event(GdkEventAny* event)
{
  gdk_keyboard_grab(Glib::unwrap(get_window()), FALSE, GDK_CURRENT_TIME);

  return Gtk::Window::on_map_event(event);
}

void InputWindow::read_keymap_config()
{
  g_assert(keymaps_.size() == KEYBOARD_COUNT);

  Glib::KeyFile keyfile;
  keyfile.load_from_file("ui/keymap.conf");

  bind_hotkey(keyfile.get_locale_string("global", "hotkey"));

  static const char sections[KEYBOARD_COUNT][8] = { "raw", "caos", "cpm", "tpkc" };

  for (unsigned int i = KEYBOARD_RAW; i < KEYBOARD_COUNT; ++i)
    try
    {
      const Glib::ustring group = sections[i];
      const std::vector<Glib::ustring> keys = keyfile.get_keys(group);

      KeyMap keymap;
      std::transform(keys.begin(), keys.end(), std::inserter(keymap, keymap.end()),
                     LookupMappedKey(keyfile, group));
      keymaps_[i].swap(keymap);
    }
    catch (const Glib::KeyFileError& error)
    {
      const Glib::ustring message = error.what();
      g_warning("%s", message.c_str());
    }
}

void InputWindow::bind_hotkey(const Glib::ustring& signature)
{
  GError* error = 0;

  if (hotkey_)
  {
    if (gtk_hotkey_info_is_bound(hotkey_.get()) // racy...
        && !gtk_hotkey_info_unbind(hotkey_.get(), &error))
      throw Glib::Error(error);

    HotkeyInfoPtr().swap(hotkey_);
  }
  Gtk::AccelGroup::parse(signature, hotkey_val_, hotkey_mod_);

  HotkeyInfoPtr hotkey (gtk_hotkey_info_new("kckeyb", "toggle-grab", signature.c_str(), 0));
  g_return_if_fail(hotkey);

  if (!gtk_hotkey_info_bind(hotkey.get(), &error))
    throw Glib::Error(error);

  g_signal_connect_swapped(hotkey.get(), "activated",
                           G_CALLBACK(&gtk_window_present_with_time), gobj());
  hotkey.swap(hotkey_);
}

std::string InputWindow::translate_scancode(unsigned int keycode)
{
  GdkDisplay *const display = gtk_widget_get_display(Gtk::Widget::gobj());
  unsigned int keyval = GDK_VoidSymbol;

  gdk_keymap_translate_keyboard_state(gdk_keymap_get_for_display(display),
                                      keycode, GdkModifierType(0), 0, &keyval, 0, 0, 0);

  g_assert(KEYBOARD_RAW < keymaps_.size());

  const KeyMap& keymap = keymaps_[KEYBOARD_RAW];
  const KeyMap::const_iterator pos = keymap.find(MappedKey(keyval, Gdk::ModifierType()));

  if (pos != keymap.end())
    return pos->sequence;

  return std::string();
}

std::string InputWindow::translate_keyval(unsigned int keyval, Gdk::ModifierType state) const
{
  unsigned int lower = keyval;
  unsigned int upper = keyval;

  gdk_keyval_convert_case(keyval, &lower, &upper);

  if (lower != upper && (state & Gdk::LOCK_MASK) != 0)
    state ^= Gdk::LOCK_MASK | Gdk::SHIFT_MASK;

  state &= Gtk::AccelGroup::get_default_mod_mask();

  unsigned int mode = controller_.get_mode();
  g_assert(mode < keymaps_.size());
  do
  {
    const KeyMap& keymap = keymaps_[mode];
    const KeyMap::const_iterator pos = keymap.find(MappedKey(lower, state));

    if (pos != keymap.end())
      return pos->sequence;
  }
  while (mode-- == KEYBOARD_TPKC);

  if ((state & ~Gdk::SHIFT_MASK) == 0)
  {
    if (const unsigned int uc = gdk_keyval_to_unicode((state == 0) ? upper : lower))
      if (const unsigned char kc = kc_from_wide_char(uc))
        return std::string(1u, char(kc));
  }
  else if ((state & ~Gdk::SHIFT_MASK) == Gdk::CONTROL_MASK)
  {
    const unsigned int uc = gdk_keyval_to_unicode(upper);

    if ((uc > 0 && uc <= 0x1F) || (uc >= 0x40 && uc <= 0x5F))
      return std::string(1u, char(uc & 0x1F));
  }

  return std::string();
}

} // namespace KC
