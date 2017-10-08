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
	strcpy(src_path, pv->wd);
	strcpy(dest_path, sv->wd);
	enter_dir(src_path, pv->file_list[pv->selection]->file_name);
	prompt("new name", PATH_MAX, dest_path);
	file_move(src_path, dest_path);
	free(src_path);
	free(dest_path);
	scan_dir(pv->wd, &pv->file_list, &pv->num_files);
	scan_dir(sv->wd, &sv->file_list, &sv->num_files);
	pv->selection = 0;
	file_view_redraw(pv);
	file_view_redraw(sv);
}

static void copy_file(struct file_view* pv, struct file_view* sv) {
	char* src_path = malloc(PATH_MAX);
	char* dest_path = malloc(PATH_MAX);
	strcpy(src_path, pv->wd);
	strcpy(dest_path, sv->wd);
	enter_dir(src_path, pv->file_list[pv->selection]->file_name);
	prompt("copy name", PATH_MAX, dest_path);
	file_copy(src_path, dest_path);
	free(src_path);
	free(dest_path);
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

static char* help = "Usage: hund [OPTION]...\n"
"Options:\n"
"  -c, --chdir\t\tchange initial directory\n"
"  -v, --verbose\t\tbe verbose\n"
"  -h, --help\t\tdisplay this help message\n";

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
	raw();
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

	get_cwd(pv->wd);
	get_cwd(sv->wd);
	scan_dir(pv->wd, &pv->file_list, &pv->num_files);
	scan_dir(sv->wd, &sv->file_list, &sv->num_files);
	file_view_redraw(pv);
	file_view_redraw(sv);

	int ch;
	while ((ch = wgetch(panel_window(pv->pan))) != 'q') {
		switch (ch) {
		case '\t':
			swap_views(&pv, &sv);
			break;
		case 'j':
			if (pv->selection < pv->num_files-1) {
				pv->selection += 1;
			}
			break;
		case 'k':
			if (pv->selection > 0) {
				pv->selection -= 1;
			}
			break;
		case 'e':
		case 'i':
			go_enter_dir(pv);
			break;
		case 'd':
		case 'u':
			go_up_dir(pv);
			break;
		case 'c':
			copy_file(pv, sv);
			break;
		case 'm':
			move_file(pv, sv);
			break;
		case 'r':
			remove_file(pv);
			break;
		case 'n':
			create_directory(pv);
			break;
		case 's':
			scan_dir(pv->wd, &pv->file_list, &pv->num_files);
			file_view_redraw(pv);
			break;
		default:
			break;
		}

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
		update_panels();
		doupdate();
		refresh();
	}

	for (int i = 0; i < 2; i++) {
		delete_file_list(&pp[i].file_list, pp[i].num_files);
	}
	syslog(LOG_DEBUG, "exit");
	closelog();
	endwin();
	file_view_pair_delete(pp);
	exit(EXIT_SUCCESS);
}
