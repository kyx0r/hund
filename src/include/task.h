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

#ifndef _DEFAULT_SOURCE
	#define _DEFAULT_SOURCE
#endif

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
	TASK_CHOWN,
	TASK_CHGRP,
};

#define NOUN 0
#define ING 1
#define PAST 2
static const utf8* const task_strings[][3] = {
	[TASK_NONE] = { NULL, NULL, NULL},
	[TASK_RM] = { "remove", "removing", "removed" },
	[TASK_COPY] = { "copy", "copying", "copied" },
	[TASK_MOVE] = { "move", "moving", "moved" },
	[TASK_RENAME] = { "rename", NULL, "renamed" },
	[TASK_CD] = { "open", NULL, "opened" },
	[TASK_MKDIR] = { "create directory", NULL, "created directory" }
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

enum fs_walk {
	AT_NOWHERE = 0,
	AT_FILE,
	AT_DIR,
	AT_END,
};

struct dirtree {
	struct dirtree* up;
	DIR* cd;
	struct stat cs;
	struct dirent* ce;
};

/* AKA long task
 * Requires long disk operations
 * Displays state
 */
struct task {
	enum task_state s;
	enum task_type t;
	bool running;
	utf8 *src, *dst, *newname;

	char wpath[PATH_MAX+1];
	struct dirtree* dir;
	int in, out;
	ssize_t size_total, size_done;
	int files_total, files_done;
	int dirs_total, dirs_done;
};

void task_new(struct task*, enum task_type, utf8*, utf8*, utf8*);

int task_build_file_list(struct task*, char*);
void task_clean(struct task*);

utf8* build_new_path(struct task*, utf8*);

int do_task(struct task*, int);

int rename_if_same_fs(const struct task* const);
#endif
