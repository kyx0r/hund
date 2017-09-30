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

#include "include/file.h"

void get_cwd(char b[PATH_MAX]) {
	getcwd(b, PATH_MAX);
}

struct passwd* get_pwd(void) {
	return getpwuid(geteuid());
}

/* Cleans up old data and scans working directory,
 * putting data into variables passed in arguments.
 */
void scan_dir(char* wd, struct file_record*** file_list, int* num_files) {
	if (*num_files != 0) {
		for (int i = 0; i < *num_files; i++) {
			free((*file_list)[i]->file_name);
			free((*file_list)[i]);
		}
		free(*file_list);
		*file_list = NULL;
		*num_files = 0;
	}
	struct dirent** namelist;
	DIR* dir = opendir(wd);
	int n = scandir(wd, &namelist, NULL, alphasort);
	closedir(dir);
	*num_files = n;
	*file_list = malloc(sizeof(struct file_record*) * (*num_files));
	char path[PATH_MAX];
	for (int i = 0; i < (*num_files); i++) {
		(*file_list)[i] = malloc(sizeof(struct file_record));
		strcpy(path, wd);
		enter_dir(path, namelist[i]->d_name);
		const size_t name_len = strlen(namelist[i]->d_name);
		(*file_list)[i]->file_name = malloc(name_len+1); // +1 because cstring
		memcpy((*file_list)[i]->file_name, namelist[i]->d_name, name_len+1);
		lstat(path, &(*file_list)[i]->s);
		switch ((*file_list)[i]->s.st_mode & S_IFMT) {
		case S_IFBLK: (*file_list)[i]->t = BLOCK; break;
		case S_IFCHR: (*file_list)[i]->t = CHARACTER; break;
		case S_IFDIR: (*file_list)[i]->t = DIRECTORY; break;
		case S_IFIFO: (*file_list)[i]->t = FIFO; break;
		case S_IFLNK: (*file_list)[i]->t = LINK; break;
		case S_IFREG: (*file_list)[i]->t = REGULAR; break;
		case S_IFSOCK: (*file_list)[i]->t = SOCKET; break;
		default: (*file_list)[i]->t = UNKNOWN; break;
		}
		free(namelist[i]);
	}
	free(namelist);
}

void delete_file_list(struct file_record*** file_list, int num_files) {
	for (int i = 0; i < num_files; i++) {
		free((*file_list)[i]->file_name);
		free((*file_list)[i]);
	}
	free(*file_list);
}


int file_move(const char* src, const char* dest) {
	return rename(src, dest);
}

int file_remove(const char* src) {
	struct stat s;
	int r = lstat(src, &s); 
	if (r == -1) {
		perror("lstat failed");
		return r;
	}
	r = unlink(src);
	if (r == -1) {
		perror("unlink failed");
		return r;
	}
	return r;
}

int file_copy(const char* src, const char* dest) {
	return 0;
}
