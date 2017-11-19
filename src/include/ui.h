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

#define MSG_BUFFER_SIZE 256

enum mode {
	MODE_HELP = 0,
	MODE_MANAGER,
	MODE_CHMOD,
	MODE_PROMPT,
	MODE_FIND,
	MODE_WAIT,
	MODE_NUM
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
	CMD_TOGGLE_LINK_TRANSPARENCY,
	CMD_CD,
	CMD_OPEN_FILE,
	CMD_HELP,
	CMD_FIND,
	CMD_CHMOD,

	CMD_RETURN,
	CMD_CHANGE,
	CMD_CHOWN,
	CMD_CHGRP,
	CMD_TOGGLE_UR,
	CMD_TOGGLE_UW,
	CMD_TOGGLE_UX,
	CMD_TOGGLE_UIOX,
	CMD_TOGGLE_GR,
	CMD_TOGGLE_GW,
	CMD_TOGGLE_GX,
	CMD_TOGGLE_GIOX,
	CMD_TOGGLE_OR,
	CMD_TOGGLE_OW,
	CMD_TOGGLE_OX,
	CMD_TOGGLE_SB,

	CMD_HELP_QUIT,
	CMD_HELP_UP,
	CMD_HELP_DOWN,

	CMD_NUM,
};

enum input_type {
	END = 0,
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

struct input get_input(WINDOW* const);

#define INPUT_LIST_LENGTH 4

struct input2cmd {
	struct input i[INPUT_LIST_LENGTH];
	enum mode m;
	enum command c;
};

#define ENDK { .t = END }
#define UTF8(K) { .t = UTF8, .utf = K }
#define SPEC(K) { .t = SPECIAL, .c = K }
#define CTRL(K) { .t = CTRL, .ctrl = K }

static const struct input2cmd default_mapping[] = {
	/* MODE MANGER */
	{ .i={ UTF8("q"), UTF8("q"), ENDK }, .m=MODE_MANAGER, .c=CMD_QUIT },

	{ .i={ UTF8("g"), UTF8("g"), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTRY_FIRST },
	{ .i={ SPEC(KEY_HOME), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTRY_FIRST },

	{ .i={ UTF8("G"), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTRY_LAST },
	{ .i={ SPEC(KEY_END), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTRY_LAST },

	{ .i={ UTF8("j"), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTRY_DOWN },
	{ .i={ CTRL('N'), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTRY_DOWN },

	{ .i={ UTF8("k"), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTRY_UP },
	{ .i={ CTRL('P'), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTRY_UP },

	{ .i={ UTF8("c"), UTF8("p"), ENDK }, .m=MODE_MANAGER, .c=CMD_COPY },

	{ .i={ UTF8("r"), UTF8("m"), ENDK }, .m=MODE_MANAGER, .c=CMD_REMOVE },
	{ .i={ CTRL('K'), ENDK }, .m=MODE_MANAGER, .c=CMD_REMOVE },

	{ .i={ UTF8("r"), UTF8("n"), ENDK }, .m=MODE_MANAGER, .c=CMD_RENAME },

	{ .i={ UTF8("m"), UTF8("v"), ENDK }, .m=MODE_MANAGER, .c=CMD_MOVE },

	{ .i={ CTRL('I'), ENDK }, .m=MODE_MANAGER, .c=CMD_SWITCH_PANEL },

	{ .i={ UTF8("r"), UTF8("r"), ENDK }, .m=MODE_MANAGER, .c=CMD_REFRESH },
	{ .i={ CTRL('L'), ENDK }, .m=MODE_MANAGER, .c=CMD_REFRESH },

	{ .i={ UTF8("m"), UTF8("k"), ENDK }, .m=MODE_MANAGER, .c=CMD_CREATE_DIR },

	{ .i={ UTF8("u"), ENDK }, .m=MODE_MANAGER, .c=CMD_UP_DIR },
	{ .i={ UTF8("d"), ENDK }, .m=MODE_MANAGER, .c=CMD_UP_DIR },

	{ .i={ UTF8("i"), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTER_DIR },
	{ .i={ UTF8("e"), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTER_DIR },

	{ .i={ UTF8("o"), ENDK }, .m=MODE_MANAGER, .c=CMD_OPEN_FILE },

	{ .i={ UTF8("/"), ENDK }, .m=MODE_MANAGER, .c=CMD_FIND },

	{ .i={ UTF8("h"), ENDK }, .m=MODE_MANAGER, .c=CMD_TOGGLE_HIDDEN },

	{ .i={ UTF8("t"), UTF8("l"), ENDK }, .m=MODE_MANAGER, .c=CMD_TOGGLE_LINK_TRANSPARENCY },
	{ .i={ UTF8("?"), ENDK }, .m=MODE_MANAGER, .c=CMD_HELP },

	{ .i={ UTF8("c"), UTF8("d"), ENDK }, .m=MODE_MANAGER, .c=CMD_CD },

	{ .i={ UTF8("c"), UTF8("h"), ENDK }, .m=MODE_MANAGER, .c=CMD_CHMOD },

	/* MODE CHMOD */
	{ .i={ UTF8("q"), UTF8("q"), ENDK }, .m=MODE_CHMOD, .c=CMD_RETURN },
	{ .i={ UTF8("c"), UTF8("h"), ENDK }, .m=MODE_CHMOD, .c=CMD_CHANGE },
	{ .i={ UTF8("c"), UTF8("o"), ENDK }, .m=MODE_CHMOD, .c=CMD_CHOWN },
	{ .i={ UTF8("c"), UTF8("g"), ENDK }, .m=MODE_CHMOD, .c=CMD_CHGRP },
	{ .i={ UTF8("u"), UTF8("i"), ENDK }, .m=MODE_CHMOD, .c=CMD_TOGGLE_UIOX },
	{ .i={ UTF8("g"), UTF8("i"), ENDK }, .m=MODE_CHMOD, .c=CMD_TOGGLE_GIOX },
	{ .i={ UTF8("o"), UTF8("s"), ENDK }, .m=MODE_CHMOD, .c=CMD_TOGGLE_SB },
	{ .i={ UTF8("u"), UTF8("r"), ENDK }, .m=MODE_CHMOD, .c=CMD_TOGGLE_UR },
	{ .i={ UTF8("u"), UTF8("w"), ENDK }, .m=MODE_CHMOD, .c=CMD_TOGGLE_UW },
	{ .i={ UTF8("u"), UTF8("x"), ENDK }, .m=MODE_CHMOD, .c=CMD_TOGGLE_UX },
	{ .i={ UTF8("g"), UTF8("r"), ENDK }, .m=MODE_CHMOD, .c=CMD_TOGGLE_GR },
	{ .i={ UTF8("g"), UTF8("w"), ENDK }, .m=MODE_CHMOD, .c=CMD_TOGGLE_GW },
	{ .i={ UTF8("g"), UTF8("x"), ENDK }, .m=MODE_CHMOD, .c=CMD_TOGGLE_GX },
	{ .i={ UTF8("o"), UTF8("r"), ENDK }, .m=MODE_CHMOD, .c=CMD_TOGGLE_OR },
	{ .i={ UTF8("o"), UTF8("w"), ENDK }, .m=MODE_CHMOD, .c=CMD_TOGGLE_OW },
	{ .i={ UTF8("o"), UTF8("x"), ENDK }, .m=MODE_CHMOD, .c=CMD_TOGGLE_OX },

	/* MODE HELP */
	{ .i={ UTF8("q"), ENDK }, .m=MODE_HELP, .c=CMD_HELP_QUIT },

	{ .i={ UTF8("j"), ENDK }, .m=MODE_HELP, .c=CMD_HELP_DOWN },
	{ .i={ CTRL('N'), ENDK }, .m=MODE_HELP, .c=CMD_HELP_DOWN },

	{ .i={ UTF8("k"), ENDK }, .m=MODE_HELP, .c=CMD_HELP_UP },
	{ .i={ CTRL('P'), ENDK }, .m=MODE_HELP, .c=CMD_HELP_UP },
};

static const size_t default_mapping_length = (sizeof(default_mapping)/sizeof(struct input2cmd));

struct cmd2help {
	enum command c;
	utf8 *hint, *help;
};

static const struct cmd2help cmd_help[] = {
	{ .c = CMD_QUIT, .hint = "quit", .help = "Quit hund." },
	{ .c = CMD_HELP, .hint = "help", .help = "Display help screen." },
	{ .c = CMD_COPY, .hint = "copy", .help = "Copy selected file to the other directory. Prompts for name for new file." },
	{ .c = CMD_MOVE, .hint = "move", .help = "Move selected file to the other directory." },
	{ .c = CMD_REMOVE, .hint = "remove", .help = "Remove selected file." },
	{ .c = CMD_SWITCH_PANEL, .hint = "switch", .help = "Switch active panel." },
	{ .c = CMD_UP_DIR, .hint = "up dir", .help = "Move up in directory tree." },
	{ .c = CMD_ENTER_DIR, .hint = "enter dir", .help = "Enter selected directory." },
	{ .c = CMD_REFRESH, .hint = "refresh", .help = "Rescan directories and redraw window." },
	{ .c = CMD_ENTRY_UP, .hint = "up", .help = "Select previous entry." },
	{ .c = CMD_ENTRY_DOWN, .hint = "down", .help = "Select next entry." },
	{ .c = CMD_CREATE_DIR, .hint = "create dir", .help = "Create new directory. Prompts for name." },
	{ .c = CMD_ENTRY_FIRST, .hint = "top", .help = "Select top file in directory." },
	{ .c = CMD_ENTRY_LAST, .hint = "bottom", .help = "Select bottom file in directory." },
	{ .c = CMD_RENAME, .hint = "rename", .help = "Rename selected file. Prompts for new name." },
	{ .c = CMD_TOGGLE_HIDDEN, .hint = "hide", .help = "Switch between hiding/showing hidden files." },
	{ .c = CMD_TOGGLE_LINK_TRANSPARENCY, .hint = "link transparency", .help = "Switch method of handling symlinks." },
	{ .c = CMD_CD, .hint = "change dir", .help = "Jump to some directory. Prompts for path." },
	{ .c = CMD_OPEN_FILE, .hint = "open", .help = "Open selected file in less." },
	{ .c = CMD_FIND, .hint = "find", .help = "Search for files in current directory. Case sensitive." },

	{ .c = CMD_CHMOD, .hint = "chmod", .help = "Change permissions of selected file." },
	{ .c = CMD_RETURN, .hint = "return", .help = "Abort changes and return." },
	{ .c = CMD_CHOWN, .hint = "change owner", .help = "Change owner of file. Prompts for login." },
	{ .c = CMD_CHGRP, .hint = "change group", .help = "Change group of file. Prompts for group name." },
	{ .c = CMD_CHANGE, .hint = "change ", .help = "Apply changes and return." },
	{ .c = CMD_TOGGLE_UIOX, .hint = "toggle setuid", .help = "Toggle set user ID on execution." },
	{ .c = CMD_TOGGLE_GIOX, .hint = "toggle setgid", .help = "Toggle set group ID on execution." },
	{ .c = CMD_TOGGLE_SB, .hint = "toggle sticky bit", .help = "Toggle sticky bit." },
	{ .c = CMD_TOGGLE_UR, .hint = "toggle user read", .help = "Toggle user read." },
	{ .c = CMD_TOGGLE_UW, .hint = "toggle user write", .help = "Toggle user write." },
	{ .c = CMD_TOGGLE_UX, .hint = "toggle user execute", .help = "Toggle user execute." },
	{ .c = CMD_TOGGLE_GR, .hint = "toggle group read", .help = "Toggle group read." },
	{ .c = CMD_TOGGLE_GW, .hint = "toggle group write", .help = "Toggle group write." },
	{ .c = CMD_TOGGLE_GX, .hint = "toggle group execute", .help = "Toggle group execute." },
	{ .c = CMD_TOGGLE_OR, .hint = "toggle other read", .help = "Toggle other read." },
	{ .c = CMD_TOGGLE_OW, .hint = "toggle other write", .help = "Toggle other write." },
	{ .c = CMD_TOGGLE_OX, .hint = "toggle other execute", .help = "Toggle other execute." },

	{ .c = CMD_HELP_UP, .hint = "up", .help = "Scroll up." },
	{ .c = CMD_HELP_DOWN, .hint = "down", .help = "Scroll down." },
	{ .c = CMD_HELP_QUIT, .hint = "quit", .help = "Quit help screen." },
};

static const size_t cmd_help_length = sizeof(cmd_help)/sizeof(struct cmd2help);

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
	bool run;
	enum mode m;

	PANEL* fvp[2];
	struct file_view* fvs[2];
	struct file_view* pv;
	struct file_view* sv;

	PANEL* status;
	size_t helpy;
	PANEL* help;

	struct ui_prompt* prompt;
	struct ui_chmod* chmod;
	struct ui_find* find;
	utf8 error[MSG_BUFFER_SIZE];
	utf8 info[MSG_BUFFER_SIZE];

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

void help_open(struct ui*);
void help_close(struct ui*);

enum command get_cmd(struct ui*);
int fill_textbox(utf8*, utf8**, size_t, int, WINDOW*);

#endif
