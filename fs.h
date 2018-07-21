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

#ifndef LOGIN_NAME_MAX
	#define LOGIN_NAME_MAX _SC_LOGIN_NAME_MAX
#endif

#define PATH_BUF_SIZE (PATH_MAX)
#define PATH_MAX_LEN (PATH_MAX-1)

#define NAME_BUF_SIZE (NAME_MAX+1)
#define NAME_MAX_LEN (NAME_MAX)

#define LOGIN_BUF_SIZE (LOGIN_NAME_MAX+1)
#define LOGIN_MAX_LEN (LOGIN_NAME_MAX)

#define MIN(A,B) (((A) < (B)) ? (A) : (B))

#define S_ISTOOSPECIAL(M) (((M & S_IFMT) == S_IFBLK) \
		|| ((M & S_IFMT) == S_IFCHR) \
		|| ((M & S_IFMT) == S_IFIFO) \
		|| ((M & S_IFMT) == S_IFSOCK))

#define DOTDOT(N) (!strncmp((N), ".", 2) || !strncmp((N), "..", 3))
#define EXECUTABLE(M) ((M & 0111) && S_ISREG(M))
#define PATH_IS_RELATIVE(P) ((P)[0] != '/' && (P)[0] != '~')

#define MKDIR_DEFAULT_PERM (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)

#define UPPERCASE(C) ('A' <= (C) && (C) <= 'Z')
#define LOWERCASE(C) ('a' <= (C) && (C) <= 'z')

typedef unsigned int fnum_t; // Number of Files

/* From LSB to MSB, by bit index */
static const char* const mode_bit_meaning[] = {
	"execute/search by others",
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

char* xstrlcpy(char*, const char*, size_t);

char* get_home(void);

int relative_chmod(const char* const, const mode_t, const mode_t);

bool same_fs(const char* const, const char* const);

struct file {
	struct stat s;
	bool selected;
	unsigned char nl;
	char name[];
};

void file_list_clean(struct file*** const, fnum_t* const);
int scan_dir(const char* const, struct file*** const,
		fnum_t* const, fnum_t* const);


int link_copy_recalculate(const char* const,
		const char* const, const char* const);
int link_copy_raw(const char* const, const char* const);

#define SIZE_BUF_SIZE (5+1)
/*
 * Possible formats:
 * XU
 * XXU
 * XXXU
 * X.XU
 * X.XXU
 */
void pretty_size(off_t, char* buf);

int pushd(char* const, size_t* const, const char* const, size_t);
void popd(char* const, size_t* const);

int cd(char* const, size_t* const, const char* const, size_t);

int prettify_path_i(const char* const);
int current_dir_i(const char* const);

size_t imb(const char*, const char*);
bool contains(const char* const, const char* const);


struct string {
	unsigned char len;
	char str[];
};

struct string_list {
	struct string** arr;
	fnum_t len;
};

fnum_t list_push(struct string_list* const, const char* const, size_t);
void list_copy(struct string_list* const, const struct string_list* const);
void list_free(struct string_list* const);

int file_to_list(const int, struct string_list* const);
int list_to_file(const struct string_list* const, int);
fnum_t string_on_list(const struct string_list* const, const char* const, size_t);
fnum_t blank_lines(const struct string_list* const);
bool duplicates_on_list(const struct string_list* const);

#endif
