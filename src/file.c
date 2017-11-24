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
	file_list_clean(fl, nf);
	int r = 0;
	DIR* dir = opendir(wd);
	if (!dir) return errno;
	char fpath[PATH_MAX]; // Stack should be fine with them or FIXME?
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

static int _fcmp(const void* p1, const void* p2) {
	const struct file_record* const fr1 = *((struct file_record**) p1);
	const struct file_record* const fr2 = *((struct file_record**) p2);
	return strcmp(fr1->file_name, fr2->file_name);
}

int sort_file_list(struct file_record** fl, fnum_t nf) {
	qsort(fl, nf, sizeof(struct file_record*), _fcmp);
	return 0;
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

int dir_make(const char* path) {
	if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
		return errno;
	}
	return 0;
}
