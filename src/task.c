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

void task_new(struct task* t, enum task_type tp, utf8* src, utf8* dst) {
	*t = (struct task) {
		.s = TASK_STATE_GATHERING_DATA,
		.t = tp,
		.src = src,
		.dst = dst,
		.checklist = NULL,
		.in = -1,
		.out = -1,
		.size_total = 0,
		.size_done = 0,
		.files_total = 0,
		.files_done = 0
	};
	t->src_2repl = malloc(PATH_MAX);
	const size_t cdpos = current_dir_i(t->src)-1;
	strncpy(t->src_2repl, t->src, cdpos);
	t->src_2repl[cdpos] = 0;
}

static void _push_file(struct task* t, struct stat s, utf8* p, enum todo td) {
	struct file_todo* ft = malloc(sizeof(struct file_todo));
	*ft = (struct file_todo) {
		.next = t->checklist,
		.td = td,
		.path = p,
		.s = s,
		.progress = 0
	};
	if ((t->t == TASK_RM && td == TODO_REMOVE) ||
			(td == TODO_COPY && (t->t == TASK_COPY || t->t == TASK_MOVE))) {
		t->files_total += 1;
		t->size_total += s.st_size;
	}
	t->checklist = ft;
}

/* Checklist is a linked list - queue */
// TODO follow links
/* Only handles TASK_{RM,MOVE,COPY} */
static int _build_file_list(struct task* t, utf8* path, enum task_type tt) {
	int r = 0;
	struct stat s;
	if (lstat(path, &s)) return errno;
	if (tt == TASK_RM || tt == TASK_MOVE) _push_file(t, s, path, TODO_REMOVE);
	if (S_ISDIR(s.st_mode)) {
		DIR* dir = opendir(path);
		struct dirent* de;
		while ((de = readdir(dir)) != NULL) {
			if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
				continue;
			}
			utf8* fpath = malloc(strlen(path)+1+strlen(de->d_name)+1);
			strcpy(fpath, path);
			strcat(fpath, "/");
			strcat(fpath, de->d_name);
			struct stat ss;
			if (lstat(fpath, &ss)) return errno;
			if (S_ISDIR(ss.st_mode)) {
				r |= _build_file_list(t, fpath, tt);
			}
			else {
				if (tt == TASK_RM) {
					_push_file(t, ss, fpath, TODO_REMOVE);
				}
				else if (tt == TASK_COPY) {
					_push_file(t, ss, fpath, TODO_COPY);
				}
				else if (tt == TASK_MOVE) {
					_push_file(t, ss, fpath, TODO_REMOVE);
					_push_file(t, ss, strdup(fpath), TODO_COPY);
				}
			}
		}
		closedir(dir);
	}
	if (tt == TASK_COPY || tt == TASK_MOVE) {
		if (tt == TASK_MOVE) path = strdup(path);
		_push_file(t, s, path, TODO_COPY);
	}
	return r;
}

int task_build_file_list(struct task* t) {
	utf8* path = malloc(strlen(t->src)+1);
	strcpy(path, t->src);
	return _build_file_list(t, path, t->t);
}

void task_check_file(struct task* t) {
	if (!t->checklist) return;
	struct file_todo* head = t->checklist;
	t->checklist = head->next;
	if (head->td == TODO_REMOVE && t->t == TASK_RM) t->files_done += 1;
	if (head->td == TODO_COPY && (t->t == TASK_MOVE || t->t == TASK_COPY)) {
		t->files_done += 1;
	}
	free(head->path);
	free(head);
}

void task_clean(struct task* t) {
	if (t->src)	free(t->src);
	if (t->dst) free(t->dst);
	if (t->src_2repl) free(t->src_2repl);
	t->src = NULL;
	t->dst = NULL;
	t->src_2repl = NULL;
	struct file_todo* n = t->checklist;
	struct file_todo* p;
	while (n) {
		p = n;
		n = n->next;
		free(p->path);
		free(p);
	}
	t->s = TASK_STATE_CLEAN;
	t->t = TASK_NONE;
	t->checklist = NULL;
	if (t->in) close(t->in);
	t->in = -1;
	if (t->out) close(t->out);
	t->out = -1;
}

static ssize_t _copy(int in, int out, void* buf, ssize_t n) {
	int rb = read(out, buf, n);
	if (!rb) return rb;
	if (rb == -1) return errno;
	int wb = write(in, buf, rb);
	if (wb == -1) return errno;
	return wb;
}

static int _copy_some(struct task* t, utf8* npath, void* buf, int* c) {
	if (t->out == -1) {
		syslog(LOG_DEBUG, "%s -> %s", t->checklist->path, npath);
		t->out = open(t->checklist->path, O_RDONLY);
		if (t->out == -1) return errno;
		t->in = open(npath, O_CREAT | t->checklist->s.st_mode);
		if (t->in == -1) return errno;
	}
	ssize_t d = -1;
	while (*c > 0 && d) {
		d = _copy(t->in, t->out, buf, BUFSIZ);
		t->size_done += d;
		t->checklist->progress += d;
		*c -= 1;
	}
	if (!d) {
		close(t->in);
		close(t->out);
		t->in = -1;
		t->out = -1;
		task_check_file(t);
	}
	return 0;
}

int do_task(struct task* t, int c) {
	int r = 0;
	while (t->checklist && c > 0) {
		bool isdir = S_ISDIR(t->checklist->s.st_mode);
		if (t->checklist->td == TODO_COPY) {
			utf8* npath = malloc(PATH_MAX);
			strcpy(npath, t->checklist->path);
			substitute(npath, t->src_2repl, t->dst);
			if (isdir) {
				if (mkdir(npath, t->checklist->s.st_mode)) r |= errno;
				task_check_file(t);
				c -= 1;
			}
			else {
				char buf[BUFSIZ];
				r |= _copy_some(t, npath, buf, &c);
			}
			free(npath);
		}
		else {
			if (isdir) {
				if (rmdir(t->checklist->path)) r |= errno;
				task_check_file(t);
				c -= 1;
			}
			else {
				if (unlink(t->checklist->path)) r |= errno;
				task_check_file(t);
				c -= 1;
			}
		}
	}
	if (!t->checklist) t->s = TASK_STATE_FINISHED;
	return r;
}
