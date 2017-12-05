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

void task_new(struct task* t, enum task_type tp,
		utf8* src, utf8* dst, utf8* newname) {
	*t = (struct task) {
		.s = TASK_STATE_GATHERING_DATA,
		.t = tp,
		.running = true,
		.src = src, // Full path of source file/dir
		.dst = dst, // Contains working directory of the other panel
		.newname = newname, // Used for conflicts
		.in = -1,
		.out = -1,
		.size_total = 0,
		.size_done = 0,
		.files_total = 0,
		.files_done = 0,
		.dirs_total = 0,
		.dirs_done = 0
	};
}

/* TODO symlinks
 */
int task_build_file_list(struct task* t, utf8* path) {
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
				r |= task_build_file_list(t, fpath);
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

void task_clean(struct task* t) {
	if (t->src) free(t->src);
	if (t->dst) free(t->dst);
	if (t->newname) free(t->newname);
	t->src = NULL;
	t->dst = NULL;
	t->newname = NULL;
	t->s = TASK_STATE_CLEAN;
	t->t = TASK_NONE;
	if (t->in != -1) close(t->in);
	t->in = -1;
	if (t->out != -1) close(t->out);
	t->out = -1;
}

static int _close_inout(struct task* t) {
	int r = 0; // TODO
	r |= close(t->in);
	r |= close(t->out);
	t->in = -1;
	t->out = -1;
	return r;
}

static inline int _copy_some(struct task* t, utf8* npath, void* buf, int* c) {
	/*if (t->out == -1 || t->in == -1) {
		t->out = open(t->checklist->path, O_RDONLY);
		if (t->out == -1) return errno;
		t->in = open(npath, O_CREAT | O_WRONLY | t->checklist->s.st_mode);
		if (t->in == -1) return errno;

		struct stat outs;
		fstat(t->out, &outs);
		int e = posix_fallocate(t->in, 0, outs.st_size);
		// TODO detect earlier if fallocate is supported
		if (e != EOPNOTSUPP && e != ENOSYS) return errno;
	}
	ssize_t wb = -1, rb = -1;
	while (*c > 0) {
		rb = read(t->out, buf, BUFSIZ);
		if (!rb) {
			task_file_done(t);
			return _close_inout(t);
		}
		if (rb == -1) {
			return errno | _close_inout(t);
		}
		wb = write(t->in, buf, rb);
		if (wb == -1) {
			return errno | _close_inout(t);
		}
		t->size_done += wb;
		t->checklist->progress += wb;
		*c -= 1;
	}*/
	return 0;
}

/* How does hund determine paths of files when performing operations?
 * (see build_file_list() too)
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
 *
 * TODO block doing anything to block devices, sockets and other non-regular files
 */
utf8* build_new_path(struct task* t, utf8* cp) {
	utf8* new_path = malloc(PATH_MAX+1);
	strncpy(new_path, cp, PATH_MAX);
	if (t->newname) { // name colission; using new name
		static char _dst[PATH_MAX+1];
		strncpy(_dst, t->dst, PATH_MAX);
		if (append_dir(_dst, t->newname)) return NULL;
		substitute(new_path, t->src, _dst);
	}
	else { // name stays the same
		const size_t repll = current_dir_i(t->src)-1;
		utf8* repl = malloc(repll);
		strncpy(repl, t->src, repll);
		repl[repll] = 0;
		substitute(new_path, repl, t->dst);
		free(repl);
	}
	return new_path;
}

void dir_start(struct task* t) {
	t->dir = malloc(sizeof(struct dirtree));
	strncpy(t->wpath, t->src, PATH_MAX);
	t->dir->cd = opendir(t->wpath);
	t->dir->up = NULL;
	t->dir->ce = NULL;
}

void dir_down(struct task* t) {
	const char* const d_name = t->dir->ce->d_name;
	fprintf(stderr, "%s {\n", d_name);
	struct dirtree* const dt = malloc(sizeof(struct dirtree));
	dt->up = t->dir;
	t->dir = dt;
	dt->ce = NULL;
	dt->cd = NULL;
	append_dir(t->wpath, d_name);
	t->dir->cd = opendir(t->wpath);
}

void dir_up(struct task* t) {
	fprintf(stderr, "}\n");
	struct dirtree* tmp = t->dir;
	t->dir = tmp->up;
	closedir(tmp->cd);
	free(tmp);
	up_dir(t->wpath);
}

enum fs_walk tree_walk(struct task* t, char* path) {
	if (!t->dir) return AT_NOWHERE;
	do {
		t->dir->ce = readdir(t->dir->cd);
	} while (t->dir->ce && (
			!strcmp(t->dir->ce->d_name, ".") ||
			!strcmp(t->dir->ce->d_name, "..")));
	if (!t->dir->ce) {
		return AT_END;
	}
	strcpy(path, t->wpath);
	append_dir(path, t->dir->ce->d_name);
	lstat(path, &t->dir->cs);
	if (S_ISDIR(t->dir->cs.st_mode)) {
		return AT_DIR;
	}
	return AT_FILE;
}

int do_task(struct task* t, int c) {
	/*while (t->checklist && c > 0) {
		mode_t ftype = (t->checklist->s.st_mode & S_IFMT);
		if (ftype == S_IFBLK || ftype == S_IFCHR ||
				ftype == S_IFIFO || ftype == S_IFSOCK) {
			t->size_done += s.st_size;
			t->files_done += 1;
			c -= 1;
			continue;
		}
		bool isdir = S_ISDIR(t->checklist->s.st_mode);
		bool islnk = (t->checklist->s.st_mode & S_IFMT) == S_IFLNK;
		if (islnk) {
			if (t->t == TASK_COPY || t->t == TASK_MOVE) {
				utf8* npath = build_new_path(t, t->checklist->path);
				link_copy(t->src, t->checklist->path, npath);
				free(npath);
			}
			if (t->t == TASK_REMOVE || t->t == TASK_MOVE) {
				if (unlink(t->checklist->path)) return errno;
			}
			c -= 1;
			t->size_done += s.st_size;
			t->files_done += 1;
		}
		else if (isdir) {
			if (t->t == TASK_COPY || t->t == TASK_MOVE) {
				utf8* npath = build_new_path(t, t->checklist->path);
				if (mkdir(npath, t->checklist->s.st_mode)) return errno;
				free(npath);
			}
			if (t->t == TASK_REMOVE || t->t == TASK_MOVE) {
				if (rmdir(t->checklist->path)) return errno;
			}
			c -= 1;
			t->size_done += s.st_size;
			t->files_done += 1;
		}
		else { // regular file
			if (t->t == TASK_COPY || t->t == TASK_MOVE) {
				utf8* npath = build_new_path(t, t->checklist->path); // TODO cache
				static char buf[BUFSIZ];
				int e = _copy_some(t, npath, buf, &c);
				free(npath);
				if (e) return e;
			}
			if (t->t == TASK_REMOVE || t->t == TASK_MOVE) {
				if (unlink(t->checklist->path)) return errno;
				t->size_done += s.st_size;
				task_file_done(t);
				c -= 1;
			}
		}
	}
	if (!t->checklist) t->s = TASK_STATE_FINISHED;*/
	return 0;
}

/* If files are on the same filesystem, then the quickest
 * way to move files or whole directories is to rename() them.
 *
 * Returns 0 if was same fs and operation was succesful
 * Returns -1 if wasn't the same fs
 * Returns errno code if something went wrong
 */
int rename_if_same_fs(const struct task* const t) {
	if (same_fs(t->src, t->dst)) {
		char* ndst = malloc(PATH_MAX);
		strncpy(ndst, t->dst, PATH_MAX);
		append_dir(ndst, t->src+current_dir_i(t->src));
		if (rename(t->src, ndst)) {
			int e = errno;
			free(ndst);
			return e;
		}
		free(ndst);
		return 0;
	}
	return -1;
}
