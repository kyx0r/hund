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

#include "include/prompt.h"

int prompt(char prompt[], size_t bufsize, char buf[bufsize]) {
	int scrh, scrw;
	getmaxyx(stdscr, scrh, scrw);
	const int box_w = 50;
	const int box_h = 3;
	WINDOW* fw = newwin(box_h, box_w, 0, 0);
	PANEL* fp = new_panel(fw);
	move_panel(fp, (scrh-box_h)/2, (scrw-box_w)/2);
	wrefresh(fw);
	curs_set(2);
	box(fw, 0, 0);
	//wborder(fw, '|', '|', '-', '-', '+', '+', '+', '+');
	mvwprintw(fw, 0, (box_w - strlen(prompt))/2, prompt);
	update_panels();
	doupdate();
	refresh();
	int cursor = strlen(buf);
	wtimeout(fw, 100);
	keypad(fw, TRUE);
	int c;
	while ((c = wgetch(fw)) != '\n') {
		switch (c) {
		//case KEY_LEFT: cursor -= 1; break;
		//case KEY_RIGHT: cursor += 1; break;
		case KEY_BACKSPACE:
			if (cursor == 0) break;
			buf[cursor-1] = 0;
			cursor -= 1;
			break;
		default:
			if (cursor+1 >= box_w-2) break;
			if (c < 127 && c >= ' ') {
				syslog(LOG_DEBUG, "character: %c", c);
				buf[cursor] = c;
				buf[cursor+1] = 0;
				cursor += 1;
			}
			break;
		}
		mvwprintw(fw, 1, 1, "%s", buf);
		const int buflen = strlen(buf);
		if (buflen <= box_w - 2) {
			mvwprintw(fw, 1, strlen(buf)+1, "%*c", (box_w-2)-strlen(buf)-1, ' ');
		}
		wmove(fw, 1, 1 + cursor);
	}
	curs_set(0);
	del_panel(fp);
	delwin(fw);
	update_panels();
	doupdate();
	refresh();
	return 0;
}
