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

/* Cleans up old data and scans working directory,
 * putting data into variables passed in arguments.
 */
void scan_dir(const char* wd, struct file_record*** file_list, int* num_files) {
	if (*num_files != 0) {
		for (int i = 0; i < *num_files; i++) {
			free((*file_list)[i]->file_name);
			free((*file_list)[i]->link_path);
			free((*file_list)[i]);
		}
		free(*file_list);
		*file_list = NULL;
		*num_files = 0;
	}
	struct dirent** namelist;
	DIR* dir = opendir(wd);
	*num_files = scandir(wd, &namelist, NULL, alphasort);
	closedir(dir);
	*file_list = malloc(sizeof(struct file_record*) * (*num_files));
	char* path = malloc(PATH_MAX);
	char* link_path = malloc(PATH_MAX);
	for (int i = 0; i < (*num_files); i++) {
		strcpy(path, wd);
		enter_dir(path, namelist[i]->d_name);
		const size_t name_len = strlen(namelist[i]->d_name);
		(*file_list)[i] = malloc(sizeof(struct file_record));
		struct file_record* const fr = (*file_list)[i];
		fr->file_name = malloc(name_len+1); // +1 because cstring
		fr->link_path = NULL;
		memcpy(fr->file_name, namelist[i]->d_name, name_len+1);
		lstat(path, &fr->s);
		switch (fr->s.st_mode & S_IFMT) {
		case S_IFBLK: fr->t = BLOCK; break;
		case S_IFCHR: fr->t = CHARACTER; break;
		case S_IFDIR: fr->t = DIRECTORY; break;
		case S_IFIFO: fr->t = FIFO; break;
		case S_IFLNK: fr->t = LINK; break;
		case S_IFREG: fr->t = REGULAR; break;
		case S_IFSOCK: fr->t = SOCKET; break;
		default: fr->t = UNKNOWN; break;
		}
		if (S_ISLNK(fr->s.st_mode)) {
			memset(link_path, 0, PATH_MAX);
			readlink(path, link_path, PATH_MAX);
			const size_t lp_len = strlen(link_path)+1;
			fr->link_path = malloc(lp_len);
			memcpy(fr->link_path, link_path, lp_len);
		}
		free(namelist[i]);
	}
	free(link_path);
	free(path);
	free(namelist);
}

void delete_file_list(struct file_record*** file_list, int num_files) {
	for (int i = 0; i < num_files; i++) {
		free((*file_list)[i]->file_name);
		free((*file_list)[i]);
	}
	free(*file_list);
}

/* Finds file with given name and returns it's position in file_list
 * If not found, returns -1;
 */
int file_index(struct file_record** fl, int nf, const char* name) {
	for (int i = 0; i < nf; i++) {
		if (strcmp(fl[i]->file_name, name) == 0) {
			return i;
		}
	}
	return -1;
}

int file_move(const char* src, const char* dest) {
	return rename(src, dest);
}

int file_remove(const char* src) {
	syslog(LOG_DEBUG, "file_remove(\"%s\")", src);
	struct stat s;
	int r = lstat(src, &s); 
	if (r == -1) {
		perror("lstat failed");
		abort();
	}
	if ((s.st_mode & S_IFMT) == S_IFDIR) {
		struct file_record** fl = NULL;
		int fn = 0;
		scan_dir(src, &fl, &fn);
		char* path = malloc(PATH_MAX);
		// 2 -> skip . and ..
		for (int i = 2; i < fn; i++) {
			strcpy(path, src);
			enter_dir(path, fl[i]->file_name);
			file_remove(path);
		}
		delete_file_list(&fl, fn);
		free(path);
		r = rmdir(src);
	}
	else {
		r = unlink(src);
	}
	return r;
}

int file_copy(const char* src, const char* dest) {
	FILE* input_file = fopen(src, "rb");
	FILE* output_file = fopen(dest, "wb");
	const size_t buffer_size = 4096;
	char* buffer = malloc(buffer_size);
	int rr, wr;
	int acc = 0;
	while ((rr = fread(buffer, 1, buffer_size, input_file)) != 0) {
		wr = fwrite(buffer, 1, rr, output_file);
		acc += wr;
		syslog(LOG_DEBUG, "copied %d bytes", acc);
		if (wr != rr) {
			syslog(LOG_DEBUG, "something weird");
		}
	}
	fclose(output_file);
	fclose(input_file);
	free(buffer);
	return 0;
}

int dir_make(const char* name) {
	return mkdir(name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}
