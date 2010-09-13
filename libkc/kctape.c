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

#include <build/config.h>
#include "libkc.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wchar.h>

static const char extensions[][4] =
{
  "KCB", "KCC", "SSS", "TAP", "TTT", "UUU"
};

static const unsigned char formats[] =
{
  KC_FORMAT_KCB, KC_FORMAT_KCC, KC_FORMAT_SSS,
  KC_FORMAT_TAP, KC_FORMAT_TTT, KC_FORMAT_UUU
};

KCFileFormat
kc_format_from_name(const char* name)
{
  assert(name != 0);

  for (size_t i = 0; i < sizeof formats; ++i)
  {
    if (strcasecmp(name, extensions[i]) == 0)
      return formats[i];
  }
  return KC_FORMAT_ANY;
}

const char*
kc_format_name(KCFileFormat format)
{
  for (size_t i = 0; i < sizeof formats; ++i)
  {
    if (format == formats[i])
      return extensions[i];
  }
  return 0;
}

KCFileFormat
kc_format_from_filename(const char* filename)
{
  if (filename)
  {
    size_t len = strlen(filename);

    if (len >= 4 && filename[len - 4] == '.')
      return kc_format_from_name(&filename[len - 3]);
  }
  return KC_FORMAT_ANY;
}

void
kc_filename_to_tape(KCFileFormat format, const char* filename, uint8_t* buf)
{
  assert(filename != 0);
  assert(buf != 0);

  const char* extension = kc_format_name(format);

  if (KC_BASE_FORMAT(format) == KC_FORMAT_SSS)
  {
    unsigned int sig = (unsigned char)extension[0] | 0x80u;

    for (size_t i = 0; i < 3; ++i)
      *buf++ = sig;
  }
  else
    memcpy(&buf[8], extension, 3);

  const char* dirsep = strrchr(filename, '/');

  if (dirsep)
    filename = dirsep + 1;

  size_t length = strlen(filename);

  if (length > 4 && filename[length - 4] == '.')
    length -= 4;

  size_t offset = 0;
  wchar_t wc = L'\0';
  mbstate_t state;

  memset(&state, 0, sizeof state);

  for (size_t i = 0; i < 8; ++i)
  {
    unsigned int c = 0x20;

    if (length > 0)
    {
      size_t nbytes = mbrtowc(&wc, filename + offset, length, &state);

      if (nbytes != (size_t)-1 && nbytes != (size_t)-2)
      {
        offset += nbytes;
        length -= nbytes;

        c = kc_from_wide_char(wc);

        if (c >= 0x61 && c <= 0x7A)
          c &= ~0x20u; // ASCII uppercase
      }
      else
        length = 0;
    }
    buf[i] = c;
  }
}
