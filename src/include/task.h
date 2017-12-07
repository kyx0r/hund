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
	TASK_REMOVE,
	TASK_COPY,
	TASK_MOVE,
};

#define NOUN 0
#define ING 1
#define PAST 2
static const char* const task_strings[][3] = {
	[TASK_NONE] = { NULL, NULL, NULL},
	[TASK_REMOVE] = { "remove", "removing", "removed" },
	[TASK_COPY] = { "copy", "copying", "copied" },
	[TASK_MOVE] = { "move", "moving", "moved" },
};

enum task_state {
	TASK_STATE_CLEAN = 0,
	TASK_STATE_EXECUTING,
	TASK_STATE_FINISHED, // AKA all done, cleanme
	//TASK_STATE_FAILED
	TASK_STATE_NUM
};

enum tree_walk_state {
	AT_NOWHERE = 0, // finished walking
	AT_INIT, // right after start_dir(), expects directory
	AT_FILE, // on file
	AT_DIR, // on dir (will enter this dir)
	AT_END, // finished reading dir, will go up
};

struct dirtree {
	struct dirtree* up; // ..
	DIR* cd; // Current Directory
	struct stat cs; // Current Stat
	struct dirent* ce; // Current Entry
};

/* It's basically an iterative directory tree walker
 * tree_walk_step() goes to the next POINT
 * POINT may be a file, directory OR directory end or end of tree
 * Reacting to those POINTS is done in a simple loop and a switch statement.
 */
struct tree_walk {
	enum tree_walk_state tws;
	struct dirtree* dt;
	char* wpath; // working path; needed for the 'walker'
	char* cpath; // current path; will contain path of file/dir at current step
};

/* Requires long disk operations
 * Displays state
 */
struct task {
	enum task_state s;
	enum task_type t;
	bool running; // used for pausing
	char *src, *dst, *newname; // newname is used for name conflicts

	struct tree_walk tw;

	int in, out; // when copying, fd of old and new files are held here

	ssize_t size_total, size_done;
	int files_total, files_done;
	int dirs_total, dirs_done;
};

void task_new(struct task*, enum task_type, char*, char*, char*);

int task_estimate_file_volume(struct task*, char*);
void task_clean(struct task*);

char* build_new_path(struct task*, char*);

void tree_walk_start(struct tree_walk*, const char* const);
void tree_walk_end(struct tree_walk*);
void tree_walk_step(struct tree_walk*);

int do_task(struct task*, int);

int rename_if_same_fs(const struct task* const);
#endif
