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

#ifndef UI_H
#define UI_H

#define _GNU_SOURCE
#include <ncurses.h>
#include <panel.h>
#include <linux/limits.h>
#include <locale.h>
#include <syslog.h>

#include "path.h"
#include "file.h"

enum command {
	NONE = 0,
	QUIT,
	COPY,
	MOVE,
	REMOVE,
	SWITCH_PANEL,
	UP_DIR,
	ENTER_DIR,
	REFRESH,
	ENTRY_UP,
	ENTRY_DOWN,
	CREATE_DIR,
};

#define MAX_KEYSEQ_LENGTH 4

struct key2cmd {
	int ks[MAX_KEYSEQ_LENGTH]; // Key Sequence
	char* d;
	enum command c;
};

static struct key2cmd key_mapping[] = {
	{ .ks = { 'q', 'q', 0, 0 }, .d = "quit", .c = QUIT  },
	{ .ks = { 'j', 0, 0, 0 }, .d = "down", .c = ENTRY_DOWN },
	{ .ks = { 'k', 0, 0, 0 }, .d = "up", .c = ENTRY_UP },
	{ .ks = { 'c', 'p', 0, 0 }, .d = "copy", .c = COPY },
	{ .ks = { 'r', 'm', 0, 0 }, .d = "remove", .c = REMOVE },
	{ .ks = { 'm', 'v', 0, 0 }, .d = "move", .c = MOVE },
	{ .ks = { '\t', 0, 0, 0 }, .d = "switch panel", .c = SWITCH_PANEL },
	{ .ks = { 'r', 'r', 0, 0 }, .d = "refresh", .c = REFRESH },
	{ .ks = { 'm', 'k', 0, 0 }, .d = "create dir", .c = CREATE_DIR },
	{ .ks = { 'u', 0, 0, 0 }, .d = "up dir", .c = UP_DIR },
	{ .ks = { 'd', 0, 0, 0 }, .d = "up dir", .c = UP_DIR },
	{ .ks = { 'i', 0, 0, 0 }, .d = "enter dir", .c = ENTER_DIR },
	{ .ks = { 'e', 0, 0, 0 }, .d = "enter dir", .c = ENTER_DIR },

	{ .ks = { 'x', 'x', 0, 0 }, .d = "quit", .c = QUIT },
	{ .ks = { 'x', 'y', 0, 0 }, .d = "quit", .c = QUIT },
	{ .ks = { 'x', 'z', 0, 0 }, .d = "quit", .c = QUIT },

	{ .ks = { 0, 0, 0, 0 }, .d = NULL, .c = NONE }
};

struct file_view {
	char wd[PATH_MAX];
	struct file_record** file_list;
	int num_files;
	int selection;
	int view_offset;
};

/* UI is intended only to handle drawing functions
 * No FS logic or data manipulation
 */
struct ui {
	int scrh, scrw;
	int active_view;
	PANEL* fvp[2];
	struct file_view fvs[2];
	char* prompt_title;
	char* prompt_textbox;
	int prompt_textbox_size;
	PANEL* prompt;
	PANEL* hint;
	int kml;
	int* mks; // Matching Key Sequence
};

void ui_init(struct ui* const);
void ui_end(struct ui* const);
void ui_draw(struct ui* const);
void ui_update_geometry(struct ui* const);
void prompt_open(struct ui* i, char* ptt, char* ptb, int ptbs);
void prompt_close(struct ui*);
enum command get_cmd(struct ui*);

#endif
