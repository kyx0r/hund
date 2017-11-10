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
	return !((d->d_name[0] == '.' && d->d_name[1] == 0) ||
			(d->d_name[0] == '.' && d->d_name[1] == '.' && d->d_name[2] == 0));
}

static int file_sort(const struct dirent** a, const struct dirent** b) {
	// Directories first, then other files
	if ((*a)->d_type == DT_DIR && (*b)->d_type != DT_DIR) return -1;
	else if ((*b)->d_type == DT_DIR && (*a)->d_type != DT_DIR) return 1;
	else return strcmp((*a)->d_name, (*b)->d_name);
}

/* Checks if that thing pointed by path is a directory
 * If path points to link, then pointed file is checked
 */
bool is_dir(const char* path) {
	struct stat s;
	int r = stat(path, &s);
	return (!r && S_ISDIR(s.st_mode));
}

bool file_exists(const char* path) {
	return !access(path, F_OK);
}

/* Cleans up old data and scans working directory,
 * putting data into variables passed in arguments.
 */
int scan_dir(const char* wd, struct file_record*** file_list,
		fnum_t* num_files) {
	delete_file_list(file_list, num_files);
	struct dirent** namelist;
	DIR* dir = opendir(wd);
	int r = scandir(wd, &namelist, file_filter, file_sort);
	if (r == -1) return errno;
	if (closedir(dir)) return errno;
	*num_files = r;
	*file_list = malloc(sizeof(struct file_record*) * (*num_files));
	char* path = malloc(PATH_MAX);
	char* link_path = malloc(PATH_MAX);
	for (fnum_t i = 0; i < (*num_files); ++i) {
		strcpy(path, wd);
		enter_dir(path, namelist[i]->d_name);
		(*file_list)[i] = malloc(sizeof(struct file_record));
		struct file_record* const fr = (*file_list)[i];
		fr->file_name = strdup(namelist[i]->d_name);
		fr->link_path = NULL;
		if (lstat(path, &fr->s)) {
			for (fnum_t j = 0; j <= i; ++j) {
				free((*file_list)[i]->file_name);
				if ((*file_list)[i]->link_path) {
					free((*file_list)[i]->link_path);
				}
				free((*file_list)[i]);
			}
			*num_files = 0;
			*file_list = NULL;
			return errno;
		}
		else if (S_ISLNK(fr->s.st_mode)) {
			// Readlink does not append NULL terminator,
			// but it returns the length of copied path
			const int lp_len = readlink(path, link_path, PATH_MAX);
			fr->link_path = malloc(lp_len+1);
			memcpy(fr->link_path, link_path, lp_len+1);
			fr->link_path[lp_len] = 0; // NULL terminator
		}
		// I'm keeping d_name, don't need the rest
		free(namelist[i]);
	}
	free(link_path);
	free(path);
	free(namelist);
	return 0;
}

void delete_file_list(struct file_record*** file_list, fnum_t* num_files) {
	if (*num_files) {
		for (fnum_t i = 0; i < *num_files; i++) {
			free((*file_list)[i]->file_name);
			free((*file_list)[i]->link_path);
			free((*file_list)[i]);
		}
		free(*file_list);
		*file_list = NULL;
		*num_files = 0;
	}
}

/* Finds file with given name and returns it's position in file_list
 * If not found, leaves SELection unchanged
 */
void file_index(struct file_record** fl, fnum_t nf,
		const char* const name, fnum_t* sel) {
	fnum_t i = 0;
	while (i < nf && strcmp(fl[i]->file_name, name)) {
		i += 1;
	}
	if (i != nf) {
		*sel = i;
	}
}

#define abs(V) ((V) < 0 ? -(V) : (V))
#define min(A,B) ((A) > (B) ? (B) : (A))
void file_find(struct file_record** fl, fnum_t nf,
		const char* const name, fnum_t* sel, fnum_t start) {
	fnum_t bm = *sel; // Best Match
	unsigned int bmv = ~0; // Best Match Value
	for (fnum_t i = start; i < nf; ++i) {
		for (size_t j = 0; strlen(fl[i]->file_name+j); ++j) {
			size_t s = min(strlen(fl[i]->file_name+j), strlen(name));
			unsigned int c = abs(strncmp(fl[i]->file_name+j, name, s));
			if (c < bmv) {
				bmv = c;
				bm = i;
			}
		}
	}
	if (!bmv) {
		syslog(LOG_DEBUG, "found: %s", fl[bm]->file_name);
		*sel = bm;
	}
}
#undef abs
#undef min

int file_move(const char* src, const char* dst) {
	// Split dst[] to name[] and dst_dir[]
	// to check if destination and source are on the same filesystem
	// In case they are on the same FS,
	// then it's just one library function to call: rename()
	char* name = malloc(NAME_MAX);
	current_dir(dst, name);
	char* dst_dir = strdup(dst);
	up_dir(dst_dir);

	struct stat src_stat, dst_stat;
	int sse, dse; // Source Stat Error, Destination Stat Error
	if ((sse = lstat(src, &src_stat)) || (dse = lstat(dst_dir, &dst_stat))) {
		return errno;
	}
	if (src_stat.st_dev == dst_stat.st_dev) {
		// Within single filesystem, moving is easy
		if (rename(src, dst)) return errno;
	}
	else {
		int r;
		r = file_copy(src, dst);
		if (r) return errno;
		r = file_remove(src);
		if (r) return errno;
	}
	free(name);
	free(dst_dir);
	return 0;
}

/* On failure, last errno value is returned.
 * (All functions that may fail here, return errno)
 */
int file_remove(const char* src) {
	struct stat s;
	if (lstat(src, &s)) return errno;
	if (S_ISDIR(s.st_mode)) {
		struct file_record** fl = NULL;
		fnum_t fn = 0;
		scan_dir(src, &fl, &fn);
		char* path = malloc(PATH_MAX);
		for (fnum_t i = 0; i < fn; ++i) {
			// TODO minimalize copying paths if possible
			strcpy(path, src);
			enter_dir(path, fl[i]->file_name);
			int r;
			if ((r = file_remove(path))) return r;
		}
		delete_file_list(&fl, &fn);
		free(path);
		if (rmdir(src)) return errno;
	}
	else if (unlink(src)) return errno;
	return 0;
}

int file_copy(const char* src, const char* dest) {
	struct stat srcs;
	if (lstat(src, &srcs)) return errno;
	if (S_ISDIR(srcs.st_mode)) {
		dir_make(dest);
		struct file_record** fl = NULL;
		fnum_t fn = 0;
		scan_dir(src, &fl, &fn);
		char* src_path = malloc(PATH_MAX);
		char* dst_path = malloc(PATH_MAX);
		for (fnum_t i = 0; i < fn; ++i) {
			strcpy(src_path, src);
			strcpy(dst_path, dest);
			enter_dir(src_path, fl[i]->file_name);
			enter_dir(dst_path, fl[i]->file_name);
			int r;
			if ((r = file_copy(src_path, dst_path))) {
				return r;
			}
		}
		delete_file_list(&fl, &fn);
		free(src_path);
		free(dst_path);
	}
	else {
		FILE* input_file = fopen(src, "rb");
		if (!input_file) return errno;
		FILE* output_file = fopen(dest, "wb");
		if (!output_file) {
			int e = errno;
			fclose(input_file);
			// TODO fclose() can fail and set errno; how to handle that?
			return e;
		}
		char* buffer = malloc(BUFSIZ);
		int rr, wr, acc = 0;
		while ((rr = fread(buffer, 1, BUFSIZ, input_file)) != 0) {
			wr = fwrite(buffer, 1, rr, output_file);
			if (ferror(output_file) || wr != rr) return -1; // TODO
			acc += wr;
		}
		if (!feof(input_file) || ferror(input_file)) return -1; // TODO
		fclose(output_file);
		fclose(input_file);
		free(buffer);
	}
	return 0;
}

int dir_make(const char* path) {
	if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
		return errno;
	}
	return 0;
}
