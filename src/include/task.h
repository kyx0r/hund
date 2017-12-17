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

#include "fs.h"
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

/*
 * If Link Transparency is true in tree_walk,
 * tree_walk_step will output AT_FILE or AT_DIR,
 * If LT is false, then AT_LINK is outputted
 */
enum tree_walk_state {
	AT_NOWHERE = 0,
	AT_EXIT, // finished reading tree
	AT_INIT,
	AT_FILE,
	AT_LINK,
	AT_DIR, // on dir (will enter this dir)
	AT_DIR_END, // finished reading dir (will go up)
};

struct dirtree {
	struct dirtree* up; // ..
	DIR* cd; // Current Directory
};

/*
 * It's basically an iterative directory tree walker
 * Reacting to AT_* steps is done in a simple loop and a switch statement.
 *
 * Iterative; non-recursive
 */
struct tree_walk {
	enum tree_walk_state tws;
	bool tl; // Transparent Links
	struct dirtree* dt;
	char* wpath;

	struct stat cs; // Current Stat
	char* dname;
	char* cpath; // current path; will contain path of file/dir at current step
};

/*
 * Long disk operations
 * Displays state
 */
struct task {
	enum task_type t;
	bool paused;
	bool done;
	bool rl; // Raw Links
	char *src, *dst, *newname; // newname is used for name conflicts
	struct tree_walk tw;
	int in, out; // when copying, fd of old and new files are held here
	ssize_t size_total, size_done;
	int files_total, files_done;
	int dirs_total, dirs_done;
};

void task_new(struct task* const, const enum task_type,
		char* const, char* const, char* const);
void task_clean(struct task* const);

int estimate_volume(char*, ssize_t* const, int* const, int* const, const bool);

char* build_new_path(const struct task* const,
		const char* const, char* const);

void tree_walk_start(struct tree_walk*, const char* const);
void tree_walk_end(struct tree_walk*);
void tree_walk_step(struct tree_walk*);

int do_task(struct task*, int);
#endif
