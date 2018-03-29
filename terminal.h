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

#ifndef TERMINAL_H
#define TERMINAL_H

#ifndef _DEFAULT_SOURCE
	#define _DEFAULT_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#include "utf8.h"

#define ESC_TIMEOUT_MS 125

#define CTRL_KEY(K) ((K) & 0x1f)

enum input_type {
	I_NONE = 0,
	I_UTF8, // utf will contain zero-terminated bytes of the glyph
	I_CTRL, // utf[0] will contain character
	I_ARROW_UP,
	I_ARROW_DOWN,
	I_ARROW_RIGHT,
	I_ARROW_LEFT,
	I_HOME,
	I_END,
	I_PAGE_UP,
	I_PAGE_DOWN,
	I_INSERT,
	I_BACKSPACE,
	I_DELETE,
	I_ESCAPE,
};

struct input {
	enum input_type t : 8;
	char utf[5];
};

struct s2i { // Sequence to Input
	char* seq;
	enum input_type t : 8;
};

// Sequence -> Key Mapping
static const struct s2i SKM[] = {
	{ "\x1b[@", I_INSERT },
	{ "\x1b[A", I_ARROW_UP },
	{ "\x1b[B", I_ARROW_DOWN },
	{ "\x1b[C", I_ARROW_RIGHT },
	{ "\x1b[D", I_ARROW_LEFT },
	{ "\x1b[H", I_HOME },
	{ "\x1b[F", I_END },
	{ "\x1b[P", I_DELETE },
	{ "\x1b[V", I_PAGE_UP },
	{ "\x1b[U", I_PAGE_DOWN },
	{ "\x1b[Y", I_END },

	{ "\x1bOA", I_ARROW_UP },
	{ "\x1bOB", I_ARROW_DOWN },
	{ "\x1bOC", I_ARROW_RIGHT },
	{ "\x1bOD", I_ARROW_LEFT },
	{ "\x1bOH", I_HOME },
	{ "\x1bOF", I_END },

	{ "\x1b[1~", I_HOME },
	{ "\x1b[3~", I_DELETE },
	{ "\x1b[4~", I_END },
	{ "\x1b[5~", I_PAGE_UP },
	{ "\x1b[6~", I_PAGE_DOWN },
	{ "\x1b[7~", I_HOME },
	{ "\x1b[8~", I_END },
	{ "\x1b[4h", I_INSERT },
	{ "\x1b", I_ESCAPE },
	{ NULL, I_NONE },
};

ssize_t xread(int, void*, ssize_t, int);
int start_raw_mode(struct termios* const);
int stop_raw_mode(struct termios* const);
struct input get_input(int);

enum char_attr {
	ATTR_NORMAL = 0,
	ATTR_BOLD = 1,
	ATTR_FAINT = 2,
	ATTR_ITALIC = 3,
	ATTR_UNDERLINE = 4,
	ATTR_BLINK = 5,
	ATTR_INVERSE = 7,
	ATTR_INVISIBLE = 8,

	ATTR_NOT_BOLD_OR_FAINT = 22,
	ATTR_NOT_ITALIC = 23,
	ATTR_NOT_UNDERLINE = 24,
	ATTR_NOT_BLINK = 25,
	ATTR_NOT_INVERSE = 27,
	ATTR_NOT_INVISIBLE = 28,

	ATTR_BLACK = '0',
	ATTR_RED = '1',
	ATTR_GREEN = '2',
	ATTR_YELLOW = '3',
	ATTR_BLUE = '4',
	ATTR_MAGENTA = '5',
	ATTR_CYAN = '6',
	ATTR_WHITE = '7',
	ATTR_DEFAULT = '9',

	ATTR_FOREGROUND = 1<<8,
	ATTR_BACKGROUND = 1<<9,
	ATTR_COLOR_256 = 1<<10,
	ATTR_COLOR_TRUE = 1<<11,
};

int char_attr(char* const, const size_t, const int,
		const unsigned char* const);

int move_cursor(const unsigned int, const unsigned int);
int window_size(int* const, int* const);

#define APPEND_BUFFER_INC 64
struct append_buffer {
	char* buf;
	size_t top;
	size_t capacity;
};

size_t append(struct append_buffer* const, const char* const, const size_t);
size_t append_attr(struct append_buffer* const, const int,
		const unsigned char* const);
size_t fill(struct append_buffer* const, const char, const size_t);

#define CSI_CLEAR_ALL "\x1b[2J", 4
#define CSI_CLEAR_LINE "\x1b[K", 3
#define CSI_CURSOR_TOP_LEFT "\x1b[H", 3
#define CSI_CURSOR_SHOW "\x1b[?25h", 6
#define CSI_CURSOR_HIDE "\x1b[?25l", 6
#define CSI_SCREEN_ALTERNATIVE "\x1b[?47h", 6
#define CSI_SCREEN_NORMAL "\x1b[?47l", 6
#define CSI_CURSOR_HIDE_TOP_LEFT "\x1b[?25l\x1b[H", 9

#endif
