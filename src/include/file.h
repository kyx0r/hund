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

#ifndef FILE_H
#define FILE_H

// dirent.h and fnctl.h stuff
#define _GNU_SOURCE

#ifndef _XOPEN_SOURCE
	#define _XOPEN_SOURCE // S_ISSOCK
#endif
//#define _XOPEN_SOURCE_EXTENDED

#include <linux/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <syslog.h>

#include "path.h"

enum file_type {
	BLOCK,
	CHARACTER,
	DIRECTORY,
	FIFO,
	LINK,
	REGULAR,
	SOCKET,
	UNKNOWN
};

struct file_record {
	char* file_name;
	struct stat s;
	enum file_type t;
	char* link_path;
};

void scan_dir(const char*, struct file_record***, int*);
void delete_file_list(struct file_record***, int);
int file_index(struct file_record**, int, const char*);

int file_move(const char*, const char*);
int file_remove(const char*);
int file_copy(const char*, const char*);

int dir_make(const char*);
#endif
