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

/* This file contains UI-related functions
 * These functions are supposed to draw elements of UI.
 * They are supposed to read file_view contents, but never modify it.
 */

/* get cmd2help coresponding to given command */
static const struct cmd2help* get_help_data(const enum command c) {
	for (size_t i = 0; i < CMD_NUM && i < cmd_help_length; ++i) {
		if (cmd_help[i].c == c) return &cmd_help[i];
	}
	return NULL;
}

static int mode2type(const mode_t m, const mode_t n) {
	// TODO find a better way
	// See sys_stat.h manpage for more info
	switch (m & S_IFMT) {
	case S_IFBLK: return 0;
	case S_IFCHR: return 1;
	case S_IFIFO: return 2;
	case S_IFREG: return 3;
	case S_IFDIR: return 4;
	case S_IFSOCK: return 5;
	case S_IFLNK:
		switch (n & S_IFMT) {
		case S_IFBLK:
		case S_IFCHR:
		case S_IFIFO:
		case S_IFREG:
		case S_IFSOCK: return 7;
		case S_IFDIR: return 6;
		default: return 8;
		}
	default: return 8;
	}
}

struct ui ui_init(struct file_view* const pv,
		struct file_view* const sv) {
	struct ui i;
	setlocale(LC_ALL, "");
	initscr();
	if (has_colors() == FALSE) {
		endwin();
		printf("no colors :(\n"); // TODO
		exit(EXIT_FAILURE);
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

	i.fvs[0] = i.pv = pv;
	i.fvs[1] = i.sv = sv;
	for (int x = 0; x < 2; ++x) {
		WINDOW* tmpwin = newwin(1, 1, 0, 0);
		i.fvp[x] = new_panel(tmpwin);
	}
	i.scrw = i.scrh = 0;
	i.m = MODE_MANAGER;
	i.prompt = NULL;
	i.chmod = NULL;
	i.find = NULL;
	i.error[0] = i.info[0] = 0;
	i.helpy = 0;
	i.help = NULL;
	i.run = true;
	i.kml = default_mapping_length;
	i.mks = calloc(default_mapping_length, sizeof(int));
	i.kmap = malloc(default_mapping_length*sizeof(struct input2cmd));
	// TODO
	for (size_t k = 0; k < default_mapping_length; ++k) {
		i.kmap[k].m = default_mapping[k].m;
		i.kmap[k].c = default_mapping[k].c;
		for (int il = 0; il < INPUT_LIST_LENGTH; ++il) {
			i.kmap[k].i[il].t = default_mapping[k].i[il].t;
			switch (i.kmap[k].i[il].t) {
			case END: break;
			case UTF8:
				strncpy(i.kmap[k].i[il].utf, default_mapping[k].i[il].utf, 5);
				break;
			case SPECIAL:
				i.kmap[k].i[il].c = default_mapping[k].i[il].c;
				break;
			case CTRL:
				i.kmap[k].i[il].ctrl = default_mapping[k].i[il].ctrl;
				break;
			}
		}
	}
	WINDOW* sw = newwin(1, 1, 0, 0);
	keypad(sw, TRUE);
	//wtimeout(hw, DEFAULT_GETCH_TIMEOUT);
	i.status = new_panel(sw);
	return i;
}

/* Delete panel first, THEN it's window.
 * If you resize the terminal window at least once and then quit (q)
 * then the program will hang in infinite loop
 * (I attached to it via gdb and it was stuck either on del_panel or poll.)
 * or segfault/double free/corrupted unsorted chunks.
 */
void ui_end(struct ui* const i) {
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

static void _printw_pathbar(const char* const path,
		WINDOW* const w, fnum_t width) {
	const struct passwd* const pwd = get_pwd();
	const int pi = prettify_path_i(path, pwd->pw_dir);
	const size_t path_width = utf8_width(path+pi) + (pi ? 1 : 0);
	wattron(w, COLOR_PAIR(2));
	if (path_width <= width-2) {
		mvwprintw(w, 0, 0, "%s%s%*c ",
				(pi ? " ~" : " "),
				path+pi, width-utf8_width(path+pi), ' ');
	}
	else {
		const size_t sg = path_width - (width-2) - (pi?1:0);
		mvwprintw(w, 0, 0, " %s ",
				path+pi+utf8_slice_length(path+pi, sg));
	}
	wattroff(w, COLOR_PAIR(2));
}

static void _printw_entry(WINDOW* const w, const fnum_t dr,
		const struct file_record* const cfr,
		const fnum_t width, const bool highlight) {
	// TODO is is utf-8 at all? validate
	const int type = mode2type(cfr->s.st_mode, cfr->l->st_mode);
	const char type_symbol = type_symbol_mapping[type][0];
	int color_pair_enabled = type_symbol_mapping[type][1];
	const size_t bufl = width*4;
	char* const buf = malloc(bufl);
	memset(buf, ' ', bufl);
	buf[bufl-1] = 0;
	buf[strlen(buf)] = ' ';

	if (highlight) {
		color_pair_enabled += 1;
	}
	wattron(w, COLOR_PAIR(color_pair_enabled));
	fnum_t space = width;
	fnum_t begin = 0;
	mvwprintw(w, dr, begin, "%c", type_symbol);
	space -= 1;
	begin += 1;

	const utf8* const fn = cfr->file_name;
	const utf8* const lp = cfr->link_path;

	const size_t fnw = utf8_width(fn);
	const size_t printfn = (space > fnw ? fnw : space);
	mvwprintw(w, dr, begin, "%.*s", utf8_slice_length(fn, printfn), fn);
	space -= printfn;
	begin += printfn;

	if (!highlight) {
		// TODO what if cfr->l == NULL?
		if (S_ISDIR(cfr->l->st_mode)) wattron(w, COLOR_PAIR(1));
		else wattron(w, COLOR_PAIR(9));
	}
	if ((cfr->s.st_mode & S_IFMT) == S_IFLNK) {
		const size_t lpw = utf8_width(lp);
		const size_t printlp = (space > 4+lpw ? 4+lpw : space);
		// TODO what if link path is corrupt?
		mvwprintw(w, dr, begin, " -> %.*s",
				utf8_slice_length(lp, printlp), lp);
		space -= printlp;
		begin += printlp;
	}
	if (!highlight) {
		if (S_ISDIR(cfr->l->st_mode)) wattroff(w, COLOR_PAIR(1));
		else wattroff(w, COLOR_PAIR(9));
	}
	mvwprintw(w, dr, begin, "%*c", space, ' ');
	wattroff(w, COLOR_PAIR(color_pair_enabled));
	free(buf);
}

/* I'm drawing N entries before selection,
 * selection itself
 * and as many entries after selection as needed
 * to fill remaining space. */

/* - Max Entries = how many entries I need to fill
 * all available space in file view
 * (selection excluded - it's always drawn)
 *
 * me = panel height - 1 for path bar, 1 for selection, 1 for info bar
 *
 * - Entries Over = how many entries are over selection
 * - Entries Under = how many entries are under selection
 *
 * - Begin Index = from which index should I start looking for
 * visible entries to catch all I need
 *
 * - Over Index = iterator; an index offset relative from selection,
 *   selection+oi = effective index
 * - Under Index = iterator; an index offset relative from selection,
 *   selection-ui = effective index
 */
static void ui_draw_panel(struct ui* const i, const int v) {
	// TODO make readable
	const struct file_view* const s = i->fvs[v];
	const bool sh = s->show_hidden;
	WINDOW* const w = panel_window(i->fvp[v]);
	int _ph = 0, _pw = 0;
	getmaxyx(w, _ph, _pw);
	if (_ph < 0 || _ph < 0) return; // these may be -1
	const fnum_t ph = _ph, pw = _pw;

	/* Top pathbar */
	_printw_pathbar(s->wd, w, pw);

	/* Entry list */
	fnum_t nhf = 0; // Number of Hidden Files
	fnum_t nsl = 0; // Number of SymLinks
	fnum_t hi = 0; // Highlighted file Index
	bool sisl = false; // Selected Is SymLink
	const struct file_record *hfr = NULL; // Highlighted File Record
	for (fnum_t i = 0; i < s->num_files; ++i) {
		if (hidden(s, i)) nhf += 1;
		const bool sl = S_ISLNK(s->file_list[i]->s.st_mode);
		if (sl) nsl += 1;
		if (i == s->selection) {
			hi = i-nhf;
			sisl = sl;
			hfr = s->file_list[i];
		}
	}

	fnum_t me = ph - 3; // Max Entries
	// 3 = 1 path bar + 1 statusbar + 1 infobar
	fnum_t eo = 0; // Entries Over
	fnum_t oi = 1; // Over Index
	fnum_t bi = 0; // Begin Index
	fnum_t eu = 0; // Entries Under
	fnum_t ui = 1; // Under Index
	/* How many entries are under selection? */
	while (s->num_files-nhf && s->selection+ui < s->num_files && eu <= me/2) {
		if (visible(s, s->selection+ui)) eu += 1;
		ui += 1;
	}
	/* How many entries are over selection?
	 * (If there are few entries under, then use up all remaining space)
	 */
	while (s->num_files-nhf && s->selection >= oi && eo + 1 + eu <= me) {
		if (visible(s, s->selection-oi)) eo += 1;
		bi = s->selection-oi;
		oi += 1;
	}

	fnum_t dr = 1; // Drawing Row
	fnum_t e = bi;
	while (s->num_files-nhf && e < s->num_files && dr < ph-1) {
		if (!visible(s, e)) {
			e += 1;
			continue;
		}
		bool hl = (e == s->selection && i->fvs[v] == i->pv);
		_printw_entry(w, dr, s->file_list[e], pw, hl);
		dr += 1;
		e += 1;
	}
	for (; dr < ph-1; ++dr) {
		mvwprintw(w, dr, 0, "%*c", pw, ' ');
	}

	/* Infobar */
	/* Padded on both sides with space */
	wattron(w, COLOR_PAIR(2));
	const size_t status_size = pw*4; // TODO
	char* const status = malloc(status_size);
	static const char* const timefmt = "%Y-%m-%d %H:%M";
	const size_t time_size = 4+1+2+1+2+1+2+1+2+1;
	char time[time_size]; // TODO
	memset(time, 0, time_size);

	off_t fsize = 0;
	if (hfr) {
		fsize = (s->selection < s->num_files ? hfr->l->st_size : 0);
		const time_t lt = hfr->l->st_mtim.tv_sec;
		const struct tm* const tt = localtime(&lt);
		strftime(time, time_size, timefmt, tt);
	}
	snprintf(status, status_size,
			"%u/%u %c%u %c%u hL%lu, %o %zuB",
			(s->num_files ? hi+1 : 0),
			s->num_files-(sh ? 0 : nhf),
			(sh ? 'H' : 'h'), nhf,
			(sisl ? 'L' : 'l'), nsl,
			(hfr ? hfr->l->st_nlink : 0),
			(hfr ? (hfr->l->st_mode & 0xfff) : 0),
			fsize);
	/* BTW chmod man page says chmod
	 * cannot change symlink permissions.
	 * ...but that is not and issue since
	 * symlinks permissions are never used.
	 */

	mvwprintw(w, dr, 0, " %s%*c%s ", status,
			pw-utf8_width(status)-strnlen(time, time_size)-2, ' ', time);
	wattroff(w, COLOR_PAIR(2));
	free(status);
	wrefresh(w);
}

static void ui_draw_help(struct ui* const i) {
	WINDOW* hw = panel_window(i->help);
	int hheight = i->scrh-1;
	int lines = 4*2 + cmd_help_length - 1; // TODO
	int dr = -i->helpy;
	if (dr + lines < hheight) {
		dr = hheight - lines;
		i->helpy = -dr;
	}
	for (size_t m = 0; m < MODE_NUM; ++m) {
		wattron(hw, A_BOLD);
		switch (m) {
		case MODE_HELP:
			mvwprintw(hw, dr, 0, "HELP SCREEN%*c", i->scrw-4, ' ');
			break;
		case MODE_CHMOD:
			mvwprintw(hw, dr, 0, "CHMOD%*c", i->scrw-5, ' ');
			break;
		case MODE_MANAGER:
			mvwprintw(hw, dr, 0, "FILE VIEW%*c", i->scrw-7, ' ');
			break;
		case MODE_WAIT:
			mvwprintw(hw, dr, 0, "WAIT%*c", i->scrw-7, ' ');
			break;
		default: continue;
		}
		wattroff(hw, A_BOLD);
		dr += 1;
		for (size_t c = 1; c < CMD_NUM; ++c) {
			size_t w = 0;
			int last = -1;
			for (size_t k = 0; k < i->kml; ++k) {
				if (i->kmap[k].c != c || i->kmap[k].m != m) continue;
				int ks = 0;
				last = k;
				const int cp = 9; // TODO
				wattron(hw, COLOR_PAIR(cp));
				wattron(hw, A_BOLD);
				int ww = 0;
				while (i->kmap[k].i[ks].t != END) {
					switch (i->kmap[k].i[ks].t) {
					case UTF8:
						mvwprintw(hw, dr, w, "%s", i->kmap[k].i[ks].utf);
						w += utf8_width(i->kmap[k].i[ks].utf);
						ww += utf8_width(i->kmap[k].i[ks].utf);
						break;
					case CTRL:
						mvwprintw(hw, dr, w, "^%c", i->kmap[k].i[ks].ctrl);
						w += 2;
						ww += 2;
						break;
					case SPECIAL:
						mvwprintw(hw, dr, w, "%s",
								keyname(i->kmap[k].i[ks].c)+4);
						w += strlen(keyname(i->kmap[k].i[ks].c)+4);
						ww += strlen(keyname(i->kmap[k].i[ks].c)+4);
						break;
					default: break;
					}
					ks += 1;
				}
				wattroff(hw, COLOR_PAIR(cp));
				wattroff(hw, A_BOLD);
				const int tab = INPUT_LIST_LENGTH+2;
				mvwprintw(hw, dr, w, "%*c", tab-ww, ' ');
				w += tab-ww;
			}
			if (last != -1) { // -1 suggests that there was no matching command
				const int tab = (INPUT_LIST_LENGTH+2)*(INPUT_LIST_LENGTH-1);
				utf8* help = get_help_data(i->kmap[last].c)->help;
				mvwprintw(hw, dr, w, "%*c%s%*c",
						tab-w+1, ' ', help,
						i->scrw-w-strlen(help), ' ');
				dr += 1;
			}
		}
		mvwprintw(hw, dr, 0, "%*c", i->scrw, ' ');
		dr += 1;
	}
	wrefresh(hw);
}

static void ui_draw_hintbar(struct ui* const i, WINDOW* const sw) {
	mvwprintw(sw, 0, 0, "%*c", i->scrw-1, ' ');
	wmove(sw, 0, 0);
	for (size_t x = 0; x < i->kml; ++x) {
		if (!i->mks[x]) continue;
		wprintw(sw, " ");
		const struct input* const ins = i->kmap[x].i;
		int c = 0;
		while (ins[c].t != END && c < INPUT_LIST_LENGTH) {
			switch (ins[c].t) {
			case END: break;
			case UTF8:
				wprintw(sw, "%s", ins[c].utf);
				break;
			case SPECIAL:
				wprintw(sw, "%s", keyname(ins[c].c)+4);
				break;
			case CTRL:
				wprintw(sw, "^%c", ins[c].ctrl);
				break;
			default:
				break;
			}
			c += 1;
		}
		wattron(sw, COLOR_PAIR(2));
		const struct cmd2help* ch = get_help_data(i->kmap[x].c);
		wprintw(sw, "%s", (ch ? ch->hint : "(no hint)"));
		wattroff(sw, COLOR_PAIR(2));
	}
}

static void ui_draw_chmod(struct ui* const i) {
	WINDOW* cw = panel_window(i->chmod->p);
	box(cw, 0, 0);
	mode_t m = i->chmod->m;
	mvwprintw(cw, 1, 2, "permissions: %o", m);
	mvwprintw(cw, 2, 2, "owner: %s%*c", i->chmod->owner,
			// 7 = "owner: ", 2 = x offset + 1 for border
			i->chmod->ww-(utf8_width(i->chmod->owner)+7+2+1), ' ');
	mvwprintw(cw, 3, 2, "group: %s%*c", i->chmod->group,
			i->chmod->ww-(utf8_width(i->chmod->group)+7+2+1), ' ');
	for (int b = 0; b < 12; ++b) {
		char s = (m & (mode_t)1 ? 'x' : ' ');
		mvwprintw(cw, i->chmod->wh-2-b, 2, "[%c] %s",
				s, mode_bit_meaning[b]);
		m >>= 1;
	}
	wrefresh(cw);
}

void ui_draw(struct ui* const i) {
	switch (i->m) {
	case MODE_HELP:
		ui_draw_help(i);
		break;
	case MODE_CHMOD:
		ui_draw_chmod(i);
		break;
	case MODE_MANAGER:
		for (int v = 0; v < 2; ++v) {
			ui_draw_panel(i, v);
		}
		break;
	default:
	case MODE_PROMPT:
		break;
	}

	WINDOW* sw = panel_window(i->status);
	if (i->error[0]) {
		wattron(sw, COLOR_PAIR(4));
		mvwprintw(sw, 0, 0, "%s", i->error);
		wattroff(sw, COLOR_PAIR(4));
		mvwprintw(sw, 0, strlen(i->error), "%*c",
				i->scrw-utf8_width(i->error), ' ');
		i->error[0] = 0;
	}
	else if (i->info[0]) {
		wattron(sw, COLOR_PAIR(10));
		mvwprintw(sw, 0, 0, "%s", i->info);
		wattroff(sw, COLOR_PAIR(10));
		mvwprintw(sw, 0, strlen(i->info), "%*c",
				i->scrw-utf8_width(i->info), ' ');
		i->info[0] = 0;
	}
	else if (i->find) {
		mvwprintw(sw, 0, 0, "/%s%*c", i->find->t,
				i->scrw-(utf8_width(i->find->t)+1), ' ');
	}
	else if (i->prompt) {
		mvwprintw(sw, 0, 0, "%s%*c", i->prompt->tb,
				i->scrw-(utf8_width(i->prompt->tb)+2), ' ');
	}
	else {
		ui_draw_hintbar(i, sw);
	}
	wrefresh(sw);

	update_panels();
	doupdate();
	refresh();
}

/* Sometimes it's better to create a new window
 * instead of resizing existing one.
 * If you just resize window (to bigger size),
 * borders stay in buffer and the only thing
 * you can do about them is explicitly overwrite them by
 * mvwprintw or something.
 * I do overwrite all buffer with spaces, so I dont have to.
 * ...but if you do, delete old window AFTER you assign new one to panel.
 */
void ui_update_geometry(struct ui* const i) {
	int newscrh, newscrw;
	getmaxyx(stdscr, newscrh, newscrw);
	if (newscrh == i->scrh && newscrw == i->scrw) return;
	syslog(LOG_DEBUG, "resized to %ux%u", newscrw, newscrh);
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
	if (i->help) {
		WINDOW* hw = panel_window(i->help);
		PANEL* hp = i->help;
		wresize(hw, i->scrh-1, i->scrw);
		move_panel(hp, 0, 0);
	}
}

int chmod_open(struct ui* i, utf8* path) {
	struct stat s;
	if (stat(path, &s)) return errno;
	errno = 0;
	struct passwd* pwd = getpwuid(s.st_uid);
	if (!pwd) return errno;
	struct group* grp = getgrgid(s.st_gid);
	if (!grp) return errno;

	i->chmod = malloc(sizeof(struct ui_chmod));
	i->chmod->o = s.st_uid;
	i->chmod->g = s.st_gid;
	i->chmod->mb = i->m;
	i->chmod->path = path;
	i->chmod->m = s.st_mode;
	WINDOW* cw = newwin(1, 1, 0, 0);
	i->chmod->p = new_panel(cw);
	i->m = MODE_CHMOD;
	strcpy(i->chmod->owner, pwd->pw_name);
	strcpy(i->chmod->group, grp->gr_name);
	i->chmod->ww = 34;
	i->chmod->wh = 17;
	i->scrw = i->scrh = 0; // Forces geometry update
	return 0;
}

void chmod_close(struct ui* i) {
	i->m = i->chmod->mb;
	WINDOW* cw = panel_window(i->chmod->p);
	del_panel(i->chmod->p);
	delwin(cw);
	free(i->chmod->path);
	free(i->chmod);
	i->chmod = NULL;
}

/* If tb_top is NULL, then it is set to tb */
void prompt_open(struct ui* i, utf8* tb, utf8* tb_top, size_t tb_size) {
	i->prompt = malloc(sizeof(struct ui_prompt));
	i->prompt->mb = i->m;
	i->prompt->tb = tb;
	i->prompt->tb_top = (tb_top ? tb_top : tb);
	i->prompt->tb_size = tb_size;
	i->m = MODE_PROMPT;
}

void prompt_close(struct ui* i) {
	i->m = i->prompt->mb;
	free(i->prompt);
	i->prompt = NULL;
}

/* If t_top is NULL, then it is set to t */
void find_open(struct ui* i, utf8* t, utf8* t_top, size_t t_size) {
	i->find = malloc(sizeof(struct ui_find));
	i->find->mb = i->m;
	i->find->sbfc = i->pv->selection;
	i->find->t = t;
	i->find->t_top = (t_top ? t_top : t);
	i->find->t_size = t_size;
	i->m = MODE_FIND;
}

void find_close(struct ui* i, bool success) {
	if (!success) i->pv->selection = i->find->sbfc;
	i->m = i->find->mb;
	free(i->find->t);
	free(i->find);
	i->find = NULL;
}

void help_open(struct ui* i) {
	WINDOW* nw = newwin(1, 1, 0, 0);
	i->help = new_panel(nw);
	i->scrw = i->scrh = 0;
	i->m = MODE_HELP;
}

void help_close(struct ui* i) {
	WINDOW* hw = panel_window(i->help);
	del_panel(i->help);
	delwin(hw);
	i->help = NULL;
	i->m = MODE_MANAGER;
}

struct input get_input(WINDOW* const w) {
	struct input r;
	memset(&r, 0, sizeof(struct input));
	size_t utflen = 0;
	int init = wgetch(w);
	const utf8 u = (utf8)init;
	const char* kn = keyname(init);
	//syslog(LOG_DEBUG, "get_input: %s (%d)", kn, init);
	if (init == -1) {
		r.t = END;
	}
	else if (strlen(kn) == 2 && kn[0] == '^') {
		r.t = CTRL;
		r.ctrl = kn[1];
	}
	else if (has_key(init)) {
		r.t = SPECIAL;
		r.c = init;
	}
	else if ((utflen = utf8_g2nb(&u))) {
		r.t = UTF8;
		r.utf[0] = u;
		for (size_t i = 1; i < utflen; ++i) {
			r.utf[i] = (utf8)wgetch(w);
		}
	}
	else {
		r.t = END;
	}
	return r;
}

/* Find matching mappings
 *
 * If input is ESC or no matches,
 * clear ili, and hint/matching table.
 * If there are a few, do nothing, wait longer.
 * If there is only one, send it.
 */
enum command get_cmd(struct ui* const i) {
	memset(i->mks, 0, i->kml*sizeof(int));
	static struct input il[INPUT_LIST_LENGTH];
	static int ili = 0;
	struct input newinput = get_input(stdscr);

	if (newinput.t == END) return CMD_NONE;
	else if (newinput.t == CTRL && newinput.ctrl == '[') {
		ili = 0;
		return CMD_NONE;
	}
	il[ili] = newinput;
	ili += 1;

	for (size_t m = 0; m < i->kml; ++m) {
		const struct input2cmd* const i2c = &i->kmap[m];
		if (i2c->m != i->m) continue; // mode mismatch
		const struct input* const in = i2c->i;
		for (int s = 0; s < ili; ++s) {
			if (in[s].t != il[s].t) break;
			bool match = false;
			switch (in[s].t) {
			case END: break;
			case UTF8:
				match = !strcmp(in[s].utf, il[s].utf);
				break;
			case SPECIAL:
				match = in[s].c == il[s].c;
				break;
			case CTRL:
				match = in[s].ctrl == il[s].ctrl;
				break;
			}
			if (match) {
				i->mks[m] += 1;
			}
			else {
				i->mks[m] = 0;
				break;
			}
		}
	}

	int matches = 0;
	size_t mi = 0;
	for (size_t m = 0; m < i->kml; ++m) {
		if (i->mks[m]) {
			matches += 1;
			mi = m;
		}
	}

	if (!matches) {
		ili = 0;
		memset(il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
		memset(i->mks, 0, i->kml*sizeof(int));
		return CMD_NONE;
	}
	else if (matches == 1) {
		bool fullmatch = true;
		for (int s = 0; s < INPUT_LIST_LENGTH &&
				i->kmap[mi].i[s].t != END; ++s) {
			fullmatch = fullmatch && il[s].t == i->kmap[mi].i[s].t;
			switch (il[s].t) {
			case END: break;
			case UTF8:
				fullmatch = fullmatch &&
					!strcmp(il[s].utf, i->kmap[mi].i[s].utf);
				break;
			case SPECIAL:
				fullmatch = fullmatch && il[s].c == i->kmap[mi].i[s].c;
				break;
			case CTRL:
				fullmatch = fullmatch && il[s].ctrl == i->kmap[mi].i[s].ctrl;
				break;
			}
		}
		if (fullmatch) {
			ili = 0;
			memset(il, 0, sizeof(struct input)*INPUT_LIST_LENGTH);
			// Not clearing on full match to preserve mks information for hints
			// Instead, mks is cleared when entered get_cmd()
			return i->kmap[mi].c;
		}
	}
	else { // some matches

	}
	return CMD_NONE;
}

/* Gets input to buffer
 * Responsible for cursor movement (buftop) and guarding buffer bounds (bsize)
 * If text is ready (enter pressed) returns 0,
 * If aborted, returns -1.
 * If keeps gathering, returns 1.
 * Additionally:
 * returns 2 on ^N and -2 on ^P
 */
int fill_textbox(utf8* const buf, utf8** const buftop, const size_t bsize,
		const int coff, WINDOW* const w) {
	curs_set(2);
	wmove(w, 0, utf8_width(buf)-utf8_width(*buftop)+coff);
	wrefresh(w);
	struct input i = get_input(w);
	curs_set(0);
	if (i.t == END) return 1;
	if (i.t == CTRL && i.ctrl == '[') return -1;
	else if (i.t == CTRL && i.ctrl == 'N') return 2;
	else if (i.t == CTRL && i.ctrl == 'P') return -2;
	else if ((i.t == CTRL && i.ctrl == 'J') ||
			(i.t == UTF8 && (i.utf[0] == '\n' || i.utf[0] == '\r'))) {
		if (*buftop != buf) return 0;
	}
	else if (i.t == UTF8 && (size_t)(*buftop - buf) < bsize) {
		utf8_insert(buf, i.utf, utf8_ng_till(buf, *buftop));
		*buftop += utf8_g2nb(i.utf);
	}
	else if ((i.t == CTRL && i.ctrl == 'H') ||
			(i.t == SPECIAL && i.c == KEY_BACKSPACE)) {
		if (*buftop != buf) {
			const size_t before = strlen(buf);
			utf8_remove(buf, utf8_ng_till(buf, *buftop)-1);
			*buftop -= before - strlen(buf);
		}
		else if (strlen(buf));
 		// Exit only when buf is empty
		else return -1;
	}
	else if ((i.t == CTRL && i.ctrl == 'D') ||
			(i.t == SPECIAL && i.c == KEY_DC)) {
		utf8_remove(buf, utf8_ng_till(buf, *buftop));
	}
	else if ((i.t == CTRL && i.ctrl == 'A') ||
			(i.t == SPECIAL && i.c == KEY_HOME)) {
		*buftop = buf;
	}
	else if ((i.t == CTRL && i.ctrl == 'E') ||
			(i.t == SPECIAL && i.c == KEY_END)) {
		*buftop = buf+strlen(buf);
	}
	else if (i.t == CTRL && i.ctrl == 'U') {
		*buftop = buf;
		memset(buf, 0, bsize);
	}
	else if (i.t == CTRL && i.ctrl == 'K') {
		memset(*buftop, 0, strlen(*buftop));
	}
	else if ((i.t == CTRL && i.ctrl == 'F') ||
			(i.t == SPECIAL && i.c == KEY_RIGHT)) {
		if ((size_t)(*buftop - buf) < bsize) {
			*buftop += utf8_g2nb(*buftop);
		}
	}
	else if ((i.t == CTRL && i.ctrl == 'B') ||
			(i.t == SPECIAL && i.c == KEY_LEFT)) {
		if ((size_t)(*buftop - buf) != 0) {
			const size_t gt = utf8_ng_till(buf, *buftop);
			*buftop = buf;
			for (size_t i = 0; i < gt-1; ++i) {
				*buftop += utf8_g2nb(*buftop);
			}
		}
	}
	return 1;
}
