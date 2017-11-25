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

// strtok_r
#ifndef _POSIX_C_SOURCE
	#define _POSIX_C_SOURCE 200809L
#endif

#include <linux/limits.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>

void get_cwd(char*);
struct passwd* get_pwd(void);

int enter_dir(char* const, const char*);
int up_dir(char*);
int prettify_path(char*, char*);
void current_dir(const char*, char*);
bool path_is_relative(const char* const);

int prettify_path_i(const char*, const char*); // TODO tests
int current_dir_i(const char*);

#endif
