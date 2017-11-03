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

static int mode2type(mode_t m) {
	// TODO find a better way
	// See sys_stat.h manpage for more info
	switch (m & S_IFMT) {
	case S_IFBLK: return 0;
	case S_IFCHR: return 1;
	case S_IFIFO: return 2;
	case S_IFREG: return 3;
	case S_IFDIR: return 4;
	case S_IFLNK: return 5;
	case S_IFSOCK: return 6;
	default: return 7;
	}
}

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
	//timeout(DEFAULT_GETCH_TIMEOUT);
	curs_set(0);

	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_BLACK, COLOR_WHITE);

	init_pair(3, COLOR_RED, COLOR_BLACK);
	init_pair(4, COLOR_BLACK, COLOR_RED);

	init_pair(5, COLOR_GREEN, COLOR_BLACK);
	init_pair(6, COLOR_BLACK, COLOR_GREEN);

	init_pair(7, COLOR_BLUE, COLOR_BLACK);
	init_pair(8, COLOR_BLACK, COLOR_BLUE);

	init_pair(9, COLOR_CYAN, COLOR_BLACK);
	init_pair(10, COLOR_BLACK, COLOR_CYAN);

	init_pair(11, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(12, COLOR_BLACK, COLOR_MAGENTA);

	init_pair(13, COLOR_YELLOW, COLOR_BLACK);
	init_pair(14, COLOR_BLACK, COLOR_YELLOW);

	i.fvs[0] = pv;
	i.fvs[1] = sv;
	for (int x = 0; x < 2; ++x) {
		WINDOW* tmpwin = newwin(1, 1, 0, 0);
		i.fvp[x] = new_panel(tmpwin);
	}
	i.scrw = i.scrh = 0;
	i.active_view = 0;
	i.m = MODE_MANAGER;
	i.prompt = NULL;
	i.chmod = NULL;
	i.find = NULL;
	i.error = NULL;
	WINDOW* sw = newwin(1, 1, 0, 0);
	keypad(sw, TRUE);
	//wtimeout(hw, DEFAULT_GETCH_TIMEOUT);
	i.status = new_panel(sw);
	i.kml = 0;
	while (key_mapping[i.kml].ks[0]) {
		i.kml += 1;
	}
	i.mks = calloc(i.kml, sizeof(int));
	return i;
}

void ui_end(struct ui* const i) {
	/* Delete panel first, THEN it's window.
	 * If you resize the terminal window at least once and then quit (q)
	 * then the program will hang in infinite loop
	 * (I attached to it via gdb and it was stuck either on del_panel or poll.)
	 * or segfault/double free/corrupted unsorted chunks.
	 */
	for (int x = 0; x < 2; x++) {
		PANEL* p = i->fvp[x];
		WINDOW* w = panel_window(p);
		del_panel(p);
		delwin(w);
	}
	WINDOW* sw = panel_window(i->status);
	del_panel(i->status);
	delwin(sw);
	free(i->mks);
	endwin();
}

void ui_draw(struct ui* const i) {
	for (int v = 0; v < 2; ++v) {
		const struct file_view* const s = i->fvs[v];
		const bool sh = s->show_hidden;
		WINDOW* const w = panel_window(i->fvp[v]);
		int _ph, _pw;
		getmaxyx(w, _ph, _pw);
		if (_ph < 0 || _ph < 0) return; // these may be -1
		const fnum_t ph = _ph, pw = _pw;
		//box(w, 0, 0);
		//wborder(w, '|', '|', '-', '-', '+', '+', '+', '+');

		/* Top pathbar */
		struct passwd* pwd = get_pwd();
		const int pi = prettify_path_i(s->wd, pwd->pw_dir);
		/* TODO if path does not fit into bar, shorten it somehow */
		wattron(w, COLOR_PAIR(2));
		if (pi) mvwprintw(w, 0, 0, "~");
		mvwprintw(w, 0, (pi ? 1 : 0), "%s%*c", s->wd+pi, pw-strlen(s->wd+pi), ' ');
		wattroff(w, COLOR_PAIR(2));

		/* Entry list */
		/* I'm drawing N entries before selection,
		 * selection itself
		 * and as many entries after selection as needed to fill remaining space.
		 */
		fnum_t me = ph - 3; // Max Entries
		// 3 = 1 path bar + 1 statusbar + 1 infobar
		fnum_t eo = 0; // Entries Over
		fnum_t oi = 1; // Over Index
		fnum_t bi = 0; // Begin Index
		fnum_t eu = 0; // Entries Under
		fnum_t ui = 1; // Under Index
		/* How many entries are under selection? */
		while (s->selection+ui < s->num_files && eu <= me/2) {
			if (sh || ifaiv(s, s->selection+ui)) {
				eu += 1;
			}
			ui += 1;
		}
		/* How many entries are over selection?
		 * (If there are few entries under, then use up all remaining space)
		 */
		while (s->selection >= oi && eo + 1 + eu <= me) {
			if (sh || ifaiv(s, s->selection-oi)) {
				eo += 1;
			}
			bi = s->selection-oi;
			oi += 1;
		}

		fnum_t dr = 1; // Drawing Row
		fnum_t nhf = 0; // Number of Hidden Files
		fnum_t hi = 0; // Highlighted file Index
		fnum_t e = bi;
		for (fnum_t i = 0; i < s->num_files && !sh; ++i) {
			if (!ifaiv(s, i)) nhf += 1;
			if (i == s->selection) hi = i-nhf;
		}
		while (e < s->num_files && dr < ph-1) {
			if (!sh && !ifaiv(s, e)) {
				e += 1;
				continue;
			}
			const struct file_record* const cfr = s->file_list[e];
			const int type = mode2type(cfr->s.st_mode);
			const char type_symbol = type_symbol_mapping[type][0];
			int color_pair_enabled = type_symbol_mapping[type][1];
			const size_t fnlen = strlen(cfr->file_name); // File Name LENgth
			size_t enlen; // ENtry LENgth
			int padding;
			if (fnlen > pw - 3) {
				// If file name can't fit in line, its just cut
				padding = 0;
				enlen = pw - 1; // 1 is for type symbol
			}
			else {
				padding = (pw - 1) - fnlen;
				enlen = fnlen + 1;
			}
			if (e == s->selection && v == i->active_view) {
				color_pair_enabled += 1;
			}
			wattron(w, COLOR_PAIR(color_pair_enabled));
			mvwprintw(w, dr, 0, "%c%.*s", type_symbol, enlen, cfr->file_name);
			if (padding) {
				mvwprintw(w, dr, enlen, "%*c", padding, ' ');
			}
			wattroff(w, COLOR_PAIR(color_pair_enabled));
			dr += 1;
			e += 1;
		}
		for (; dr < ph-1; ++dr) {
			mvwprintw(w, dr, 0, "%*c", pw, ' ');
		}

		/* Infobar */
		wattron(w, COLOR_PAIR(2));
		const size_t status_size = 256;
		char* status = malloc(status_size);
		const size_t time_size = 64;
		char* time = malloc(64);
		static char* empty = "(empty)";
		if (!s->num_files) {
			snprintf(status, status_size, empty);
		}
		else if (s->num_files - nhf) {
			const size_t fsize = (s->selection < s->num_files ?
					s->file_list[s->selection]->s.st_size : 0);
			time_t lt = s->file_list[s->selection]->s.st_mtim.tv_sec;
			struct tm* tt = localtime(&lt);
			strftime(time, time_size, "%Y-%m-%d %H:%M", tt);
			if (s->show_hidden) {
				snprintf(status, status_size, "%u/%u %o %zuB",
						s->selection+1, s->num_files,
						s->file_list[s->selection]->s.st_mode & 0xfff, fsize);
			}
			else {
				snprintf(status, status_size, "%u/%u h%u %o %zuB",
						hi+1, s->num_files-nhf, nhf,
						s->file_list[s->selection]->s.st_mode & 0xfff, fsize);
			}
		}
		else {
			snprintf(status, status_size, "h%u", nhf);
		}
		mvwprintw(w, dr, 0, " %s%*c%s ", status,
				pw-strlen(status)-strlen(time)-2, ' ', time);
		wattroff(w, COLOR_PAIR(2));
		free(time);
		free(status);
		wrefresh(w);
	}

	WINDOW* sw = panel_window(i->status);
	if (i->error) {
		wattron(sw, COLOR_PAIR(4));
		mvwprintw(sw, 0, 0, "%s%*c", i->error,
				i->scrw-(strlen(i->error)+1), ' ');
		wattroff(sw, COLOR_PAIR(4));
	}
	else if (i->find) {
		mvwprintw(sw, 0, 0, "/%s%*c", i->find->t,
				i->scrw-(strlen(i->find->t)+1), ' ');
	}
	else if (i->prompt) {
		mvwprintw(sw, 0, 0, "%s%*c", i->prompt->tb,
				i->scrw-(strlen(i->prompt->tb)+2), ' ');
	}
	else {
		mvwprintw(sw, 0, 0, "%*c", i->scrw-1, ' ');
		wmove(sw, 0, 0);
		for (int x = 0; x < i->kml; ++x) {
			if (!i->mks[x]) continue;
			int c = 0;
			wprintw(sw, " ");
			int k;
			while ((k = key_mapping[x].ks[c])) {
				switch (k) {
				case '\t':
					wprintw(sw, "TAB");
					break;
				default:
					wprintw(sw, "%c", key_mapping[x].ks[c]);
					break;
				}
				c += 1;
			}
			wattron(sw, COLOR_PAIR(2));
			wprintw(sw, "%s", key_mapping[x].d);
			wattroff(sw, COLOR_PAIR(2));
		}
	}
	wrefresh(sw);

	if (i->chmod) {
		WINDOW* cw = panel_window(i->chmod->p);
		box(cw, 0, 0);
		mode_t m = i->chmod->m;
		// fno = File Name Offset (in path)
		const size_t fno = current_dir_i(i->chmod->path);
		// TODO what if filename is like 230 characters?
		mvwprintw(cw, 1, 2, "file: %s%*c", i->chmod->path+fno,
				i->chmod->ww-(strlen(i->chmod->path+fno)+6+2+1), ' ');
		mvwprintw(cw, 2, 2, "permissions: %o", m);
		mvwprintw(cw, 3, 2, "owner: %s%*c", i->chmod->owner,
				// 7 = "owner: ", 2 = x offset + 1 for border
				i->chmod->ww-(strlen(i->chmod->owner)+7+2+1), ' ');
		mvwprintw(cw, 4, 2, "group: %s%*c", i->chmod->group,
				i->chmod->ww-(strlen(i->chmod->group)+7+2+1), ' ');
		for (int b = 0; b < 12; ++b) {
			char s = (m & (mode_t)1 ? 'x' : ' ');
			mvwprintw(cw, i->chmod->wh-2-b, 2, "[%c] %s", s, mode_bit_meaning[b]);
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
	 */
	int newscrh, newscrw;
	getmaxyx(stdscr, newscrh, newscrw);
	if (newscrh == i->scrh && newscrw == i->scrw) return;
	i->scrh = newscrh;
	i->scrw = newscrw;
	int w[2] = { i->scrw/2, i->scrw - w[0] };
	int px[2] = { 0, w[0] };
	for (int x = 0; x < 2; ++x) {
		PANEL* p = i->fvp[x];
		WINDOW* ow = panel_window(p);
		wresize(ow, i->scrh-1, w[x]);
		move_panel(p, 0, px[x]);
	}

	WINDOW* sw = panel_window(i->status);
	wresize(sw, 1, i->scrw);
	move_panel(i->status, i->scrh-1, 0);

	if (i->chmod) {
		WINDOW* cw = panel_window(i->chmod->p);
		PANEL* cp = i->chmod->p;
		wresize(cw, i->chmod->wh, i->chmod->ww);
		move_panel(cp, (i->scrh-i->chmod->wh)/2, (i->scrw-i->chmod->ww)/2);
	}
}

void chmod_open(struct ui* i, char* path, mode_t m) {
	i->chmod = malloc(sizeof(struct ui_chmod));
	i->chmod->mb = i->m;
	i->chmod->path = path;
	i->chmod->m = m;
	i->chmod->tmp = NULL;
	WINDOW* cw = newwin(1, 1, 0, 0);
	i->chmod->p =new_panel(cw);
	i->m = MODE_CHMOD;
	struct stat s;
	int r = lstat(i->chmod->path, &s); // TODO handle stat errors
	i->chmod->o = s.st_uid;
	i->chmod->g = s.st_gid;
	struct passwd* pwd = getpwuid(s.st_uid);
	struct group* grp = getgrgid(s.st_gid);
	strcpy(i->chmod->owner, pwd->pw_name);
	strcpy(i->chmod->group, grp->gr_name);
	/* TODO should I care about smaller terminal?
	 * What does the standard say?
	 */
	i->chmod->ww = 34;
	i->chmod->wh = 18;
	i->scrw = i->scrh = 0; // Forces geometry update
}

void chmod_close(struct ui* i) {
	i->m = i->chmod->mb;
	WINDOW* cw = panel_window(i->chmod->p);
	del_panel(i->chmod->p);
	delwin(cw);
	if (i->chmod->tmp) free(i->chmod->tmp);
	free(i->chmod->path);
	free(i->chmod);
	i->chmod = NULL;
	i->scrw = i->scrh = 0; // Forces geometry update
}

void prompt_open(struct ui* i, char* tb, char* tb_top, size_t tb_size) {
	i->prompt = malloc(sizeof(struct ui_prompt));
	i->prompt->mb = i->m;
	i->prompt->tb = tb;
	i->prompt->tb_top = tb_top;
	i->prompt->tb_size = tb_size;
	i->m = MODE_PROMPT;
}

void prompt_close(struct ui* i) {
	i->m = i->prompt->mb;
	//free(i->prompt->tb);
	free(i->prompt);
	i->prompt = NULL;
}

void find_open(struct ui* i, char* t, char* t_top, size_t t_size) {
	i->find = malloc(sizeof(struct ui_find));
	i->find->mb = i->m;
	i->find->sbfc = i->fvs[i->active_view]->selection;
	i->find->t = t;
	i->find->t_top = t_top;
	i->find->t_size = t_size;
	i->m = MODE_FIND;
}

void find_close(struct ui* i, bool success) {
	if (!success) i->fvs[i->active_view]->selection = i->find->sbfc;
	i->m = i->find->mb;
	free(i->find->t);
	free(i->find);
	i->find = NULL;
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
int fill_textbox(char* buf, char** buftop, size_t bsize, int coff, WINDOW* w) {
	curs_set(2);
	wmove(w, 0, (*buftop)-buf+coff);
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
