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

#include "file.h"

enum mode {
	MODE_MANAGER,
	MODE_PROMPT
};

enum command {
	CMD_NONE = 0,
	CMD_QUIT,
	CMD_COPY,
	CMD_MOVE,
	CMD_REMOVE,
	CMD_SWITCH_PANEL,
	CMD_UP_DIR,
	CMD_ENTER_DIR,
	CMD_REFRESH,
	CMD_ENTRY_UP,
	CMD_ENTRY_DOWN,
	CMD_CREATE_DIR,
};

#define MAX_KEYSEQ_LENGTH 4

struct key2cmd {
	int ks[MAX_KEYSEQ_LENGTH]; // Key Sequence
	char* d;
	enum mode m;
	enum command c;
};

static const struct key2cmd key_mapping[] = {
	{ .ks = { 'q', 'q', 0, 0 }, .d = "quit", .m = MODE_MANAGER, .c = CMD_QUIT  },
	{ .ks = { 'j', 0, 0, 0 }, .d = "down", .m = MODE_MANAGER, .c = CMD_ENTRY_DOWN },
	{ .ks = { 'k', 0, 0, 0 }, .d = "up", .m = MODE_MANAGER, .c = CMD_ENTRY_UP },
	{ .ks = { 'c', 'p', 0, 0 }, .d = "copy", .m = MODE_MANAGER, .c = CMD_COPY },
	{ .ks = { 'r', 'm', 0, 0 }, .d = "remove", .m = MODE_MANAGER, .c = CMD_REMOVE },
	{ .ks = { 'm', 'v', 0, 0 }, .d = "move", .m = MODE_MANAGER, .c = CMD_MOVE },
	{ .ks = { '\t', 0, 0, 0 }, .d = "switch panel", .m = MODE_MANAGER, .c = CMD_SWITCH_PANEL },
	{ .ks = { 'r', 'r', 0, 0 }, .d = "refresh", .m = MODE_MANAGER, .c = CMD_REFRESH },
	{ .ks = { 'm', 'k', 0, 0 }, .d = "create dir", .m = MODE_MANAGER, .c = CMD_CREATE_DIR },
	{ .ks = { 'u', 0, 0, 0 }, .d = "up dir", .m = MODE_MANAGER, .c = CMD_UP_DIR },
	{ .ks = { 'd', 0, 0, 0 }, .d = "up dir", .m = MODE_MANAGER, .c = CMD_UP_DIR },
	{ .ks = { 'i', 0, 0, 0 }, .d = "enter dir", .m = MODE_MANAGER, .c = CMD_ENTER_DIR },
	{ .ks = { 'e', 0, 0, 0 }, .d = "enter dir", .m = MODE_MANAGER, .c = CMD_ENTER_DIR },

	{ .ks = { 'x', 'x', 0, 0 }, .d = "quit", .m = MODE_MANAGER, .c = CMD_QUIT },
	{ .ks = { 'x', 'y', 0, 0 }, .d = "quit", .m = MODE_MANAGER, .c = CMD_QUIT },
	{ .ks = { 'x', 'z', 0, 0 }, .d = "quit", .m = MODE_MANAGER, .c = CMD_QUIT },

	{ .ks = { 0, 0, 0, 0 }, .d = NULL, .m = 0, .c = CMD_NONE } // Null terminator
	/* TODO if it's global static and const it's size is known at compile time;
	 * get rid of that null terminator
	 * OR
	 * TODO move to some local scope; const can't be altered, and I do want to
	 * alter this thing with a config or something
	 * Where should it be? UI? Probably. But how initialized?
	 */
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
	enum mode m;
	PANEL* fvp[2];
	struct file_view fvs[2]; // Dunno where to put it
	char* prompt_title;
	char* prompt_textbox;
	int prompt_textbox_top;
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
void prompt_close(struct ui*, enum mode);
enum command get_cmd(struct ui*);

#endif
