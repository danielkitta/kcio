/*
 * Copyright (c) 2008-2010  Daniel Elstner <daniel.kitta@gmail.com>
 *
 * LibKC is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LibKC is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBKC_LIBKC_H_INCLUDED
#define LIBKC_LIBKC_H_INCLUDED

#include <stdint.h>
#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  KC_FORMAT_ANY = 0,   /* auto-select by file extension */
  KC_FORMAT_TAP = 010, /* TAP format by Arne Fitzenreiter */
  KC_FORMAT_KCC = 020, /* Plain COM files with header */
  KC_FORMAT_KCB = 021,
  KC_FORMAT_SSS = 030, /* HC-BASIC binary tape format */
  KC_FORMAT_TTT = 031,
  KC_FORMAT_UUU = 032
}
KCFileFormat;

#define KC_BASE_FORMAT(format) ((KCFileFormat)((format) & ~07u))

enum { KC_TAP_MAGIC_LEN = 16 };
static const char *const KC_TAP_MAGIC = "\303KC-TAPE by AF. ";

unsigned int  kc_to_wide_char(unsigned char kc)  G_GNUC_CONST;
unsigned char kc_from_wide_char(unsigned int wc) G_GNUC_CONST;

KCFileFormat kc_format_from_name(const char* name) G_GNUC_PURE;
const char* kc_format_name(KCFileFormat format) G_GNUC_PURE;
KCFileFormat kc_format_from_filename(const char* filename) G_GNUC_PURE;
void kc_filename_to_tape(KCFileFormat format, const char* filename, uint8_t* buf);

void kc_exit_error(const char* where) G_GNUC_NORETURN;
int kc_parse_arg_num(const char* arg, double minval, double maxval, double scale);
int kc_parse_arg_int(const char* arg, int minval, int maxval);
KCFileFormat kc_parse_arg_format(const char* arg);

G_END_DECLS

#endif /* !LIBKC_LIBKC_H_INCLUDED */
