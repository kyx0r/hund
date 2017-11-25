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

#include "include/path.h"

void get_cwd(char* b) {
	getcwd(b, PATH_MAX);
}

struct passwd* get_pwd(void) {
	return getpwuid(geteuid());
}

/* path[] must be absolute and not prettified
 * dir[] does not have to be single file, can be a path
 * Return values:
 * 0 if operation successful
 * -1 if PATH_MAX would be exceeded; path left unchanged
 *
 * I couldn't find any standard function that would parse path and shorten it.
 */
int enter_dir(char* const path, const char* dir) {
	if (!path_is_relative(dir)) {
		if (dir[0] == '~') {
			struct passwd* pwd = get_pwd();
			strcpy(path, pwd->pw_dir);
			strcat(path, dir+1);
		}
		else {
			strcpy(path, dir);
		}
		return 0;
	}
	const size_t plen = strlen(path);
	/* path[] may contain '/' at the end
	 * enter_dir appends entries with '/' PREpended;
	 * path[] = '/a/b/c/' -> '/a/b/c'
	 * if dir[] contains 'd....' it appends "/d"
	 * If path ends with /, it would be doubled later
	 */
	if (path[plen-1] == '/' && plen > 1) {
		path[plen-1] = 0;
	}
	char* save_ptr = NULL;
	char* dirdup = strdup(dir);
	char* entry = strtok_r(dirdup, "/", &save_ptr);
	while (entry) {
		if (!strcmp(entry, ".")); // Do nothing; Skip the conditional block
		else if (!strcmp(entry, "..")) {
			char* p = path + strlen(path);
			// At this point path never ends with /
			// p points null pointer
			// Go back till nearest /
			while (*p != '/' && p != path) {
				*p = 0;
				p -= 1;
			}
			// loop stopped at /, which must be deleted too
			*p = 0;
		}
		else {
			// Check if PATH_MAX is respected
			if ((strlen(path) + strlen(entry)) < PATH_MAX) {
				if (path[0] == '/' && strlen(path) > 1) {
					// dont prepend / in root directory
					strcat(path, "/");
				}
				strcat(path, entry);
			}
			else {
				return -1;
			}
		}
		entry = strtok_r(NULL, "/", &save_ptr);
	}
	return 0;
}

/* Basically removes everything after last '/', including that '/'
 * Return values:
 * 0 if operation succesful
 * -1 if path == '/'
 */
int up_dir(char* path) {
	if (!strcmp(path, "/")) return -1;
	int i;
	for (i = strlen(path); i > 0 && path[i] != '/'; i--) {
		path[i] = 0;
	}
	// At this point i points that last '/'
	// But if it's root, don't delete it
	if (i) {
		path[i] = 0;
	}
	return 0;
}

/* Finds substring home in path and changes it to ~
 * /home/user/.config becomes ~/.config
 * Return values:
 * 0 if found home in path and changed
 * -1 if not found home in path; path unchanged
 */
int prettify_path(char* path, char* home) {
	const int hlen = strlen(home);
	const int plen = strlen(path);
	if (!memcmp(path, home, hlen)) {
		path[0] = '~';
		memmove(path+1, path+hlen, plen-hlen+1);
		return 0;
	}
	return -1;
}

// Places current directory in dir[]
void current_dir(const char* path, char* dir) {
	const int plen = strlen(path);
	int i = plen-1; // i will point last slash in path
	while (path[i] != '/' && i >= 0) {
		i -= 1;
	}
	if (!i && plen == 1) {
		memcpy(dir, "/", 2);
		return;
	}
	memcpy(dir, path+i+1, strlen(path+i));
}

bool path_is_relative(const char* const path) {
	return (path[0] != '/' && path[0] != '~') || (path[0] == '.' && path[1] == '/');
}

/* Same as current_dir_i()
 * Returns place in buffer, where path after ~ starts
 * So that pretty path is just
 * printf("~%s", path+prettify_path_i(path));
 * If cannot be prettified, returns 0
 */
int prettify_path_i(const char* path, const char* home) {
	const int hlen = strlen(home);
	if (!memcmp(path, home, hlen)) {
		return hlen;
	}
	return 0;
}

/* Instead of populating another buffer,
 * it just points to place in buffer,
 * where current dir's name starts
 */
int current_dir_i(const char* path) {
	const int plen = strlen(path);
	int i = plen-1; // i will point last slash in path
	while (path[i] != '/' && i >= 0) {
		i -= 1;
	}
	return i+1; // i will point last slash in path
}
