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

#ifndef UI_H
#define UI_H

#ifndef _DEFAULT_SOURCE
	#define _DEFAULT_SOURCE
#endif

#include <limits.h>
#include <locale.h>
#include <time.h>
#include <sys/wait.h>
#include <signal.h>

#include "panel.h"
#include "utf8.h"
#include "terminal.h"

#define MSG_BUFFER_SIZE 128
#define KEYNAME_BUF_SIZE 16
// TODO adjust KEYNAME_BUF_SIZE

enum mode {
	MODE_MANAGER = 0,
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

	CMD_LINK,

	CMD_UP_DIR,
	CMD_ENTER_DIR, // TODO RENAME

	CMD_ENTRY_UP,
	CMD_ENTRY_DOWN,

	CMD_SCREEN_UP,
	CMD_SCREEN_DOWN,

	CMD_ENTRY_FIRST,
	CMD_ENTRY_LAST,

	CMD_COMMAND,
	CMD_CD,

	CMD_REFRESH,
	CMD_SWITCH_PANEL,
	CMD_DUP_PANEL,
	CMD_SWAP_PANELS,

	CMD_DIR_VOLUME,
	CMD_TOGGLE_HIDDEN,

	CMD_SORT_REVERSE,
	CMD_SORT_CHANGE,

	CMD_COL,

	CMD_SELECT_FILE,
	CMD_SELECT_ALL,
	CMD_SELECT_NONE,

	CMD_SELECTED_NEXT,
	CMD_SELECTED_PREV,

	CMD_MARK_NEW,
	CMD_MARK_JUMP,

	CMD_FIND,

	CMD_CHMOD,
	CMD_CHANGE,
	CMD_RETURN,
	CMD_CHOWN,
	CMD_CHGRP,

	CMD_A,
	CMD_U,
	CMD_G,
	CMD_O,
	CMD_PL,
	CMD_MI,

	CMD_TASK_QUIT,
	CMD_TASK_PAUSE,
	CMD_TASK_RESUME,

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
	THEME_ENTRY_LNK_UNS,
	THEME_ENTRY_LNK_SEL,

	THEME_ELEM_NUM
};

struct theme_attrs {
	int fg, bg;
	unsigned char fg_color[3], bg_color[3];
};

#define S_IFMT_TZERO 12
// ^ Trailing ZEROes
static const char mode_type_symbols[] = {
	[S_IFIFO>>S_IFMT_TZERO] = 'p',
	[S_IFCHR>>S_IFMT_TZERO] = 'c',
	[S_IFDIR>>S_IFMT_TZERO] = 'd',
	[S_IFBLK>>S_IFMT_TZERO] = 'b',
	[S_IFREG>>S_IFMT_TZERO] = '-',
	[S_IFLNK>>S_IFMT_TZERO] = 'l',
	[S_IFSOCK>>S_IFMT_TZERO] = 's',
};

static const char file_symbols[] = {
	[THEME_ENTRY_BLK_UNS] = '+',
	[THEME_ENTRY_CHR_UNS] = '-',
	[THEME_ENTRY_FIFO_UNS] = '|',
	[THEME_ENTRY_REG_UNS] = ' ',
	[THEME_ENTRY_REG_EXE_UNS] = '*',
	[THEME_ENTRY_DIR_UNS] = '/',
	[THEME_ENTRY_SOCK_UNS] = '=',
	[THEME_ENTRY_LNK_UNS] = '~',
};

static const struct theme_attrs theme_scheme[THEME_ELEM_NUM] = {
	[THEME_OTHER] = { ATTR_BLACK, ATTR_BLACK, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_PATHBAR] = { ATTR_BLACK, ATTR_WHITE, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_STATUSBAR] = { ATTR_BLACK, ATTR_WHITE, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ERROR] = { ATTR_BLACK, ATTR_RED, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_INFO] = { ATTR_WHITE, ATTR_BLACK, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_BLK_UNS] = { ATTR_RED, ATTR_BLACK, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_BLK_SEL] = { ATTR_BLACK, ATTR_RED, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_CHR_UNS] = { ATTR_YELLOW, ATTR_BLACK, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_CHR_SEL] = { ATTR_BLACK, ATTR_YELLOW, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_FIFO_UNS] = { ATTR_GREEN, ATTR_BLACK, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_FIFO_SEL] = { ATTR_BLACK, ATTR_GREEN, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_REG_UNS] = { ATTR_WHITE, ATTR_BLACK, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_REG_SEL] = { ATTR_BLACK, ATTR_WHITE, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_REG_EXE_UNS] = { ATTR_MAGENTA, ATTR_BLACK, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_REG_EXE_SEL] = { ATTR_BLACK, ATTR_MAGENTA, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_DIR_UNS] = { ATTR_CYAN, ATTR_BLACK, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_DIR_SEL] = { ATTR_BLACK, ATTR_CYAN, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_SOCK_UNS] = { ATTR_GREEN, ATTR_BLACK, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_SOCK_SEL] = { ATTR_BLACK, ATTR_GREEN, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_LNK_UNS] = { ATTR_CYAN, ATTR_BLACK, { 0, 0, 0 }, { 0, 0, 0 } },
	[THEME_ENTRY_LNK_SEL] = { ATTR_BLACK, ATTR_CYAN, { 0, 0, 0 }, { 0, 0, 0 } },
};

#define INPUT_LIST_LENGTH 4

struct input2cmd {
	struct input i[INPUT_LIST_LENGTH];
	enum mode m : 8;
	enum command c : 8;
};

#define IS_CTRL(I,K) (((I).t == I_CTRL) && ((I).utf[0] == (K)))

#define KUTF8(K) { .t = I_UTF8, .utf = K }
#define KSPEC(K) { .t = (K) }
#define KCTRL(K) { .t = I_CTRL, .utf[0] = (K) }

static struct input2cmd default_mapping[] = {
	/* MODE MANGER */
	{ { KUTF8("q"), KUTF8("q") }, MODE_MANAGER, CMD_QUIT },

	{ { KUTF8("g"), KUTF8("g") }, MODE_MANAGER, CMD_ENTRY_FIRST },
	{ { KSPEC(I_HOME) }, MODE_MANAGER, CMD_ENTRY_FIRST },

	{ { KUTF8("G") }, MODE_MANAGER, CMD_ENTRY_LAST },
	{ { KSPEC(I_END) }, MODE_MANAGER, CMD_ENTRY_LAST },

	{ { KCTRL('B') }, MODE_MANAGER, CMD_SCREEN_UP },
	{ { KCTRL('F') }, MODE_MANAGER, CMD_SCREEN_DOWN },

	{ { KUTF8("j") }, MODE_MANAGER, CMD_ENTRY_DOWN },
	{ { KCTRL('N') }, MODE_MANAGER, CMD_ENTRY_DOWN },
	{ { KSPEC(I_ARROW_DOWN) }, MODE_MANAGER, CMD_ENTRY_DOWN },

	{ { KUTF8("k") }, MODE_MANAGER, CMD_ENTRY_UP },
	{ { KCTRL('P') }, MODE_MANAGER, CMD_ENTRY_UP },
	{ { KSPEC(I_ARROW_UP) }, MODE_MANAGER, CMD_ENTRY_UP },

	{ { KUTF8("g"), KUTF8("c") }, MODE_MANAGER, CMD_COPY },
	{ { KUTF8("g"), KUTF8("r") }, MODE_MANAGER, CMD_REMOVE },
	{ { KUTF8("g"), KUTF8("n") }, MODE_MANAGER, CMD_RENAME },
	{ { KUTF8("g"), KUTF8("m") }, MODE_MANAGER, CMD_MOVE },
	{ { KUTF8("g"), KUTF8("l")}, MODE_MANAGER, CMD_LINK },

	{ { KCTRL('I') }, MODE_MANAGER, CMD_SWITCH_PANEL },
	{ { KUTF8(" ") }, MODE_MANAGER, CMD_SWITCH_PANEL },

	{ { KUTF8("z") }, MODE_MANAGER, CMD_DUP_PANEL },
	{ { KUTF8("Z") }, MODE_MANAGER, CMD_SWAP_PANELS },

	{ { KUTF8("r"), KUTF8("r") }, MODE_MANAGER, CMD_REFRESH },
	{ { KCTRL('L') }, MODE_MANAGER, CMD_REFRESH },

	{ { KUTF8("g"), KUTF8("d") }, MODE_MANAGER, CMD_CREATE_DIR },

	{ { KUTF8("u") }, MODE_MANAGER, CMD_UP_DIR },
	{ { KUTF8("h") }, MODE_MANAGER, CMD_UP_DIR },
	{ { KSPEC(I_BACKSPACE) }, MODE_MANAGER, CMD_UP_DIR },

	{ { KUTF8("i") }, MODE_MANAGER, CMD_ENTER_DIR },
	{ { KCTRL('M') }, MODE_MANAGER, CMD_ENTER_DIR },
	{ { KCTRL('J') }, MODE_MANAGER, CMD_ENTER_DIR },
	{ { KUTF8("l") }, MODE_MANAGER, CMD_ENTER_DIR },

	/* <TODO> */
	{ { KUTF8("v") }, MODE_MANAGER, CMD_SELECT_FILE },
	{ { KUTF8("V") }, MODE_MANAGER, CMD_SELECT_ALL },
	{ { KUTF8("0") }, MODE_MANAGER, CMD_SELECT_NONE },

	{ { KUTF8(".") }, MODE_MANAGER, CMD_SELECTED_NEXT },
	{ { KUTF8(",") }, MODE_MANAGER, CMD_SELECTED_PREV },
	/* </TODO> */

	{ { KUTF8("m") }, MODE_MANAGER, CMD_MARK_NEW },
	{ { KUTF8("'") }, MODE_MANAGER, CMD_MARK_JUMP },

	{ { KUTF8("/") }, MODE_MANAGER, CMD_FIND },
	{ { KCTRL('V') }, MODE_MANAGER, CMD_DIR_VOLUME },

	{ { KUTF8("x") }, MODE_MANAGER, CMD_TOGGLE_HIDDEN },

	{ { KUTF8("?") }, MODE_MANAGER, CMD_HELP },

	{ { KUTF8(":") }, MODE_MANAGER, CMD_COMMAND },
	{ { KUTF8("c"), KUTF8("d") }, MODE_MANAGER, CMD_CD },

	{ { KUTF8("c"), KUTF8("c") }, MODE_MANAGER, CMD_CHMOD },

	{ { KUTF8("s"), KUTF8("r") }, MODE_MANAGER, CMD_SORT_REVERSE },
	{ { KUTF8("s"), KUTF8("c") }, MODE_MANAGER, CMD_SORT_CHANGE },

	{ { KUTF8("t") }, MODE_MANAGER, CMD_COL, },

	/* MODE CHMOD */
	{ { KUTF8("q"), KUTF8("q") }, MODE_CHMOD, CMD_RETURN },
	{ { KUTF8("c"), KUTF8("c") }, MODE_CHMOD, CMD_CHANGE },

	{ { KUTF8("c"), KUTF8("o") }, MODE_CHMOD, CMD_CHOWN },
	{ { KUTF8("c"), KUTF8("g") }, MODE_CHMOD, CMD_CHGRP },

	{ { KUTF8("a") }, MODE_CHMOD, CMD_A, },
	{ { KUTF8("u") }, MODE_CHMOD, CMD_U, },
	{ { KUTF8("g") }, MODE_CHMOD, CMD_G, },
	{ { KUTF8("o") }, MODE_CHMOD, CMD_O, },
	{ { KUTF8("+") }, MODE_CHMOD, CMD_PL, },
	{ { KUTF8("-") }, MODE_CHMOD, CMD_MI, },

	/* MODE WAIT */
	{ { KUTF8("q"), KUTF8("q") }, MODE_WAIT, CMD_TASK_QUIT },
	{ { KUTF8("p"), KUTF8("p") }, MODE_WAIT, CMD_TASK_PAUSE },
	{ { KUTF8("r"), KUTF8("r") }, MODE_WAIT, CMD_TASK_RESUME },

};

static const size_t default_mapping_length =
	(sizeof(default_mapping)/sizeof(default_mapping[0]));

static const char* const cmd_help[] = {
	[CMD_QUIT] = "Quit hund",
	[CMD_HELP] = "Display help",

	[CMD_COPY] = "Copy selected file to the other directory",
	[CMD_MOVE] = "Move selected file to the other directory",
	[CMD_REMOVE] = "Remove selected file",
	[CMD_CREATE_DIR] = "Create new directories",
	[CMD_RENAME] = "Rename selected files",

	[CMD_LINK] = "Create symlinks to selected files",

	[CMD_UP_DIR] = "Go up in directory tree",
	[CMD_ENTER_DIR] = "Enter highlighted directory or open file",

	[CMD_ENTRY_UP] = "Go to previous entry",
	[CMD_ENTRY_DOWN] = "Go to next entry",

	[CMD_SCREEN_UP] = "Scroll 1 screen up",
	[CMD_SCREEN_DOWN] = "Scroll 1 screen down",

	[CMD_ENTRY_FIRST] = "Go to the top file in directory",
	[CMD_ENTRY_LAST] = "Go to the bottom file in directory",

	[CMD_COMMAND] = "Open command line",

	[CMD_CD] = "Jump to some directory",
	[CMD_REFRESH] = "Rescan directories and redraw UI",
	[CMD_SWITCH_PANEL] = "Switch active panel",
	[CMD_DUP_PANEL] = "Open current directory in the other panel",
	[CMD_SWAP_PANELS] = "Swap panels",

	[CMD_DIR_VOLUME] = "Calcualte volume of selected directory",
	[CMD_TOGGLE_HIDDEN] = "Toggle between hiding/showing hidden files",

	[CMD_SORT_REVERSE] = "Switch between ascending/descending sorting",
	[CMD_SORT_CHANGE] = "Change sorting",

	[CMD_COL] = "Change column",

	[CMD_SELECT_FILE] = "Select/unselect file",
	[CMD_SELECT_ALL] = "Select all visible files",
	[CMD_SELECT_NONE] = "Unselect all files",

	[CMD_SELECTED_NEXT] = "Jump to the next selected file",
	[CMD_SELECTED_PREV] = "Jump to the previous selected file",

	[CMD_MARK_NEW] = "Set mark at highlighted file",
	[CMD_MARK_JUMP] = "Jump to a mark",
	[CMD_FIND] = "Search for files in current directory",

	[CMD_CHMOD] = "Change permissions of selected files",
	[CMD_CHANGE] = "Apply changes and return",
	[CMD_RETURN] = "Abort changes and return",
	[CMD_CHOWN] = "Change owner of file",
	[CMD_CHGRP] = "Change group of file",

	[CMD_A] = "Modify permissions for all",
	[CMD_U] = "Modify permissions for owner",
	[CMD_G] = "Modify permissions for group",
	[CMD_O] = "Modify permissions for others",
	[CMD_PL] = "Add permissions for all",
	[CMD_MI] = "Remove permissions for all",

	[CMD_TASK_QUIT] = "Abort task",
	[CMD_TASK_PAUSE] = "Pause task",
	[CMD_TASK_RESUME] = "Resume task",

	[CMD_NUM] = NULL,
};

static const char* const mode_strings[] = {
	[MODE_CHMOD] = "CHMOD",
	[MODE_MANAGER] = "FILE VIEW",
	[MODE_WAIT] = "WAIT",
};

static const char* const more_help[] = {
	"COMMAND LINE",
	"Press `:` to enter command line",
	"q\tExit hund",
	"h/help\tOpen help",
	"lm\tList marks",
	"noh/nos\tClear selection",
	"+x\tQuick chmod +x",
	"sh\tOpen shell",
	"sh ...\tExecute command in shell",
	"",
	"SORTING",
	"+\tascending",
	"-\tdescending",
	"n\tsort by name",
	"s\tsort by size",
	"a\tsort by atime",
	"c\tsort by ctime",
	"m\tsort by mtime",
	"d\tdirectories first*",
	"p\tsort by permission",
	"x\texecutables first*",
	"i\tsort by inode number",
	"u\tsort by uid",
	"U\tsort by user name",
	"g\tsort by gid",
	"G\tsort by group name",
	"*\treversing order of sorting makes them last",
	"Most important sorting key is at the end",
	"EXAMPLES",
	"+d\tdirectories are first",
	"-d\tdirectories are last",
	"+xd\tdirectories are first",
	"+dx\texecutables are first",
	"+nx\texecutables are first, then within each block",
	"   \t(executables and others) files are sorted by name",
	"",
	"COLUMN",
	"Only one column at a time can be visible",
	"t\thide column",
	"s/S\tshort/long size",
	"a/A\tshort/long atime",
	"c/C\tshort/long ctime",
	"m/M\tshort/long mtime",
	"p/P\tshort/long permission",
	"i\tinode number",
	"u\tuid",
	"U\tuser name",
	"g\tgid",
	"G\tgroup name",
	"",
	"MARKS",
	"m<letter>\tSet mark",
	"'<letter>\tGoto mark",
	"[a-z]\tSave path to highlighted file.",
	"[A-Z]\tSave only the working directory.",
	"     \tJump to the working directory and highlight first entry.",
	"",
	"CHMOD",
	"", // TODO
	NULL,
};

static const char* const copyright_notice[] = {
	"Hund  Copyright (C) 2017-2018  Michał Czarnecki",
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

enum {
	BUF_PATHBAR = 0,
	BUF_PANELS,
	BUF_STATUSBAR,
	BUF_BOTTOMBAR,
	BUF_NUM
};

enum dirty_flag {
	DIRTY_PATHBAR = 1<<BUF_PATHBAR,
	DIRTY_PANELS = 1<<BUF_PANELS,
	DIRTY_STATUSBAR = 1<<BUF_STATUSBAR,
	DIRTY_BOTTOMBAR = 1<<BUF_BOTTOMBAR,
	DIRTY_ALL = DIRTY_PATHBAR|DIRTY_PANELS|DIRTY_STATUSBAR|DIRTY_BOTTOMBAR,
};

struct ui {
	int scrh, scrw; // Last window dimensions
	int pw[2]; // Panel Width
	int ph; // Panel Height
	int pxoff[2]; // Panel X OFFset

	bool run;

	enum mode m;

	enum msg_type mt;
	char msg[MSG_BUFFER_SIZE];

	char prch[16]; // TODO adjust size
	char* prompt;
	int prompt_cursor_pos;

	int timeout; // microseconds

	struct append_buffer B[BUF_NUM];
	enum dirty_flag dirty;
	struct termios T;

	struct panel* fvs[2];
	struct panel* pv;
	struct panel* sv;

	struct input2cmd* kmap;
	size_t kml; // Key Mapping Length
	struct input K[INPUT_LIST_LENGTH];

	char* path; // path of chmodded file

	// [0] = old value
	// [1] = new/edited value
	mode_t perm[2]; // permissions of chmodded file
	mode_t plus, minus;
	uid_t o[2];
	gid_t g[2];
	char perms[10];
	char time[TIME_SIZE];
	char user[LOGIN_BUF_SIZE];
	char group[LOGIN_BUF_SIZE];
};

typedef void (*draw_t)(struct ui* const, struct append_buffer* const);
void ui_pathbar(struct ui* const, struct append_buffer* const);
void ui_panels(struct ui* const, struct append_buffer* const);
void ui_statusbar(struct ui* const, struct append_buffer* const);
void ui_bottombar(struct ui* const, struct append_buffer* const);

static const draw_t do_draw[] = {
	[BUF_PATHBAR] = ui_pathbar,
	[BUF_PANELS] = ui_panels,
	[BUF_STATUSBAR] = ui_statusbar,
	[BUF_BOTTOMBAR] = ui_bottombar,
};


void ui_init(struct ui* const, struct panel* const,
		struct panel* const);
void ui_end(struct ui* const);
int help_to_fd(struct ui* const, const int);
void ui_draw(struct ui* const);
void ui_update_geometry(struct ui* const);

int chmod_open(struct ui* const, char* const);
void chmod_close(struct ui* const);

struct select_option {
	struct input i;
	char* h; // Hint
};

int ui_ask(struct ui* const, const char* const q,
		const struct select_option*, const size_t);

enum command get_cmd(struct ui* const);
int fill_textbox(struct ui* const, char* const,
		char** const, const size_t, struct input* const);

int prompt(struct ui* const, char* const, char*, const size_t);

void failed(struct ui* const, const char* const, const char* const);
bool ui_rescan(struct ui* const, struct panel* const,
		struct panel* const);

enum spawn_flags {
	SF_SLIENT = 1<<0,
};

int spawn(char* const[], const enum spawn_flags);

size_t append_theme(struct append_buffer* const, const enum theme_element);

#endif
