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
#define MSG_BUFFER_SIZE 256

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
	CMD_OPEN_FILE,
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

enum input_type {
	NONE = 0,
	UTF8,
	SPECIAL,
	CTRL,
};

struct input {
	enum input_type t;
	union {
		char utf[5];
		char ctrl;
		int c;
	};
};

struct input get_input(WINDOW*);

#define INPUT_LIST_LENGTH 8

struct input2cmd {
	struct input i[INPUT_LIST_LENGTH];
	enum mode m;
	enum command c;
	utf8* sd; // Short Description (to be displayed as hint)
	utf8* ld; // Long Description (to be displayed in help screen)
};

#define END { .t = NONE }
#define UTF8(K) { .t = UTF8, .utf = K }
#define SPEC(K) { .t = SPECIAL, .c = K }
#define CTRL(K) { .t = CTRL, .ctrl = K }

static const struct input2cmd default_mapping[] = {
	/* MODE MANGER */
	{ .i={ UTF8("q"), UTF8("q"), END }, .m=MODE_MANAGER, .c=CMD_QUIT, .sd="quit", .ld=""},
	{ .i={ UTF8("g"), UTF8("g"), END }, .m=MODE_MANAGER, .c=CMD_ENTRY_FIRST, .sd="top", .ld=""},
	{ .i={ UTF8("G"), END }, .m=MODE_MANAGER, .c=CMD_ENTRY_LAST, .sd="bottom", .ld=""},
	{ .i={ UTF8("j"), END }, .m=MODE_MANAGER, .c=CMD_ENTRY_DOWN, .sd="down", .ld=""},
	{ .i={ CTRL('N'), END }, .m=MODE_MANAGER, .c=CMD_ENTRY_DOWN, .sd="down", .ld=""},
	{ .i={ UTF8("k"), END }, .m=MODE_MANAGER, .c=CMD_ENTRY_UP, .sd="up", .ld=""},
	{ .i={ CTRL('P'), END }, .m=MODE_MANAGER, .c=CMD_ENTRY_UP, .sd="up", .ld=""},
	{ .i={ UTF8("c"), UTF8("p"), END }, .m=MODE_MANAGER, .c=CMD_COPY, .sd="copy", .ld=""},
	{ .i={ UTF8("r"), UTF8("m"), END }, .m=MODE_MANAGER, .c=CMD_REMOVE, .sd="remove", .ld=""},
	{ .i={ UTF8("r"), UTF8("n"), END }, .m=MODE_MANAGER, .c=CMD_RENAME, .sd="rename", .ld=""},
	{ .i={ UTF8("m"), UTF8("v"), END }, .m=MODE_MANAGER, .c=CMD_MOVE, .sd="move", .ld=""},
	{ .i={ CTRL('I'), END }, .m=MODE_MANAGER, .c=CMD_SWITCH_PANEL, .sd="switch panel", .ld=""},
	{ .i={ UTF8("r"), UTF8("r"), END }, .m=MODE_MANAGER, .c=CMD_REFRESH, .sd="refresh", .ld=""},
	{ .i={ UTF8("m"), UTF8("k"), END }, .m=MODE_MANAGER, .c=CMD_CREATE_DIR, .sd="create dir", .ld=""},
	{ .i={ UTF8("u"), END }, .m=MODE_MANAGER, .c=CMD_UP_DIR, .sd="up dir", .ld=""},
	{ .i={ UTF8("d"), END }, .m=MODE_MANAGER, .c=CMD_UP_DIR, .sd="up dir", .ld=""},
	{ .i={ UTF8("i"), END }, .m=MODE_MANAGER, .c=CMD_ENTER_DIR, .sd="enter dir", .ld=""},
	{ .i={ UTF8("e"), END }, .m=MODE_MANAGER, .c=CMD_ENTER_DIR, .sd="enter dir", .ld=""},
	{ .i={ UTF8("o"), END }, .m=MODE_MANAGER, .c=CMD_OPEN_FILE, .sd="open file", .ld=""},
	{ .i={ UTF8("/"), END }, .m=MODE_MANAGER, .c=CMD_FIND, .sd="find", .ld=""},
	{ .i={ UTF8("h"), END }, .m=MODE_MANAGER, .c=CMD_TOGGLE_HIDDEN, .sd="toggle hidden", .ld=""},
	{ .i={ UTF8("c"), UTF8("d"), END }, .m=MODE_MANAGER, .c=CMD_CD, .sd="open dir", .ld=""},
	{ .i={ UTF8("c"), UTF8("h"), END }, .m=MODE_MANAGER, .c=CMD_CHMOD, .sd="change permissions", .ld=""},

	/* MODE CHMOD */
	{ .i={ UTF8("q"), UTF8("q"), END }, .m=MODE_CHMOD, .c=CMD_RETURN, .sd="return", .ld=""},
	{ .i={ UTF8("c"), UTF8("h"), END }, .m=MODE_CHMOD, .c=CMD_CHANGE, .sd="change", .ld=""},
	{ .i={ UTF8("c"), UTF8("o"), END }, .m=MODE_CHMOD, .c=CMD_CHOWN, .sd="change owner", .ld=""},
	{ .i={ UTF8("c"), UTF8("g"), END }, .m=MODE_CHMOD, .c=CMD_CHGRP, .sd="change group", .ld=""},
	{ .i={ UTF8("u"), UTF8("i"), END }, .m=MODE_CHMOD, .c=CMD_TOGGLE_UIOX, .sd="toggle set user id on execution", .ld=""},
	{ .i={ UTF8("g"), UTF8("i"), END }, .m=MODE_CHMOD, .c=CMD_TOGGLE_GIOX, .sd="toggle set group id on execution", .ld=""},
	{ .i={ UTF8("o"), UTF8("s"), END }, .m=MODE_CHMOD, .c=CMD_TOGGLE_SB, .sd="toggle sticky bit", .ld=""},
	{ .i={ UTF8("u"), UTF8("r"), END }, .m=MODE_CHMOD, .c=CMD_TOGGLE_UR, .sd="toggle user read", .ld=""},
	{ .i={ UTF8("u"), UTF8("w"), END }, .m=MODE_CHMOD, .c=CMD_TOGGLE_UW, .sd="toggle user write", .ld=""},
	{ .i={ UTF8("u"), UTF8("x"), END }, .m=MODE_CHMOD, .c=CMD_TOGGLE_UX, .sd="toggle user execute", .ld=""},
	{ .i={ UTF8("g"), UTF8("r"), END }, .m=MODE_CHMOD, .c=CMD_TOGGLE_GR, .sd="toggle group read", .ld=""},
	{ .i={ UTF8("g"), UTF8("w"), END }, .m=MODE_CHMOD, .c=CMD_TOGGLE_GW, .sd="toggle group write", .ld=""},
	{ .i={ UTF8("g"), UTF8("x"), END }, .m=MODE_CHMOD, .c=CMD_TOGGLE_GX, .sd="toggle group execute", .ld=""},
	{ .i={ UTF8("o"), UTF8("r"), END }, .m=MODE_CHMOD, .c=CMD_TOGGLE_OR, .sd="toggle other read", .ld=""},
	{ .i={ UTF8("o"), UTF8("w"), END }, .m=MODE_CHMOD, .c=CMD_TOGGLE_OW, .sd="toggle other write", .ld=""},
	{ .i={ UTF8("o"), UTF8("x"), END }, .m=MODE_CHMOD, .c=CMD_TOGGLE_OX, .sd="toggle other execute", .ld=""},
};

static const size_t default_mapping_length = (sizeof(default_mapping)/sizeof(struct input2cmd));

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
	utf8* t_top; // Used by fill_textbox. Does not have to == t
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

	struct input2cmd* kmap;
	size_t kml; // Key Mapping Length
	int* mks; // Matching Key Sequence
};

struct ui ui_init(struct file_view*, struct file_view*);
void ui_system(const char* const);
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
