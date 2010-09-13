/*
 * Copyright (c) 2010  Daniel Elstner <daniel.kitta@gmail.com>
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
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void
kc_exit_error(const char* where)
{
  perror(where);
  exit(1);
}

/* Parse a floating point number in the format defined by the currently active
 * locale.  Validate the input against the specified range [minval, maxval].
 * Return the result scaled by scale and rounded to the nearest integer.
 */
int
kc_parse_arg_num(const char* arg, double minval, double maxval, double scale)
{
  assert(arg && *arg != '\0');

  char* endptr = 0;
  errno = 0;

  double value = strtod(arg, &endptr);

  if ((value == 0.0 || value == -HUGE_VAL || value == HUGE_VAL) && errno != 0)
  {
    perror(arg);
    exit(1);
  }
  if (!endptr || *endptr != '\0' || !(value >= minval && value <= maxval))
  {
    fprintf(stderr, "%s: argument out of range [%g..%g]\n", arg, minval, maxval);
    exit(1);
  }
  return (int)(value * scale + 0.5);
}

/* Parse an integer from a string in base 10, base 16 or base 8.
 * Validate the input against the specified range [minval, maxval].
 */
int
kc_parse_arg_int(const char* arg, int minval, int maxval)
{
  assert(arg && *arg != '\0');

  char* endptr = 0;
  errno = 0;

  long value = strtol(arg, &endptr, 0);

  if ((value == 0 || value == LONG_MIN || value == LONG_MAX) && errno != 0)
  {
    perror(arg);
    exit(1);
  }
  if (!endptr || *endptr != '\0' || !(value >= minval && value <= maxval))
  {
    fprintf(stderr, "%s: argument out of range [%d..%d]\n", arg, minval, maxval);
    exit(1);
  }
  return value;
}

KCFileFormat
kc_parse_arg_format(const char* arg)
{
  KCFileFormat format = kc_format_from_name(arg);

  if (format == KC_FORMAT_ANY)
  {
    fprintf(stderr, "Unknown file format \"%s\"\n", arg);
    exit(1);
  }
  return format;
}
