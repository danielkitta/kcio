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

#include <gdkmm.h>
#include <gtkmm/window.h>
#include <algorithm>
#include <string>
#include <vector>

namespace KC
{

enum KeyboardMode
{
  KEYBOARD_RAW  = 0,
  KEYBOARD_CAOS = 1,
  KEYBOARD_CPM  = 2,
  KEYBOARD_TPKC = 3,
  KEYBOARD_COUNT
};

struct MappedKey
{
  unsigned int      keyval;
  Gdk::ModifierType state;
  std::string       sequence;

  MappedKey() : keyval (0), state (), sequence () {}
  MappedKey(unsigned int k, Gdk::ModifierType s) : keyval (k), state (s), sequence () {}
  explicit MappedKey(const std::string& seq) : keyval (0), state (), sequence (seq) {}
  ~MappedKey() {}

  void swap(MappedKey& b)
    { std::swap(keyval, b.keyval); std::swap(state, b.state); sequence.swap(b.sequence); }
};

inline void swap(MappedKey& a, MappedKey& b) { a.swap(b); }

class InputWindow : public Gtk::Window
{
public:
  InputWindow();
  virtual ~InputWindow();

  sigc::signal<void, std::string>& signal_translated_key() { return signal_translated_key_; }

protected:
  virtual bool on_button_press_event(GdkEventButton* event);
  virtual bool on_key_press_event(GdkEventKey* event);
  virtual bool on_key_release_event(GdkEventKey* event);
  virtual bool on_map_event(GdkEventAny* event);

private:
  typedef std::vector<MappedKey> KeyMap;

  std::vector<KeyMap> keymaps_;
  KeyboardMode        kbdmode_;

  sigc::signal<void, std::string> signal_translated_key_;

  void read_keymap_config();
  std::string translate_keyval(unsigned int keyval, Gdk::ModifierType state) const;
};

} // namespace KC

#endif /* !KCMILL_KCKEYB_INPUTWINDOW_H_INCLUDED */
