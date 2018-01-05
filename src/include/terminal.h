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
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>

#include "utf8.h"

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

static const char* const keynames[] = {
	[I_NONE] = NULL,
	[I_UTF8] = NULL,
	[I_CTRL] = NULL,
	[I_ARROW_UP] = "up",
	[I_ARROW_DOWN] = "down",
	[I_ARROW_RIGHT] = "right",
	[I_ARROW_LEFT] = "left",
	[I_HOME] = "home",
	[I_END] = "end",
	[I_PAGE_UP] = "pgup",
	[I_PAGE_DOWN] = "pgdn",
	[I_INSERT] = "ins",
	[I_BACKSPACE] = "bsp",
	[I_DELETE] = "del",
	[I_ESCAPE] = "esc"
};

ssize_t xread(int, void*, ssize_t, int);
int start_raw_mode(struct termios* const);
int stop_raw_mode(struct termios* const);
struct input get_input(int);

#endif
