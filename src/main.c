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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <getopt.h>
#include <string.h>
#include <syslog.h>

#include "include/file_view.h"
#include "include/prompt.h"

static void swap_views(struct file_view** pv, struct file_view** sv) {
	struct file_view* tmp = *pv;
	*pv = *sv;
	*sv = tmp;
	(*pv)->focused = true;
	(*sv)->focused = false;
	file_view_redraw(*sv);
}

static void create_directory(struct file_view* v) {
	char* path = malloc(PATH_MAX);
	char* name = malloc(NAME_MAX);
	name[0] = 0;
	strcpy(path, v->wd);
	prompt("directory name", NAME_MAX, name);
	enter_dir(path, name);
	dir_make(path);
	scan_dir(v->wd, &v->file_list, &v->num_files);
	v->selection = file_index(v->file_list, v->num_files, name);
	free(path);
	free(name);
	file_view_redraw(v);
}

static void remove_file(struct file_view* v) {
	char* path = malloc(PATH_MAX);
	strcpy(path, v->wd);
	enter_dir(path, v->file_list[v->selection]->file_name);
	file_remove(path);
	free(path);
	scan_dir(v->wd, &v->file_list, &v->num_files);
	v->selection -= 1;
	file_view_redraw(v);
}

static void move_file(struct file_view* pv, struct file_view* sv) {
	char* src_path = malloc(PATH_MAX);
	char* dest_path = malloc(PATH_MAX);
	char* name = malloc(NAME_MAX);
	name[0] = 0;
	strcpy(src_path, pv->wd);
	strcpy(dest_path, sv->wd);
	enter_dir(src_path, pv->file_list[pv->selection]->file_name);
	prompt("new name", NAME_MAX, name);
	enter_dir(dest_path, name);
	int fmr = file_move(src_path, dest_path);
	syslog(LOG_DEBUG, "file_move() returned %d", fmr);
	free(src_path);
	free(dest_path);
	free(name);
	scan_dir(pv->wd, &pv->file_list, &pv->num_files);
	scan_dir(sv->wd, &sv->file_list, &sv->num_files);
	pv->selection = 0;
	file_view_redraw(pv);
	file_view_redraw(sv);
}

static void copy_file(struct file_view* pv, struct file_view* sv) {
	char* src_path = malloc(PATH_MAX);
	char* dest_path = malloc(PATH_MAX);
	char* name = malloc(NAME_MAX);
	name[0] = 0;
	strcpy(src_path, pv->wd);
	strcpy(dest_path, sv->wd);
	enter_dir(src_path, pv->file_list[pv->selection]->file_name);
	prompt("copy name", NAME_MAX, name);
	enter_dir(dest_path, name);
	int fcr = file_copy(src_path, dest_path);
	syslog(LOG_DEBUG, "file_copy() returned %d", fcr);
	free(src_path);
	free(dest_path);
	free(name);
	scan_dir(sv->wd, &sv->file_list, &sv->num_files);
	file_view_redraw(sv);
}

static void go_up_dir(struct file_view* v) {
	char* prevdir = malloc(NAME_MAX);
	current_dir(v->wd, prevdir);
	int r = up_dir(v->wd);
	if (!r) {
		scan_dir(v->wd, &v->file_list, &v->num_files);
		v->selection = file_index(v->file_list, v->num_files, prevdir);
	}
	free(prevdir);
}

static void go_enter_dir(struct file_view* v) {
	if (v->file_list[v->selection]->t == DIRECTORY) {
		enter_dir(v->wd, v->file_list[v->selection]->file_name);
		v->selection = 0;
		scan_dir(v->wd, &v->file_list, &v->num_files);
	}
	else if (v->file_list[v->selection]->t == LINK) {
		enter_dir(v->wd, v->file_list[v->selection]->link_path);
		v->selection = 0;
		scan_dir(v->wd, &v->file_list, &v->num_files);
	}
}

enum command {
	NONE = 0,
	QUIT,
	COPY,
	MOVE,
	REMOVE,
	SWITCH_PANEL,
	UP_DIR,
	ENTER_DIR,
	REFRESH,
	ENTRY_UP,
	ENTRY_DOWN,
	CREATE_DIR,
};

static char* help = "Usage: hund [OPTION]...\n"
"Options:\n"
"  -c, --chdir\t\tchange initial directory\n"
"  -v, --verbose\t\tbe verbose\n"
"  -h, --help\t\tdisplay this help message\n";

#define MAX_KEYSEQ_LENGTH 4

struct key2cmd {
	int ks[MAX_KEYSEQ_LENGTH]; // Key Sequence
	char* d;
	enum command c;
};

/* Such table is cool since it's easy to change with a config */
static struct key2cmd key_mapping[] = {
	{ .ks = { 'q', 'q', 0, 0 }, .d = "quit", .c = QUIT  },
	{ .ks = { 'j', 0, 0, 0 }, .d = "down", .c = ENTRY_DOWN },
	{ .ks = { 'k', 0, 0, 0 }, .d = "up", .c = ENTRY_UP },
	{ .ks = { 'c', 'p', 0, 0 }, .d = "copy", .c = COPY },
	{ .ks = { 'r', 'm', 0, 0 }, .d = "remove", .c = REMOVE },
	{ .ks = { 'm', 'v', 0, 0 }, .d = "move", .c = MOVE },
	{ .ks = { '\t', 0, 0, 0 }, .d = "switch panel", .c = SWITCH_PANEL },
	{ .ks = { 'r', 'r', 0, 0 }, .d = "refresh", .c = REFRESH },
	{ .ks = { 'm', 'k', 0, 0 }, .d = "create dir", .c = CREATE_DIR },
	{ .ks = { 'u', 0, 0, 0 }, .d = "up dir", .c = UP_DIR },
	{ .ks = { 'd', 0, 0, 0 }, .d = "up dir", .c = UP_DIR },
	{ .ks = { 'i', 0, 0, 0 }, .d = "enter dir", .c = ENTER_DIR },
	{ .ks = { 'e', 0, 0, 0 }, .d = "enter dir", .c = ENTER_DIR },

	{ .ks = { 'x', 'x', 0, 0 }, .d = "quit", .c = QUIT },
	{ .ks = { 'x', 'y', 0, 0 }, .d = "quit", .c = QUIT },
	{ .ks = { 'x', 'z', 0, 0 }, .d = "quit", .c = QUIT }
};

int main(int argc, char* argv[])  {
	static char sopt[] = "vhc:";
	static struct option lopt[] = {
		{"chdir", required_argument, 0, 'c'},
		{"verbose", no_argument, 0, 'v'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
    };
	int o, opti = 0;
	while ((o = getopt_long(argc, argv, sopt, lopt, &opti)) != -1) {
		if (o == -1) break;
		switch (o) {
		case 'c':
			chdir(optarg);
			break;
		case 'v':
			puts("woof!");
			exit(EXIT_SUCCESS);
		case 'h':
			printf(help);
			exit(EXIT_SUCCESS);
		default:
			puts("unknown argument");
			exit(1);
		}
	}

	while (optind < argc) {
		printf("non-option argument at index %d: %s\n", optind, argv[optind]);
		optind += 1;
	}

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
	curs_set(0);

	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_BLACK, COLOR_WHITE);
	init_pair(3, COLOR_CYAN, COLOR_BLACK);
	init_pair(4, COLOR_BLACK, COLOR_CYAN);
	init_pair(5, COLOR_RED, COLOR_BLACK);
	init_pair(6, COLOR_BLACK, COLOR_RED);
	init_pair(7, COLOR_YELLOW, COLOR_BLACK);
	init_pair(8, COLOR_BLACK, COLOR_YELLOW);

	openlog(argv[0], LOG_PID, LOG_USER);
	syslog(LOG_NOTICE, "%s started", argv[0]+2);

	struct file_view pp[2];
	int scrh, scrw;
	getmaxyx(stdscr, scrh, scrw);
	file_view_pair_setup(pp, scrh, scrw);
	struct file_view* pv = &pp[0];
	struct file_view* sv = &pp[1];

//	WINDOW* hintwin = newwin(1, scrw, 0, 0);
//	PANEL* hintpan = new_panel(hintwin);
//	move_panel(hintpan, scrh-1, 0);
//	mvwprintw(hintwin, 0, 1, "hello");
//	wrefresh(hintwin);

	get_cwd(pv->wd);
	get_cwd(sv->wd);
	scan_dir(pv->wd, &pv->file_list, &pv->num_files);
	scan_dir(sv->wd, &sv->file_list, &sv->num_files);
	file_view_redraw(pv);
	file_view_redraw(sv);

	int keyseq[MAX_KEYSEQ_LENGTH]; // Key Sequence
	int ksi = 0; // Key Sequence Index (keyseq is basically a tiny stack)
	memset(keyseq, 0, sizeof(keyseq));
	enum command cmd = NONE;
	bool run = true;

	while (run) {
		int c = wgetch(panel_window(pv->pan));
		if (c != -1) {
			// 27 = ESCAPE KEY
			if (c == 27) {
				memset(keyseq, 0, sizeof(keyseq));
				ksi = 0;
			}
			else {
				//syslog(LOG_DEBUG, "input: %d", c);
				keyseq[ksi] = c;
				ksi += 1;
			}
		}

		// Key Mapping Length
		const int kml = sizeof(key_mapping)/sizeof(struct key2cmd);
		int mks[kml]; // Matching Key Sequence
		memset(mks, 0, sizeof(mks));

		for (int c = 0; c < kml; c++) {
			int s = 0;
			// Skip zeroes; these does not matter
			while (keyseq[s]) {
				/* mks[c] will contain length of matching sequence
				 * mks[c] will be zeroed if sequence broken at any point
				 */
				if (key_mapping[c].ks[s] == keyseq[s]) {
					mks[c] += 1;
				}
				else {
					mks[c] = 0;
					break;
				}
				s += 1;
			}
		}

		bool match = false; // At least one match
		for (int c = 0; c < kml; c++) {
			match = match || mks[c];
		}

//		wmove(hintwin, 0, 0);
//		if (match) {
//			for (int c = 0; c < kml; c++) {
//				if (mks[c]) {
//					wprintw(hintwin, "%s, ", key_mapping[c].d);
//				}
//			}
//		}
//		wprintw(hintwin, "%*c", 10, ' ');

		if (match) {
			for (int c = 0; c < kml; c++) {
				int ksl = 0;
				for (int s = 0; key_mapping[c].ks[s]; s++) {
					ksl += 1;
				}
				if (mks[c] == ksl) {
					cmd = key_mapping[c].c;
				}
			}
		}

		switch (cmd) {
		case QUIT:
			run = false;
			break;
		case SWITCH_PANEL:
			swap_views(&pv, &sv);
			break;
		case ENTRY_DOWN:
			if (pv->selection < pv->num_files-1) {
				pv->selection += 1;
			}
			break;
		case ENTRY_UP:
			if (pv->selection > 0) {
				pv->selection -= 1;
			}
			break;
		case ENTER_DIR:
			go_enter_dir(pv);
			break;
		case UP_DIR:
			go_up_dir(pv);
			break;
		case COPY:
			copy_file(pv, sv);
			break;
		case MOVE:
			move_file(pv, sv);
			break;
		case REMOVE:
			remove_file(pv);
			break;
		case CREATE_DIR:
			create_directory(pv);
			break;
		case REFRESH:
			scan_dir(pv->wd, &pv->file_list, &pv->num_files);
			file_view_redraw(pv);
			break;
		case NONE:
			break;
		}

		/* If keyseq is full and no command is found for current key sequence
		 * then there is no command coresponding to that key sequence
		 */
		if (!match || ksi == MAX_KEYSEQ_LENGTH || cmd != NONE) {
			cmd = NONE;
			memset(keyseq, 0, sizeof(keyseq));
			ksi = 0;
		}

		// Check if screen size changed and resize panels if yes
		int new_scrh, new_scrw;
		getmaxyx(stdscr, new_scrh, new_scrw);
		if (new_scrh != scrh || new_scrw != scrw) {
			scrh = new_scrh;
			scrw = new_scrw;
			pp[0].width = scrw/2;
			pp[0].height = scrh;
			pp[0].position_x = 0;
			pp[0].position_y = 0;
			pp[1].width = scrw/2;
			pp[1].height = scrh;
			pp[1].position_x = scrw/2;
			pp[1].position_y = 0;
			file_view_pair_update_geometry(pp);
			file_view_redraw(pv);
			file_view_redraw(sv);
		}
		// If screen size not changed, then only redraw active view.
		else {
			file_view_redraw(pv);
		}
		// TODO read manuals to find out whether I need them all
		update_panels();
		doupdate();
		refresh();
	}

	for (int i = 0; i < 2; i++) {
		delete_file_list(&pp[i].file_list, &pp[i].num_files);
	}
	syslog(LOG_DEBUG, "exit");
	closelog();
	endwin();
	file_view_pair_delete(pp);
	exit(EXIT_SUCCESS);
}
