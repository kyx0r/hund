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

int main(int argc, char* argv[])  {
	int o;
	static char sopt[] = "vht:";
	static struct option lopt[] = {
		{"chdir", required_argument, 0, 't'},
		{"verbose", no_argument, 0, 'v'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
    };
    int opti = 0;
	while ((o = getopt_long(argc, argv, sopt, lopt, &opti)) != -1) {
		if (o == -1) break;
		switch (o) {
		case 't':
			chdir(optarg);
			break;
		case 'v':
			puts("woof!");
			break;
		case 'h':
			puts("help?");
			break;
		default:
			puts("unknown argument");
			exit(1);
			break;
		}
	}

	while (optind < argc) {
		printf("non-option argument at index %d: %s\n", optind, argv[optind]);
		optind += 1;
	}

	openlog(argv[0], LOG_PID, LOG_USER);
	syslog(LOG_NOTICE, "%s started", argv[0]+2);

	setlocale(LC_ALL, "UTF-8");
	initscr();
	if (has_colors() == FALSE) {
		endwin();
		printf("no colors :(\n");
		exit(1);
	}
	start_color();
	noecho();
	nonl();
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

	struct file_view pp[2];
	int scrh, scrw;
	getmaxyx(stdscr, scrh, scrw);
	file_view_pair_setup(pp, scrh, scrw);
	struct file_view* primary_view = &pp[0];
	struct file_view* secondary_view = &pp[1];

	get_cwd(primary_view->wd);
	get_cwd(secondary_view->wd);
	scan_dir(primary_view->wd, &primary_view->file_list, &primary_view->num_files);
	scan_dir(secondary_view->wd, &secondary_view->file_list, &secondary_view->num_files);
	file_view_redraw(primary_view);
	file_view_redraw(secondary_view);

	int ch, r;
	char prevdir[NAME_MAX];
	memset(prevdir, 0, sizeof(prevdir));
	while ((ch = wgetch(panel_window(primary_view->pan))) != 'q') {
		switch (ch) {
		case '\t':
			{
			struct file_view* tmp = primary_view;
			primary_view = secondary_view;
			secondary_view = tmp;
			primary_view->focused = true;
			secondary_view->focused = false;
			file_view_redraw(secondary_view);
			}
			break;
		case 'j':
			if (primary_view->selection < primary_view->num_files-1) {
				primary_view->selection += 1;
			}
			break;
		case 'k':
			if (primary_view->selection > 0) {
				primary_view->selection -= 1;
			}
			break;
		case 'e':
		case 'i':
			if (primary_view->file_list[primary_view->selection]->t == DIRECTORY) {
				enter_dir(primary_view->wd, primary_view->file_list[primary_view->selection]->file_name);
				primary_view->selection = 0;
				scan_dir(primary_view->wd, &primary_view->file_list, &primary_view->num_files);
			}
			else if (primary_view->file_list[primary_view->selection]->t == LINK) {
				enter_dir(primary_view->wd, primary_view->file_list[primary_view->selection]->link_path);
				primary_view->selection = 0;
				scan_dir(primary_view->wd, &primary_view->file_list, &primary_view->num_files);
			}
			break;
		case 'd':
		case 'u':
			/* Remember previous directory to later
			 * move selection to that directory.
			 */
			current_dir(primary_view->wd, prevdir);
			r = up_dir(primary_view->wd);
			if (!r) {
				scan_dir(primary_view->wd, &primary_view->file_list, &primary_view->num_files);
				primary_view->selection = 0;
				for (int i = 0; i < primary_view->num_files; i++) {
					if (strcmp(primary_view->file_list[i]->file_name, prevdir) == 0) {
						primary_view->selection = i;
						break;
					}
				}
			}
			break;
		case 'm':
			{
			char* src_name = primary_view->file_list[primary_view->selection]->file_name;
			char src_path[PATH_MAX];
			char dest_path[PATH_MAX];
			strcpy(src_path, primary_view->wd);
			strcpy(dest_path, secondary_view->wd);
			enter_dir(src_path, src_name);
			enter_dir(dest_path, src_name);
			syslog(LOG_DEBUG, "%s -> %s", src_path, dest_path);
			file_move(src_path, dest_path);
			scan_dir(primary_view->wd, &primary_view->file_list, &primary_view->num_files);
			scan_dir(secondary_view->wd, &secondary_view->file_list, &secondary_view->num_files);
			file_view_redraw(primary_view);
			file_view_redraw(secondary_view);
			primary_view->selection = 0;
			}
			break;
		case 'r':
			{
			char* src_name = primary_view->file_list[primary_view->selection]->file_name;
			char src_path[PATH_MAX];
			strcpy(src_path, primary_view->wd);
			enter_dir(src_path, src_name);
			syslog(LOG_DEBUG, "remove %s", src_path);
			file_remove(src_path);
			scan_dir(primary_view->wd, &primary_view->file_list, &primary_view->num_files);
			file_view_redraw(primary_view);
			primary_view->selection = 0;
			}
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
			file_view_redraw(primary_view);
			file_view_redraw(secondary_view);
		}
		// If screen size not changed, then only redraw active view.
		else {
			file_view_redraw(primary_view);
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
