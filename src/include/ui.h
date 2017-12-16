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

#ifndef _DEFAULT_SOURCE
	#define _DEFAULT_SOURCE
#endif

#include <ncurses.h> // TODO
#include <panel.h>
#include <linux/limits.h>
#include <locale.h>
#include <time.h>

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
	[THEME_INFO] = { 0, COLOR_WHITE, COLOR_BLACK },
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

union input_data {
	char utf[5];
	int c; // ctrl code or special key
};

struct input {
	enum input_type t : 8;
	union input_data d;
};

struct input get_input(WINDOW* const);

#define INPUT_LIST_LENGTH 4

struct input2cmd {
	struct input i[INPUT_LIST_LENGTH];
	enum mode m : 8;
	enum command c : 8;
};

#define ENDK { .t = END }
#define UTF8(K) { .t = UTF8, .d.utf = K }
#define SPEC(K) { .t = SPECIAL, .d.c = (int)K }
#define CTRL(K) { .t = CTRL, .d.c = (int)K }

static struct input2cmd default_mapping[] = {
	/* MODE MANGER */
	{ { UTF8("q"), UTF8("q"), ENDK }, MODE_MANAGER, CMD_QUIT },

	{ { UTF8("g"), UTF8("g"), ENDK }, MODE_MANAGER, CMD_ENTRY_FIRST },
	{ { SPEC(KEY_HOME), ENDK }, MODE_MANAGER, CMD_ENTRY_FIRST },

	{ { UTF8("G"), ENDK }, MODE_MANAGER, CMD_ENTRY_LAST },
	{ { SPEC(KEY_END), ENDK }, MODE_MANAGER, CMD_ENTRY_LAST },

	{ { UTF8("j"), ENDK }, MODE_MANAGER, CMD_ENTRY_DOWN },
	{ { CTRL('N'), ENDK }, MODE_MANAGER, CMD_ENTRY_DOWN },
	{ { SPEC(KEY_DOWN), ENDK }, MODE_MANAGER, CMD_ENTRY_DOWN },

	{ { UTF8("k"), ENDK }, MODE_MANAGER, CMD_ENTRY_UP },
	{ { CTRL('P'), ENDK }, MODE_MANAGER, CMD_ENTRY_UP },
	{ { SPEC(KEY_UP), ENDK }, MODE_MANAGER, CMD_ENTRY_UP },

	{ { UTF8("c"), UTF8("p"), ENDK }, MODE_MANAGER, CMD_COPY },

	{ { UTF8("r"), UTF8("e"), UTF8("m"), ENDK }, MODE_MANAGER, CMD_REMOVE },

	{ { UTF8("r"), UTF8("n"), ENDK }, MODE_MANAGER, CMD_RENAME },

	{ { UTF8("m"), UTF8("v"), ENDK }, MODE_MANAGER, CMD_MOVE },

	{ { CTRL('I'), ENDK }, MODE_MANAGER, CMD_SWITCH_PANEL },

	{ { UTF8("r"), UTF8("r"), ENDK }, MODE_MANAGER, CMD_REFRESH },
	{ { CTRL('L'), ENDK }, MODE_MANAGER, CMD_REFRESH },

	{ { UTF8("m"), UTF8("k"), ENDK }, MODE_MANAGER, CMD_CREATE_DIR },

	{ { UTF8("u"), ENDK }, MODE_MANAGER, CMD_UP_DIR },
	{ { SPEC(KEY_BACKSPACE), ENDK }, MODE_MANAGER, CMD_UP_DIR },

	{ { UTF8("i"), ENDK }, MODE_MANAGER, CMD_ENTER_DIR },
	{ { CTRL('J'), ENDK }, MODE_MANAGER, CMD_ENTER_DIR },

	{ { UTF8("o"), ENDK }, MODE_MANAGER, CMD_OPEN_FILE },
	{ { UTF8("e"), UTF8("d"), ENDK }, MODE_MANAGER, CMD_EDIT_FILE },

	{ { UTF8("/"), ENDK }, MODE_MANAGER, CMD_FIND },

	{ { UTF8("x"), ENDK }, MODE_MANAGER, CMD_TOGGLE_HIDDEN },
	{ { CTRL('H'), ENDK }, MODE_MANAGER, CMD_TOGGLE_HIDDEN },

	{ { UTF8("?"), ENDK }, MODE_MANAGER, CMD_HELP },

	{ { UTF8("c"), UTF8("d"), ENDK }, MODE_MANAGER, CMD_CD },

	{ { UTF8("c"), UTF8("h"), ENDK }, MODE_MANAGER, CMD_CHMOD },

	{ { UTF8("s"), UTF8("n"), UTF8("a"), ENDK }, MODE_MANAGER, CMD_SORT_BY_NAME_ASC },
	{ { UTF8("s"), UTF8("n"), UTF8("d"), ENDK }, MODE_MANAGER, CMD_SORT_BY_NAME_DESC },
	{ { UTF8("s"), UTF8("d"), UTF8("a"), ENDK }, MODE_MANAGER, CMD_SORT_BY_DATE_ASC },
	{ { UTF8("s"), UTF8("d"), UTF8("d"), ENDK }, MODE_MANAGER, CMD_SORT_BY_DATE_DESC },
	{ { UTF8("s"), UTF8("s"), UTF8("a"), ENDK }, MODE_MANAGER, CMD_SORT_BY_SIZE_ASC },
	{ { UTF8("s"), UTF8("s"), UTF8("d"), ENDK }, MODE_MANAGER, CMD_SORT_BY_SIZE_DESC },

	/* MODE CHMOD */
	{ { UTF8("q"), UTF8("q"), ENDK }, MODE_CHMOD, CMD_RETURN },
	{ { UTF8("c"), UTF8("h"), ENDK }, MODE_CHMOD, CMD_CHANGE },
	{ { UTF8("c"), UTF8("o"), ENDK }, MODE_CHMOD, CMD_CHOWN },
	{ { UTF8("c"), UTF8("g"), ENDK }, MODE_CHMOD, CMD_CHGRP },
	{ { UTF8("u"), UTF8("i"), ENDK }, MODE_CHMOD, CMD_TOGGLE_UIOX },
	{ { UTF8("g"), UTF8("i"), ENDK }, MODE_CHMOD, CMD_TOGGLE_GIOX },
	{ { UTF8("o"), UTF8("s"), ENDK }, MODE_CHMOD, CMD_TOGGLE_SB },
	{ { UTF8("u"), UTF8("r"), ENDK }, MODE_CHMOD, CMD_TOGGLE_UR },
	{ { UTF8("u"), UTF8("w"), ENDK }, MODE_CHMOD, CMD_TOGGLE_UW },
	{ { UTF8("u"), UTF8("x"), ENDK }, MODE_CHMOD, CMD_TOGGLE_UX },
	{ { UTF8("g"), UTF8("r"), ENDK }, MODE_CHMOD, CMD_TOGGLE_GR },
	{ { UTF8("g"), UTF8("w"), ENDK }, MODE_CHMOD, CMD_TOGGLE_GW },
	{ { UTF8("g"), UTF8("x"), ENDK }, MODE_CHMOD, CMD_TOGGLE_GX },
	{ { UTF8("o"), UTF8("r"), ENDK }, MODE_CHMOD, CMD_TOGGLE_OR },
	{ { UTF8("o"), UTF8("w"), ENDK }, MODE_CHMOD, CMD_TOGGLE_OW },
	{ { UTF8("o"), UTF8("x"), ENDK }, MODE_CHMOD, CMD_TOGGLE_OX },

	/* MODE WAIT */
	{ { UTF8("q"), UTF8("q"), ENDK }, MODE_WAIT, CMD_TASK_QUIT },
	{ { UTF8("p"), UTF8("p"), ENDK }, MODE_WAIT, CMD_TASK_PAUSE },
	{ { UTF8("r"), UTF8("r"), ENDK }, MODE_WAIT, CMD_TASK_RESUME },


	/* MODE HELP */
	{ { UTF8("q"), ENDK }, MODE_HELP, CMD_HELP_QUIT },

	{ { UTF8("j"), ENDK }, MODE_HELP, CMD_HELP_DOWN },
	{ { CTRL('N'), ENDK }, MODE_HELP, CMD_HELP_DOWN },
	{ { SPEC(KEY_DOWN), ENDK }, MODE_HELP, CMD_HELP_DOWN },

	{ { UTF8("k"), ENDK }, MODE_HELP, CMD_HELP_UP },
	{ { CTRL('P'), ENDK }, MODE_HELP, CMD_HELP_UP },
	{ { SPEC(KEY_UP), ENDK }, MODE_HELP, CMD_HELP_UP },
};

static const size_t default_mapping_length = (sizeof(default_mapping)/sizeof(struct input2cmd));

static const char* const cmd_help[] = {
	[CMD_QUIT] = "Quit hund.",
	[CMD_HELP] = "Display help screen.",
	[CMD_COPY] = "Copy selected file to the other directory.",
	[CMD_MOVE] = "Move selected file to the other directory.",
	[CMD_REMOVE] = "Remove selected file.",
	[CMD_SWITCH_PANEL] = "Switch active panel.",
	[CMD_UP_DIR] = "Move up in directory tree.",
	[CMD_ENTER_DIR] = "Enter selected directory.",
	[CMD_REFRESH] = "Rescan directories and redraw window.",
	[CMD_ENTRY_UP] = "Select previous entry.",
	[CMD_ENTRY_DOWN] = "Select next entry.",
	[CMD_CREATE_DIR] = "Create new directory. Prompts for name.",
	[CMD_ENTRY_FIRST] = "Select top file in directory.",
	[CMD_ENTRY_LAST] = "Select bottom file in directory.",
	[CMD_RENAME] = "Rename selected file. Prompts for new name.",
	[CMD_TOGGLE_HIDDEN] = "Switch between hiding/showing hidden files.",
	[CMD_CD] = "Jump to some directory. Prompts for path.",
	[CMD_OPEN_FILE] = "Open selected file in less.",
	[CMD_EDIT_FILE] = "Open selected file in vi.",
	[CMD_FIND] = "Search for files in current directory.",

	[CMD_CHMOD] = "Change permissions of selected file.",
	[CMD_RETURN] = "Abort changes and return.",
	[CMD_CHOWN] = "Change owner of file. Prompts for login.",
	[CMD_CHGRP] = "Change group of file. Prompts for group name.",
	[CMD_SORT_BY_NAME_ASC] = "Sort by name ascending.",
	[CMD_SORT_BY_NAME_DESC] = "Sort by name descending.",
	[CMD_SORT_BY_DATE_ASC] = "Sort by date ascending.",
	[CMD_SORT_BY_DATE_DESC] = "Sort by date descending.",
	[CMD_SORT_BY_SIZE_ASC] = "Sort by size ascending.",
	[CMD_SORT_BY_SIZE_DESC] = "Sort by size descending.",

	[CMD_CHANGE] = "Apply changes and return.",
	[CMD_TOGGLE_UIOX] = "Toggle set user ID on execution.",
	[CMD_TOGGLE_GIOX] = "Toggle set group ID on execution.",
	[CMD_TOGGLE_SB] = "Toggle sticky bit.",
	[CMD_TOGGLE_UR] = "Toggle user read.",
	[CMD_TOGGLE_UW] = "Toggle user write.",
	[CMD_TOGGLE_UX] = "Toggle user execute.",
	[CMD_TOGGLE_GR] = "Toggle group read.",
	[CMD_TOGGLE_GW] = "Toggle group write.",
	[CMD_TOGGLE_GX] = "Toggle group execute.",
	[CMD_TOGGLE_OR] = "Toggle other read.",
	[CMD_TOGGLE_OW] = "Toggle other write.",
	[CMD_TOGGLE_OX] = "Toggle other execute.",

	[CMD_TASK_QUIT] = "Abort task.",
	[CMD_TASK_PAUSE] = "Pause task.",
	[CMD_TASK_RESUME] = "Resume task.",

	[CMD_HELP_UP] = "Scroll up.",
	[CMD_HELP_DOWN] = "Scroll down.",
	[CMD_HELP_QUIT] = "Quit help screen.",
};

static const char* const mode_strings[] = {
	[MODE_HELP] = "HELP",
	[MODE_CHMOD] = "CHMOD",
	[MODE_MANAGER] = "FILE VIEW",
	[MODE_WAIT] = "WAIT",
};

static const char* const copyright_notice[] = {
	"Hund  Copyright (C) 2017  Michał Czarnecki",
	"Hund comes with ABSOLUTELY NO WARRANTY.",
	"This is free software, and you are welcome to",
	"redistribute it under terms of GNU General Public License.",
	NULL,
};

enum msg_type {
	MSG_NONE = 0,
	MSG_INFO,
	//MSG_WARNING,
	MSG_ERROR,
};

struct ui {
	int scrh, scrw; // Last window dimensions

	bool ui_needs_refresh;
	bool run;

	enum mode m;
	enum msg_type mt;

	utf8 msg[MSG_BUFFER_SIZE];

	char prch;
	utf8* prompt;

	PANEL* fvp[2];
	struct file_view* fvs[2];
	struct file_view* pv;
	struct file_view* sv;

	PANEL* status;
	size_t helpy;

	struct input2cmd* kmap;
	size_t kml; // Key Mapping Length
	int* mks; // Matching Key Sequence // TODO char would be enough

	struct input il[INPUT_LIST_LENGTH];
	int ili;

	utf8* path; // path of chmodded file
	mode_t perm; // permissions of chmodded file
	uid_t o;
	gid_t g;
	/* These are only to limit syscalls.
	 * Their existence is checked after prompt.
	 * If correct, updated
	 * If incorrect, stay as they are
	 */
	utf8 owner[LOGIN_NAME_MAX];
	utf8 group[LOGIN_NAME_MAX];
};

struct ui ui_init(struct file_view* const, struct file_view* const);
void ui_end(struct ui* const);
void ui_draw(struct ui* const);
void ui_update_geometry(struct ui* const);

int chmod_open(struct ui* const, utf8* const);
void chmod_close(struct ui* const);

struct input get_input(WINDOW* const);
enum command get_cmd(struct ui* const);
int fill_textbox(utf8* const, utf8** const, const size_t, WINDOW* const);

#endif
