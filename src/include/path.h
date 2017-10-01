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

// TODO warns about redefinition
#define _XOPEN_SOURCE // strtok_r

#include <linux/limits.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>

void get_cwd(char[PATH_MAX]);
struct passwd* get_pwd(void);

int enter_dir(char [PATH_MAX], char [PATH_MAX]);
int up_dir(char [PATH_MAX]);
int prettify_path(char [PATH_MAX], char [PATH_MAX]);
void current_dir(char [PATH_MAX], char [NAME_MAX]);
bool path_is_relative(char [PATH_MAX]);

#endif
