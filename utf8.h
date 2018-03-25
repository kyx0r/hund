/*
 *  Copyright (C) 2017-2018 by Michał Czarnecki <czarnecky@va.pl>
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

#include "widechars.h"

typedef unsigned int codepoint_t;

void utf8_cp2b(char* const, codepoint_t);
codepoint_t utf8_b2cp(const char* const);
size_t utf8_g2nb(const char* const);
size_t utf8_cp2nb(const codepoint_t);
size_t utf8_g2w(const codepoint_t);
size_t utf8_width(const char*);
size_t utf8_w2nb(const char* const, size_t);
size_t utf8_wtill(const char*, const char* const);
bool utf8_validate(const char* const);

void utf8_insert(char*, const char* const, const size_t);
size_t utf8_remove(char* const, const size_t);

unsigned cut_unwanted(const char*, char*, const char, size_t);

bool cp_in(const codepoint_t [][2], size_t, const codepoint_t);

#endif
