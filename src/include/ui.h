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
#include <time.h>
#include <syslog.h>

#include "file_view.h"
#include "utf8.h"

//#define DEFAULT_GETCH_TIMEOUT 500

enum mode {
	MODE_MANAGER,
	MODE_PROMPT,
	MODE_FIND,
	MODE_CHMOD,
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
	CMD_ENTRY_FIRST,
	CMD_ENTRY_LAST,
	CMD_RENAME,
	CMD_TOGGLE_HIDDEN,
	CMD_CD,
	CMD_FIND,

	CMD_CHMOD,
	CMD_RETURN,
	CMD_CHOWN,
	CMD_CHGRP,
	CMD_CHANGE,
	CMD_TOGGLE_UIOX,
	CMD_TOGGLE_GIOX,
	CMD_TOGGLE_SB,
	CMD_TOGGLE_UR,
	CMD_TOGGLE_UW,
	CMD_TOGGLE_UX,
	CMD_TOGGLE_GR,
	CMD_TOGGLE_GW,
	CMD_TOGGLE_GX,
	CMD_TOGGLE_OR,
	CMD_TOGGLE_OW,
	CMD_TOGGLE_OX,
};

#define MAX_KEYSEQ_LENGTH 4

struct key2cmd {
	int ks[MAX_KEYSEQ_LENGTH]; // Key Sequence
	char* d; // description (to be displayed as hint)
	enum mode m; // mode in which command is usable
	enum command c; // triggered command
};

static const struct key2cmd key_mapping[] = {
	/* MODE_MANAGER */
	{ .ks = { 'q', 'q', 0, 0 }, .d = "quit", .m = MODE_MANAGER, .c = CMD_QUIT  },
	{ .ks = { 'g', 'g', 0, 0 }, .d = "top", .m = MODE_MANAGER, .c = CMD_ENTRY_FIRST  },
	{ .ks = { 'G', 0, 0, 0 }, .d = "bottom", .m = MODE_MANAGER, .c = CMD_ENTRY_LAST  },
	{ .ks = { 'j', 0, 0, 0 }, .d = "down", .m = MODE_MANAGER, .c = CMD_ENTRY_DOWN },
	{ .ks = { 'k', 0, 0, 0 }, .d = "up", .m = MODE_MANAGER, .c = CMD_ENTRY_UP },
	{ .ks = { 'c', 'p', 0, 0 }, .d = "copy", .m = MODE_MANAGER, .c = CMD_COPY },
	{ .ks = { 'r', 'm', 0, 0 }, .d = "remove", .m = MODE_MANAGER, .c = CMD_REMOVE },
	{ .ks = { 'r', 'n', 0, 0 }, .d = "rename", .m = MODE_MANAGER, .c = CMD_RENAME },
	{ .ks = { 'm', 'v', 0, 0 }, .d = "move", .m = MODE_MANAGER, .c = CMD_MOVE },
	{ .ks = { '\t', 0, 0, 0 }, .d = "switch panel", .m = MODE_MANAGER, .c = CMD_SWITCH_PANEL },
	{ .ks = { 'r', 'r', 0, 0 }, .d = "refresh", .m = MODE_MANAGER, .c = CMD_REFRESH },
	{ .ks = { 'm', 'k', 0, 0 }, .d = "create dir", .m = MODE_MANAGER, .c = CMD_CREATE_DIR },
	{ .ks = { 'u', 0, 0, 0 }, .d = "up dir", .m = MODE_MANAGER, .c = CMD_UP_DIR },
	{ .ks = { 'd', 0, 0, 0 }, .d = "up dir", .m = MODE_MANAGER, .c = CMD_UP_DIR },
	{ .ks = { 'i', 0, 0, 0 }, .d = "enter dir", .m = MODE_MANAGER, .c = CMD_ENTER_DIR },
	{ .ks = { 'e', 0, 0, 0 }, .d = "enter dir", .m = MODE_MANAGER, .c = CMD_ENTER_DIR },
	{ .ks = { '/', 0, 0, 0 }, .d = "find", .m = MODE_MANAGER, .c = CMD_FIND },
	{ .ks = { 'h', 0, 0, 0 }, .d = "toggle hidden", .m = MODE_MANAGER, .c = CMD_TOGGLE_HIDDEN },
	{ .ks = { 'c', 'd', 0, 0 }, .d = "open directory", .m = MODE_MANAGER, .c = CMD_CD },
	{ .ks = { 'c', 'h', 0, 0 }, .d = "change permission", .m = MODE_MANAGER, .c = CMD_CHMOD },

	/* MODE_CHMOD */
	{ .ks = { 'q', 'q', 0, 0 }, .d = "return", .m = MODE_CHMOD, .c = CMD_RETURN  },
	{ .ks = { 'c', 'h', 0, 0 }, .d = "change", .m = MODE_CHMOD, .c = CMD_CHANGE  },
	{ .ks = { 'c', 'o', 0, 0 }, .d = "change owner", .m = MODE_CHMOD, .c = CMD_CHOWN },
	{ .ks = { 'c', 'g', 0, 0 }, .d = "change group", .m = MODE_CHMOD, .c = CMD_CHGRP },
	{ .ks = { 'u', 'i', 0, 0 }, .d = "toggle set user id on execution", .m = MODE_CHMOD, .c = CMD_TOGGLE_UIOX  },
	{ .ks = { 'g', 'i', 0, 0 }, .d = "toggle set group id on execution", .m = MODE_CHMOD, .c = CMD_TOGGLE_GIOX  },
	{ .ks = { 'o', 's', 0, 0 }, .d = "toggle sticky bit", .m = MODE_CHMOD, .c = CMD_TOGGLE_SB  },
	{ .ks = { 'u', 'r', 0, 0 }, .d = "toggle user read", .m = MODE_CHMOD, .c = CMD_TOGGLE_UR  },
	{ .ks = { 'u', 'w', 0, 0 }, .d = "toggle user write", .m = MODE_CHMOD, .c = CMD_TOGGLE_UW  },
	{ .ks = { 'u', 'x', 0, 0 }, .d = "toggle user execute", .m = MODE_CHMOD, .c = CMD_TOGGLE_UX  },
	{ .ks = { 'g', 'r', 0, 0 }, .d = "toggle group read", .m = MODE_CHMOD, .c = CMD_TOGGLE_GR  },
	{ .ks = { 'g', 'w', 0, 0 }, .d = "toggle group write", .m = MODE_CHMOD, .c = CMD_TOGGLE_GW  },
	{ .ks = { 'g', 'x', 0, 0 }, .d = "toggle group execute", .m = MODE_CHMOD, .c = CMD_TOGGLE_GX  },
	{ .ks = { 'o', 'r', 0, 0 }, .d = "toggle other read", .m = MODE_CHMOD, .c = CMD_TOGGLE_OR  },
	{ .ks = { 'o', 'w', 0, 0 }, .d = "toggle other write", .m = MODE_CHMOD, .c = CMD_TOGGLE_OW  },
	{ .ks = { 'o', 'x', 0, 0 }, .d = "toggle other execute", .m = MODE_CHMOD, .c = CMD_TOGGLE_OX  },

	{ .ks = { 0, 0, 0, 0 }, .d = NULL, .m = 0, .c = 0 } // Null terminator
	/* TODO if it's global static and const it's size is known at compile time;
	 * get rid of that null terminator
	 * OR
	 * TODO move to some local scope; const can't be altered, and I do want to
	 * alter this thing with a config or something
	 * Where should it be? UI? Probably. But how initialized?
	 */
};

static const char type_symbol_mapping[][2] = {
	// See mode2type in ui.c
	[0] = { '+', 11 }, // BLK
	[1] = { '-', 11 }, // CHR
	[2] = { '|', 1 }, // FIFO
	[3] = { ' ', 1 }, // REG
	[4] = { '/', 9 }, // DIR
	[5] = { '~', 3 }, // LNK
	[6] = { '=', 5 }, // SOCK
	[7] = { '?', 1 }, // default
};

struct ui_find {
	utf8* t; // Pointer to buffer where searched name will be held
	char* t_top; // Used by fill_textbox. Does not have to == t
	size_t t_size; // Size of buffer; for fill_textbox
	fnum_t sbfc; // Selection Before Find Command
	/* If find was escaped, it returns to sbfc.
	 * If entered, stays on found entry.
	 */
	enum mode mb; // Mode Before find mode
};

struct ui_chmod {
	PANEL* p; // TODO dont want it here
	mode_t m; // permissions of chmodded file
	utf8* path; // path of chmodded file
	utf8* tmp; // Used to hold buffer used by prompt
	uid_t o; // owner uid
	gid_t g; // group gid
	/* These are only to limit syscalls.
	 * Their existence is checked after prompt.
	 * If correct, updated
	 * If incorrect, stay as they are
	 */
	utf8 owner[LOGIN_NAME_MAX];
	utf8 group[LOGIN_NAME_MAX];
	enum mode mb; // Mode Before find mode
	int wh, ww; // Window Width, Window Height // TODO dont want it here
};

struct ui_prompt {
	utf8* tb; // TextBox buffer
	utf8* tb_top;
	/* ^ Used to prompt with non-empty buffer;
	 * this is where cursor will be
	 */
	size_t tb_size; // size of buffer pointed by tb
	enum mode mb; // Mode Before find mode
};

struct ui {
	int scrh, scrw; // Need this for detecting changes in window size
	enum mode m;

	int active_view;
	PANEL* fvp[2];
	struct file_view* fvs[2];

	PANEL* status;

	struct ui_prompt* prompt;
	struct ui_chmod* chmod;
	struct ui_find* find;
	utf8* error;
	utf8* info;

	int kml; // Key Mapping Length
	int* mks; // Matching Key Sequence
};

struct ui ui_init(struct file_view*, struct file_view*);
void ui_end(struct ui* const);
void ui_draw(struct ui* const);
void ui_update_geometry(struct ui* const);

int chmod_open(struct ui*, utf8*, mode_t);
void chmod_close(struct ui*);
void prompt_open(struct ui*, utf8*, utf8*, size_t);
void prompt_close(struct ui*);
void find_open(struct ui*, utf8*, utf8*, size_t);
void find_close(struct ui*, bool);

enum command get_cmd(struct ui*);
int fill_textbox(utf8*, utf8**, size_t, int, WINDOW*);

#endif
