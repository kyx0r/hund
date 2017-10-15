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

static int file_filter(const struct dirent* d) {
	// Skip . and ..
	// Don't need them
	// I'm avoiding strcmp. It would probably be slower.
	return !((d->d_name[0] == '.' && d->d_name[1] == 0) || (d->d_name[0] == '.' && d->d_name[1] == '.' && d->d_name[2] == 0));
}

static int file_sort(const struct dirent** a, const struct dirent** b) {
	// Directories first, then other files
	if ((*a)->d_type == DT_DIR && (*b)->d_type != DT_DIR) return -1;
	else if ((*b)->d_type == DT_DIR && (*a)->d_type != DT_DIR) return 1;
	else return strcmp((*a)->d_name, (*b)->d_name);
}

/* Cleans up old data and scans working directory,
 * putting data into variables passed in arguments.
 */
void scan_dir(const char* wd, struct file_record*** file_list, int* num_files) {
	if (*num_files) {
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
	*num_files = scandir(wd, &namelist, file_filter, file_sort);
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
		fr->file_name = strdup(namelist[i]->d_name);
		fr->link_path = NULL;
		if (!lstat(path, &fr->s)) {
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
				// Readlink does not append NULL terminator,
				// but it returns the length of copied path
				const int lp_len = readlink(path, link_path, PATH_MAX);
				fr->link_path = malloc(lp_len+1);
				memcpy(fr->link_path, link_path, lp_len+1);
				fr->link_path[lp_len] = 0; // NULL terminator
			}
		}
		else {
			syslog(LOG_ERR, "lstat(\"%s\"), called in scan_dir() failed", path);
			fr->t = UNKNOWN;
			continue;
		}
		// I'm keeping d_name, don't need the rest
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
	int i = 0;
	while (i < nf && strcmp(fl[i]->file_name, name)) {
		i += 1;
	}
	return (i != nf) ? i : -1;
}

int file_move(const char* src, const char* dest) {
	return rename(src, dest);
}

int file_remove(const char* src) {
	syslog(LOG_DEBUG, "file_remove(\"%s\")", src);
	struct stat s;
	if (lstat(src, &s)) {
		syslog(LOG_ERR, "lstat failed");
		return errno;
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
			if (file_remove(path)) {
				syslog(LOG_ERR, "recursive call of file_remove(\"%s\") failed", path);
			}
		}
		delete_file_list(&fl, fn);
		free(path);
		return rmdir(src);
	}
	else {
		return unlink(src);
	}
}

int file_copy(const char* src, const char* dest) {
	FILE* input_file = fopen(src, "rb");
	if (!input_file) {
		return -1;
	}
	FILE* output_file = fopen(dest, "wb");
	if (!output_file) {
		fclose(input_file);
		return -1;
	}
	const size_t buffer_size = 4096;
	char* buffer = malloc(buffer_size);
	int rr, wr, acc = 0;
	while ((rr = fread(buffer, 1, buffer_size, input_file)) != 0) {
		wr = fwrite(buffer, 1, rr, output_file);
		if (ferror(output_file)) {
			syslog(LOG_ERR, "file error");
			// An error occured
			// TODO handle
		}
		acc += wr;
		if (wr != rr) {
			syslog(LOG_DEBUG, "something weird");
		}
	}
	if (!feof(input_file) || ferror(input_file)) {
		syslog(LOG_ERR, "file error");
		// An error occured
		// TODO handle
	}
	fclose(output_file);
	fclose(input_file);
	free(buffer);
	return 0;
}

int dir_make(const char* name) {
	return mkdir(name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}
