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
		char* src, char* dst, char* newname) {
	*t = (struct task) {
		.s = TASK_STATE_FINISHED,
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
	memset(&t->tw, 0, sizeof(struct tree_walk));
}

/* TODO symlinks
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

static inline int _copy_some(struct task* t, char* npath, void* buf, int* c) {
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
char* build_new_path(struct task* t, char* cp) {
	char* new_path = malloc(PATH_MAX+1);
	strncpy(new_path, cp, PATH_MAX);
	if (t->newname) { // name colission; using new name
		static char _dst[PATH_MAX+1];
		strncpy(_dst, t->dst, PATH_MAX);
		if (append_dir(_dst, t->newname)) return NULL;
		substitute(new_path, t->src, _dst);
	}
	else { // name stays the same
		const size_t repll = current_dir_i(t->src)-1;
		char* repl = malloc(repll);
		strncpy(repl, t->src, repll);
		repl[repll] = 0;
		substitute(new_path, repl, t->dst);
		free(repl);
	}
	return new_path;
}

void tree_walk_start(struct tree_walk* tw, const char* const path) {
	tw->cpath = calloc(PATH_MAX+1, sizeof(char));
	tw->wpath = malloc(PATH_MAX+1);
	tw->dt = malloc(sizeof(struct dirtree));
	strncpy(tw->wpath, path, PATH_MAX);
	tw->dt->cd = opendir(tw->wpath); // TODO errors
	tw->dt->up = NULL;
	tw->dt->ce = NULL;
	tw->tws = AT_INIT;
}

static void tree_walk_down(struct tree_walk* tw) {
	const char* const d_name = tw->dt->ce->d_name;
	struct dirtree* const dt = malloc(sizeof(struct dirtree));
	dt->up = tw->dt;
	tw->dt = dt;
	dt->ce = NULL;
	dt->cd = NULL;
	append_dir(tw->wpath, d_name);
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
	// TODO remove tw->dt->... if failed or something
}

void tree_walk_step(struct tree_walk* tw) {
	// TODO error handling
	switch (tw->tws)  {
	case AT_NOWHERE: return;
	case AT_DIR: tree_walk_down(tw); break;
	case AT_END: tree_walk_up(tw); break;
	default: break;
	}

	if (!tw->dt) { // dirtree is empty
		tw->tws = AT_NOWHERE;
		return;
	}
	do {
		tw->dt->ce = readdir(tw->dt->cd);
	} while (tw->dt->ce && (
			!strcmp(tw->dt->ce->d_name, ".") ||
			!strcmp(tw->dt->ce->d_name, "..")));
	if (!tw->dt->ce) { // directory is empty or reached end of directory
		tw->tws = AT_END;
		return;
	}
	strncpy(tw->cpath, tw->wpath, PATH_MAX);
	append_dir(tw->cpath, tw->dt->ce->d_name);
	lstat(tw->cpath, &tw->dt->cs);
	if (S_ISDIR(tw->dt->cs.st_mode)) { // entry is directory
		tw->tws = AT_DIR;
		return;
	}
	tw->tws = AT_FILE; // entry is file
	// TODO links
}

int do_task(struct task* t, int c) {
	struct tree_walk* const tw = &t->tw; // FIXME
	tree_walk_start(tw, t->src);
	syslog(LOG_DEBUG, "entry %s %s\n", tw->cpath, tw->wpath);
	while (tw->tws != AT_NOWHERE) {
		tree_walk_step(tw);
		switch (tw->tws) {
		case AT_NOWHERE:
			syslog(LOG_DEBUG, "finished %s %s\n", tw->cpath, tw->wpath);
			break;
		case AT_INIT:
			syslog(LOG_DEBUG, "init %s %s\n", tw->cpath, tw->wpath);
			break;
		case AT_FILE:
			syslog(LOG_DEBUG, "file %s %s\n", tw->cpath, tw->wpath);
			break;
		case AT_DIR:
			syslog(LOG_DEBUG, "dir: %s %s\n", tw->cpath, tw->wpath);
			break;
		case AT_END:
			syslog(LOG_DEBUG, "end: %s %s\n", tw->cpath, tw->wpath);
			break;
		default: break;
		}
	}
	tree_walk_end(tw);
	t->s = TASK_STATE_FINISHED;
	/*dir_start(t);
	enum fs_walk fsw = AT_INIT;
	while ((fsw = tree_walk(t, t->wpath)), c) {
		switch (fsw) {
		case AT_INIT:
			if (t->t == TASK_COPY || t->t == TASK_MOVE) {
				utf8* npath = build_new_path(t, t->wpath);
				if (mkdir(npath, t->dir->cs.st_mode)) return errno;
				free(npath);
			}
			break;
		case AT_NOWHERE:
			if (t->t == TASK_REMOVE || t->t == TASK_MOVE) {
				if (rmdir(t->wpath)) return errno;
			}
			t->s = TASK_STATE_FINISHED;
			return 0;
		case AT_FILE:
			syslog(LOG_DEBUG, "file %s\n", t->wpath);
			if (t->t == TASK_COPY || t->t == TASK_MOVE) {
				utf8* npath = build_new_path(t, t->wpath); // TODO cache
				static char buf[BUFSIZ];
				int e = _copy_some(t, npath, buf, &c);
				free(npath);
				if (e) return e;
			}
			if (t->t == TASK_REMOVE || t->t == TASK_MOVE) {
				if (unlink(t->wpath)) return errno;
				t->size_done += t->dir->cs.st_size;
				c -= 1;
			}
			break;
		case AT_DIR:
			syslog(LOG_DEBUG, "dir %s\n", t->wpath);
			if (t->t == TASK_COPY || t->t == TASK_MOVE) {
				utf8* npath = build_new_path(t, t->wpath);
				if (mkdir(npath, t->dir->cs.st_mode)) return errno;
				free(npath);
			}
			if (t->t == TASK_REMOVE || t->t == TASK_MOVE) {
				if (rmdir(t->wpath)) return errno;
			}
			c -= 1;
			t->size_done += t->dir->cs.st_size;
			t->files_done += 1;
			dir_down(t);
			break;
		case AT_END:
			syslog(LOG_DEBUG, "end %s\n", t->wpath);
			dir_up(t);
			syslog(LOG_DEBUG, "in %s\n", t->wpath);
			break;
		default: break;
		}
	}*/
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
		else { // regular file
	}*/
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
