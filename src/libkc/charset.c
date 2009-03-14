/*
 * Copyright (c) 2008  Daniel Elstner <daniel.kitta@gmail.com>
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

#include <config.h>
#include "libkc.h"

static const unsigned short kc2wchar_table[128] =
{
  L'\u2423', L'\u2B11', L'\u21A4', L'\u0299',
  L'\u25A6', L'\u25A5', L'\u25A4', L'\u2313',
  L'\u2190', L'\u2192', L'\u2193', L'\u2191',
  L'\u21B0', L'\u21B5', L'\u25A9', L'\u222B',
  L'\u21F1', L'\u2912', L'\u2913', L's',
  L'\u2299', L'\u25A7', L'\u21C5', L'\u25A8',
  L'\u21E5', L'\u21E4', L'\u21D2', L'\u2591',
  L'\u029F', L'\u0280', L'c',      L'\u21D0',
  L' ',      L'!',      L'"',      L'#',
  L'$',      L'%',      L'&',      L'\'',
  L'(',      L')',      L'*',      L'+',
  L',',      L'-',      L'.',      L'/',
  L'0',      L'1',      L'2',      L'3',
  L'4',      L'5',      L'6',      L'7',
  L'8',      L'9',      L':',      L';',
  L'<',      L'=',      L'>',      L'?',
  L'@',      L'A',      L'B',      L'C',
  L'D',      L'E',      L'F',      L'G',
  L'H',      L'I',      L'J',      L'K',
  L'L',      L'M',      L'N',      L'O',
  L'P',      L'Q',      L'R',      L'S',
  L'T',      L'U',      L'V',      L'W',
  L'X',      L'Y',      L'Z',      L'\u25A0',
  L'|',      L'\u00AC', L'^',      L'_',
  L'\u00A9', L'a',      L'b',      L'c',
  L'd',      L'e',      L'f',      L'g',
  L'h',      L'i',      L'j',      L'k',
  L'l',      L'm',      L'n',      L'o',
  L'p',      L'q',      L'r',      L's',
  L't',      L'u',      L'v',      L'w',
  L'x',      L'y',      L'z',      L'\u00E4',
  L'\u00F6', L'\u00FC', L'\u00DF', L'\u25A1'
};

static const unsigned char wchar2kc_table[256] =
{
  [L'\0'] = 0x00, [L'\3'] = 0x03, [L'\a']  = 0x07, [L'\b']  = 0x01,
  [L'\n'] = 0x0A, [L'\r'] = 0x0D, [L'\23'] = 0X13, [L'\33'] = 0x1B,
  [L' ']  = 0x20, [L'!']  = 0x21, [L'"']   = 0x22, [L'#']   = 0x23,
  [L'$']  = 0x24, [L'%']  = 0x25, [L'&']   = 0x26, [L'\'']  = 0x27,
  [L'(']  = 0x28, [L')']  = 0x29, [L'*']   = 0x2A, [L'+']   = 0x2B,
  [L',']  = 0x2C, [L'-']  = 0x2D, [L'.']   = 0x2E, [L'/']   = 0x2F,
  [L'0']  = 0x30, [L'1']  = 0x31, [L'2']   = 0x32, [L'3']   = 0x33,
  [L'4']  = 0x34, [L'5']  = 0x35, [L'6']   = 0x36, [L'7']   = 0x37,
  [L'8']  = 0x38, [L'9']  = 0x39, [L':']   = 0x3A, [L';']   = 0x3B,
  [L'<']  = 0x3C, [L'=']  = 0x3D, [L'>']   = 0x3E, [L'?']   = 0x3F,
  [L'@']  = 0x40, [L'A']  = 0x41, [L'B']   = 0x42, [L'C']   = 0x43,
  [L'D']  = 0x44, [L'E']  = 0x45, [L'F']   = 0x46, [L'G']   = 0x47,
  [L'H']  = 0x48, [L'I']  = 0x49, [L'J']   = 0x4A, [L'K']   = 0x4B,
  [L'L']  = 0x4C, [L'M']  = 0x4D, [L'N']   = 0x4E, [L'O']   = 0x4F,
  [L'P']  = 0x50, [L'Q']  = 0x51, [L'R']   = 0x52, [L'S']   = 0x53,
  [L'T']  = 0x54, [L'U']  = 0x55, [L'V']   = 0x56, [L'W']   = 0x57,
  [L'X']  = 0x58, [L'Y']  = 0x59, [L'Z']   = 0x5A, [L'^']   = 0x5E,
  [L'_']  = 0x5F, [L'a']  = 0x61, [L'b']   = 0x62, [L'c']   = 0x63,
  [L'd']  = 0x64, [L'e']  = 0x65, [L'f']   = 0x66, [L'g']   = 0x67,
  [L'h']  = 0x68, [L'i']  = 0x69, [L'j']   = 0x6A, [L'k']   = 0x6B,
  [L'l']  = 0x6C, [L'm']  = 0x6D, [L'n']   = 0x6E, [L'o']   = 0x6F,
  [L'p']  = 0x70, [L'q']  = 0x71, [L'r']   = 0x72, [L's']   = 0x73,
  [L't']  = 0x74, [L'u']  = 0x75, [L'v']   = 0x76, [L'w']   = 0x77,
  [L'x']  = 0x78, [L'y']  = 0x79, [L'z']   = 0x7A, [L'|']   = 0x5C,
  [L'\177']   = 0x1F, [L'\u00A9'] = 0x60,
  [L'\u00AC'] = 0x5D, [L'\u00C4'] = 0x7B,
  [L'\u00D6'] = 0x7C, [L'\u00DC'] = 0x7D,
  [L'\u00DF'] = 0x7E, [L'\u00E4'] = 0x7B,
  [L'\u00F6'] = 0x7C, [L'\u00FC'] = 0x7D
};

#define N_ELEMENTS(array) (sizeof(array) / sizeof((array)[0]))

unsigned int kc_to_wide_char(unsigned int kc)
{
  return kc2wchar_table[kc % N_ELEMENTS(kc2wchar_table)];
}

unsigned int kc_from_wide_char(unsigned int wc)
{
  if (wc < N_ELEMENTS(wchar2kc_table))
    return wchar2kc_table[wc];

  switch (wc)
  {
    case L'\u25A0': return 0x5B;
    case L'\u25A1': return 0x7F;
  }
  return 0;
}
