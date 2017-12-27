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

#include "include/fs.h"

bool is_lnk(const char* path) {
	struct stat s;
	return (!lstat(path, &s) && S_ISLNK(s.st_mode));
}

bool is_dir(const char* path) {
	struct stat s;
	return (!stat(path, &s) && S_ISDIR(s.st_mode));
}

/*
 * Checks of two files are on the same filesystem.
 * If they are, moving files (and even whole directories)
 * can be done with just rename() function.
 */
bool same_fs(const char* const a, const char* const b) {
	struct stat sa, sb;
	return !stat(a, &sa) && !stat(b, &sb) && (sa.st_dev == sb.st_dev);
}

bool executable(const mode_t m, const mode_t n) {
	return (m & 0111) != 0 || (n & 0111) != 0; // TODO ???
}

void file_list_clean(struct file_record*** const fl, fnum_t* const nf) {
	if (!*nf) return;
	for (fnum_t i = 0; i < *nf; ++i) {
		free((*fl)[i]->file_name);
		free((*fl)[i]->link_path);
		if ((*fl)[i]->l != &(*fl)[i]->s) {
			free((*fl)[i]->l);
		}
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
	if (!dir) {
		//closedir(dir); // TODO ???
		return errno;
	}
	char fpath[PATH_MAX+1];
	char lpath[PATH_MAX+1];
	struct dirent* de;
	while ((de = readdir(dir)) != NULL) {
		*nf += 1;
	}
	*nf -= 2; // skip . and ..
	if (!*nf) {
		*fl = NULL;
		closedir(dir);
		return r;
	}
	rewinddir(dir);
	void* tmp = calloc(*nf, sizeof(struct file_record*));
	if (!tmp) {
		closedir(dir);
		return ENOMEM;
	}
	*fl = tmp;
	fnum_t gf = 0;
	while ((de = readdir(dir)) != NULL) {
		if (!strncmp(de->d_name, ".", 2) ||
		    !strncmp(de->d_name, "..", 3)) {
			continue;
		}
		if (de->d_name[0] == '.') {
			*nhf += 1;
		}
		struct file_record* nfr = malloc(sizeof(struct file_record));
		if (!nfr) {
			r = ENOMEM;
			*nhf = 0;
			file_list_clean(fl, nf);
			break;
		}
		(*fl)[gf] = nfr;
		gf += 1;
		const size_t namelen = strnlen(de->d_name, NAME_MAX);
		nfr->file_name = malloc(namelen+1);
		nfr->dir_volume = -1;
		nfr->selected = false;
		if (!nfr->file_name) {
			r = ENOMEM;
			*nhf = 0;
			file_list_clean(fl, nf);
			break;
		}
		strncpy(nfr->file_name, de->d_name, namelen+1);
		nfr->link_path = NULL;
		if (append_dir(strncpy(fpath, wd, PATH_MAX), nfr->file_name));
		else if (lstat(fpath, &nfr->s)) {
			memset(&nfr->s, 0, sizeof(struct stat));
		}
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

// TODO how to sort non-dirs?
int cmp_size_asc(const void* p1, const void* p2) {
	const struct file_record* const fr1 = *((struct file_record**) p1);
	const struct file_record* const fr2 = *((struct file_record**) p2);
	const ssize_t s1 = (fr1->dir_volume != -1 ?
			fr1->dir_volume : fr1->s.st_size);
	const ssize_t s2 = (fr2->dir_volume != -1 ?
			fr2->dir_volume : fr2->s.st_size);
	return s1 > s2;
}

// TODO
int cmp_size_desc(const void* p1, const void* p2) {
	const struct file_record* const fr1 = *((struct file_record**) p1);
	const struct file_record* const fr2 = *((struct file_record**) p2);
	const ssize_t s1 = (fr1->dir_volume != -1 ?
			fr1->dir_volume : fr1->s.st_size);
	const ssize_t s2 = (fr2->dir_volume != -1 ?
			fr2->dir_volume : fr2->s.st_size);
	return s1 < s2;
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
	// TODO merge sort, because it would respect previous order
	qsort(fl, nf, sizeof(struct file_record*), cmp);
}

/*
 * Copies link from src to dst, relative to wd
 * all paths must be absolute.
 * There are two instances, when symlink path is copied raw:
 * - when it is absolute
 * - when target is within copy operation
 *
 * TODO detect loops/recursion
 * TODO lots of testing
 * FIXME 2*PATH_MAX
 *
 * Working Directory - root of the operation
 * Source, Destination
 */
int link_copy(const char* const wd,
		const char* const src, const char* const dst) {
	struct stat src_s;
	if (!wd || !src || !dst) return EINVAL;
	if (lstat(src, &src_s)) return errno;
	if (!S_ISLNK(src_s.st_mode)) return EINVAL;

	char lpath[PATH_MAX+1];
	const ssize_t lpath_len = readlink(src, lpath, PATH_MAX);
	if (lpath_len == -1) return errno;
	lpath[lpath_len] = 0;
	if (access(lpath, F_OK)) return ENOENT;
	if (!path_is_relative(lpath)) {
		return (symlink(lpath, dst) ? errno : 0);
	}
	else {
		const size_t src_dir_off = current_dir_i(src)-1;
		const size_t src_dir_len = strnlen(src+src_dir_off, PATH_MAX);
		char* target = malloc(src_dir_len+lpath_len+1);
		strncpy(target, src, src_dir_len);
		target[src_dir_off] = 0;

		int err;
		if ((err = enter_dir(target, lpath))) {
			free(target);
			return err;
		}

		if (contains(target, wd)) {
			free(target);
			return (symlink(lpath, dst) ? errno : 0);
		}

		const size_t dst_dir_off = current_dir_i(dst)-1;
		const size_t dst_dir_len = strnlen(dst+dst_dir_off, PATH_MAX);
		char* _dst = malloc(dst_dir_len+1);
		strncpy(_dst, dst, dst_dir_off);
		_dst[dst_dir_off] = 0;

		char newlpath[PATH_MAX+1];
		newlpath[0] = 0;

		while (!contains(target, _dst)) {
			up_dir(_dst);
			strncat(newlpath, "../", 4);
		}
		strcat(newlpath, target+strnlen(_dst, PATH_MAX)+1); // TODO
		free(_dst);
		free(target);
		return (symlink(newlpath, dst) ? errno : 0);
	}
	//return 0; // unreachable
}

int link_copy_raw(const char* const src, const char* const dst) {
	char lpath[PATH_MAX+1];
	const ssize_t ll = readlink(src, lpath, PATH_MAX);
	if (ll == -1) return errno;
	lpath[ll] = 0;
	return (symlink(lpath, dst) ? errno : 0);
}

void pretty_size(off_t s, char* const buf) {
	static const char units[] = { 'B', 'K', 'M', 'G', 'T', 'P', 'E', 'Z' };
	const char* unit = units;
	unsigned rest = 0;
	while (s >= 1024) {
		rest = s % 1024;
		s /= 1024;
		unit += 1;
	}
	rest *= 1000;
	rest /= 1024;
	rest /= 10;
	char r[2] = { '0' + (rest / 10), '0' + (rest % 10) };
	if (r[1] != '0') snprintf(buf, SIZE_BUF_SIZE, "%u.%c%c%c", (unsigned)s, r[0], r[1], *unit);
	else if (r[0] != '0') snprintf(buf, SIZE_BUF_SIZE, "%u.%c%c", (unsigned)s, r[0], *unit);
	else snprintf(buf, SIZE_BUF_SIZE, "%u%c", (unsigned)s, *unit);
}

/*
 * Appends dir to path.
 * Expects path to be PATH_MAX+1 long
 * and dir to be NAME_MAX long.
 *
 * returns ENAMETOOLONG if buffer would overflow and leaves path unchanged
 * returns 0 if succesful or dir is empty string
 */
int append_dir(char* const path, const char* const dir) {
	const size_t dl = strnlen(dir, NAME_MAX);
	if (!dl) return 0;
	const size_t pl = strnlen(path, PATH_MAX);
	if (pl+2+dl > PATH_MAX) return ENAMETOOLONG;
	strcat(path, "/");
	strncat(path, dir, dl+1);
	return 0;
}

/*
 * path[] must be absolute and not prettified
 * dir[] does not have to be single file, can be a path
 * Returns ENAMETOOLONG if PATH_MAX would be exceeded; leaves path unchanged
 */
int enter_dir(char* const path, const char* const dir) {
	if (!path_is_relative(dir)) {
		if (dir[0] == '~') {
			struct passwd* pwd = getpwuid(geteuid());
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

/*
 * Basically removes everything after last '/', including that '/'
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
/*int prettify_path(char* const path, const char* const home) {
	const int hlen = strnlen(home, PATH_MAX);
	const int plen = strnlen(path, PATH_MAX);
	if (!memcmp(path, home, hlen)) {
		path[0] = '~';
		memmove(path+1, path+hlen, plen-hlen+1);
		return 0;
	}
	return -1;
}*/

// Places current directory in dir[]
/*void current_dir(const char* const path, char* const dir) {
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
}*/

bool path_is_relative(const char* const path) {
	return (path[0] != '/' && path[0] != '~') ||
		(path[0] == '.' && path[1] == '/');
}

/*
 * Same as current_dir_i()
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

/*
 * Instead of populating another buffer,
 * it just points to place in buffer,
 * where current dir's name starts
 */
int current_dir_i(const char* const path) {
	const int plen = strnlen(path, PATH_MAX);
	int i = plen-1; // i will point last slash in path
	while (path[i] != '/' && i >= 0) {
		i -= 1;
	}
	return i+1; // i will point last slash in path
}

/*
 * Finds SUBString at the begining of STRing and changes it ti REPLacement
 */
bool substitute(char* const str,
		const char* const subs, const char* const repl) {
	const size_t subs_l = strnlen(subs, PATH_MAX);
	const size_t str_l = strnlen(str, PATH_MAX);
	if (subs_l > str_l || memcmp(str, subs, subs_l)) return false;
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

char* list_push(struct string_list* const list, const char* const s) {
	void* tmp = realloc(list->str, (list->len+1) * sizeof(char*));
	if (!tmp) return NULL;
	list->str = tmp;
	const size_t slen = strnlen(s, NAME_MAX);
	list->str[list->len] = malloc(slen+1); // TODO
	memcpy(list->str[list->len], s, slen+1);
	list->str[list->len][slen] = 0;
	list->len += 1;
	return list->str[list->len - 1];
}

/*
 * Reads file from fd and forms a list of lines
 *
 * TODO flexible buffer length
 * TODO more testing
 */
int file_to_list(const int fd, struct string_list* const list) {
	list->str = NULL;
	list->len = 0;
	char name[NAME_MAX+1];
	size_t nlen = 0, top = 0;
	ssize_t rd = 0;
	char* nl;
	if (lseek(fd, 0, SEEK_SET) == -1) return errno;
	memset(name, 0, sizeof(name));
	for (;;) {
		rd = read(fd, name+top, NAME_MAX+1-top);
		if (rd == -1) {
			int e = errno;
			free_list(list);
			return e;
		}
		if (!rd && !*name) break;
		nl = memchr(name, '\n', sizeof(name));
		if (!nl && !(nl = memchr(name, 0, sizeof(name)))) {
			free_list(list);
			return ENAMETOOLONG;
		}
		*nl = 0;
		nlen = nl-name;
		void* tmp_str = realloc(list->str, (list->len+1) * sizeof(char*));
		if (!tmp_str) {
			int e = errno;
			free_list(list);
			return e;
		}
		list->str = tmp_str;
		if (nlen) {
			list->str[list->len] = strncpy(malloc(nlen+1), name, nlen+1);
		}
		else {
			list->str[list->len] = NULL;
		}
		top = NAME_MAX+1-(nlen+1);
		memmove(name, name+nlen+1, top);
		memset(name+top, 0, nlen+1);
		list->len += 1;
	}
	return 0;
}

int list_to_file(const struct string_list* const list, int fd) {
	if (lseek(fd, 0, SEEK_SET)) return errno;
	char name[NAME_MAX+1];
	for (fnum_t i = 0; i < list->len; ++i) {
		const size_t len = strnlen(list->str[i], NAME_MAX);
		memcpy(name, list->str[i], len);
		name[len] = '\n';
		if (write(fd, name, len+1) <= 0) return errno;
	}
	return 0;
}

void list_copy(struct string_list* const dst_list,
		const struct string_list* const src_list) {
	dst_list->len = src_list->len;
	dst_list->str = malloc(dst_list->len*sizeof(char*));
	for (fnum_t i = 0; i < dst_list->len; ++i) {
		dst_list->str[i] = strdup(src_list->str[i]); // TODO
	}
}

void free_list(struct string_list* const list) {
	if (!list->str) return;
	for (fnum_t i = 0; i < list->len; ++i) {
		if (list->str[i]) free(list->str[i]);
	}
	free(list->str);
	list->str = NULL;
	list->len = 0;
}

bool blank_lines(const struct string_list* const list) {
	for (fnum_t i = 0; i < list->len; ++i) {
		if (!list->str[i]) return true;
	}
	return false;
}

/* Tells if there are duplicates on list */
bool duplicates_on_list(const struct string_list* const list) {
	for (fnum_t f = 0; f < list->len; ++f) {
		for (fnum_t g = 0; g < list->len; ++g) {
			if (f == g) continue;
			if (list->str[f]
			    && list->str[g]
				&& !strcmp(list->str[f], list->str[g])) return true;
		}
	}
	return false;
}
