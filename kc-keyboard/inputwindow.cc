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
#include "inputwindow.h"
#include <libkc.h>
#include <libkcui.h>
#include <glibmm.h>
#include <cairomm/cairomm.h>
#include <gtkmm/accelgroup.h>
#include <gtkmm/accelmap.h>
#include <gtkmm/main.h>
#include <gtkmm/stock.h>
#include <algorithm>
#include <functional>
#include <iterator>
#include <cstring>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gtkhotkey.h>
#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>

namespace
{

static const double rounding_radius = 8.0;

struct MappedKeyEqual : public std::binary_function<bool, KC::MappedKey, KC::MappedKey>
{
  bool operator()(const KC::MappedKey& a, const KC::MappedKey& b) const
    { return (a.keyval == b.keyval && a.state == b.state); }
};

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

static Cairo::RefPtr<Cairo::ImageSurface>
render_image_surface(const std::string& basename, Cairo::Format format)
{
  const std::string filename = Util::locate_data_file(basename);
  GError* error = 0;
  const KC::GObjectPtr<RsvgHandle> svg (rsvg_handle_new_from_file(filename.c_str(), &error));

  if (!svg)
  {
    g_warning("%s", error->message);
    return Cairo::RefPtr<Cairo::ImageSurface>();
  }
  RsvgDimensionData dims = { 0, 0, 0.0, 0.0 };
  rsvg_handle_get_dimensions(svg.get(), &dims);

  const Cairo::RefPtr<Cairo::ImageSurface>
    surface = Cairo::ImageSurface::create(format, dims.width, dims.height);

  const Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create(surface);
  rsvg_handle_render_cairo(svg.get(), context->cobj());

  return surface;
}

static void path_rounded_rectangle(const Cairo::RefPtr<Cairo::Context>& context,
                                   double x0, double y0, double x1, double y1, double r)
{
  const double s = 0.73478351016045599078453650314;
  const double t = 0.48042959726148712607318499348;
  const double u = 0.10535684036541782512512628231;
  const double v = 0.292893218813452475599155637895;

  context->move_to(x1 - r, y0);
  context->curve_to(x1 - s*r, y0, x1 - t*r, y0 + u*r, x1 - v*r, y0 + v*r);
  context->curve_to(x1 - u*r, y0 + t*r, x1, y0 + s*r, x1, y0 + r);

  context->line_to(x1, y1 - r);
  context->curve_to(x1, y1 - s*r, x1 - u*r, y1 - t*r, x1 - v*r, y1 - v*r);
  context->curve_to(x1 - t*r, y1 - u*r, x1 - s*r, y1, x1 - r, y1);

  context->line_to(x0 + r, y1);
  context->curve_to(x0 + s*r, y1, x0 + t*r, y1 - u*r, x0 + v*r, y1 - v*r);
  context->curve_to(x0 + u*r, y1 - t*r, x0, y1 - s*r, x0, y1 - r);

  context->line_to(x0, y0 + r);
  context->curve_to(x0, y0 + s*r, x0 + u*r, y0 + t*r, x0 + v*r, y0 + v*r);
  context->curve_to(x0 + t*r, y0 + u*r, x0 + s*r, y0, x0 + r, y0);

  context->close_path();
}

static void warn_on_keyfile_error(const Glib::KeyFileError& error)
{
  if (error.code() != Glib::KeyFileError::GROUP_NOT_FOUND)
  {
    const Glib::ustring message = error.what();
    g_warning("%s", message.c_str());
  }
}

extern "C"
{
static void on_accel_toggle_changed(GtkAccelMap*, char* accel_path,
                                    unsigned int accel_key, GdkModifierType accel_mods,
                                    void* user_data)
{
  try
  {
    g_return_if_fail(std::strcmp(accel_path, "<KC-Keyboard>/Toggle") == 0);

    KC::InputWindow& input_window = *static_cast<KC::InputWindow*>(user_data);
    input_window.set_toggle_hotkey(accel_key, Gdk::ModifierType(accel_mods));
  }
  catch (...)
  {
    Glib::exception_handlers_invoke();
  }
}
} // extern "C"

} // anonymous namespace

namespace KC
{

AutoConnection::~AutoConnection()
{
  if (id_ != 0)
    g_signal_handler_disconnect(instance_, id_);
}

void swap(MappedKey& a, MappedKey& b)
{
  std::swap(a.keyval, b.keyval);
  std::swap(a.state, b.state);
  a.sequence.swap(b.sequence);
}

InputWindow::InputWindow(Controller& controller)
:
  Gtk::Window    (Gtk::WINDOW_POPUP),
  controller_    (controller),
  keymaps_       (KEYBOARD_COUNT),
  status_icon_   (Gtk::StatusIcon::create("kc-keyboard")),
  ui_manager_    (Gtk::UIManager::create()),
  action_toggle_ (Gtk::ToggleAction::create("Toggle", "_Toggle keyboard"))
{
  add_events(Gdk::EXPOSURE_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK
             | Gdk::STRUCTURE_MASK);
  set_app_paintable(true);
  set_rgba_colormap();

  signal_composited_changed().connect(sigc::mem_fun(*this, &InputWindow::on_composited_changed));
  signal_grab_broken_event() .connect(sigc::mem_fun(*this, &InputWindow::on_grab_broken_event));

  status_icon_->signal_activate() // this hack is fugly and it's Murray's fault
    .connect(sigc::mem_fun(*action_toggle_.operator->(), &Gtk::Action::activate));
  status_icon_->signal_popup_menu()
    .connect(sigc::mem_fun(*this, &InputWindow::on_status_popup_menu));

  status_icon_->set_tooltip("KC-Keyboard");
  set_title("KC-Keyboard");
  set_size_request(500, 250);
  set_position(Gtk::WIN_POS_CENTER);

  init_ui_actions();
  read_keymap_config();
}

InputWindow::~InputWindow()
{}

void InputWindow::set_toggle_hotkey(unsigned int accel_key, Gdk::ModifierType accel_mods)
{
  GError* error = 0;

  if (hotkey_)
  {
    GObjectPtr<GtkHotkeyInfo> hotkey;
    hotkey.swap(hotkey_);

    if (gtk_hotkey_info_is_bound(hotkey.get()) // racy...
        && !gtk_hotkey_info_unbind(hotkey.get(), &error))
      g_warning("%s", error->message);
  }

  if (accel_key != GDK_VoidSymbol)
  {
    const Glib::ustring signature = Gtk::AccelGroup::name(accel_key, accel_mods);
    GObjectPtr<GtkHotkeyInfo> hotkey (gtk_hotkey_info_new("KC-Keyboard", "Toggle",
                                                          signature.c_str(), 0));
    g_return_if_fail(hotkey);

    if (!gtk_hotkey_info_bind(hotkey.get(), &error))
      g_warning("%s", error->message);

    g_signal_connect_swapped(hotkey.get(), "activated",
                             G_CALLBACK(&gtk_action_activate),
                             Glib::unwrap(action_toggle_));
    hotkey.swap(hotkey_);
  }
}

void InputWindow::on_realize()
{
  key_image_  = render_image_surface("enter-key.svg", Cairo::FORMAT_ARGB32);
  logo_image_ = render_image_surface("keyboard-logo.svg", Cairo::FORMAT_A8);

  Gtk::Window::on_realize();

  update_window_shape();
}

void InputWindow::on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen)
{
  if (has_screen())
  {
    set_rgba_colormap();
    status_icon_->set_screen(get_screen());
  }
  Gtk::Window::on_screen_changed(previous_screen);
}

void InputWindow::set_rgba_colormap()
{
  if (!is_realized() && has_screen())
  {
    const Glib::RefPtr<Gdk::Screen> screen = get_screen();
    const Glib::RefPtr<Gdk::Colormap> colormap =
      (screen->is_composited()) ? screen->get_rgba_colormap() : screen->get_default_colormap();

    g_return_if_fail(colormap);
    set_colormap(colormap);
  }
}

void InputWindow::update_window_shape()
{
  const Glib::RefPtr<Gdk::Window> window = get_window();

  if (is_composited())
  {
    window->unset_shape_combine_mask();
    window->input_shape_combine_region(Gdk::Region(), 0, 0);
  }
  else
  {
    gdk_window_input_shape_combine_region(window->gobj(), 0, 0, 0);

    int width  = 0;
    int height = 0;
    window->get_size(width, height);

    // There is no Gdk::Bitmap::create() to create an empty bitmap in gdkmm.
    const Glib::RefPtr<Gdk::Bitmap> bitmap = Glib::RefPtr<Gdk::Bitmap>
      ::cast_dynamic(Glib::wrap(gdk_pixmap_new(window->gobj(), width, height, 1), false));
    {
      const Cairo::RefPtr<Cairo::Context> context = bitmap->create_cairo_context();

      context->set_operator(Cairo::OPERATOR_CLEAR);
      context->set_source_rgba(0.0, 0.0, 0.0, 0.0);
      context->paint();

      path_rounded_rectangle(context, 0.0, 0.0, width, height, rounding_radius + 1.0);
      context->set_operator(Cairo::OPERATOR_SOURCE);
      context->set_source_rgba(1.0, 1.0, 1.0, 1.0);
      context->fill();
    }
    window->shape_combine_mask(bitmap, 0, 0);
  }
}

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
  const std::string kcseq = (controller_.get_mode() == KEYBOARD_RAW)
                            ? translate_scancode(event->hardware_keycode)
                            : translate_keyval(event);
  if (!kcseq.empty())
  {
    if (!controller_.send_key_codes(kcseq))
      error_bell();

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
      if (controller_.break_enabled_for_key(*scancode.begin()))
        controller_.send_key_codes('\xF0' + scancode);

      return true; // handled
    }
  }
  return Gtk::Window::on_key_release_event(event);
}

bool InputWindow::on_map_event(GdkEventAny* event)
{
  if (!Gtk::Window::on_map_event(event))
  {
    const Gdk::GrabStatus status = get_window()->keyboard_grab(false, gtk_get_current_event_time());

    if (status != Gdk::GRAB_SUCCESS)
    {
      g_warning("Failed to acquire keyboard grab (status code %d)", int(status));
      hide();
      return true; // stop emission
    }
  }
  return Gtk::Window::on_map_event(event);
}

bool InputWindow::on_expose_event(GdkEventExpose*)
{
  const double fill_r = 0.75;
  const double fill_g = 0.71875;
  const double fill_b = 0.625;

  const bool compositing = is_composited();
  const Cairo::RefPtr<Cairo::Context> context = get_window()->create_cairo_context();

  if (compositing)
  {
    context->set_source_rgba(0.0, 0.0, 0.0, 0.0);
    context->set_operator(Cairo::OPERATOR_CLEAR);
  }
  else
  {
    context->set_source_rgba(fill_r, fill_g, fill_b, 0.75);
    context->set_operator(Cairo::OPERATOR_SOURCE);
  }
  context->paint();

  const int width  = get_width();
  const int height = get_height();

  path_rounded_rectangle(context, 1.0, 1.0, width - 1, height - 1, rounding_radius);

  if (compositing)
  {
    context->set_source_rgba(fill_r, fill_g, fill_b, 0.625);
    context->set_operator(Cairo::OPERATOR_SOURCE);
    context->fill_preserve();
  }
  context->set_source_rgba(1.0, 1.0, 1.0, 0.625);
  context->stroke();

  context->set_operator(Cairo::OPERATOR_OVER);

  if (key_image_)
  {
    context->set_source(key_image_, width  - 28 - key_image_->get_width(),
                                    height - 26 - key_image_->get_height());
    context->paint_with_alpha(0.875);
  }
  if (logo_image_)
  {
    context->set_source_rgba(0.0, 0.0, 0.0, 0.75);
    context->mask(logo_image_, 32.0, 32.0);
  }
  return true;
}

bool InputWindow::on_configure_event(GdkEventConfigure* event)
{
#if 0
  if (event->width != get_width() || event->height != get_height())
    update_window_shape();
#endif
  return Gtk::Window::on_configure_event(event);
}

bool InputWindow::on_delete_event(GdkEventAny*)
{
  action_toggle_->set_active(false);
  return true;
}

void InputWindow::on_composited_changed()
{
  set_rgba_colormap();

  if (is_realized())
    update_window_shape();
}

bool InputWindow::on_grab_broken_event(GdkEventGrabBroken* event)
{
  if (event->keyboard && event->window != event->grab_window
      && action_toggle_->get_active())
  {
    g_warning("Keyboard grab broken involuntarily");
    hide();
    return true;
  }
  return false;
}

void InputWindow::init_ui_actions()
{
  GtkAccelMap *const accel_map = gtk_accel_map_get();

  AutoConnection accel_connection (accel_map,
      g_signal_connect(accel_map, "changed::<KC-Keyboard>/Toggle",
                       G_CALLBACK(&on_accel_toggle_changed), this));

  const Glib::RefPtr<Gtk::ActionGroup> group = Gtk::ActionGroup::create("KC-Keyboard");

  group->add(action_toggle_,
             Gtk::AccelKey(GDK_Return, Gdk::SUPER_MASK, "<KC-Keyboard>/Toggle"),
             sigc::mem_fun(*this, &InputWindow::on_action_toggle));

  group->add(Gtk::Action::create("Quit", Gtk::Stock::QUIT),
             Gtk::AccelKey(GDK_q, Gdk::SUPER_MASK, "<KC-Keyboard>/Quit"),
             &Gtk::Main::quit);

  group->add(Gtk::Action::create("About", Gtk::Stock::ABOUT),
             sigc::mem_fun(*this, &InputWindow::on_action_about));

  ui_manager_->insert_action_group(group);
  add_accel_group(ui_manager_->get_accel_group());

  ui_manager_->add_ui_from_string("<popup name='Menu'>"
                                    "<menuitem action='Toggle'/>"
                                    "<menuitem action='About'/>"
                                    "<menuitem action='Quit'/>"
                                  "</popup>");
  accel_connection_.swap(accel_connection);
}

void InputWindow::read_keymap_config()
{
  Glib::KeyFile keyfile;
  keyfile.load_from_file(Util::locate_data_file("keymap.conf"));

  try
  {
    const Glib::ustring group = "Hotkeys";
    const std::vector<Glib::ustring> keys = keyfile.get_keys(group);

    for (std::vector<Glib::ustring>::const_iterator p = keys.begin(); p != keys.end(); ++p)
    {
      const Glib::ustring signature  = keyfile.get_value(group, *p);
      unsigned int        accel_key  = GDK_VoidSymbol;
      Gdk::ModifierType   accel_mods = Gdk::ModifierType(0);

      Gtk::AccelGroup::parse(signature, accel_key, accel_mods);
      Gtk::AccelMap::change_entry("<KC-Keyboard>/" + *p, accel_key, accel_mods, true);
    }
  }
  catch (const Glib::KeyFileError& error)
  {
    warn_on_keyfile_error(error);
  }

  static const char sections[KEYBOARD_COUNT][8] = { "Raw", "CAOS", "CP/M", "TPKC" };
  g_assert(keymaps_.size() == KEYBOARD_COUNT);

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
      warn_on_keyfile_error(error);
    }
}

std::string InputWindow::translate_scancode(unsigned int keycode)
{
  GdkDisplay *const display = gtk_widget_get_display(Gtk::Widget::gobj());
  unsigned int keyval = GDK_VoidSymbol;

  gdk_keymap_translate_keyboard_state(gdk_keymap_get_for_display(display),
                                      keycode, GdkModifierType(0), 0, &keyval, 0, 0, 0);
  g_assert(!keymaps_.empty());

  const KeyMap& keymap = keymaps_[KEYBOARD_RAW];
  const KeyMap::const_iterator pos = keymap.find(MappedKey(keyval, Gdk::ModifierType()));

  if (pos != keymap.end())
    return pos->sequence;

  return std::string();
}

std::string InputWindow::translate_keyval(const GdkEventKey* event)
{
  const Gdk::ModifierType modmask = Gtk::AccelGroup::get_default_mod_mask();
  Gdk::ModifierType       state   = Gdk::ModifierType(event->state);
  unsigned int            keyval  = event->keyval;

  if ((state & Gdk::MOD2_MASK) == 0 || !(keyval >= GDK_KP_Multiply && keyval <= GDK_KP_Divide))
  {
    unsigned int mode = controller_.get_mode();
    g_assert(mode < keymaps_.size());
    do
    {
      const KeyMap& keymap = keymaps_[mode];
      const KeyMap::const_iterator pos = keymap.find(MappedKey(keyval, state & modmask));

      if (pos != keymap.end())
        return pos->sequence;
    }
    while (mode-- == KEYBOARD_TPKC);
  }
  gdk_keymap_translate_keyboard_state(get_display()->get_keymap(),
                                      event->hardware_keycode,
                                      GdkModifierType((state ^ Gdk::LOCK_MASK) | Gdk::MOD2_MASK),
                                      event->group, &keyval, 0, 0, 0);

  if (const unsigned int uc = gdk_keyval_to_unicode(keyval))
    switch (state & modmask & ~Gdk::SHIFT_MASK)
    {
      case 0:
        if (const unsigned char kc = kc_from_wide_char(uc))
          return std::string(1, char(kc));
        break;
      case Gdk::CONTROL_MASK:
        if (uc < 0x80)
          return std::string(1, char(uc & 0x1F));
        break;
      default:
        break;
    }
  return std::string();
}

void InputWindow::on_action_toggle()
{
  const unsigned int event_time = gtk_get_current_event_time();

  if (action_toggle_->get_active())
    present(event_time);
  else
  {
    get_display()->keyboard_ungrab(event_time);
    hide();
  }
}

void InputWindow::on_action_about()
{
  g_printerr("%s\n", __PRETTY_FUNCTION__);
}

void InputWindow::on_status_popup_menu(unsigned int button, guint32 activate_time)
{
  if (Gtk::Menu *const menu = dynamic_cast<Gtk::Menu*>(ui_manager_->get_widget("/Menu")))
  {
    // Prevent the popup menu from breaking the input window's keyboard grab.
    menu->set_take_focus(!action_toggle_->get_active());
    status_icon_->popup_menu_at_position(*menu, button, activate_time);
  }
}

} // namespace KC