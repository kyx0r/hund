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

#include "include/ui.h"

void ui_init(struct ui* const i) {
	setlocale(LC_ALL, "");
	initscr();
	if (has_colors() == FALSE) {
		endwin();
		printf("no colors :(\n");
		exit(1);
	}
	start_color();
	noecho();
	//nonl();
	//raw();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	timeout(100);
	curs_set(0);

	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_BLACK, COLOR_WHITE);
	init_pair(3, COLOR_CYAN, COLOR_BLACK);
	init_pair(4, COLOR_BLACK, COLOR_CYAN);
	init_pair(5, COLOR_RED, COLOR_BLACK);
	init_pair(6, COLOR_BLACK, COLOR_RED);
	init_pair(7, COLOR_YELLOW, COLOR_BLACK);
	init_pair(8, COLOR_BLACK, COLOR_YELLOW);

	i->fvs[0] = (struct file_view) {
		.file_list = NULL,
		.num_files = 0,
		.selection = 0,
		.view_offset = 0,
	};
	i->fvs[1] = (struct file_view) {
		.file_list = NULL,
		.num_files = 0,
		.selection = 0,
		.view_offset = 0,
	};
	for (int x = 0; x < 2; x++) {
		WINDOW* tmpwin = newwin(1, 1, 0, 0);
		i->fvp[x] = new_panel(tmpwin);
	}
	i->scrw = i->scrh = 0;
	i->active_view = 0;
	i->prompt_title = NULL;
	i->prompt_textbox = NULL;
	i->prompt_textbox_size = 0;
	i->prompt = NULL;
	WINDOW* hw = newwin(1, 1, 0, 0);
	i->hint = new_panel(hw);
	syslog(LOG_DEBUG, "ncurses initialized");
}

void ui_end(struct ui* const i) {
	/* Delete panel first, THEN it's window.
	 * If you resize the terminal window at least once and then quit (q)
	 * then the program will hang in infinite loop
	 * (I attached to it via gdb and it was stuck either on del_panel or poll.)
	 * or segfault/double free/corrupted unsorted chunks.
	 *
	 * my_remove_panel() in demo_panels.c from ncurses-examples
	 * did this is this order
	 * http://invisible-island.net/ncurses/ncurses-examples.html
	 */
	for (int x = 0; x < 2; x++) {
		delete_file_list(&i->fvs[x].file_list, &i->fvs[x].num_files);
	}
	for (int x = 0; x < 2; x++) {
		PANEL* p = i->fvp[x];
		WINDOW* w = panel_window(p);
		del_panel(p);
		delwin(w);
	}
	WINDOW* hw = panel_window(i->hint);
	del_panel(i->hint);
	delwin(hw);
	endwin();
}

void ui_draw(struct ui* const i) {
	for (int v = 0; v < 2; v++) {
		struct file_view* s = &i->fvs[v];
		PANEL* p = i->fvp[v];
		int ph, pw;
		getmaxyx(panel_window(p), ph, pw);
		if (s->selection == -1) {
			s->selection = 0;
		}
		WINDOW* w = panel_window(p);
		wborder(w, '|', '|', '-', '-', '+', '+', '+', '+');
		struct passwd* pwd = get_pwd();
		char* pretty = malloc(PATH_MAX);
		strcpy(pretty, s->wd);
		prettify_path(pretty, pwd->pw_dir);
		wattron(w, COLOR_PAIR(4));
		mvwprintw(w, 0, 2, "%s", pretty);
		wattroff(w, COLOR_PAIR(4));
		free(pretty);
		// First, adjust view_offset to selection
		// (Keep selection in center if possible)
		if (s->num_files <= ph - 1) {
			s->view_offset = 0;
		}
		else if (s->selection < ph/2) {
			s->view_offset = 0;
		}
		else if (s->selection > s->num_files - ph/2) {
			s->view_offset = s->num_files - ph + 2;
		}
		else {
			s->view_offset = s->selection - ph/2 + 1;
		}
		int view_row = 1; // Skipping border
		int ei = s->view_offset; // Entry Index
		while (ei < s->num_files && view_row < ph-1) {
			// Current File Record
			const struct file_record* cfr = s->file_list[ei];
			char type_symbol = '?';
			int color_pair_enabled = 0;
			switch (cfr->t) {
			case BLOCK:
				type_symbol = '+';
				color_pair_enabled = 7;
				break;
			case CHARACTER:
				type_symbol = '-';
				color_pair_enabled = 7;
				break;
			case DIRECTORY:
				type_symbol = '/';
				color_pair_enabled = 3;
				break;
			case FIFO:
				type_symbol = '|';
				color_pair_enabled = 1;
				break;
			case LINK:
				type_symbol = '~';
				color_pair_enabled = 5;
				break;
			case REGULAR:
				type_symbol = ' ';
				color_pair_enabled = 1;
				break;
			case SOCKET:
				type_symbol = '=';
				color_pair_enabled = 5;
				break;
			case UNKNOWN:
			default:
				type_symbol = '?';
				color_pair_enabled = 1;
				break;
			}
			const int fnlen = strlen(cfr->file_name); // File Name Length
			int enlen; // entry length
			int padding;
			if (fnlen > pw - 3) {
				// If file name can't fit in line, its just cut
				padding = 0;
				enlen = pw - 3;
			}
			else {
				padding = (pw - 3) - fnlen;
				enlen = fnlen;
			}
			if (ei == s->selection && v == i->active_view) {
				color_pair_enabled += 1;
			}
			wattron(w, COLOR_PAIR(color_pair_enabled));
			mvwprintw(w, view_row, 1, "%c%.*s", type_symbol, enlen, cfr->file_name);
			if (padding) {
				mvwprintw(w, view_row, 1 + enlen + 1, "%*c", padding, ' ');
			}
			wattroff(w, COLOR_PAIR(color_pair_enabled));
			view_row += 1;
			ei += 1;
		}
		while (view_row < ph-1) {
			mvwprintw(w, view_row, 1, "%*c", pw-2, ' ');
			view_row += 1;
		}
		mvwprintw(w, view_row, 2, "files: %d", s->num_files);
		wrefresh(w);
	}
	update_panels();
	doupdate();
	refresh();
}

void ui_update_geometry(struct ui* const i) {
	/* Sometimes it's better to create a new window
	 * instead of resizing existing one.
	 * If you just resize window (to bigger size),
	 * borders stay in buffer and the only thing
	 * you can do about them is explicitly overwrite them by
	 * mvwprintw or something.
	 * I do overwrite all buffer with spaces, so I dont have to.
	 * ...but if you do, delete old window AFTER you assign new one to panel.
	 * All ncurses examples do so, so it's probably THE way to do it.
	 */
	int newscrh, newscrw;
	getmaxyx(stdscr, newscrh, newscrw);
	if (newscrh == i->scrh && newscrw == i->scrw) {
		return;
	}
	i->scrh = newscrh;
	i->scrw = newscrw;
	int w[2] = { i->scrw/2, i->scrw - w[0] };
	int px[2] = { 0, w[0] };
	for (int x = 0; x < 2; x++) {
		PANEL* p = i->fvp[x];
		WINDOW* ow = panel_window(p);
		wresize(ow, i->scrh-1, w[x]);
		wborder(ow, '|', '|', '-', '-', '+', '+', '+', '+');
		move_panel(p, 0, px[x]);
	}
	WINDOW* hw = panel_window(i->hint);
	wresize(hw, 1, i->scrw);
	move_panel(i->hint, i->scrh-1, 0);
}

void ui_prompt_open(struct ui* i, char* ptt, char* ptb, int ptbs) {
	int scrh, scrw;
	getmaxyx(stdscr, scrh, scrw);
	const int box_w = 50;
	const int box_h = 3;
	WINDOW* fw = newwin(box_h, box_w, 0, 0);
	PANEL* fp = new_panel(fw);
	move_panel(fp, (scrh-box_h)/2, (scrw-box_w)/2);
	wrefresh(fw);
	curs_set(2);
	//keypad(fw, TRUE);
	//wtimeout(fw, 100);
	box(fw, 0, 0);
	i->prompt = fp;
	i->prompt_title = ptt;
	i->prompt_textbox = ptb;
	i->prompt_textbox_size = ptbs;
}

void ui_prompt_close(struct ui* i) {
	curs_set(0);
	WINDOW* fw = panel_window(i->prompt);
	delwin(fw);
	del_panel(i->prompt);
	i->prompt = NULL;
}
