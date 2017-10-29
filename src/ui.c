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

struct ui ui_init(struct file_view* pv, struct file_view* sv) {
	struct ui i;
	setlocale(LC_ALL, "");
	initscr();
	if (has_colors() == FALSE) {
		endwin();
		printf("no colors :(\n");
		exit(1);
	}
	start_color();
	noecho();
	//cbreak();
	//nonl();
	//raw();
	//intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	timeout(DEFAULT_GETCH_TIMEOUT);
	curs_set(0);

	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_BLACK, COLOR_WHITE);
	init_pair(3, COLOR_CYAN, COLOR_BLACK);
	init_pair(4, COLOR_BLACK, COLOR_CYAN);
	init_pair(5, COLOR_RED, COLOR_BLACK);
	init_pair(6, COLOR_BLACK, COLOR_RED);
	init_pair(7, COLOR_YELLOW, COLOR_BLACK);
	init_pair(8, COLOR_BLACK, COLOR_YELLOW);

	i.fvs[0] = pv;
	i.fvs[1] = sv;
	for (int x = 0; x < 2; ++x) {
		WINDOW* tmpwin = newwin(1, 1, 0, 0);
		i.fvp[x] = new_panel(tmpwin);
	}
	i.scrw = i.scrh = 0;
	i.active_view = 0;
	i.m = MODE_MANAGER;
	i.prompt_textbox = NULL;
	i.prompt_textbox_top = NULL;
	i.prompt_textbox_size = 0;
	i.find = NULL;
	i.find_top = NULL;
	i.find_size = 0;
	i.find_init = 0;
	i.chmod_panel = NULL;
	i.chmod_mode = 0;
	i.chmod_path = NULL;
	WINDOW* hw = newwin(1, 1, 0, 0);
	keypad(hw, TRUE);
	wtimeout(hw, DEFAULT_GETCH_TIMEOUT);
	i.hint = new_panel(hw);
	i.kml = 0;
	while (key_mapping[i.kml].ks[0]) {
		i.kml += 1;
	}
	i.mks = calloc(i.kml, sizeof(int));
	//syslog(LOG_DEBUG, "UI initialized");
	return i;
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
		PANEL* p = i->fvp[x];
		WINDOW* w = panel_window(p);
		del_panel(p);
		delwin(w);
	}
	WINDOW* hw = panel_window(i->hint);
	del_panel(i->hint);
	delwin(hw);
	WINDOW* cw = panel_window(i->chmod_panel);
	del_panel(i->chmod_panel);
	delwin(cw);
	free(i->mks);
	endwin();
}

void ui_draw(struct ui* const i) {
	for (int v = 0; v < 2; ++v) {
		struct file_view* const s = i->fvs[v];
		PANEL* p = i->fvp[v];
		int _ph, _pw;
		WINDOW* w = panel_window(p);
		getmaxyx(w, _ph, _pw);
		if (_ph < 0 || _ph < 0) return; // these may be -1
		fnum_t ph = _ph, pw = _pw;
		box(w, 0, 0);
		//wborder(w, '|', '|', '-', '-', '+', '+', '+', '+');
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
		fnum_t view_offset = 0;
		if (s->num_files <= ph - 1) {
			view_offset = 0;
		}
		else if (s->selection < ph/2) {
			view_offset = 0;
		}
		else if (s->selection > s->num_files - ph/2) {
			view_offset = s->num_files - ph + 2;
		}
		else {
			view_offset = s->selection - ph/2 + 1;
		}
		unsigned view_row = 1; // Skipping border
		fnum_t ei = view_offset; // Entry Index
		fnum_t hc = 0; // Hidden Count
		fnum_t vi = 0;
		fnum_t vsi = 0; // Visible Selected Index
		while (ei < s->num_files && view_row < ph-1) {
			// Current File Record
			const struct file_record* cfr = s->file_list[ei];
			if (cfr->file_name[0] == '.' && !s->show_hidden) {
				ei += 1;
				hc += 1;
				continue;
			}
			char type_symbol = '?';
			int color_pair_enabled = 0;
			type_symbol = type_symbol_mapping[cfr->t][0];
			color_pair_enabled = type_symbol_mapping[cfr->t][1];
			const size_t fnlen = strlen(cfr->file_name); // File Name Length
			size_t enlen; // entry length
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
				vsi = vi;
				color_pair_enabled += 1;
			}
			wattron(w, COLOR_PAIR(color_pair_enabled));
			mvwprintw(w, view_row, 1, "%c%.*s",
					type_symbol, enlen, cfr->file_name);
			if (padding) {
				mvwprintw(w, view_row, 1 + enlen + 1, "%*c", padding, ' ');
			}
			wattroff(w, COLOR_PAIR(color_pair_enabled));
			view_row += 1;
			vi += 1;
			ei += 1;
		}
		while (view_row < ph-1) {
			mvwprintw(w, view_row, 1, "%*c", pw-2, ' ');
			view_row += 1;
		}
		if (s->num_files && s->num_files-hc) {
			const size_t fsize = (s->selection < s->num_files ?
					s->file_list[s->selection]->s.st_size : 0);
			if (s->show_hidden) {
				mvwprintw(w, view_row, 2, " %u/%u, %uB, %o ",
						vsi+1, s->num_files-hc,
						fsize, s->file_list[s->selection]->s.st_mode);
			}
			else {
				mvwprintw(w, view_row, 2, " %u/%u, h%d, %uB, %o ",
						vsi+1, s->num_files-hc, hc,
						fsize, s->file_list[s->selection]->s.st_mode);
			}
		}
		wrefresh(w);
	}
	WINDOW* hw = panel_window(i->hint);
	mvwprintw(hw, 0, 0, "%*c", i->scrw-1, ' ');
	wmove(hw, 0, 0);

	if (i->m == MODE_FIND) {
		mvwprintw(hw, 0, 1, "/%s", i->find);
	}
	else if (i->m == MODE_PROMPT) {
		mvwprintw(hw, 0, 1, "%s", i->prompt_textbox);
	}
	else {
		for (int x = 0; x < i->kml; ++x) {
			if (!i->mks[x]) continue;
			int c = 0;
			wprintw(hw, " ");
			int k;
			while ((k = key_mapping[x].ks[c])) {
				switch (k) {
				case '\t':
					wprintw(hw, "TAB");
					break;
				default:
					wprintw(hw, "%c", key_mapping[x].ks[c]);
					break;
				}
				c += 1;
			}
			wattron(hw, COLOR_PAIR(4));
			wprintw(hw, "%s", key_mapping[x].d);
			wattroff(hw, COLOR_PAIR(4));
		}
	}
	wrefresh(hw);

	if (i->chmod_panel) {
		WINDOW* cw = panel_window(i->chmod_panel);
		box(cw, 0, 0);
		//wborder(cw, '|', '|', '-', '-', '+', '+', '+', '+');
		mode_t m = i->chmod_mode;
		struct passwd* pwd = get_pwd();
		struct group* grp = getgrgid(pwd->pw_gid);
		if (grp) {
			mvwprintw(cw, 1, 2, "%o %s %s", m, pwd->pw_name, grp->gr_name);
		}
		for (int i = 0; i < 12; ++i) {
			char s = (m & (mode_t)1 ? 'x' : ' ');
			mvwprintw(cw, 13-i, 2, "[%c] %s", s, mode_bit_meaning[i]);
			m >>= 1;
		}
		wrefresh(cw);
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
	for (int x = 0; x < 2; ++x) {
		PANEL* p = i->fvp[x];
		WINDOW* ow = panel_window(p);
		wresize(ow, i->scrh-1, w[x]);
		wborder(ow, '|', '|', '-', '-', '+', '+', '+', '+');
		move_panel(p, 0, px[x]);
	}

	WINDOW* hw = panel_window(i->hint);
	wresize(hw, 1, i->scrw);
	move_panel(i->hint, i->scrh-1, 0);

	if (i->chmod_panel) {
		const int cpw = 40;
		const int cph = 15;
		WINDOW* cw = panel_window(i->chmod_panel);
		PANEL* cp = i->chmod_panel;
		wresize(cw, cph, cpw);
		move_panel(cp, (i->scrh-cph)/2, (i->scrw-cpw)/2);
	}
}

void chmod_open(struct ui* i, char* path, mode_t m) {
	//syslog(LOG_DEBUG, "chmod %s", path);
	i->chmod_path = path;
	i->chmod_mode = m;
	WINDOW* cw = newwin(1, 1, 0, 0);
	PANEL* cp = new_panel(cw);
	i->chmod_panel = cp;
	i->m = MODE_CHMOD;
	i->scrw = i->scrh = 0; // Forces geometry update
}

void chmod_close(struct ui* i, enum mode m) {
	WINDOW* cw = panel_window(i->chmod_panel);
	del_panel(i->chmod_panel);
	delwin(cw);
	i->chmod_panel = NULL;
	free(i->chmod_path);
	i->chmod_path = NULL;
	i->chmod_mode = 0;
	i->m = m;
}

enum command get_cmd(struct ui* i) {
	static int keyseq[MAX_KEYSEQ_LENGTH] = { 0 };
	static int ksi = 0;
	int c = getch();
	//syslog(LOG_DEBUG, "%d, (%d) %d %d %d %d",
			//c, ksi, keyseq[0], keyseq[1], keyseq[2], keyseq[3]);

	if (c == -1 && !ksi) return CMD_NONE;
	if (c == 27) {
		//syslog(LOG_DEBUG, "esc?");
		//timeout();
		int cc = getch();
		//timeout(DEFAULT_GETCH_TIMEOUT);
		if (cc == -1) {
			//syslog(LOG_DEBUG, "esc");
			memset(i->mks, 0, i->kml*sizeof(int));
			memset(keyseq, 0, sizeof(keyseq));
			ksi = 0;
			return CMD_NONE;
		}
		else {
			//syslog(LOG_DEBUG, "alt");
			return CMD_NONE;
		}
	}
	if (c != -1 || !ksi) {
		keyseq[ksi] = c;
		ksi += 1;
	}

	memset(i->mks, 0, i->kml*sizeof(int));
	for (int c = 0; c < i->kml; c++) {
		if (key_mapping[c].m != i->m) continue;
		int s = 0;
		while (keyseq[s]) {
			/* mks[c] will contain length of matching sequence
			 * mks[c] will be zeroed if sequence broken at any point
			 */
			if (key_mapping[c].ks[s] == keyseq[s]) {
				i->mks[c] += 1;
			}
			else {
				i->mks[c] = 0;
				break;
			}
			s += 1;
		}
	}

	bool match = false; // At least one match
	for (int c = 0; c < i->kml; c++) {
		match = match || i->mks[c];
	}

	enum command cmd = CMD_NONE;
	if (match) {
		for (int c = 0; c < i->kml; c++) {
			int ksl = 0;
			for (int s = 0; key_mapping[c].ks[s]; s++) {
				ksl += 1;
			}
			if (i->mks[c] == ksl) {
				cmd = key_mapping[c].c;
			}
		}
	}
	if (!match || ksi == MAX_KEYSEQ_LENGTH || cmd != CMD_NONE) {
		memset(keyseq, 0, sizeof(keyseq));
		ksi = 0;
	}
	return cmd;
}

/* Gets input to buffer
 * Responsible for cursor movement (buftop) and guarding buffer bounds (bsize)
 * If text is ready (enter pressed) returns 0,
 * If aborted, returns -1.
 * If keeps gathering, returns 1.
 */
int fill_textbox(char* buf, char** buftop, size_t bsize, WINDOW* w) {
	curs_set(2);
	wmove(w, 1, (*buftop)-buf+1);
	wrefresh(w);
	int c = wgetch(w);
	curs_set(0);
	if (c == -1) return 1;
	else if (c == 27) {
		// evil ESC
		return -1;
	}
	else if (c == '\n' || c == '\r') return 0;
	else if (c == KEY_BACKSPACE) {
		if (*buftop - buf > 0) {
			**buftop = 0;
			*buftop -= 1;
			**buftop = 0;
		}
		else return 0;
	}
	else if ((size_t)(*buftop - buf) < bsize) {
		**buftop = c;
		*buftop += 1;
	}
	return 1;
}
