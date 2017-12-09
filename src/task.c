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
	t->paused = t->done = false;
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
 * TODO what about non-{dirs,files,links}?
 * Calculates size of all files and subdirectories in directory
 */
int task_estimate_file_volume(struct task* t, char* path) {
	int r = 0;
	struct stat s;
	if (lstat(path, &s)) return errno;
	if (S_ISDIR(s.st_mode)) {
		DIR* dir = opendir(path);
		struct dirent* de;
		while ((de = readdir(dir)) != NULL) {
			if (!strncmp(de->d_name, ".", 2) ||
				!strncmp(de->d_name, "..", 3)) {
				continue;
			}
			static char fpath[PATH_MAX+1];
			strncpy(fpath, path, PATH_MAX);
			append_dir(fpath, de->d_name); // TODO handle error
			struct stat ss;
			if (lstat(fpath, &ss)) return errno;
			if (S_ISDIR(ss.st_mode)) {
				t->dirs_total += 1;
				r |= task_estimate_file_volume(t, fpath);
			}
			else {
				t->size_total += ss.st_size;
				t->files_total += 1;
			}
			up_dir(fpath);
		}
		t->size_total += s.st_size;
		t->dirs_total += 1;
		closedir(dir);
	}
	else {
		t->size_total += s.st_size;
		t->files_total += 1;
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
		static char _dst[PATH_MAX+1];
		strncpy(_dst, t->dst, PATH_MAX);
		if (append_dir(_dst, t->newname)) return NULL;
		substitute(buf, t->src, _dst);
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
	tw->cpath = strncpy(malloc(PATH_MAX+1), path, PATH_MAX);
	tw->wpath = strncpy(malloc(PATH_MAX+1), path, PATH_MAX);
	tw->dname = calloc(NAME_MAX+1, 1);
	tw->dt = malloc(sizeof(struct dirtree));
	tw->dt->cd = NULL;
	tw->dt->up = NULL;
	lstat(path, &tw->cs);
	tw->tws = AT_INIT;
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
	lstat(tw->cpath, &tw->cs);
	if (S_ISDIR(tw->cs.st_mode)) { // entry is directory
		tw->tws = AT_DIR;
		return;
	}
	tw->tws = AT_FILE; // entry is file
}

/*
 * npath is a buffer to be used by build_new_path()
 * buf is a buffer to be used bu _copy_some()
 */
int _at_step(struct task* const t, int* const c,
		char* const npath, char* const buf, const size_t bufsize) {
	const bool copy = t->t == TASK_COPY || t->t == TASK_MOVE;
	const bool remove = t->t == TASK_MOVE || t->t == TASK_REMOVE;
	const bool link = (t->tw.cs.st_mode & S_IFMT) == S_IFLNK;
	switch (t->tw.tws) {
	case AT_INIT:
		if (copy && remove && same_fs(t->src, t->dst)) {
			strncpy(npath, t->dst, PATH_MAX);
			append_dir(npath, t->src+current_dir_i(t->src));
			t->tw.tws = AT_EXIT;
			if (rename(t->src, npath)) return errno;
		}
		break;
	case AT_FILE:
		if (copy) {
			build_new_path(t, t->tw.cpath, npath);
			if (link) {
				return link_copy(t->src, t->tw.cpath, npath);
			}
			else {
				return _copy_some(t, t->tw.cpath, npath, buf, bufsize, c);
			}
		}
		if (remove) {
			if (unlink(t->tw.cpath)) return errno;
		}
		if (remove && !copy) {
			t->size_done += t->tw.cs.st_size;
			t->files_done += 1;
			*c -= 1;
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
	char buf[BUFSIZ];
	while (t->tw.tws != AT_EXIT && c > 0) {
		_at_step(t, &c, npath, buf, BUFSIZ); // TODO error
		if (t->in == -1 && t->out == -1) tree_walk_step(&t->tw);
	}
	if (t->tw.tws == AT_EXIT) {
		t->done = true;
		tree_walk_end(&t->tw);
	}
	return 0;
}
