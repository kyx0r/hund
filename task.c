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

#include "task.h"

xtime_ms_t xtime(void) {
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

void task_new(struct task* const t, const enum task_type tp,
		const enum task_flags tf,
		char* const src, char* const dst,
		const struct string_list* const sources,
		const struct string_list* const renamed) {
	t->t = tp;
	t->ts = TS_ESTIMATE;
	t->tf = tf;
	t->src = src; // TODO lengths
	t->dst = dst;
	t->sources = *sources;
	t->renamed = *renamed;
	t->in = t->out = -1;
	t->current_source = 0;
	t->err = 0;
	t->conflicts = t->symlinks = t->specials = 0;
	t->size_total = t->size_done = 0;
	t->files_total = t->files_done = t->dirs_total = t->dirs_done = 0;
	t->chp = t->chm = 0;
	t->cho = -1;
	t->chg = -1;
	memset(&t->tw, 0, sizeof(struct tree_walk));
}

static void _close_files(struct task* const t) {
	if (t->in != -1) close(t->in);
	if (t->out != -1) close(t->out);
	t->in = t->out = -1;
}

void task_clean(struct task* const t) {
	list_free(&t->sources);
	list_free(&t->renamed);
	t->src = t->dst = NULL;
	t->t = TASK_NONE;
	t->tf = 0;
	t->ts = TS_CLEAN;
	t->conflicts = t->specials = t->err = 0;
	_close_files(t);
}

int task_build_path(const struct task* const t, char* R) {
	// TODO
	// TODO be smarter about checking the length
	const char* S = NULL;
	const char* D = NULL;
	size_t D_len = 0;
	size_t old_len = strnlen(t->src, PATH_MAX_LEN);
	if (t->renamed.len && t->renamed.arr[t->current_source]) {
		S = t->sources.arr[t->current_source]->str;
		D = t->renamed.arr[t->current_source]->str;
		D_len += strnlen(D, NAME_MAX_LEN);
		old_len += 1+strnlen(S, NAME_MAX_LEN);
	}
	char* const _R = R;
	memset(_R, 0, PATH_BUF_SIZE);
	const size_t dst_len = strnlen(t->dst, PATH_MAX_LEN);
	memcpy(R, t->dst, dst_len);
	R += dst_len;
	if (*(R-1) != '/') {
		*R = '/';
		R += 1;
	}
	if (R - _R > PATH_MAX_LEN) {
		memset(_R, 0, PATH_BUF_SIZE);
		return ENAMETOOLONG;
	}
	memcpy(R, D, D_len);
	R += D_len;
	if (*(R-1) != '/') {
		*R = '/';
		R += 1;
	}
	if (R - _R > PATH_MAX_LEN) {
		memset(_R, 0, PATH_BUF_SIZE);
		return ENAMETOOLONG;
	}
	const char* P = t->tw.path;
	P += old_len;
	if (*P == '/') {
		P += 1;
		old_len -= 1;
	}
	const size_t ppart = t->tw.pathlen-old_len;
	if ((R - _R)+ppart > PATH_MAX_LEN) {
		memset(_R, 0, PATH_BUF_SIZE);
		return ENAMETOOLONG;
	}
	memcpy(R, P, ppart);
	if (*(R+ppart-1) == '/') { // TODO
		*(R+ppart-1) = 0;
	}
	return 0;
}

void task_action_chmod(struct task* const t, int* const c) {
	switch (t->tw.tws) {
	case AT_LINK:
	case AT_FILE:
		t->files_done += 1;
		break;
	case AT_DIR:
		t->dirs_done += 1;
		break;
	default:
		break;
	}
	if ((t->chp != 0 || t->chm != 0)
	&& (t->err = relative_chmod(t->tw.path, t->chp, t->chm))) {
		return;
	}
	if (chown(t->tw.path, t->cho, t->chg) ? (t->err = errno) : 0) return;
	if ((t->err = tree_walk_step(&t->tw))) return;
	if (!(t->tf & TF_RECURSIVE_CHMOD)) { // TODO find a better way to chmod once
		t->tw.tws = AT_EXIT;
	}
	*c -= 1; // TODO
}

void task_action_estimate(struct task* const t, int* const c) {
	switch (t->tw.tws) {
	case AT_LINK:
		if (!(t->tf & (TF_ANY_LINK_METHOD))) {
			*c = 0; // TODO
			return;
		}
		t->symlinks += 1;
		t->files_total += 1;
		break;
	case AT_FILE:
		t->files_total += 1;
		break;
	case AT_DIR:
		t->dirs_total += 1;
		break;
	default:
		t->specials += 1;
		break;
	}
	const bool Q = t->tw.tws & (AT_DIR | AT_LINK | AT_FILE);
	if ((t->t == TASK_COPY || t->t == TASK_MOVE) && Q) {
		char new_path[PATH_BUF_SIZE];
		task_build_path(t, new_path);
		if (!access(new_path, F_OK)) {
			t->conflicts += 1;
		}
	}
	t->size_total += t->tw.cs.st_size; // TODO
	*c -= 1;
	if ((t->err = tree_walk_step(&t->tw))) {
		t->ts = TS_FAILED;
	}
}

void task_do(struct task* const t, task_action ta,
		const enum task_state onend) {
	int c;
	if (t->ts & TS_ESTIMATE) {
		c = 1024*2;
	}
	else {
		c = 1024 * 1024 * 16;
	}
	if (t->tw.tws == AT_NOWHERE) {
		t->err = tree_walk_start(&t->tw, t->src,
			t->sources.arr[t->current_source]->str,
			t->sources.arr[t->current_source]->len);
		if (t->err) {
			t->tw.tws = AT_NOWHERE;
			return;
		}
	}
	if (t->tf & TF_DEREF_LINKS) {
		t->tw.tl = true;
	}
	while (t->tw.tws != AT_EXIT && !t->err && c > 0) {
		ta(t, &c);
	}
	if (t->err) {
		t->ts = TS_FAILED;
	}
	else if (t->tw.tws == AT_EXIT) {
		t->current_source += 1;
		t->tw.tws = AT_NOWHERE;
		if (t->current_source == t->sources.len) {
			t->ts = onend;
			tree_walk_end(&t->tw);
			t->current_source = 0;
		}
	}
}

static bool _files_opened(const struct task* const t) {
	return t->out != -1 && t->in != -1;
}

static int _open_files(struct task* const t,
		const char* const dst, const char* const src) {
	t->out = open(src, O_RDONLY);
	if (t->out == -1) {
		return errno; // TODO TODO IMPORTANT
	}
	t->in = open(dst, O_WRONLY | O_CREAT, t->tw.cs.st_mode);
	if (t->in == -1) {
		close(t->out);
		t->out = -1;
		return errno;
	}
	struct stat outs;
	if (fstat(t->out, &outs)) return errno;
#if HAS_FALLOCATE
	if (outs.st_size > 0) {
		int e = posix_fallocate(t->in, 0, outs.st_size);
		// TODO detect earlier if fallocate is supported
		if (e != EOPNOTSUPP && e != ENOSYS) return e;
	}
#endif
	return 0;
}

static int _copy(struct task* const t, const char* const src,
		const char* const dst, int* const c) {
	char buf[BUFSIZ];
	// TODO if it fails at any point it should seek back
	// to enable retrying
	int e = 0;
	if (!_files_opened(t) && (e = _open_files(t, dst, src))) {
		return e;
	}
	ssize_t wb = -1, rb = -1;
	while (*c > 0 && _files_opened(t)) {
		rb = read(t->out, buf, sizeof(buf));
		if (!rb) { // done copying
			t->files_done += 1;
			_close_files(t);
			return e;
		}
		if (rb == -1) {
			e = errno;
			_close_files(t);
			return e;
		}
		wb = write(t->in, buf, rb);
		if (wb == -1) {
			e = errno;
			_close_files(t);
			return e;
		}
		t->size_done += wb;
		*c -= wb;
	}
	return 0;
}

/*
 * No effects on failure
 */
static int _stat_file(struct tree_walk* const tw) {
	// lstat/stat can errno:
	// ENOENT, EACCES, ELOOP, ENAMETOOLONG, ENOMEM, ENOTDIR, EOVERFLOW
	const struct stat old_cs = tw->cs;
	const enum tree_walk_state old_tws = tw->tws;
	if (lstat(tw->path, &tw->cs)) {
		tw->cs = old_cs;
		return errno;
	}
	tw->tws = AT_NOWHERE;
	do {
		switch (tw->cs.st_mode & S_IFMT) {
		case S_IFDIR:
			tw->tws = AT_DIR;
			break;
		case S_IFREG:
			tw->tws = AT_FILE;
			break;
		case S_IFLNK:
			if (!tw->tl) {
				tw->tws = AT_LINK;
			}
			else if (stat(tw->path, &tw->cs)) {
				int err = errno;
				if (err == ENOENT || err == ELOOP) {
					tw->tws = AT_LINK;
				}
				else {
					tw->cs = old_cs;
					tw->tws = old_tws;
					return err;
				}
			}
			break;
		default:
			tw->tws = AT_SPECIAL;
			break;
		}
	} while (tw->tws == AT_NOWHERE);
	return 0;
}

int tree_walk_start(struct tree_walk* const tw,
		const char* const path,
		const char* const file,
		const size_t file_len) {
	if (tw->path) free(tw->path);
	tw->pathlen = strnlen(path, PATH_MAX_LEN);
	tw->path = malloc(PATH_BUF_SIZE);
	memcpy(tw->path, path, tw->pathlen+1);
	pushd(tw->path, &tw->pathlen, file, file_len);
	tw->tl = false;
	if (tw->dt) free(tw->dt);
	tw->dt = calloc(1, sizeof(struct dirtree));
	return _stat_file(tw);
}

void tree_walk_end(struct tree_walk* const tw) {
	struct dirtree* DT = tw->dt;
	while (DT) {
		struct dirtree* UP = DT->up;
		free(DT);
		DT = UP;
	}
	free(tw->path);
	memset(tw, 0, sizeof(struct tree_walk));
}

//TODO: void tree_walk_skip(struct tree_walk* const tw) {}

int tree_walk_step(struct tree_walk* const tw) {
	struct dirtree *new_dt, *up;
	switch (tw->tws)  {
	case AT_LINK:
	case AT_FILE:
		if (!tw->dt->cd) {
			tw->tws = AT_EXIT;
			return 0;
		}
		popd(tw->path, &tw->pathlen);
		break;
	case AT_DIR:
		/* Go deeper */
		new_dt = calloc(1, sizeof(struct dirtree));
		new_dt->up = tw->dt;
		tw->dt = new_dt;
		errno = 0;
		if (!(new_dt->cd = opendir(tw->path))) {
			tw->dt = new_dt->up;
			free(new_dt);
			return errno;
		}
		break;
	case AT_DIR_END:
		/* Go back */
		if (tw->dt->cd) closedir(tw->dt->cd);
		up = tw->dt->up;
		free(tw->dt);
		tw->dt = up;
		popd(tw->path, &tw->pathlen);
		break;
	default:
		break;
	}
	if (!tw->dt || !tw->dt->up) { // last dir
		free(tw->dt);
		tw->dt = NULL;
		tw->tws = AT_EXIT;
		return 0;
	}

	struct dirent* ce;
	do {
		errno = 0;
		ce = readdir(tw->dt->cd);
	} while (ce && DOTDOT(ce->d_name));

	// TODO errno
	if (!ce) {
		tw->tws = AT_DIR_END;
		return errno;
	}
	const size_t nl = strnlen(ce->d_name, NAME_MAX_LEN);
	pushd(tw->path, &tw->pathlen, ce->d_name, nl);
	return _stat_file(tw);
}

inline static int _copyremove_step(struct task* const t, int* const c) {
	// TODO absolute mess; simplify
	// TODO skipped counter
	char np[PATH_BUF_SIZE];
	const bool cp = t->t & (TASK_COPY | TASK_MOVE);
	const bool rm = t->t & (TASK_MOVE | TASK_REMOVE);
	const bool ov = t->tf & TF_OVERWRITE_CONFLICTS;
	int err = 0;

	/* QUICK MOVE */
	if ((t->t & TASK_MOVE) && same_fs(t->src, t->dst)) {
		task_build_path(t, np);
		if (rename(t->tw.path, np)) {
			return errno;
		}
		t->tw.tws = AT_EXIT;
		t->size_done = t->size_total;
		t->files_done = t->files_total;
		t->dirs_done = t->dirs_total;
		return 0;
	}

	/* SKIP LINKS FLAG */
	if ((t->tw.tws & AT_LINK) && (t->tf & TF_SKIP_LINKS)) {
		return 0;
	}

	/* COPYING */
	if (cp) {
		task_build_path(t, np);

		/* IF DESTINATION EXISTS */
		if (!access(np, F_OK)) {
			if (t->tf & TF_SKIP_CONFLICTS) return 0;
			if (ov || (t->tf & TF_OVERWRITE_ONCE)) {
				t->tf &= ~TF_OVERWRITE_ONCE;
				if ((t->tw.tws & (AT_FILE | AT_LINK))
					&& unlink(np)) return errno;
			}
			else if (t->tf & TF_ASK_CONFLICTS) {
				return EEXIST;
			}
		}

		switch (t->tw.tws) {
		case AT_FILE:
			if ((err = _copy(t, t->tw.path, np, c))) {
				return err;
			}
			/* Opened = unfinished.
			 * Next call will push copying forward. */
			if (_files_opened(t)) {
				return 0;
			}
			break;
		case AT_LINK:
			if (t->tf & TF_RAW_LINKS) {
				err = link_copy_raw(t->tw.path, np);
			}
			else {
				err = link_copy_recalculate(t->src,
						t->tw.path, np);
			}
			if (err) return err;
			t->size_done += t->tw.cs.st_size;
			t->files_done += 1;
			break;
		case AT_DIR:
			if (mkdir(np, t->tw.cs.st_mode)) {
				/* TODO
				 * One cannot remove non-empty directory
				 * to prevent EEXIST error on overwrite flag
				 */
				err = errno;
				if (ov && err == EEXIST) {
					err = 0;
				}
			}
			if (err) return err;
			t->size_done += t->tw.cs.st_size;
			t->dirs_done += 1;
			*c -= t->tw.cs.st_size;
			break;
		default:
			break;
		}
	}
	if (rm) {
		if (t->tw.tws & (AT_FILE | AT_LINK)) {
			if (unlink(t->tw.path)) {
				return errno;
			}
			if (!cp) { // TODO
				t->size_done += t->tw.cs.st_size;
				t->files_done += 1;
				*c -= t->tw.cs.st_size;
			}
		}
		else if (t->tw.tws & AT_DIR_END) {
			if (rmdir(t->tw.path)) {
				return errno;
			}
			t->size_done += t->tw.cs.st_size;
			t->dirs_done += 1;
			*c -= t->tw.cs.st_size;
		}
	}
	return 0;
}

void task_action_copyremove(struct task* const t, int* const c) {
	if ((t->err = _copyremove_step(t, c))
	|| (_files_opened(t))
	|| (t->err = tree_walk_step(&t->tw))) {
		return;
	}
}
