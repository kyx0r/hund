/*
 *  Copyright (C) 2017-2018 by Michał Czarnecki <czarnecky@va.pl>
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

#include "fs.h"

/*
 * Chmods given file using plus and minus masks
 * The following should be true: (plus & minus) == 0
 * (it's not checked)
 */
int relative_chmod(const char* const file,
		const mode_t plus, const mode_t minus) {
	struct stat s;
	if (stat(file, &s)) return errno;
	mode_t p = s.st_mode & 07777;
	p |= plus;
	p &= ~minus;
	return (chmod(file, p) ? errno : 0);
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

/*
 * Cleans up list created by scan_dir()
 */
void file_list_clean(struct file*** const fl, fnum_t* const nf) {
	if (!*nf) return;
	for (fnum_t i = 0; i < *nf; ++i) {
		free((*fl)[i]);
	}
	free(*fl);
	*fl = NULL;
	*nf = 0;
}

/*
 * Cleans up old data and scans working directory,
 * putting data into variables passed in arguments.
 *
 * On ENOMEM: cleans everything
 * TODO test
 *
 * wd = Working Directory
 * fl = File List
 * nf = Number of Files
 * nhf = Number of Hidden Files
 */
int scan_dir(const char* const wd, struct file*** const fl,
		fnum_t* const nf, fnum_t* const nhf) {
	file_list_clean(fl, nf);
	*nhf = 0;
	int err = 0;
	DIR* dir = opendir(wd);
	if (!dir) return errno;
	char fpath[PATH_BUF_SIZE];
	memset(fpath, 0, sizeof(fpath));
	struct dirent* de;
	while ((de = readdir(dir)) != NULL) {
		*nf += 1;
	}
	*nf -= 2; // skip . and ..
	if (!*nf) {
		*fl = NULL;
		closedir(dir);
		return err;
	}
	rewinddir(dir);
	void* tmp = calloc(*nf, sizeof(struct file*));
	if (!tmp) {
		closedir(dir);
		return ENOMEM;
	}
	*fl = tmp;
	fnum_t gf = 0; // Gathered Files
	while ((de = readdir(dir)) != NULL) {
		if (DOTDOT(de->d_name)) continue;
		if (de->d_name[0] == '.') {
			*nhf += 1;
		}
		const size_t nl = strnlen(de->d_name, NAME_MAX_LEN);
		struct file* nfr = malloc(sizeof(*nfr)+nl+1);
		if (!nfr) {
			err = ENOMEM;
			*nhf = 0;
			file_list_clean(fl, nf);
			break;
		}
		(*fl)[gf] = nfr;
		gf += 1;
		nfr->nl = (unsigned char)nl;
		nfr->dir_volume = -1;
		nfr->selected = false;
		strncpy(nfr->name, de->d_name, nl+1);
		strncpy(fpath, wd, PATH_BUF_SIZE);
		if ((err = append_dir(fpath, nfr->name))
		   || lstat(fpath, &nfr->s)) {
			err = errno; // TODO
			memset(&nfr->s, 0, sizeof(struct stat));
		}
	}
	closedir(dir);
	return err;
}

/*
 * Copies link from src to dst, relative to wd
 * all paths must be absolute.
 * There are two instances, when symlink path is copied raw:
 * - when it is absolute
 * - when target is within copy operation // TODO don't do that
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

	char lpath[PATH_BUF_SIZE];
	const ssize_t lpath_len = readlink(src, lpath, sizeof(lpath));
	if (lpath_len == -1) return errno;
	lpath[lpath_len] = 0;
	if (access(lpath, F_OK)) return errno;
	if (!path_is_relative(lpath)) {
		return (symlink(lpath, dst) ? errno : 0);
	}
	else {
		const size_t src_dir_off = current_dir_i(src)-1;
		const size_t src_dir_len = strnlen(src+src_dir_off, PATH_MAX_LEN);
		char* target = malloc(src_dir_len+lpath_len+1);
		strncpy(target, src, src_dir_len);
		target[src_dir_off] = 0;

		int err;
		if ((err = cd(target, lpath))) {
			free(target);
			return err;
		}

		if (contains(target, wd)) {
			free(target);
			return (symlink(lpath, dst) ? errno : 0);
		}

		const size_t dst_dir_off = current_dir_i(dst)-1;
		const size_t dst_dir_len = strnlen(dst+dst_dir_off, PATH_MAX_LEN);
		char* _dst = malloc(dst_dir_len+1);
		strncpy(_dst, dst, dst_dir_off);
		_dst[dst_dir_off] = 0;

		char newlpath[PATH_BUF_SIZE];
		memset(newlpath, 0, sizeof(newlpath));

		while (!contains(target, _dst)) {
			up_dir(_dst);
			strncat(newlpath, "../", 4);
		}
		strcat(newlpath, target+strnlen(_dst, PATH_MAX_LEN)+1); // TODO
		free(_dst);
		free(target);
		return (symlink(newlpath, dst) ? errno : 0);
	}
}

/*
 * Copies link without recalculating path
 * Allows dangling pointers
 * TODO TEST
 */
int link_copy_raw(const char* const src, const char* const dst) {
	char lpath[PATH_BUF_SIZE];
	const ssize_t ll = readlink(src, lpath, sizeof(lpath));
	if (ll == -1) return errno;
	lpath[ll] = 0;
	if (symlink(lpath, dst)) {
		int e = errno;
		if (e != ENOENT) return e;
	}
	return 0;
}

void pretty_size(off_t s, char* const buf) {
	static const char* const units = "BKMGTPEZ";
	const char* unit = units;
	unsigned rest = 0;
	while (s >= 1024) {
		rest = s % 1024;
		s /= 1024;
		unit += 1;
	}
	if (s >= 1000) {
		unit += 1;
		rest = s;
		s = 0;
	}
	rest *= 1000;
	rest /= 1024;
	rest /= 10;
	const char d[3] = { s/100, (s/10)%10, s%10 };
	const char r[2] = { rest/10, rest%10 };
	memset(buf, 0, SIZE_BUF_SIZE);
	size_t top = 0;
	for (size_t i = 0; i < 3; ++i) {
		if (d[i] || top || (!top && i == 2)) {
			buf[top++] = '0'+d[i];
		}
	}
	if (r[0] || r[1]) {
		buf[top++] = '.';
		buf[top++] = '0'+r[0];
		if (r[1]) buf[top++] = '0'+r[1];
	}
	buf[top] = *unit;
}

/*
 * Push Directory
 */
int pushd(char* const P, size_t* const Pl, const char* const D, size_t Dl) {
	if (memcmp(P, "/", 2)) {
		if (*Pl+1+Dl > PATH_MAX_LEN) return ENAMETOOLONG;
		P[*Pl] = '/';
		*Pl += 1;
	}
	else if (*Pl+Dl > PATH_MAX_LEN) return ENAMETOOLONG;
	memcpy(P+*Pl, D, Dl+1);
	*Pl += Dl;
	return 0;
}

/*
 * Pop directory
 */
void popd(char* const P, size_t* const Pl) {
	while (*Pl > 1 && P[*Pl-1] != '/') {
		P[*Pl-1] = 0;
		*Pl -= 1;
	}
	if (*Pl > 1) {
		P[*Pl-1] = 0;
		*Pl -= 1;
	}
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
	const size_t dl = strnlen(dir, NAME_MAX_LEN);
	if (!dl) return 0;
	size_t pl = strnlen(path, PATH_MAX_LEN);
	if (pl+2+dl > PATH_MAX_LEN) return ENAMETOOLONG;
	if (memcmp(path, "/", 2)) {
		memcpy(path+pl, "/", 2);
		pl += 1;
	}
	memcpy(path+pl, dir, dl+1);
	return 0;
}

/*
 * cd - Change Directory
 * Does not change current working directory,
 * but modifies path passed in arguments.
 *
 * Returns ENAMETOOLONG if PATH_MAX would be exceeded.
 * In such case 'current' is left unchanged.
 *
 * 'current' must contain an absolute path.
 *
 * 'dest' may be either absolute or relative path.
 * Besides directory names it can contain . or ..
 * which are appiled to current directory.
 * '~' at the begining is an alias for user's HOME directory.
 *
 * Path returned in 'current' will always be absolute
 * and it will never contain '/' at the end
 * (with exception of root directory).
 *
 * If both 'current' and 'dest' are empty strings
 * 'current' will be corrected to "/".
 *
 * If 'dest' is empty string and 'current' ends with '/',
 * that '/' will be cleared.
 *
 * If 'current' is empty string,
 * then it is assumed to be root directory.
 *
 * Multiple consecutive '/' will be shortened to only one '/'
 * e.g. cd("/", "a///b////c") -> "/a/b/c"
 */
int cd(char* const current, const char* const dest) {
	size_t cl = strnlen(current, PATH_MAX_LEN);
	char B[PATH_BUF_SIZE];
	size_t Bl;
	const char* a = dest;
	size_t rem = strnlen(dest, PATH_MAX_LEN);
	if (dest[0] == '/') {
		memset(B, 0, sizeof(B));
		Bl = 0;
		a += 1;
		rem -= 1;
	}
	else {
		if (!memcmp(dest, "/", 2)) {
			B[0] = 0;
			Bl = 0;
		}
		else {
			memcpy(B, current, cl);
			memset(B+cl, 0, PATH_BUF_SIZE-cl);
			Bl = cl;
			if (B[Bl-1] == '/') {
				B[--Bl] = 0;
			}
		}
	}
	const char* b;
	while (rem) {
		b = memchr(a, '/', rem);

		if (!b) b = a+rem;
		else rem -= 1;

		if (b-a == 1 && !memcmp(a, ".", 1));
		else if (b-a == 2 && !memcmp(a, "..", 2)) {
			while (Bl > 0 && B[Bl-1] != '/') {
				B[--Bl] = 0;
			}
			B[--Bl] = 0;
		}
		else if (b-a == 1 && a == dest && !memcmp(a, "~", 1)) {
			const char* home;
			if (!(home = getenv("HOME"))) {
				errno = 0;
				struct passwd* pwd = getpwuid(geteuid());
				if (!pwd) return errno;
				home = pwd->pw_dir;
			}
			const size_t hl = strnlen(home, PATH_MAX_LEN);
			memcpy(B, home, hl);
			memset(B+hl, 0, PATH_BUF_SIZE-hl);
			Bl = hl;
		}
		else if (b-a) {
			if (Bl + 1 + (b-a) >= PATH_MAX_LEN) {
				return ENAMETOOLONG;
			}
			B[Bl++] = '/';
			memcpy(B+Bl, a, b-a);
			Bl += b-a;
		}
		rem -= b-a;
		a = b+1;
	}
	if (B[0] == 0) {
		B[0] = '/';
		B[1] = 0;
		Bl = 1;
	}
	memcpy(current, B, Bl+1);
	return 0;
}

/*
 * Basically removes everything after last '/', including that '/'
 * Return values:
 * 0 if operation succesful
 * -1 if path == '/'
 */
int up_dir(char* const path) {
	if (!memcmp(path, "/", 2)) return -1;
	int i = strnlen(path, PATH_MAX_LEN);
	for (; i > 0 && path[i] != '/'; --i) {
		path[i] = 0;
	}
	// At this point i points that last '/'
	// But if it's root, don't delete it
	if (i) path[i] = 0;
	return 0;
}

bool path_is_relative(const char* const path) {
	return path[0] != '/' && path[0] != '~';
}

/*
 * Returns place in buffer, where path after ~ starts
 * So that pretty path is just
 * printf("~%s", path+prettify_path_i(path));
 * If cannot be prettified, returns 0
 */
int prettify_path_i(const char* const path, const char* const home) {
	const size_t hlen = strnlen(home, PATH_MAX_LEN);
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
	const int plen = strnlen(path, PATH_MAX_LEN);
	int i = plen-1; // i will point last slash in path
	while (path[i] != '/' && i >= 0) {
		i -= 1;
	}
	return i+1; // i will point last slash in path
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
	const size_t subs_len = strnlen(subs, PATH_MAX_LEN);
	const size_t str_len = strnlen(str, PATH_MAX_LEN);
	if (subs_len > str_len) return false;
	if (subs_len == str_len) return !memcmp(str, subs, str_len);
	for (size_t j = 0; strnlen(str+j, PATH_MAX_LEN) >= subs_len; ++j) {
		if (subs_len == imb(str+j, subs)) return true;
	}
	return false;
}

fnum_t list_push(struct string_list* const L, const char* const s, size_t sl) {
	void* tmp = realloc(L->arr, (L->len+1) * sizeof(struct string*));
	if (!tmp) return (fnum_t)-1;
	L->arr = tmp;
	if (s && sl == (size_t)-1) {
		sl = strnlen(s, NAME_MAX_LEN);
		L->arr[L->len] = malloc(sizeof(struct string)+sl+1);
		L->arr[L->len]->len = sl;
		memcpy(L->arr[L->len]->str, s, sl+1);
	}
	else {
		L->arr[L->len] = NULL;
	}
	L->len += 1;
	return L->len - 1;
}

void list_copy(struct string_list* const D, const struct string_list* const S) {
	D->len = S->len;
	D->arr = malloc(D->len*sizeof(struct string*));
	for (fnum_t i = 0; i < D->len; ++i) {
		D->arr[i] = malloc(sizeof(struct string) +S->arr[i]->len);
		memcpy(D->arr[i]->str, S->arr[i]->str, S->arr[i]->len+1);
		D->arr[i]->len = S->arr[i]->len;
	}
}

void list_free(struct string_list* const list) {
	if (!list->arr) return;
	for (fnum_t i = 0; i < list->len; ++i) {
		if (list->arr[i]) free(list->arr[i]);
	}
	free(list->arr);
	list->arr = NULL;
	list->len = 0;
}

/*
 * Reads file from fd and forms a list of lines
 *
 * TODO flexible buffer length
 * TODO more testing
 * TODO support for other line endings
 */
int file_to_list(const int fd, struct string_list* const list) {
	list_free(list);
	char name[NAME_BUF_SIZE];
	size_t nlen = 0, top = 0;
	ssize_t rd = 0;
	char* nl;
	if (lseek(fd, 0, SEEK_SET) == -1) return errno;
	memset(name, 0, sizeof(name));
	for (;;) {
		rd = read(fd, name+top, NAME_BUF_SIZE-top);
		if (rd == -1) {
			int e = errno;
			list_free(list);
			return e;
		}
		if (!rd && !*name) break;
		nl = memchr(name, '\n', sizeof(name));
		if (!nl && !(nl = memchr(name, 0, sizeof(name)))) {
			list_free(list);
			return ENAMETOOLONG;
		}
		*nl = 0;
		nlen = nl-name;
		void* tmp_str = realloc(list->arr,
				(list->len+1)*sizeof(struct string*));
		if (!tmp_str) {
			int e = errno;
			list_free(list);
			return e;
		}
		list->arr = tmp_str;
		if (nlen) {
			list->arr[list->len] = malloc(sizeof(struct string)+nlen+1);
			list->arr[list->len]->len = nlen;
			memcpy(list->arr[list->len]->str, name, nlen+1);
		}
		else {
			list->arr[list->len] = NULL;
		}
		top = NAME_BUF_SIZE-(nlen+1);
		memmove(name, name+nlen+1, top);
		memset(name+top, 0, nlen+1);
		list->len += 1;
	}
	return 0;
}

int list_to_file(const struct string_list* const list, int fd) {
	if (lseek(fd, 0, SEEK_SET)) return errno;
	char name[NAME_BUF_SIZE];
	for (fnum_t i = 0; i < list->len; ++i) {
		memcpy(name, list->arr[i]->str, list->arr[i]->len);
		name[list->arr[i]->len] = '\n';
		if (write(fd, name, list->arr[i]->len+1) <= 0) return errno;
	}
	return 0;
}

fnum_t string_on_list(const struct string_list* const L,
		const char* const s, size_t sl) {
	for (fnum_t f = 0; f < L->len; ++f) {
		if (!memcmp(s, L->arr[f]->str, MIN(sl, L->arr[f]->len)+1)) {
			return f;
		}
	}
	return (fnum_t)-1;
}

fnum_t blank_lines(const struct string_list* const list) {
	fnum_t n = 0;
	for (fnum_t i = 0; i < list->len; ++i) {
		if (!list->arr[i]->str) n += 1;
	}
	return n;
}

/*
 * Tells if there are duplicates on list
 * TODO optimize
 */
bool duplicates_on_list(const struct string_list* const list) {
	for (fnum_t f = 0; f < list->len; ++f) {
		for (fnum_t g = 0; g < list->len; ++g) {
			if (f == g) continue;
			const char* const a = list->arr[f]->str;
			const char* const b = list->arr[g]->str;
			size_t s = 1+MIN(list->arr[f]->len, list->arr[g]->len);
			if (a && b && !memcmp(a, b, s)) return true;
		}
	}
	return false;
}