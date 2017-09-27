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

#include "include/file_view.h"

int main(int argc, char* argv[])  {
	int o;
	static char sopt[] = "vht:";
	static struct option lopt[] = {
		{"chroot", required_argument, 0, 't'},
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
			perror("unknown argument");
			abort();
			break;
		}
	}

	setlocale(LC_ALL, "UTF-8");
	initscr();
	noecho();
	nonl();
	raw();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);

	struct file_view pp[2];
	int scrh, scrw;
	getmaxyx(stdscr, scrh, scrw);
	file_view_setup_pair(pp, scrh, scrw);
	struct file_view* primary_view = &pp[0];
	struct file_view* secondary_view = &pp[1];

	get_cwd(primary_view->wd);
	get_cwd(secondary_view->wd);
	strcat(secondary_view->wd, "/src");
	scan_wd(primary_view->wd, &primary_view->namelist, &primary_view->num_files);
	scan_wd(secondary_view->wd, &secondary_view->namelist, &secondary_view->num_files);

	int ch, x = 1, y = 1;
	while ((ch = wgetch(primary_view->win)) != 'q') {
		switch (ch) {
		case '\t':
			{
			struct file_view* tmp = primary_view;
			primary_view = secondary_view;
			secondary_view = tmp;
			}
			break;
		case 'j':
			y += 1;
			break;
		case 'k':
			y -= 1;
			break;
		case 'h':
			x -= 1;
			break;
		case 'l':
			x += 1;
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
			file_view_update_geometry(&pp[0]);
			file_view_update_geometry(&pp[1]);
		}
		//top_panel(primary_view->pan);
		file_view_refresh(primary_view);
		file_view_refresh(secondary_view);
		update_panels();
		doupdate();
		wmove(primary_view->win, y, x);
	}

	endwin();
	file_view_delete_pair(pp);
	exit(EXIT_SUCCESS);
}
