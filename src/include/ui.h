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

// vim: syntax=c

#ifndef UI_H
#define UI_H

#ifndef _DEFAULT_SOURCE
	#define _DEFAULT_SOURCE
#endif

#include <ncurses.h> // TODO
#include <panel.h>
#include <linux/limits.h>
#include <locale.h>
#include <time.h>
#include <syslog.h>

#include "file_view.h"
#include "utf8.h"

#define MSG_BUFFER_SIZE 256

// TODO maybe confirmation of removal,move,copy...
enum mode {
	MODE_HELP = 0,
	MODE_MANAGER,
	MODE_CHMOD,
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
	CMD_CD,
	CMD_OPEN_FILE,
	CMD_EDIT_FILE,
	CMD_HELP,
	CMD_FIND,
	CMD_CHMOD,

	CMD_SORT_BY_NAME_ASC,
	CMD_SORT_BY_NAME_DESC,
	CMD_SORT_BY_DATE_ASC,
	CMD_SORT_BY_DATE_DESC,
	CMD_SORT_BY_SIZE_ASC,
	CMD_SORT_BY_SIZE_DESC,

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

	CMD_TASK_QUIT,
	CMD_TASK_PAUSE,
	CMD_TASK_RESUME,

	CMD_HELP_QUIT,
	CMD_HELP_UP,
	CMD_HELP_DOWN,

	CMD_NUM,
};

enum theme_element {
	THEME_OTHER = 0,
	THEME_PATHBAR,
	THEME_STATUSBAR,
	THEME_ERROR,
	THEME_INFO,
	THEME_HINT_KEY,
	THEME_HINT_DESC,
	THEME_ENTRY_BLK_UNS, // unselected
	THEME_ENTRY_BLK_SEL, // selected
	THEME_ENTRY_CHR_UNS,
	THEME_ENTRY_CHR_SEL,
	THEME_ENTRY_FIFO_UNS,
	THEME_ENTRY_FIFO_SEL,
	THEME_ENTRY_REG_UNS,
	THEME_ENTRY_REG_SEL,
	THEME_ENTRY_REG_EXE_UNS,
	THEME_ENTRY_REG_EXE_SEL,
	THEME_ENTRY_DIR_UNS,
	THEME_ENTRY_DIR_SEL,
	THEME_ENTRY_SOCK_UNS,
	THEME_ENTRY_SOCK_SEL,
	THEME_ENTRY_LNK_DIR_UNS,
	THEME_ENTRY_LNK_DIR_SEL,
	THEME_ENTRY_LNK_OTH_UNS,
	THEME_ENTRY_LNK_OTH_SEL,
	THEME_ENTRY_LNK_PATH,
	THEME_ENTRY_LNK_PATH_INV,
	THEME_ELEM_NUM
};

static const int theme_scheme[THEME_ELEM_NUM][3] = {
	[THEME_OTHER] = { 0, COLOR_WHITE, COLOR_BLACK },
	[THEME_PATHBAR] = { 0, COLOR_BLACK, COLOR_WHITE },
	[THEME_STATUSBAR] = { 0, COLOR_BLACK, COLOR_WHITE },
	[THEME_ERROR] = { 0, COLOR_BLACK, COLOR_RED },
	[THEME_INFO] = { 0, COLOR_BLACK, COLOR_CYAN },
	[THEME_HINT_KEY] = { 0, COLOR_WHITE, COLOR_BLACK },
	[THEME_HINT_DESC] = { 0, COLOR_BLACK, COLOR_WHITE },
	[THEME_ENTRY_BLK_UNS] = { '+', COLOR_RED, COLOR_BLACK },
	[THEME_ENTRY_BLK_SEL] = { '+', COLOR_BLACK, COLOR_RED },
	[THEME_ENTRY_CHR_UNS] = { '-', COLOR_YELLOW, COLOR_BLACK },
	[THEME_ENTRY_CHR_SEL] = { '-', COLOR_BLACK, COLOR_YELLOW },
	[THEME_ENTRY_FIFO_UNS] = { '|', COLOR_GREEN, COLOR_BLACK },
	[THEME_ENTRY_FIFO_SEL] = { '|', COLOR_BLACK, COLOR_GREEN },
	[THEME_ENTRY_REG_UNS] = { ' ', COLOR_WHITE, COLOR_BLACK },
	[THEME_ENTRY_REG_SEL] = { ' ', COLOR_BLACK, COLOR_WHITE },
	[THEME_ENTRY_REG_EXE_UNS] = { '*', COLOR_MAGENTA, COLOR_BLACK },
	[THEME_ENTRY_REG_EXE_SEL] = { '*', COLOR_BLACK, COLOR_MAGENTA },
	[THEME_ENTRY_DIR_UNS] = { '/', COLOR_CYAN, COLOR_BLACK },
	[THEME_ENTRY_DIR_SEL] = { '/', COLOR_BLACK, COLOR_CYAN },
	[THEME_ENTRY_SOCK_UNS] = { '=', COLOR_GREEN, COLOR_BLACK },
	[THEME_ENTRY_SOCK_SEL] = { '=', COLOR_BLACK, COLOR_GREEN },
	[THEME_ENTRY_LNK_DIR_UNS] = { '~', COLOR_CYAN, COLOR_BLACK },
	[THEME_ENTRY_LNK_DIR_SEL] = { '~', COLOR_BLACK, COLOR_CYAN },
	[THEME_ENTRY_LNK_OTH_UNS] = { '@', COLOR_WHITE, COLOR_BLACK },
	[THEME_ENTRY_LNK_OTH_SEL] = { '@', COLOR_BLACK, COLOR_WHITE },
	[THEME_ENTRY_LNK_PATH] = { 0, COLOR_WHITE, COLOR_BLACK },
	[THEME_ENTRY_LNK_PATH_INV] = { 0, COLOR_RED, COLOR_BLACK },
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
	{ .i={ SPEC(KEY_DOWN), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTRY_DOWN },

	{ .i={ UTF8("k"), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTRY_UP },
	{ .i={ CTRL('P'), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTRY_UP },
	{ .i={ SPEC(KEY_UP), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTRY_UP },

	{ .i={ UTF8("c"), UTF8("p"), ENDK }, .m=MODE_MANAGER, .c=CMD_COPY },

	{ .i={ UTF8("r"), UTF8("e"), UTF8("m"), ENDK }, .m=MODE_MANAGER, .c=CMD_REMOVE },

	{ .i={ UTF8("r"), UTF8("n"), ENDK }, .m=MODE_MANAGER, .c=CMD_RENAME },

	{ .i={ UTF8("m"), UTF8("v"), ENDK }, .m=MODE_MANAGER, .c=CMD_MOVE },

	{ .i={ CTRL('I'), ENDK }, .m=MODE_MANAGER, .c=CMD_SWITCH_PANEL },

	{ .i={ UTF8("r"), UTF8("r"), ENDK }, .m=MODE_MANAGER, .c=CMD_REFRESH },
	{ .i={ CTRL('L'), ENDK }, .m=MODE_MANAGER, .c=CMD_REFRESH },

	{ .i={ UTF8("m"), UTF8("k"), ENDK }, .m=MODE_MANAGER, .c=CMD_CREATE_DIR },

	{ .i={ UTF8("u"), ENDK }, .m=MODE_MANAGER, .c=CMD_UP_DIR },
	{ .i={ SPEC(KEY_BACKSPACE), ENDK }, .m=MODE_MANAGER, .c=CMD_UP_DIR },

	{ .i={ UTF8("i"), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTER_DIR },
	{ .i={ CTRL('J'), ENDK }, .m=MODE_MANAGER, .c=CMD_ENTER_DIR },

	{ .i={ UTF8("o"), ENDK }, .m=MODE_MANAGER, .c=CMD_OPEN_FILE },
	{ .i={ UTF8("e"), UTF8("d"), ENDK }, .m=MODE_MANAGER, .c=CMD_EDIT_FILE },

	{ .i={ UTF8("/"), ENDK }, .m=MODE_MANAGER, .c=CMD_FIND },

	{ .i={ UTF8("x"), ENDK }, .m=MODE_MANAGER, .c=CMD_TOGGLE_HIDDEN },
	{ .i={ CTRL('H'), ENDK }, .m=MODE_MANAGER, .c=CMD_TOGGLE_HIDDEN },

	{ .i={ UTF8("?"), ENDK }, .m=MODE_MANAGER, .c=CMD_HELP },

	{ .i={ UTF8("c"), UTF8("d"), ENDK }, .m=MODE_MANAGER, .c=CMD_CD },

	{ .i={ UTF8("c"), UTF8("h"), ENDK }, .m=MODE_MANAGER, .c=CMD_CHMOD },

	{ .i={ UTF8("s"), UTF8("n"), UTF8("a"), ENDK }, .m=MODE_MANAGER, .c=CMD_SORT_BY_NAME_ASC },
	{ .i={ UTF8("s"), UTF8("n"), UTF8("d"), ENDK }, .m=MODE_MANAGER, .c=CMD_SORT_BY_NAME_DESC },
	{ .i={ UTF8("s"), UTF8("d"), UTF8("a"), ENDK }, .m=MODE_MANAGER, .c=CMD_SORT_BY_DATE_ASC },
	{ .i={ UTF8("s"), UTF8("d"), UTF8("d"), ENDK }, .m=MODE_MANAGER, .c=CMD_SORT_BY_DATE_DESC },
	{ .i={ UTF8("s"), UTF8("s"), UTF8("a"), ENDK }, .m=MODE_MANAGER, .c=CMD_SORT_BY_SIZE_ASC },
	{ .i={ UTF8("s"), UTF8("s"), UTF8("d"), ENDK }, .m=MODE_MANAGER, .c=CMD_SORT_BY_SIZE_DESC },

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

	/* MODE WAIT */
	{ .i={ UTF8("q"), UTF8("q"), ENDK }, .m=MODE_WAIT, .c=CMD_TASK_QUIT },
	{ .i={ UTF8("p"), UTF8("p"), ENDK }, .m=MODE_WAIT, .c=CMD_TASK_PAUSE },
	{ .i={ UTF8("r"), UTF8("r"), ENDK }, .m=MODE_WAIT, .c=CMD_TASK_RESUME },


	/* MODE HELP */
	{ .i={ UTF8("q"), ENDK }, .m=MODE_HELP, .c=CMD_HELP_QUIT },

	{ .i={ UTF8("j"), ENDK }, .m=MODE_HELP, .c=CMD_HELP_DOWN },
	{ .i={ CTRL('N'), ENDK }, .m=MODE_HELP, .c=CMD_HELP_DOWN },
	{ .i={ SPEC(KEY_DOWN), ENDK }, .m=MODE_HELP, .c=CMD_HELP_DOWN },

	{ .i={ UTF8("k"), ENDK }, .m=MODE_HELP, .c=CMD_HELP_UP },
	{ .i={ CTRL('P'), ENDK }, .m=MODE_HELP, .c=CMD_HELP_UP },
	{ .i={ SPEC(KEY_UP), ENDK }, .m=MODE_HELP, .c=CMD_HELP_UP },
};

static const size_t default_mapping_length = (sizeof(default_mapping)/sizeof(struct input2cmd));

struct cmd2help {
	enum command c;
	utf8 *hint, *help;
};

static const struct cmd2help cmd_help[] = {
	{ .c = CMD_QUIT, .hint = "quit", .help = "Quit hund." },
	{ .c = CMD_HELP, .hint = "help", .help = "Display help screen." },
	{ .c = CMD_COPY, .hint = "copy", .help = "Copy selected file to the other directory. May prompt for name if colliding." },
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
	{ .c = CMD_CD, .hint = "change dir", .help = "Jump to some directory. Prompts for path." },
	{ .c = CMD_OPEN_FILE, .hint = "open", .help = "Open selected file in less." },
	{ .c = CMD_EDIT_FILE, .hint = "edit", .help = "Open selected file in vi." },
	{ .c = CMD_FIND, .hint = "find", .help = "Search for files in current directory. Case sensitive." },

	{ .c = CMD_CHMOD, .hint = "chmod", .help = "Change permissions of selected file." },
	{ .c = CMD_RETURN, .hint = "return", .help = "Abort changes and return." },
	{ .c = CMD_CHOWN, .hint = "change owner", .help = "Change owner of file. Prompts for login." },
	{ .c = CMD_CHGRP, .hint = "change group", .help = "Change group of file. Prompts for group name." },
	{ .c = CMD_SORT_BY_NAME_ASC, .hint = "name asc", .help = "Sort by name ascending." },
	{ .c = CMD_SORT_BY_NAME_DESC, .hint = "name desc", .help = "Sort by name descending." },
	{ .c = CMD_SORT_BY_DATE_ASC, .hint = "date asc", .help = "Sort by date ascending." },
	{ .c = CMD_SORT_BY_DATE_DESC, .hint = "date desc", .help = "Sort by date descending." },
	{ .c = CMD_SORT_BY_SIZE_ASC, .hint = "size asc", .help = "Sort by size ascending." },
	{ .c = CMD_SORT_BY_SIZE_DESC, .hint = "size desc", .help = "Sort by size descending." },

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

	{ .c = CMD_TASK_QUIT, .hint = "abort", .help = "Abort task." },
	{ .c = CMD_TASK_PAUSE, .hint = "pause", .help = "Pause task." },
	{ .c = CMD_TASK_RESUME, .hint = "resume", .help = "Resume task." },

	{ .c = CMD_HELP_UP, .hint = "up", .help = "Scroll up." },
	{ .c = CMD_HELP_DOWN, .hint = "down", .help = "Scroll down." },
	{ .c = CMD_HELP_QUIT, .hint = "quit", .help = "Quit help screen." },
};

static const size_t cmd_help_length = sizeof(cmd_help)/sizeof(struct cmd2help);

struct ui_chmod {
	PANEL* p;
	mode_t m; // permissions of chmodded file
	utf8* path; // path of chmodded file
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
	int wh, ww; // Window Width, Window Height
};

struct ui {
	bool ui_needs_refresh;
	int scrh, scrw; // Last window dimensions
	bool run;
	enum mode m;

	char prch;
	utf8* prompt;

	PANEL* fvp[2];
	struct file_view* fvs[2]; // TODO FIXME
	struct file_view* pv;
	struct file_view* sv;

	PANEL* status;
	size_t helpy;
	PANEL* help;

	struct ui_chmod* chmod;
	utf8 error[MSG_BUFFER_SIZE];
	utf8 info[MSG_BUFFER_SIZE];

	struct input2cmd* kmap;
	size_t kml; // Key Mapping Length
	int* mks; // Matching Key Sequence
};

struct ui ui_init(struct file_view* const, struct file_view* const);
void ui_end(struct ui* const);
void ui_draw(struct ui* const);
void ui_update_geometry(struct ui* const);

int chmod_open(struct ui*, utf8*);
void chmod_close(struct ui*);

void help_open(struct ui*);
void help_close(struct ui*);

struct input get_input(WINDOW* const);
enum command get_cmd(struct ui* const);
int fill_textbox(utf8* const, utf8** const, const size_t, WINDOW* const);

#endif
