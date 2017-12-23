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

#include "include/task.h"

void task_new(struct task* const t, const enum task_type tp,
		char* const src, char* const dst, char* const newname) {
	t->t = tp;
	t->paused = t->done = t->rl = false;
	t->src = src;
	t->dst = dst;
	t->newname = newname;
	t->in = t->out = -1;
	t->size_total = t->size_done = 0,
	t->files_total = t->files_done = 0,
	t->dirs_total = t->dirs_done = 0;
	memset(&t->tw, 0, sizeof(struct tree_walk));
}

void task_clean(struct task* const t) {
	if (t->src) free(t->src);
	if (t->dst) free(t->dst);
	if (t->newname) free(t->newname);
	t->src = t->dst = t->newname = NULL;
	t->t = TASK_NONE;
	t->paused = t->done = false;
	if (t->in != -1) close(t->in);
	if (t->out != -1) close(t->out);
	t->in = t->out = -1;
}

/*
 * Calculates size of all files and subdirectories in directory
 * TODO more testing
 * TODO errors
 */
int estimate_volume(char* path, ssize_t* const size_total,
		int* const files_total, int* const dirs_total, const bool tl) {
	int r = 0;
	struct stat s;
	if ((tl && stat(path, &s)) || (!tl && lstat(path, &s))) return errno;
	if (S_ISDIR(s.st_mode)) {
		DIR* dir = opendir(path);
		if (!dir) return errno;
		struct dirent* de;
		while ((de = readdir(dir)) != NULL) {
			if (!strncmp(de->d_name, ".", 2) ||
				!strncmp(de->d_name, "..", 3)) {
				continue;
			}
			char fpath[PATH_MAX+1];
			strncpy(fpath, path, PATH_MAX);
			if ((r = append_dir(fpath, de->d_name))) return r;
			struct stat ss;
			if ((tl && stat(fpath, &ss))
			   || (!tl && lstat(fpath, &ss))) return errno;
			if (S_ISDIR(ss.st_mode)) {
				*dirs_total += 1;
				r |= estimate_volume(fpath, size_total,
						files_total, dirs_total, tl);
			}
			else if (S_ISREG(ss.st_mode)) {
				*size_total += ss.st_size;
				*files_total += 1;
			}
			up_dir(fpath);
		}
		*size_total += s.st_size;
		*dirs_total += 1;
		closedir(dir);
	}
	else if (S_ISREG(s.st_mode)) {
		*size_total += s.st_size;
		*files_total += 1;
	}
	return r;
}

static int _close_inout(struct task* const t) {
	int r = 0;
	r |= close(t->in);
	r |= close(t->out);
	t->in = t->out = -1;
	return r;
}

static int _copy_some(struct task* const t,
		const char* const src, const char* const dst,
		void* const buf, const size_t bufsize, int* const c) {
	// TODO better error handling
	int e = 0;
	if (t->out == -1 || t->in == -1) {
		t->out = open(src, O_RDONLY);
		if (t->out == -1) return errno;
		t->in = open(dst, O_CREAT | O_WRONLY, t->tw.cs.st_mode);
		if (t->in == -1) return errno;

		struct stat outs;
		fstat(t->out, &outs);
		if (outs.st_size > 0) {
			e = posix_fallocate(t->in, 0, outs.st_size);
			// TODO detect earlier if fallocate is supported
			if (e != EOPNOTSUPP && e != ENOSYS) return e;
		}
	}
	ssize_t wb = -1, rb = -1;
	while (*c > 0) {
		rb = read(t->out, buf, bufsize);
		if (!rb) {
			t->files_done += 1;
			return _close_inout(t);
		}
		if (rb == -1) {
			e = errno;
			_close_inout(t);
			return e;
		}
		wb = write(t->in, buf, rb);
		if (wb == -1) {
			e = errno;
			_close_inout(t);
			return e;
		}
		t->size_done += wb;
		*c -= 1;
	}
	return 0;
}

/* How does hund determine paths of files when performing operations?
 *
 * removing:
 * 		Just rmdir/unlink given path
 * 		src:       "/home/user/unwanted-file.txt"
 * 		checklist: "/home/user/unwanted-file.txt"
 * 		unlink("/home/user/unwanted-file.txt");
 *
 * 		src:       "/home/user/unwanted-dir"
 * 		checklist: "/home/user/unwanted-dir/a/deep/tree"
 * 		(if dir is empty or emptied)
 * 		rmdir("/home/user/unwanted-dir/a/deep/tree");
 *
 * moving/copying
 * (no name conflicts, newname == NULL):
 *		- Prepare replacement path;
 *		  src with cut out target file/dir together with /
 *		src  "/home/user/doc/file.pdf"
 *		repl "/home/user/doc"
 *
 *		src  "/home/user/dir"
 *		repl "/home/user"
 *		- Checklist will contain full and absolute path to file.
 *		  Now replace one and first occurence of `repl` in checklist path
 *		  with dst. (dst is always a directory)
 *		checklist: "/home/user/doc/file.pdf"
 *		src:       "/home/user/doc/file.pdf"
 *		repl:      "/home/user/doc"
 *		dst:       "/home/guest/gifts"
 *		new_path:  "/home/guest/gifts/file.pdf"
 *
 *		checklist: "/home/user/dir/a/very/deep/tree"
 *		src:       "/home/user/dir"
 *		repl:      "/home/user"
 *		dst:       "/home/guest/gifts"
 *		new_path:  "/home/guest/gifts/dir/a/very/deep/tree"
 *
 * moving/copying
 * (conflicting names, user is prompted for new name, newname != NULL):
 *		- Now repl == src
 *		- Append new name to dst
 *		  (dst may now point a file, but otherwise it's always a dir)
 *		- Do as previously
 *		checklist: "/home/user/doc/file.pdf"
 *		src:       "/home/user/doc/file.pdf"
 *		repl:      "/home/user/doc/file.pdf"
 *		new_name:  "doc.pdf"
 *		dst:       "/home/guest/gifts/doc.pdf"
 *		new_path:  "/home/guest/gifts/doc.pdf"
 *
 *		checklist: "/home/user/dir/a/very/deep/tree"
 *		src:       "/home/user/dir"
 *		repl:      "/home/user/dir"
 *		new_name:  "folder"
 *		dst:       "/home/guest/gifts/folder"
 *		new_path:  "/home/guest/gifts/folder/a/very/deep/tree"
 *
 * When new_path is determined:
 *      copying = copy checklist -> new_path
 *		moving = copy checklist -> new_path + remove checklist
 */
char* build_new_path(const struct task* const t,
		const char* const cp, char* const buf) {
	strncpy(buf, cp, PATH_MAX);
	if (t->newname) { // name colission; using new name
		const size_t dst_len = strnlen(t->dst, PATH_MAX);
		const size_t newname_len = strnlen(t->newname, NAME_MAX);
		char* _dst = malloc(dst_len+1+newname_len+1);
		strncpy(_dst, t->dst, dst_len+1);
		if (append_dir(_dst, t->newname)) {
			free(_dst);
			return NULL;
		}
		substitute(buf, t->src, _dst);
		free(_dst);
	}
	else { // name stays the same
		const size_t repll = current_dir_i(t->src)-1;
		char* const repl = strncpy(malloc(repll+1), t->src, repll);
		repl[repll] = 0;
		substitute(buf, repl, t->dst);
		free(repl);
	}
	return buf;
}

void tree_walk_start(struct tree_walk* tw, const char* const path) {
	tw->cpath = malloc(PATH_MAX+1);
	strncpy(tw->cpath, path, PATH_MAX);
	tw->wpath = malloc(PATH_MAX+1);
	strncpy(tw->wpath, path, PATH_MAX);
	tw->dname = calloc(NAME_MAX+1, 1);
	tw->dt = malloc(sizeof(struct dirtree));
	tw->dt->cd = NULL;
	tw->dt->up = NULL;
	lstat(path, &tw->cs);
	tw->tws = AT_INIT;
	tw->tl = false;
}

static void tree_walk_down(struct tree_walk* tw) {
	struct dirtree* const dt = malloc(sizeof(struct dirtree));
	dt->up = tw->dt;
	tw->dt = dt;
	dt->cd = NULL;
	append_dir(tw->wpath, tw->dname);
	strncpy(tw->cpath, tw->wpath, PATH_MAX);
	tw->dt->cd = opendir(tw->wpath);
}

static void tree_walk_up(struct tree_walk* tw) {
	struct dirtree* tmp = tw->dt;
	tw->dt = tmp->up;
	closedir(tmp->cd);
	free(tmp);
	up_dir(tw->wpath);
	up_dir(tw->cpath);
}

void tree_walk_end(struct tree_walk* tw) {
	free(tw->cpath);
	free(tw->wpath);
	free(tw->dname);
	// TODO remove tw->dt->... if failed or something
}

void tree_walk_step(struct tree_walk* tw) {
	// TODO error handling
	switch (tw->tws)  {
	case AT_INIT:
		if (S_ISDIR(tw->cs.st_mode)) {
			tw->tws = AT_DIR;
			return;
		}
		else {
			tw->tws = AT_FILE;
			return;
		}
		break;
	case AT_LINK:
	case AT_FILE:
		if (!tw->dt->cd) {
			tw->tws = AT_EXIT;
			return;
		}
		up_dir(tw->cpath);
		break;
	case AT_DIR: tree_walk_down(tw); break;
	case AT_DIR_END: tree_walk_up(tw); break;
	default: break;
	}

	if (!tw->dt || !tw->dt->up) { // last dir
		free(tw->dt);
		tw->tws = AT_EXIT;
		return;
	}

	struct dirent* ce;
	do { // Skip . and .
		ce = readdir(tw->dt->cd);
	} while (ce && (
			!strncmp(ce->d_name, ".", 2) ||
			!strncmp(ce->d_name, "..", 3)));

	if (!ce) { // directory is empty or reached end of directory
		tw->tws = AT_DIR_END;
		return;
	}
	strncpy(tw->dname, ce->d_name, NAME_MAX+1);
	strncpy(tw->cpath, tw->wpath, PATH_MAX);
	append_dir(tw->cpath, tw->dname);
	lstat(tw->cpath, &tw->cs); // TODO errno
	if (S_ISLNK(tw->cs.st_mode)) {
		stat(tw->cpath, &tw->cs); // TODO errno
		if (tw->tl) {
			if (S_ISDIR(tw->cs.st_mode)) {
				tw->tws = AT_DIR;
			}
			else tw->tws = AT_FILE;
		}
		else tw->tws = AT_LINK;
	}
	else if (S_ISDIR(tw->cs.st_mode)) { // entry is directory
		tw->tws = AT_DIR;
	}
	else tw->tws = AT_FILE; // entry is file
}

/*
 * npath is a buffer to be used by build_new_path()
 * buf is a buffer to be used bu _copy_some()
 *
 * TODO error handling
 */
int _at_step(struct task* const t, int* const c,
		char* const npath, char* const buf, const size_t bufsize) {
	const bool copy = t->t == TASK_COPY || t->t == TASK_MOVE;
	const bool remove = t->t == TASK_MOVE || t->t == TASK_REMOVE;
	switch (t->tw.tws) {
	case AT_INIT:
		if (copy && remove && same_fs(t->src, t->dst)) {
			strncpy(npath, t->dst, PATH_MAX);
			append_dir(npath, t->src+current_dir_i(t->src)); // errno
			t->tw.tws = AT_EXIT;
			if (rename(t->src, npath)) return errno;
		}
		break;
	case AT_LINK:
		if (copy) {
			build_new_path(t, t->tw.cpath, npath);
			int err;
			if (t->rl) {
				err = link_copy_raw(t->tw.cpath, npath);
			}
			else {
				err = link_copy(t->src, t->tw.cpath, npath);
			}
			if (err) return err;
		}
		if (remove) {
			if (unlink(t->tw.cpath)) return errno;
			if (!copy) {
				t->size_done += t->tw.cs.st_size;
				t->files_done += 1;
				*c -= 1;
			}
		}
		break;
	case AT_FILE:
		if (copy) {
			build_new_path(t, t->tw.cpath, npath);
			_copy_some(t, t->tw.cpath, npath, buf, bufsize, c); // TODO err
			if (t->in != -1 || t->out != -1) return 0;
		}
		if (remove) {
			if (unlink(t->tw.cpath)) return errno;
			if (!copy) {
				t->size_done += t->tw.cs.st_size;
				t->files_done += 1;
				*c -= 1;
			}
		}
		break;
	case AT_DIR:
		if (copy) {
			build_new_path(t, t->tw.cpath, npath);
			if (mkdir(npath, t->tw.cs.st_mode)) return errno;
			t->size_done += t->tw.cs.st_size;
			t->dirs_done += 1;
			*c -= 1;
		}
		break;
	case AT_DIR_END:
		if (remove) {
			if (rmdir(t->tw.cpath)) return errno;
			t->dirs_done += 1;
			*c -= 1;
		}
		break;
	default: break;
	}
	return 0;
}

int do_task(struct task* t, int c) {
	if (t->tw.tws == AT_NOWHERE) tree_walk_start(&t->tw, t->src);
	char npath[PATH_MAX+1];
	const size_t buf_size = BUFSIZ;
	char buf[buf_size];
	while (t->tw.tws != AT_EXIT && c > 0) {
		_at_step(t, &c, npath, buf, buf_size); // TODO error
		if (t->in == -1 && t->out == -1) tree_walk_step(&t->tw);
	}
	if (t->tw.tws == AT_EXIT) {
		t->done = true;
		tree_walk_end(&t->tw);
	}
	return 0;
}

/*
 * Reads file from fd and forms a list of lines
 * \n is turned into \0
 *
 * TODO flexible buffer length
 * TODO errors
 */
int file_lines_to_list(const int fd, char*** const arr, fnum_t* const lines) {
	*arr = NULL;
	*lines = 0;
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
			for (fnum_t f = 0; f < *lines; ++f) {
				free((*arr)[f]);
			}
			free(*arr);
			*lines = 0;
			return e;
		}
		if (!rd && !*name) break;
		nl = memchr(name, '\n', sizeof(name));
		if (nl) {
			*nl = 0;
			nlen = nl-name;
		}
		else {
			nlen = strnlen(name, NAME_MAX);
		}
		*arr = realloc(*arr, ((*lines)+1) * sizeof(char*));
		(*arr)[*lines] = strncpy(malloc(nlen+1), name, nlen+1);
		top = NAME_MAX+1-nlen+1;
		memmove(name, name+nlen+1, top);
		*lines += 1;
	}
	return 0;
}

/* Tells if there are duplicates on list */
bool duplicates_on_list(char** const list, const fnum_t listlen) {
	for (fnum_t f = 0; f < listlen; ++f) {
		for (fnum_t g = 0; g < listlen; ++g) {
			if (f == g) continue;
			if (!strcmp(list[f], list[g])) return true;
		}
	}
	return false;
}

void free_line_list(const fnum_t lines, char** const arr) {
	for (fnum_t i = 0; i < lines; ++i) {
		free(arr[i]);
	}
	free(arr);
}
