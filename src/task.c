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

/* TODO
 * 1. TODO fix nomenclature (targets?, renamed?)
 */

#include "include/task.h"

void task_new(struct task* const t, const enum task_type tp,
		char* const src, char* const dst,
		const struct string_list* const targets,
		const struct string_list* const renamed) {
	t->t = tp;
	t->paused = t->done = t->rl = false;
	t->src = src;
	t->dst = dst;
	t->targets = *targets;
	t->renamed = *renamed;
	t->in = t->out = -1;
	t->current_target = 0;
	t->size_total = t->size_done = 0;
	t->files_total = t->files_done = 0;
	t->dirs_total = t->dirs_done = 0;
	memset(&t->tw, 0, sizeof(struct tree_walk));
}

void task_clean(struct task* const t) {
	free_list(&t->targets);
	free_list(&t->renamed);
	t->src = t->dst = NULL;
	t->t = TASK_NONE;
	t->paused = t->done = false;
	if (t->in != -1) close(t->in);
	if (t->out != -1) close(t->out);
	t->in = t->out = -1;
}

/*
 * Calculates size of all files and subdirectories in directory
 * st = size_total
 * ft = files_total
 * dt = dirs_total
 * tl = transparent links
 *
 * TODO more testing
 */
int estimate_volume(char* path, ssize_t* const st,
		int* const ft, int* const dt, const bool tl) {
	int r = 0;
	struct stat s;
	if (tl ? stat(path, &s) : lstat(path, &s)) return errno;
	if (too_special(s.st_mode)) return 0;
	if (S_ISREG(s.st_mode) || (!tl && S_ISLNK(s.st_mode))) {
		*st += s.st_size;
		*ft += 1;
		return 0;
	}
	// This leaves only DIR
	DIR* dir = opendir(path);
	if (!dir) return errno;
	struct dirent* de;
	while ((de = readdir(dir)) != NULL) {
		if (dotdot(de->d_name)) continue;
		char fpath[PATH_MAX+1];
		strncpy(fpath, path, PATH_MAX);
		if ((r = append_dir(fpath, de->d_name))) {
			closedir(dir);
			return r;
		}
		r = estimate_volume(fpath, st, ft, dt, tl);
		up_dir(fpath);
	}
	*st += s.st_size;
	*dt += 1;
	closedir(dir);
	return r;
}

void estimate_volume_for_list(const char* const wd,
		const struct string_list* const list,
		ssize_t* const size_total, int* const files_total,
		int* const dirs_total, const bool tl) {
	char path[PATH_MAX];
	strncpy(path, wd, PATH_MAX);
	for (fnum_t f = 0; f < list->len; ++f) {
		append_dir(path, list->str[f]);
		estimate_volume(path, size_total, files_total, dirs_total, tl);
		up_dir(path);
	}
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
		if (t->out == -1) return errno; // TODO TODO IMPORTANT
		t->in = creat(dst, t->tw.cs.st_mode);
		if (t->in == -1) {
			close(t->out);
			t->out = -1;
			return errno;
		}

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

/*
 * path = path of the target
 * wd = root directory of the operation (which part of src is to be changed to dst)
 * dst = directory to which target is to be moved/copied
 * newname = new name for destination
 * result = place to put the end result
 *
 * TODO wd can be a number - slice to cut out and replace
 */
void build_new_path(const char* const path, const char* const wd,
		const char* const dst, const char* const oldname,
		const char* const newname, char* const result) {
	strncpy(result, path, PATH_MAX);
	if (newname) {
		const size_t dst_len = strnlen(dst, PATH_MAX);
		const size_t newname_len = strnlen(newname, NAME_MAX);
		char* _dst = malloc(dst_len+1+newname_len+1);
		strncpy(_dst, dst, dst_len+1);
		append_dir(_dst, newname);
		const size_t wd_len = strnlen(wd, PATH_MAX);
		const size_t oldname_len = strnlen(oldname, NAME_MAX);
		char* _wd = malloc(wd_len+1+oldname_len+1);
		strncpy(_wd, wd, wd_len+1);
		append_dir(_wd, oldname);
		substitute(result, _wd, _dst);
		free(_wd);
		free(_dst);
	}
	else {
		substitute(result, wd, dst);
	}
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
	if (!(tw->dt->cd = opendir(tw->wpath)))  {
		// TODO TODO
	}
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
	do { // Skip . and ..
		ce = readdir(tw->dt->cd);
	} while (ce && dotdot(ce->d_name));

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
		char* const new_path, char* const buf, const size_t bufsize) {
	const bool copy = t->t == TASK_COPY || t->t == TASK_MOVE;
	const bool remove = t->t == TASK_MOVE || t->t == TASK_REMOVE;
	const char* const tfn = t->targets.str[t->current_target];
	const char* rfn = NULL;
	if (t->t == TASK_COPY || t->t == TASK_MOVE) {
		rfn = t->renamed.str[t->current_target];
	}
	switch (t->tw.tws) {
	case AT_INIT:
		if (copy && remove && same_fs(t->src, t->dst)) {
			build_new_path(t->tw.cpath, t->src,
					t->dst, tfn, rfn, new_path);
			t->tw.tws = AT_EXIT;
			if (rename(t->tw.cpath, new_path)) return errno;
		}
		break;
	case AT_LINK:
		if (copy) {
			build_new_path(t->tw.cpath, t->src,
					t->dst, tfn, rfn, new_path);
			int err;
			if (t->rl) {
				err = link_copy_raw(t->tw.cpath, new_path);
			}
			else {
				err = link_copy(t->src, t->tw.cpath, new_path);
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
			build_new_path(t->tw.cpath, t->src,
					t->dst, tfn, rfn, new_path);
			_copy_some(t, t->tw.cpath, new_path, buf, bufsize, c);
			// ^^^^ TODO err
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
			build_new_path(t->tw.cpath, t->src,
					t->dst, tfn, rfn, new_path);
			if (mkdir(new_path, t->tw.cs.st_mode)) return errno;
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

char* current_target_path(struct task* const t) {
	const char* target = t->targets.str[t->current_target];
	const size_t src_len = strnlen(t->src, PATH_MAX);
	const size_t target_len = strnlen(target, NAME_MAX);
	const size_t path_len = src_len+1+target_len;
	char* path = malloc(path_len+1);
	memcpy(path, t->src, path_len+1);
	append_dir(path, target);
	return path;
}

int do_task(struct task* const t, int c) {
	if (t->tw.tws == AT_NOWHERE) {
		char* path = current_target_path(t);
		tree_walk_start(&t->tw, path);
		free(path);
	}
	char new_path[PATH_MAX+1];
	const size_t buf_size = BUFSIZ;
	char buf[buf_size];
	while (t->tw.tws != AT_EXIT && c > 0) {
		_at_step(t, &c, new_path, buf, buf_size); // TODO error
		if (t->in == -1 && t->out == -1) tree_walk_step(&t->tw);
	}
	if (t->tw.tws == AT_EXIT) {
		if (t->current_target+1 == t->targets.len) {
			t->done = true;
			tree_walk_end(&t->tw);
		}
		else {
			t->current_target += 1;
			t->tw.tws = AT_NOWHERE;
		}
	}
	return 0;
}
