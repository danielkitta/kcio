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

#ifndef LIBKC_LIBKC_H_INCLUDED
#define LIBKC_LIBKC_H_INCLUDED

#include <glib.h>

G_BEGIN_DECLS

unsigned int  kc_to_wide_char(unsigned char kc)  G_GNUC_CONST;
unsigned char kc_from_wide_char(unsigned int wc) G_GNUC_CONST;

void kc_exit_error(const char* where) G_GNUC_NORETURN;
int kc_parse_arg_num(const char* arg, double minval, double maxval, double scale);
int kc_parse_arg_int(const char* arg, int minval, int maxval);

G_END_DECLS

#endif /* !LIBKC_LIBKC_H_INCLUDED */
