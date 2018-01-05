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
#include <limits.h>
#include <locale.h>
#include <time.h>
#include <sys/wait.h>
#include <signal.h>

#include "file_view.h"
#include "utf8.h"
#include "terminal.h"

extern char** environ;

#define MSG_BUFFER_SIZE 128

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
	CMD_HELP,

	CMD_COPY,
	CMD_MOVE,
	CMD_REMOVE,
	CMD_CREATE_DIR,
	CMD_RENAME,

	CMD_UP_DIR,
	CMD_ENTER_DIR,

	CMD_ENTRY_UP,
	CMD_ENTRY_DOWN,

	CMD_ENTRY_FIRST,
	CMD_ENTRY_LAST,

	CMD_CD,
	CMD_OPEN_FILE,
	CMD_EDIT_FILE,

	CMD_REFRESH,
	CMD_SWITCH_PANEL,

	CMD_DIR_VOLUME,
	CMD_TOGGLE_HIDDEN,

	CMD_SORT_BY_NAME_ASC,
	CMD_SORT_BY_NAME_DESC,
	CMD_SORT_BY_DATE_ASC,
	CMD_SORT_BY_DATE_DESC,
	CMD_SORT_BY_SIZE_ASC,
	CMD_SORT_BY_SIZE_DESC,

	CMD_SELECT_FILE,
	CMD_SELECT_ALL,
	CMD_SELECT_NONE,

	CMD_FIND,

	CMD_CHMOD,
	CMD_CHANGE,
	CMD_RETURN,
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
	//THEME_ENTRY_LNK_PATH,
	//THEME_ENTRY_LNK_PATH_INV,

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
	//[THEME_ENTRY_LNK_PATH] = { 0, COLOR_WHITE, COLOR_BLACK },
	//[THEME_ENTRY_LNK_PATH_INV] = { 0, COLOR_RED, COLOR_BLACK },
};

#define INPUT_LIST_LENGTH 4

struct input2cmd {
	struct input i[INPUT_LIST_LENGTH];
	enum mode m : 8;
	enum command c : 8;
};

#define IS_CTRL(I,K) (((I).t == I_CTRL) && ((I).utf[0] == (K)))

#define KEND { .t = I_NONE }
#define KUTF8(K) { .t = I_UTF8, .utf = K }
#define KSPEC(K) { .t = (K), .utf = { 0, 0, 0, 0 } }
#define KCTRL(K) { .t = I_CTRL, .utf[0] = (K) }

static struct input2cmd default_mapping[] = {
	/* MODE MANGER */
	{ { KUTF8("q"), KUTF8("q"), KEND }, MODE_MANAGER, CMD_QUIT },

	{ { KUTF8("g"), KUTF8("g"), KEND }, MODE_MANAGER, CMD_ENTRY_FIRST },
	{ { KSPEC(I_HOME), KEND }, MODE_MANAGER, CMD_ENTRY_FIRST },

	{ { KUTF8("G"), KEND }, MODE_MANAGER, CMD_ENTRY_LAST },
	{ { KSPEC(I_END), KEND }, MODE_MANAGER, CMD_ENTRY_LAST },

	{ { KUTF8("j"), KEND }, MODE_MANAGER, CMD_ENTRY_DOWN },
	{ { KCTRL('N'), KEND }, MODE_MANAGER, CMD_ENTRY_DOWN },
	{ { KSPEC(I_ARROW_DOWN), KEND }, MODE_MANAGER, CMD_ENTRY_DOWN },

	{ { KUTF8("k"), KEND }, MODE_MANAGER, CMD_ENTRY_UP },
	{ { KCTRL('P'), KEND }, MODE_MANAGER, CMD_ENTRY_UP },
	{ { KSPEC(I_ARROW_UP), KEND }, MODE_MANAGER, CMD_ENTRY_UP },

	{ { KUTF8("c"), KUTF8("p"), KEND }, MODE_MANAGER, CMD_COPY },

	{ { KUTF8("r"), KUTF8("m"), KEND }, MODE_MANAGER, CMD_REMOVE },

	{ { KUTF8("r"), KUTF8("n"), KEND }, MODE_MANAGER, CMD_RENAME },

	{ { KUTF8("m"), KUTF8("v"), KEND }, MODE_MANAGER, CMD_MOVE },

	{ { KCTRL('I'), KEND }, MODE_MANAGER, CMD_SWITCH_PANEL },

	{ { KUTF8("r"), KUTF8("r"), KEND }, MODE_MANAGER, CMD_REFRESH },
	{ { KCTRL('L'), KEND }, MODE_MANAGER, CMD_REFRESH },

	{ { KUTF8("m"), KUTF8("k"), KEND }, MODE_MANAGER, CMD_CREATE_DIR },

	{ { KUTF8("u"), KEND }, MODE_MANAGER, CMD_UP_DIR },
	{ { KSPEC(I_BACKSPACE), KEND }, MODE_MANAGER, CMD_UP_DIR },

	{ { KUTF8("i"), KEND }, MODE_MANAGER, CMD_ENTER_DIR },
	{ { KCTRL('J'), KEND }, MODE_MANAGER, CMD_ENTER_DIR },

	{ { KUTF8("o"), KEND }, MODE_MANAGER, CMD_OPEN_FILE },
	{ { KUTF8("e"), KUTF8("d"), KEND }, MODE_MANAGER, CMD_EDIT_FILE },

	{ { KUTF8("v"), KEND }, MODE_MANAGER, CMD_SELECT_FILE },
	{ { KUTF8("V"), KUTF8("a"), KEND }, MODE_MANAGER, CMD_SELECT_ALL },
	{ { KUTF8("V"), KUTF8("0"), KEND }, MODE_MANAGER, CMD_SELECT_NONE },

	{ { KUTF8("/"), KEND }, MODE_MANAGER, CMD_FIND },
	{ { KCTRL('V'), KEND }, MODE_MANAGER, CMD_DIR_VOLUME },

	{ { KUTF8("x"), KEND }, MODE_MANAGER, CMD_TOGGLE_HIDDEN },
	{ { KCTRL('H'), KEND }, MODE_MANAGER, CMD_TOGGLE_HIDDEN },

	{ { KUTF8("?"), KEND }, MODE_MANAGER, CMD_HELP },

	{ { KUTF8("c"), KUTF8("d"), KEND }, MODE_MANAGER, CMD_CD },

	{ { KUTF8("c"), KUTF8("h"), KEND }, MODE_MANAGER, CMD_CHMOD },

	{ { KUTF8("s"), KUTF8("n"), KUTF8("a"), KEND }, MODE_MANAGER, CMD_SORT_BY_NAME_ASC },
	{ { KUTF8("s"), KUTF8("n"), KUTF8("d"), KEND }, MODE_MANAGER, CMD_SORT_BY_NAME_DESC },
	{ { KUTF8("s"), KUTF8("d"), KUTF8("a"), KEND }, MODE_MANAGER, CMD_SORT_BY_DATE_ASC },
	{ { KUTF8("s"), KUTF8("d"), KUTF8("d"), KEND }, MODE_MANAGER, CMD_SORT_BY_DATE_DESC },
	{ { KUTF8("s"), KUTF8("s"), KUTF8("a"), KEND }, MODE_MANAGER, CMD_SORT_BY_SIZE_ASC },
	{ { KUTF8("s"), KUTF8("s"), KUTF8("d"), KEND }, MODE_MANAGER, CMD_SORT_BY_SIZE_DESC },

	/* MODE CHMOD */
	{ { KUTF8("q"), KUTF8("q"), KEND }, MODE_CHMOD, CMD_RETURN },
	{ { KUTF8("c"), KUTF8("h"), KEND }, MODE_CHMOD, CMD_CHANGE },
	{ { KUTF8("c"), KUTF8("o"), KEND }, MODE_CHMOD, CMD_CHOWN },
	{ { KUTF8("c"), KUTF8("g"), KEND }, MODE_CHMOD, CMD_CHGRP },
	{ { KUTF8("u"), KUTF8("i"), KEND }, MODE_CHMOD, CMD_TOGGLE_UIOX },
	{ { KUTF8("g"), KUTF8("i"), KEND }, MODE_CHMOD, CMD_TOGGLE_GIOX },
	{ { KUTF8("o"), KUTF8("s"), KEND }, MODE_CHMOD, CMD_TOGGLE_SB },
	{ { KUTF8("u"), KUTF8("r"), KEND }, MODE_CHMOD, CMD_TOGGLE_UR },
	{ { KUTF8("u"), KUTF8("w"), KEND }, MODE_CHMOD, CMD_TOGGLE_UW },
	{ { KUTF8("u"), KUTF8("x"), KEND }, MODE_CHMOD, CMD_TOGGLE_UX },
	{ { KUTF8("g"), KUTF8("r"), KEND }, MODE_CHMOD, CMD_TOGGLE_GR },
	{ { KUTF8("g"), KUTF8("w"), KEND }, MODE_CHMOD, CMD_TOGGLE_GW },
	{ { KUTF8("g"), KUTF8("x"), KEND }, MODE_CHMOD, CMD_TOGGLE_GX },
	{ { KUTF8("o"), KUTF8("r"), KEND }, MODE_CHMOD, CMD_TOGGLE_OR },
	{ { KUTF8("o"), KUTF8("w"), KEND }, MODE_CHMOD, CMD_TOGGLE_OW },
	{ { KUTF8("o"), KUTF8("x"), KEND }, MODE_CHMOD, CMD_TOGGLE_OX },

	/* TODO
	 * +x
	 * -x
	 * o+x
	 * u+x
	 * ???
	 */

	/* MODE WAIT */
	{ { KUTF8("q"), KUTF8("q"), KEND }, MODE_WAIT, CMD_TASK_QUIT },
	{ { KUTF8("p"), KUTF8("p"), KEND }, MODE_WAIT, CMD_TASK_PAUSE },
	{ { KUTF8("r"), KUTF8("r"), KEND }, MODE_WAIT, CMD_TASK_RESUME },


	/* MODE HELP */
	{ { KUTF8("q"), KEND }, MODE_HELP, CMD_HELP_QUIT },

	{ { KUTF8("j"), KEND }, MODE_HELP, CMD_HELP_DOWN },
	{ { KCTRL('N'), KEND }, MODE_HELP, CMD_HELP_DOWN },
	{ { KSPEC(I_ARROW_DOWN), KEND }, MODE_HELP, CMD_HELP_DOWN },

	{ { KUTF8("k"), KEND }, MODE_HELP, CMD_HELP_UP },
	{ { KCTRL('P'), KEND }, MODE_HELP, CMD_HELP_UP },
	{ { KSPEC(I_ARROW_UP), KEND }, MODE_HELP, CMD_HELP_UP },
};

static const size_t default_mapping_length = (sizeof(default_mapping)/sizeof(struct input2cmd));

static const char* const cmd_help[] = {
	[CMD_QUIT] = "Quit hund.",
	[CMD_HELP] = "Display help screen.",

	[CMD_COPY] = "Copy highlighted file to the other directory.",
	[CMD_MOVE] = "Move highlighted file to the other directory.",
	[CMD_REMOVE] = "Remove highlighted file.",
	[CMD_CREATE_DIR] = "Create new directories.",
	[CMD_RENAME] = "Rename selected files.",

	[CMD_UP_DIR] = "Go up in directory tree.",
	[CMD_ENTER_DIR] = "Enter highlighted directory.",

	[CMD_ENTRY_UP] = "Go to previous entry.",
	[CMD_ENTRY_DOWN] = "Go to next entry.",

	[CMD_ENTRY_FIRST] = "Go to the top file in directory.",
	[CMD_ENTRY_LAST] = "Go to the bottom file in directory.",

	[CMD_CD] = "Jump to some directory.",
	[CMD_OPEN_FILE] = "Open selected file in pager.",
	[CMD_EDIT_FILE] = "Open selected file in text editor.",

	[CMD_REFRESH] = "Rescan directories and redraw UI.",
	[CMD_SWITCH_PANEL] = "Switch active panel.",

	[CMD_DIR_VOLUME] = "Calcualte volume of highlighted directory.",
	[CMD_TOGGLE_HIDDEN] = "Toggle between hiding/showing hidden files.",

	[CMD_SORT_BY_NAME_ASC] = "Sort by name ascending.",
	[CMD_SORT_BY_NAME_DESC] = "Sort by name descending.",
	[CMD_SORT_BY_DATE_ASC] = "Sort by date ascending.",
	[CMD_SORT_BY_DATE_DESC] = "Sort by date descending.",
	[CMD_SORT_BY_SIZE_ASC] = "Sort by size ascending.",
	[CMD_SORT_BY_SIZE_DESC] = "Sort by size descending.",

	[CMD_SELECT_FILE] = "Select/unselect file.",
	[CMD_SELECT_ALL] = "Select all visible files.",
	[CMD_SELECT_NONE] = "Unselect all files.",

	[CMD_FIND] = "Search for files in current directory.",

	[CMD_CHMOD] = "Change permissions of highlighted file.",
	[CMD_CHANGE] = "Apply changes and return.",
	[CMD_RETURN] = "Abort changes and return.",
	[CMD_CHOWN] = "Change owner of file.",
	[CMD_CHGRP] = "Change group of file.",

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
	MSG_INFO = 1<<0,
	//MSG_WARNING,
	MSG_ERROR = 1<<1,
};

static const char* const timefmt = "%Y-%m-%d %H:%M:%S";
#define TIME_SIZE (4+1+2+1+2+1+2+1+2+1+2+1)

struct ui {
	int scrh, scrw; // Last window dimensions

	bool ui_needs_refresh;
	bool run;

	enum mode m;
	enum msg_type mt;

	char msg[MSG_BUFFER_SIZE];

	char prch;
	char* prompt;

	PANEL* fvp[2];
	struct file_view* fvs[2];
	struct file_view* pv;
	struct file_view* sv;

	size_t helpy;

	struct input2cmd* kmap;
	size_t kml; // Key Mapping Length
	unsigned short* mks; // Matching Key Sequence

	struct input il[INPUT_LIST_LENGTH];
	int ili;

	char* path; // path of chmodded file
	// [0] = old value
	// [1] = new/edited value
	mode_t perm[2]; // permissions of chmodded file
	uid_t o[2];
	gid_t g[2];
	char perms[10];
	char time[TIME_SIZE];
	char user[LOGIN_NAME_MAX+1];
	char group[LOGIN_NAME_MAX+1];
};

struct ui ui_init(struct file_view* const, struct file_view* const);
void ui_end(struct ui* const);
void ui_draw(struct ui* const);
void ui_update_geometry(struct ui* const);

int chmod_open(struct ui* const, char* const);
void chmod_close(struct ui* const);

struct select_option {
	struct input i;
	char* h; // Hint
};

int ui_select(struct ui* const, const char* const q,
		const struct select_option*, const size_t);

enum command get_cmd(struct ui* const);
int fill_textbox(const struct ui* const, char* const,
		char** const, const size_t);

int prompt(struct ui* const, char* const, char*, const size_t);

void failed(struct ui* const, const char* const,
		const int, const char* const);

int spawn(char* const[]);

#endif
