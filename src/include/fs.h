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

#ifndef FS_H
#define FS_H

#ifndef _DEFAULT_SOURCE
	#define _DEFAULT_SOURCE
#endif

#include <pwd.h>
#include <grp.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#include "fs.h"

/* From LSB to MSB, by bit index */
static const char* const mode_bit_meaning[] = {
	"execute/sears by others",
	"write by others",
	"read by others",
	"execute/search by group",
	"write by group",
	"read by group",
	"execute/search by user",
	"write by user",
	"read by user",
	"sticky bit",
	"set group ID on execution",
	"set user ID on execution"
};

static const char* const perm2rwx[] = {
	[00] = "---",
	[01] = "--x",
	[02] = "-w-",
	[03] = "-wx",
	[04] = "r--",
	[05] = "r-x",
	[06] = "rw-",
	[07] = "rwx",
};

typedef unsigned int fnum_t; // Number of Files

/*
 * If file is a symlink, l will point heap-allocated stat of the pointed file.
 * Otherwise it will point &s.
 */
struct file_record {
	char* file_name;
	char* link_path;
	struct stat s;
	struct stat* l;
};

bool is_lnk(const char*);
bool is_dir(const char*);
bool same_fs(const char* const, const char* const);
bool executable(const mode_t, const mode_t);

bool file_exists(const char*);
void file_list_clean(struct file_record*** const, fnum_t* const);
int scan_dir(const char* const, struct file_record*** const,
		fnum_t* const, fnum_t* const);

typedef int (*sorting_foo)(const void*, const void*);
int cmp_name_asc(const void*, const void*);
int cmp_name_desc(const void*, const void*);
int cmp_size_asc(const void*, const void*);
int cmp_size_desc(const void*, const void*);
int cmp_date_asc(const void*, const void*);
int cmp_date_desc(const void*, const void*);

void sort_file_list(int (*)(const void*, const void*),
		struct file_record**, const fnum_t);

int link_copy(const char* const, const char* const, const char* const);
int link_copy_raw(const char* const, const char* const);

#define SIZE_BUF_SIZE (3+1+2+1+1)
void pretty_size(off_t, char* buf);

#define MKDIR_DEFAULT_PERM (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)

int append_dir(char* const, const char* const);
int enter_dir(char* const, const char* const);
int up_dir(char* const);
//int prettify_path(char* const, const char* const);
//void current_dir(const char* const, char* const);
bool path_is_relative(const char* const);

int prettify_path_i(const char* const, const char* const);
int current_dir_i(const char* const);

bool substitute(char* const, const char* const, const char* const);
size_t imb(const char*, const char*);
bool contains(const char* const, const char* const);

#endif
