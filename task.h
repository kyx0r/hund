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

#if defined(__OpenBSD__) // TODO
	#define HAS_FALLOCATE 0
#else
	#define HAS_FALLOCATE 1
#endif

#include <stdint.h>
#include <time.h>

#include "fs.h"
#include "utf8.h"

typedef unsigned long long xtime_ms_t;

xtime_ms_t xtime(void);

enum task_type {
	TASK_NONE = 0,
	TASK_REMOVE = 1<<0,
	TASK_COPY = 1<<1,
	TASK_MOVE = 1<<2,
	TASK_CHMOD = 1<<3,
};

/*
 * If Link Transparency is true in tree_walk,
 * tree_walk_step will output AT_FILE or AT_DIR,
 * If LT is false, then AT_LINK is outputted
 */
enum tree_walk_state {
	AT_NOWHERE = 0,
	AT_EXIT = 1<<0, // finished reading tree
	AT_FILE = 1<<1,
	AT_LINK = 1<<2,
	AT_DIR = 1<<3, // on dir (will enter this dir)
	AT_DIR_END = 1<<4, // finished reading dir (will go up)
	AT_SPECIAL = 1<<5, // anything other than link, dir or regular file
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
	char* path;
	size_t pathlen;
};

enum task_flags {
	TF_RAW_LINKS = 1<<0, // Copy links raw instead of recalculating
	TF_OVERWRITE_CONFLICTS = 1<<1,
	TF_OVERWRITE_ONCE = 1<<2,
	TF_ASK_CONFLICTS = 1<<3,
	TF_SKIP_CONFLICTS = 1<<4,
	TF_DEREF_LINKS = 1<<5, // If copying/moving links, copy what they point to
	TF_SKIP_LINKS = 1<<6,
	TF_RECURSIVE_CHMOD = 1<<7,
	TF_RECALCULATE_LINKS = 1<<8,
	TF_ANY_LINK_METHOD = (TF_RAW_LINKS | TF_DEREF_LINKS
		| TF_SKIP_LINKS | TF_RECALCULATE_LINKS),
};

enum task_state {
	TS_CLEAN = 0,
	TS_ESTIMATE = 1<<0, // after task_new; runs task_estimate
	TS_CONFIRM = 1<<1, // after task_estimate is finished; task configuration
	TS_RUNNING = 1<<2, // task runs
	TS_PAUSED = 1<<3,
	TS_FAILED = 1<<4, // if something went wrong. on some errors task can retry
	TS_FINISHED = 1<<5 // task succesfully finished; cleans up, returns to TS_CLEAN
};

struct task {
	enum task_type t;
	enum task_state ts;
	enum task_flags tf;

	//    vvv basically pointers to panel->wd; to not free
	char* src; // Source directory path
	char* dst; // Destination directory path
	struct string_list sources; // Files to be copied
	struct string_list renamed; // Same size as sources,
	fnum_t current_source;
	// NULL == no conflict, use origial name
	// NONNULL = conflict, contains pointer to name replacement

	int err; // Last errno
	struct tree_walk tw;
	int in, out;

	fnum_t conflicts, symlinks, specials;
	ssize_t size_total, size_done;
	fnum_t files_total, files_done;
	fnum_t dirs_total, dirs_done;

	mode_t chp, chm;
	uid_t cho;
	gid_t chg;
};

void task_new(struct task* const, const enum task_type,
		const enum task_flags,
		char* const, char* const,
		const struct string_list* const,
		const struct string_list* const);
void task_clean(struct task* const);
int task_build_path(const struct task* const, char*);

typedef void (*task_action)(struct task* const, int* const);
void task_action_chmod(struct task* const, int* const);
void task_action_estimate(struct task* const, int* const);
void task_action_copyremove(struct task* const, int* const);
void task_do(struct task* const, task_action, const enum task_state);

int tree_walk_start(struct tree_walk* const, const char* const,
		const char* const, const size_t);
void tree_walk_end(struct tree_walk* const);
int tree_walk_step(struct tree_walk* const);

#endif
