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

void get_cwd(char b[PATH_MAX]) {
	getcwd(b, PATH_MAX);
}

struct passwd* get_pwd(void) {
	return getpwuid(geteuid());
}

/* dir does not have to be single file, but a path
 * Return values:
 * 0 if operation successful
 * -1 if PATH_MAX would be exceeded; path left unchanged
 */
int enter_dir(char path[PATH_MAX], char dir[PATH_MAX]) {
	/* Apparenlty I have to rewrite entire function,
	 * only to support multiple ".." anywhere in the path.
	 * Symlinks can contain anything;
	 * my '/var/run' points to '../run'
	 */
	if (strcmp(dir, ".") == 0) return 0;
	if (strcmp(dir, "..") == 0) {
		up_dir(path);
	}
	// Relative path './netctl/config'
	else if (dir[0] == '.' && dir[1] == '/') {
		strcpy(path, dir+2);
	}
	// Absolute 'home' path '~/doc/wat.pdf'
	else if (dir[0] == '~') {
		struct passwd* pwd = get_pwd();
		strcpy(path, pwd->pw_dir);
		strcat(path, dir+1);
	}
	// Absolute path '/etc/sth/config'
	else if (dir[0] == '/') {
		strcpy(path, dir);
	}
	// Relative path 'wat/dir/docs'
	else {
		if (strlen(path) + strlen(dir) > PATH_MAX) return -1;
		if (strcmp(path, "/") != 0 ) {
			// The only situation when adding '/' is not desired
			strcat(path, "/");
		}
		strcat(path, dir);
	}
	return 0;
}

/* Basically removes everything after last '/', including that '/'
 * Return values:
 * 0 if operation succesful
 * -1 if path == '/'
 */
int up_dir(char path[PATH_MAX]) {
	if (strcmp(path, "/") == 0) return -1;
	int i;
	for (i = strlen(path); i > 0 && path[i] != '/'; i--) {
		path[i] = 0;
	}
	// At this point i points that last '/'
	// But if it's root, don't delete it
	if (i != 0) {
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
int prettify_path(char path[PATH_MAX], char home[PATH_MAX]) {
	const int hlen = strlen(home);
	const int plen = strlen(path);
	if (memcmp(path, home, hlen) == 0) {
		path[0] = '~';
		memmove(path+1, path+hlen, plen-hlen+1);
		return 0;
	}
	return -1;
}

void current_dir(char path[PATH_MAX], char dir[NAME_MAX]) {
	const int plen = strlen(path);
	int lastslash = 0;
	for (int i = plen-1; i >= 0; i--) {
		if (path[i] == '/') {
			lastslash = i;
			break;
		}
	}
	if (lastslash == 0 && plen == 1) {
		memcpy(dir, "/", 2);
		return;
	}
	memcpy(dir, path+lastslash+1, strlen(path+lastslash));
}

bool path_is_relative(char path[PATH_MAX]) {
	return (path[0] != '/' && path[0] != '~') || (path[0] == '.' && path[1] == '/');
}
