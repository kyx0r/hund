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
	TASK_REMOVE = 1<<0,
	TASK_COPY = 1<<1,
	TASK_MOVE = 1<<2,
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
	AT_FILE,
	AT_LINK,
	AT_DIR, // on dir (will enter this dir)
	AT_DIR_END, // finished reading dir (will go up)
	AT_SPECIAL, // anything other than link, dir or regular file
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

	struct stat cs; // Current Stat
	char* cpath;
};

enum task_flags {
	TF_NONE = 0,
	TF_RAW_LINKS = 1<<0, // Copy links raw instead of recalculating
	TF_MERGE = 1<<1, // Merge directories (overwrite filesby default)
	TF_SKIP_CONFLICTS = 1<<3, // If merging, skip (leave) conflicting
	TF_DEREF_LINKS = 1<<4, // If copying/moving links, copy what they point to
};

/*
 * Long disk operations
 * Displays state
 */
struct task {
	enum task_type t;
	bool paused;
	bool done;

	enum task_flags tf;

	//    vvv basically pointers to file_view->wd; to not free
	char* src; // Source directory path
	char* dst; // Destination directory path
	struct string_list sources; // Files to be copied
	struct string_list renamed; // Same size as sources,
	fnum_t current_source;
	// NULL == no conflict, use origial name
	// NONNULL = conflict, contains pointer to name replacement

	struct tree_walk tw;
	int in, out;

	bool estimated;
	int conflicts;
	ssize_t size_total, size_done;
	int files_total, files_done;
	int dirs_total, dirs_done;
};

void task_new(struct task* const, const enum task_type,
		char* const, char* const,
		const struct string_list* const,
		const struct string_list* const);

int task_estimate(struct task* const, int c);

void task_clean(struct task* const);

void build_new_path(const char* const, const char* const,
		const char* const, const char* const, const char* const,
		char* const);

int tree_walk_start(struct tree_walk* const, const char* const, const bool);
void tree_walk_end(struct tree_walk* const);
int tree_walk_step(struct tree_walk* const);

char* current_source_path(struct task* const);

int do_task(struct task* const, int);
#endif
