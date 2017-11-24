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

#ifndef TASK_H
#define TASK_H

// dirent.h and fnctl.h stuff
#define _GNU_SOURCE

#ifndef _XOPEN_SOURCE
	#define _XOPEN_SOURCE // S_ISSOCK
#endif
//#define _XOPEN_SOURCE_EXTENDED

#include <stdint.h>

#include "file.h"
#include "utf8.h"

enum task_type {
	TASK_NONE = 0,
	TASK_RM,
	TASK_COPY,
	TASK_MOVE,
	TASK_RENAME,
	TASK_CD,
	TASK_MKDIR,
	//TASK_CHMOD,
	//TASK_CHGRP,
};

enum task_state {
	TASK_STATE_CLEAN = 0,
	TASK_STATE_GATHERING_DATA,
	TASK_STATE_DATA_GATHERED, // AKA ready to execute
	TASK_STATE_EXECUTING,
	TASK_STATE_FINISHED, // AKA all done, cleanme
	//TASK_STATE_FAILED
	TASK_STATE_NUM
};

enum todo {
	TODO_REMOVE,
	TODO_COPY,
	//TODO_MOVE, // AKA copy first, then remove TODO will shorten the todo list
};

struct file_todo {
	struct file_todo* next;
	enum todo td;
	utf8* path;
	struct stat s; // TODO symlinks?
	ssize_t progress;
};

/* AKA long task
 * Requires long disk operations
 * Displays state
 */
struct task {
	enum task_state s;
	enum task_type t;
	utf8 *src, *dst, *newname;
	struct file_todo* checklist;
	int in, out;
	ssize_t size_total, size_done;
	int files_total, files_done;
	int dirs_total, dirs_done;
};

void task_new(struct task*, enum task_type, utf8*, utf8*, utf8*);

int task_build_file_list(struct task*);
void task_check_file(struct task*);
void task_clean(struct task*);

bool substitute(char*, char*, char*);
utf8* build_new_path(struct task*, utf8*);

int do_task(struct task*, int);

#endif
