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

#ifndef PATH_H
#define PATH_H

#ifndef _DEFAULT_SOURCE
	#define _DEFAULT_SOURCE
#endif

#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>

struct passwd* get_pwd(void);

int append_dir(char* const, const char* const);
int enter_dir(char* const, const char* const);
int up_dir(char* const);
int prettify_path(char* const, const char* const);
void current_dir(const char* const, char* const);
bool path_is_relative(const char* const);

int prettify_path_i(const char* const, const char* const);
int current_dir_i(const char* const);

bool substitute(char* const, const char* const, const char* const);
size_t imb(const char*, const char*);
bool contains(const char* const, const char* const);

#endif
