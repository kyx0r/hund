/*
 *  Copyright (C) 2017 by Michał Czarnecki <czarnecky@va.pl>
 *
 *  This file is part of the Hund.
 *
 *  The Hund is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  The Hund is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UTF8_H
#define UTF8_H

#include <string.h>
#include <stdbool.h>

typedef char utf8;
typedef unsigned int codepoint_t;

void utf8_cp2b(utf8* const, const codepoint_t);
codepoint_t utf8_b2cp(const utf8* const);
size_t utf8_g2nb(const utf8* const);
size_t utf8_cp2nb(const codepoint_t);
size_t utf8_width(const utf8* const);
size_t utf8_slice_length(const utf8* const, const size_t);
size_t utf8_ng_till(const utf8* const, const utf8* const);
bool utf8_validate(const utf8* const);

void utf8_insert(utf8* const, const utf8* const, const size_t);
void utf8_remove(utf8* const, const size_t);

#endif
