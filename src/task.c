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

struct task task_new(enum task_type t, utf8* s, utf8* d) {
	struct task r = {
		.s = TASK_STATE_GATHERING_DATA,
		.t = t,
		.src = s,
		.dst = d,
		.checklist = NULL,
		.working = NULL,
		.size_total = 0,
		.size_done = 0,
		.files_total = 0,
		.files_done = 0
	};
	return r;
}

static void _push_file(struct task* t, struct stat s, utf8* p) {
	//printf("%c %s\n", (S_ISDIR(s.st_mode) ? 'd' : 'f'), p);
	struct file_todo* ft = malloc(sizeof(struct file_todo));
	*ft = (struct file_todo) {
		.next = t->checklist,
		.path = p,
		.s = s,
		.progress = 0
	};
	t->files_total += 1;
	t->size_total += s.st_size;
	t->checklist = ft;
}

/* Checklist is a linked list - queue
 * dir_first = true: push after it's contents, so that it is before them in queue
 * dir_first = false: push before it's contents, so that is is after them in queue
 */
// TODO follow links
static int _build_file_list(struct task* t, utf8* path, bool dir_first) {
	int r = 0;
	struct stat s;
	if (lstat(path, &s)) return errno;
	if (!dir_first) _push_file(t, s, path);
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
				r |= _build_file_list(t, fpath, dir_first);
			}
			else {
				_push_file(t, ss, fpath);
			}
		}
		closedir(dir);
	}
	if (dir_first) _push_file(t, s, path);
	return r;
}

int task_build_file_list(struct task* t) {
	utf8* path = malloc(strlen(t->src)+1);
	strcpy(path, t->src);
	bool df = true;
	if (t->t == TASK_RM) df = false;
	return _build_file_list(t, path, df);
}

void task_check_file(struct task* t) {
	if (!t->checklist) return;
	struct file_todo* head = t->checklist;
	t->checklist = head->next;
	t->size_done += head->s.st_size;
	t->files_done += 1;
	free(head->path);
	free(head);
}

void task_clean(struct task* t) {
	if (t->src)	free(t->src);
	if (t->dst) free(t->dst);
	t->src = NULL;
	t->dst = NULL;
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
	t->working = NULL;
}

int do_remove(struct task* t, int c) {
	int r = 0;
	while (t->checklist && c >= 0) {
		if (S_ISDIR(t->checklist->s.st_mode)) {
			if (rmdir(t->checklist->path)) r |= errno;
		}
		else {
			if (unlink(t->checklist->path)) r |= errno;
		}
		task_check_file(t);
		c -= 1;
	}
	if (!t->checklist) t->s = TASK_STATE_FINISHED;
	return r;
}

int do_move(struct task* t, int c) {
	return 0;
}

int do_copy(struct task* t, int c) {
	return 0;
}
