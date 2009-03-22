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
#include <glibmm.h>
#include <gtkmm/accelgroup.h>
#include <algorithm>
#include <functional>
#include <iterator>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

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
  KC::MappedKey mapping (Glib::strcompress(keyfile_.get_string(group_, keyname)));
  Gtk::AccelGroup::parse(keyname, mapping.keyval, mapping.state);
  return mapping;
}

struct MappedKeyLess : std::binary_function<bool, KC::MappedKey, KC::MappedKey>
{
  bool operator()(const KC::MappedKey& a, const KC::MappedKey& b) const
    { return (a.keyval < b.keyval || (a.keyval == b.keyval && a.state < b.state)); }
};

} // anonymous namespace

namespace KC
{

InputWindow::InputWindow()
:
  Gtk::Window(Gtk::WINDOW_POPUP),
  keymaps_ (KEYBOARD_COUNT),
  kbdmode_ (KEYBOARD_CPM)
{
  add_events(Gdk::BUTTON_PRESS_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
  set_keep_above(true);
  set_position(Gtk::WIN_POS_CENTER);

  read_keymap_config();
}

InputWindow::~InputWindow()
{}

bool InputWindow::on_button_press_event(GdkEventButton*)
{
  hide();
  return true; // handled
}

bool InputWindow::on_key_press_event(GdkEventKey* event)
{
  const std::string kcseq = translate_keyval(event->keyval, Gdk::ModifierType(event->state));

  if (!kcseq.empty())
  {
    signal_translated_key_.emit(kcseq);
    return true; // handled
  }
  return Gtk::Window::on_key_press_event(event);
}

bool InputWindow::on_key_release_event(GdkEventKey* event)
{
  return Gtk::Window::on_key_release_event(event);
}

bool InputWindow::on_map_event(GdkEventAny* event)
{
  gdk_keyboard_grab(get_window()->gobj(), FALSE, GDK_CURRENT_TIME);
  return Gtk::Window::on_map_event(event);
}

void InputWindow::read_keymap_config()
{
  g_assert(keymaps_.size() == KEYBOARD_COUNT);

  Glib::KeyFile keyfile;
  keyfile.load_from_file("ui/keymap.conf");

  static const char *const sections[KEYBOARD_COUNT] = { 0, "CAOS", "CPM", "TPKC" };

  for (int i = KEYBOARD_CAOS; i < KEYBOARD_COUNT; ++i)
  {
    const Glib::ustring group = sections[i];
    KeyMap keymap;

    if (i > KEYBOARD_CPM)
      keymap = keymaps_[KEYBOARD_CPM];

    try
    {
      const std::vector<Glib::ustring> keys = keyfile.get_keys(group);

      std::transform(keys.begin(), keys.end(), std::back_inserter(keymap),
                     LookupMappedKey(keyfile, group));

      std::sort(keymap.begin(), keymap.end(), MappedKeyLess());
    }
    catch (const Glib::KeyFileError& error)
    {
      const Glib::ustring message = error.what();
      g_warning("%s", message.c_str());
    }

    keymaps_[i].swap(keymap);
  }
}

std::string InputWindow::translate_keyval(unsigned int keyval, Gdk::ModifierType state) const
{
  unsigned int lowup[2] = { keyval, keyval };
  gdk_keyval_convert_case(keyval, &lowup[0], &lowup[1]);

  if (lowup[0] != lowup[1] && (state & Gdk::LOCK_MASK) != 0)
    state ^= Gdk::LOCK_MASK | Gdk::SHIFT_MASK;

  state &= Gtk::AccelGroup::get_default_mod_mask();

  g_assert(unsigned(kbdmode_) < keymaps_.size());

  const KeyMap& keymap = keymaps_[kbdmode_];
  const KeyMap::const_iterator pos = std::lower_bound(keymap.begin(), keymap.end(),
                                                      MappedKey(lowup[0], state),
                                                      MappedKeyLess());
  if (pos != keymap.end() && pos->keyval == keyval && pos->state == state)
    return pos->sequence;

  if ((state & ~Gdk::SHIFT_MASK) == Gdk::CONTROL_MASK)
  {
    const unsigned int uc = gdk_keyval_to_unicode(lowup[1]);

    if ((uc > 0 && uc <= 0x1F) || (uc >= 0x40 && uc <= 0x5F))
      return std::string(1u, char(uc & 0x1F));
  }
  else if ((state & ~Gdk::SHIFT_MASK) == 0)
  {
    if (const unsigned int uc = gdk_keyval_to_unicode(lowup[(state & Gdk::SHIFT_MASK) == 0]))
      try
      {
        return Glib::convert(Glib::ustring(1, uc).raw(),
                             (kbdmode_ == KEYBOARD_CAOS) ? "ISO646-DE" : "CP437",
                             "UTF-8");
      }
      catch (const Glib::ConvertError&)
      {}
  }

  return std::string();
}

} // namespace KC
