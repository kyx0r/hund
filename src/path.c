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

/* Appends dir to path.
 * Expects path to be PATH_MAX+1 long
 * and dir to be NAME_MAX long.
 *
 * returns ENAMETOOLONG if buffer would overflow and leaves path unchanged
 * returns 0 if succesful
 */
int append_dir(char* const path, const char* const dir) {
	const size_t pl = strnlen(path, PATH_MAX);
	const size_t dl = strnlen(dir, NAME_MAX);
	if (pl+2+dl > PATH_MAX) return ENAMETOOLONG;
	strncat(path, "/", 2);
	strncat(path, dir, dl+1); // null-terminator
	return 0;
}

/* path[] must be absolute and not prettified
 * dir[] does not have to be single file, can be a path
 * Returns ENAMETOOLONG if PATH_MAX would be exceeded; leaves path unchanged
 *
 * I couldn't find any standard function that would parse path and shorten it.
 */
int enter_dir(char* const path, const char* const dir) {
	if (!path_is_relative(dir)) {
		if (dir[0] == '~') {
			struct passwd* pwd = get_pwd();
			strncpy(path, pwd->pw_dir, PATH_MAX);
			strncat(path, dir+1, PATH_MAX-strnlen(pwd->pw_dir, PATH_MAX));
		}
		else {
			strncpy(path, dir, PATH_MAX);
		}
		return 0;
	}
	const size_t plen = strnlen(path, PATH_MAX);
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
	char* dirdup = strndup(dir, PATH_MAX);
	char* entry = strtok_r(dirdup, "/", &save_ptr);
	char newpath[PATH_MAX+1];
	strncpy(newpath, path, PATH_MAX);
	while (entry) {
		if (!strncmp(entry, ".", 2));
		else if (!strncmp(entry, "..", 3)) {
			char* p = newpath + strnlen(newpath, PATH_MAX);
			// At this point path never ends with /
			// p points null pointer
			// Go back till nearest /
			while (*p != '/' && p != newpath) {
				*p = 0;
				p -= 1;
			}
			// loop stopped at /, which must be deleted too
			*p = 0;
		}
		else {
			// Check if PATH_MAX is respected
			const size_t plen = strnlen(newpath, PATH_MAX);
			if ((plen + strnlen(entry, PATH_MAX) + 1) > PATH_MAX) {
				free(dirdup);
				return ENAMETOOLONG;
			}
			else {
				const size_t elen = strnlen(entry, NAME_MAX);
				if (newpath[0] == '/' && plen > 1) {
					// dont prepend / in root directory
					strncat(newpath, "/", 2);
				}
				strncat(newpath, entry, elen);
			}
		}
		entry = strtok_r(NULL, "/", &save_ptr);
	}
	free(dirdup);
	strncpy(path, newpath, PATH_MAX);
	return 0;
}

/* Basically removes everything after last '/', including that '/'
 * Return values:
 * 0 if operation succesful
 * -1 if path == '/'
 */
int up_dir(char* const path) {
	if (!strncmp(path, "/", 2)) return -1;
	int i;
	for (i = strnlen(path, PATH_MAX); i > 0 && path[i] != '/'; i--) {
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
	const int hlen = strnlen(home, PATH_MAX);
	const int plen = strnlen(path, PATH_MAX);
	if (!memcmp(path, home, hlen)) {
		path[0] = '~';
		memmove(path+1, path+hlen, plen-hlen+1);
		return 0;
	}
	return -1;
}

// Places current directory in dir[]
void current_dir(const char* const path, char* const dir) {
	const int plen = strnlen(path, PATH_MAX);
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
	return (path[0] != '/' && path[0] != '~') ||
		(path[0] == '.' && path[1] == '/');
}

/* Same as current_dir_i()
 * Returns place in buffer, where path after ~ starts
 * So that pretty path is just
 * printf("~%s", path+prettify_path_i(path));
 * If cannot be prettified, returns 0
 */
int prettify_path_i(const char* const path, const char* const home) {
	const size_t hlen = strnlen(home, PATH_MAX);
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

/* Finds SUBString at the begining of STRing and changes it ti REPLacement */
bool substitute(char* const str,
		const char* const subs, const char* const repl) {
	const size_t subs_l = strnlen(subs, PATH_MAX);
	const size_t str_l = strnlen(str, PATH_MAX);
	if (subs_l > str_l) return false;
	if (memcmp(str, subs, subs_l)) return false;
	const size_t repl_l = strnlen(repl, PATH_MAX);
	memmove(str+repl_l, str+subs_l, str_l-subs_l+1);
	memcpy(str, repl, repl_l);
	return true;
}

/* Initial Matching Bytes */
size_t imb(const char* a, const char* b) {
	size_t m = 0;
	while (*a && *b && *a == *b) {
		a += 1;
		b += 1;
		m += 1;
	}
	return m;
}

/* Checks if STRing contains SUBString */
bool contains(const char* const str, const char* const subs) {
	const size_t sl = strnlen(subs, PATH_MAX);
	for (size_t j = 0; strnlen(str+j, PATH_MAX) >= sl; ++j) {
		if (sl == imb(str+j, subs)) return true;
	}
	return false;
}
