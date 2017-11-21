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

bool is_lnk(const char* path) {
	struct stat s;
	int r = lstat(path, &s);
	return (!r && S_ISLNK(s.st_mode));
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

void file_list_clean(struct file_record*** fl, fnum_t* nf) {
	if (!*nf) return;
	for (fnum_t i = 0; i < *nf; ++i) {
		free((*fl)[i]->file_name);
		free((*fl)[i]->link_path);
		if ((*fl)[i]->l != &(*fl)[i]->s) free((*fl)[i]->l);
		free((*fl)[i]);
	}
	free(*fl);
	*fl = NULL;
	*nf = 0;
}

/* Cleans up old data and scans working directory,
 * putting data into variables passed in arguments.
 *
 * On ENOMEM: cleans everything
 * On stat/lstat errors: zeroes failed fields
 *
 * TODO check if fstatat could help
 */
int scan_dir(const char* wd, struct file_record*** fl, fnum_t* nf) {
	syslog(LOG_DEBUG, "scan_dir");
	file_list_clean(fl, nf);
	int r = 0;
	DIR* dir = opendir(wd);
	if (!dir) return errno;
	char fpath[PATH_MAX]; // Stack should be fine with them
	char lpath[PATH_MAX];
	struct dirent* de;
	while ((de = readdir(dir)) != NULL) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
		void* tmp = realloc(*fl, sizeof(struct file_record*) * (*nf+1));
		if (!tmp) {
			r = ENOMEM;
			file_list_clean(fl, nf);
			break;
		}
		*nf += 1;
		*fl = tmp;
		(*fl)[(*nf)-1] = malloc(sizeof(struct file_record));
		struct file_record* nfr = (*fl)[(*nf)-1];
		if (!nfr) {
			r = ENOMEM;
			file_list_clean(fl, nf);
			break;
		}
		nfr->file_name = strdup(de->d_name); // TODO
		nfr->link_path = NULL;
		strcpy(fpath, wd);
		strcat(fpath, "/");
		strcat(fpath, de->d_name);
		if (lstat(fpath, &nfr->s)) memset(&nfr->s, 0, sizeof(struct stat));
		else if (S_ISLNK(nfr->s.st_mode)) {
			nfr->l = malloc(sizeof(struct stat)); // TODO
			if (!nfr->l);
			else if(stat(fpath, nfr->l)) {
				free(nfr->l);
				nfr->l = NULL;
			}
			else {
				const int lp_len = readlink(fpath, lpath, PATH_MAX);
				nfr->link_path = malloc(lp_len+1);
				if (memcpy(nfr->link_path, lpath, lp_len+1)) {
					nfr->link_path[lp_len] = 0;
				}
			}
		}
		else {
			nfr->l = &nfr->s;
		}
	}
	closedir(dir);
	return r;
}

/* Copies link from src to dst */
int link_copy(const char* const src, const char* const dst) {
	if (!src || !dst) return EINVAL;
	struct stat src_s;
	if (lstat(src, &src_s)) return errno;
	if (!S_ISLNK(src_s.st_mode)) return EINVAL;
	char lpath[PATH_MAX];
	const ssize_t ll = readlink(src, lpath, PATH_MAX);
	if (ll == -1) return errno;
	lpath[ll] = 0;
	if (symlink(lpath, dst)) return errno;
	return 0;
}

// Split dst[] to name[] and dst_dir[]
// to check if destination and source are on the same filesystem
// In case they are on the same FS,
// then it's just one library function to call: rename()
int file_move(const char* src, const char* dst) {
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
		if (r) return r;
		r = file_remove(src);
		if (r) return r;
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
			int r = file_remove(path);
			if (r) return r;
		}
		if (fn) { // TODO
			for (fnum_t i = 0; i < fn; i++) {
				free(fl[i]->file_name);
				free(fl[i]->link_path);
				free(fl[i]);
			}
			free(fl);
			fl = NULL;
			fn = 0;
		}
		//delete_file_list(&fl, &fn);
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
		if (fn) { // TODO
			for (fnum_t i = 0; i < fn; i++) {
				free(fl[i]->file_name);
				free(fl[i]->link_path);
				free(fl[i]);
			}
			free(fl);
			fl = NULL;
			fn = 0;
		}
		//delete_file_list(&fl, &fn);
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
