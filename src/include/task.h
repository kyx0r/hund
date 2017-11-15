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
	TASK_MKDIR, //dst = new dir path
	TASK_RM, //src = path to what to remove
	TASK_COPY, //src -> dst
	TASK_MOVE, //src -> dst
	TASK_CD, //dst = what to open
	TASK_RENAME, //src = old name, dst = new name
	TASK_CHOWN, //src = path to what change
	TASK_CHGRP, //src = path to what change
};

enum task_state {
	TASK_STATE_CLEAN = 0,
	TASK_STATE_GATHERING_DATA,
	TASK_STATE_DATA_GATHERED, // AKA ready to execute
	TASK_STATE_EXECUTING,
	TASK_STATE_FINISHED // AKA all done, cleanme
};

struct file_todo {
	struct file_todo* next;
	utf8* path;
	struct stat s;
	uintmax_t progress;
};

struct task {
	enum task_state s;
	enum task_type t;
	utf8 *src, *dst; // *ch
	struct file_todo* checklist;
	FILE* working;
	uintmax_t size_total;
	uintmax_t size_done;
	uintmax_t files_total;
	uintmax_t files_done;
};

struct task task_new(enum task_type, utf8*, utf8*);
int task_build_file_list(struct task*);
void task_check_file(struct task*);
void task_clean(struct task*);

int do_remove(struct task*, int);
int do_move(struct task*, int);
int do_copy(struct task*, int);

#endif
