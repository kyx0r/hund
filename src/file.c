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

/* Checks of two files are on the same filesystem.
 * If they are, moving files (and even whole directories)
 * can be done with just rename() function.
 */
bool same_fs(const char* const a, const char* const b) {
	struct stat sa, sb;
	if (stat(a, &sa)) return errno;
	if (stat(b, &sb)) return errno;
	return sa.st_dev == sb.st_dev;
}

bool executable(const mode_t m, const mode_t n) {
	return (m & 0111) != 0 || (n & 0111) != 0;
}

bool file_exists(const char* path) {
	return !access(path, F_OK);
}

void file_list_clean(struct file_record*** const fl, fnum_t* const nf) {
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
 * TODO test
 */
int scan_dir(const char* const wd, struct file_record*** const fl,
		fnum_t* const nf, fnum_t* const nhf) {
	file_list_clean(fl, nf);
	*nhf = 0;
	int r = 0;
	DIR* dir = opendir(wd);
	if (!dir) return errno;
	static char fpath[PATH_MAX+1];
	static char lpath[PATH_MAX+1];
	struct dirent* de;
	while ((de = readdir(dir)) != NULL) {
		if (!strncmp(de->d_name, ".", 2) ||
		    !strncmp(de->d_name, "..", 3)) continue;
		if (de->d_name[0] == '.') *nhf += 1;
		void* tmp = realloc(*fl, sizeof(struct file_record*) * ((*nf)+1));
		if (!tmp) {
			r = ENOMEM;
			*nhf = 0;
			file_list_clean(fl, nf);
			break;
		}
		*nf += 1;
		*fl = tmp;
		(*fl)[(*nf)-1] = malloc(sizeof(struct file_record));
		struct file_record* nfr = (*fl)[(*nf)-1];
		if (!nfr) {
			r = ENOMEM;
			*nhf = 0;
			file_list_clean(fl, nf);
			break;
		}
		nfr->file_name = strndup(de->d_name, NAME_MAX);
		nfr->link_path = NULL;
		if (append_dir(strncpy(fpath, wd, PATH_MAX), de->d_name));
		else if (lstat(fpath, &nfr->s)) memset(&nfr->s, 0, sizeof(struct stat));
		else if (S_ISLNK(nfr->s.st_mode)) {
			nfr->l = malloc(sizeof(struct stat));
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

int cmp_name_asc(const void* p1, const void* p2) {
	const struct file_record* const fr1 = *((struct file_record**) p1);
	const struct file_record* const fr2 = *((struct file_record**) p2);
	return strcmp(fr1->file_name, fr2->file_name);
}

int cmp_name_desc(const void* p1, const void* p2) {
	const struct file_record* const fr1 = *((struct file_record**) p1);
	const struct file_record* const fr2 = *((struct file_record**) p2);
	return strcmp(fr2->file_name, fr1->file_name);
}

int cmp_size_asc(const void* p1, const void* p2) {
	const struct file_record* const fr1 = *((struct file_record**) p1);
	const struct file_record* const fr2 = *((struct file_record**) p2);
	return fr1->s.st_size > fr2->s.st_size;
}

int cmp_size_desc(const void* p1, const void* p2) {
	const struct file_record* const fr1 = *((struct file_record**) p1);
	const struct file_record* const fr2 = *((struct file_record**) p2);
	return fr1->s.st_size < fr2->s.st_size;
}

int cmp_date_asc(const void* p1, const void* p2) {
	const struct file_record* const fr1 = *((struct file_record**) p1);
	const struct file_record* const fr2 = *((struct file_record**) p2);
	return fr1->s.st_mtim.tv_sec > fr2->s.st_mtim.tv_sec;
}

int cmp_date_desc(const void* p1, const void* p2) {
	const struct file_record* const fr1 = *((struct file_record**) p1);
	const struct file_record* const fr2 = *((struct file_record**) p2);
	return fr1->s.st_mtim.tv_sec < fr2->s.st_mtim.tv_sec;
}

void sort_file_list(int (*cmp)(const void*, const void*),
		struct file_record** fl, const fnum_t nf) {
	qsort(fl, nf, sizeof(struct file_record*), cmp);
}

/* Copies link from src to dst, relative to wd
 * all paths must be absolute.
 * There are two instances, when symlink path is copied raw:
 * - when it is absolute
 * - when target is within copy operation
 *
 * TODO detect/handle invalid links
 * TODO detect loops/recursion
 * TODO lots of testing
 * FIXME 4*PATH_MAX ~= 32786B - a lot
 *
 * Working Directory - root of the operation
 * Source, Destination */
int link_copy(const char* const wd,
		const char* const src, const char* const dst) {
	if (!wd || !src || !dst) return EINVAL;
	struct stat src_s;
	if (lstat(src, &src_s)) return errno;
	if (!S_ISLNK(src_s.st_mode)) return EINVAL;
	char lpath[PATH_MAX+1];
	const ssize_t ll = readlink(src, lpath, PATH_MAX);
	if (ll == -1) return errno;
	lpath[ll] = 0;
	if (!path_is_relative(lpath)) {
		if (symlink(lpath, dst)) return errno;
	}
	else {
		char target[PATH_MAX+1];
		const size_t sl = current_dir_i(src)-1;
		strncpy(target, src, sl); target[sl] = 0;
		enter_dir(target, lpath);

		if (contains(target, wd)) {
			if (symlink(lpath, dst)) return errno;
			return 0;
		}

		char _dst[PATH_MAX+1];
		const size_t pl = current_dir_i(dst)-1;
		strncpy(_dst, dst, pl); _dst[pl] = 0;

		char newlpath[PATH_MAX+1] = { 0 };
		while (!contains(target, _dst)) {
			up_dir(_dst);
			strcat(newlpath, "../");
		}
		strcat(newlpath, target+strnlen(_dst, PATH_MAX)+1); // TODO
		if (symlink(newlpath, dst)) return errno;
	}
	return 0;
}

int dir_make(const char* path) {
	if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
		return errno;
	}
	return 0;
}

void pretty_size(off_t s, char* const buf) {
	static const char units[] = { 'B', 'K', 'M', 'G', 'T', 'P', 'E', 'Z' };
	const char* unit = units;
	unsigned rest = 0;
	while (s > 1024) {
		rest = s % 1024;
		s /= 1024;
		unit += 1;
	}
	if (rest > 100) rest /= 10;
	if (!(rest % 10)) rest /= 10;
	if (rest > 100) rest /= 10;
	if (!(rest % 10)) rest /= 10;
	if (rest) snprintf(buf, SIZE_BUF_SIZE, "%u.%u%c", (unsigned)s, rest, *unit);
	else snprintf(buf, SIZE_BUF_SIZE, "%u%c", (unsigned)s, *unit);
}
