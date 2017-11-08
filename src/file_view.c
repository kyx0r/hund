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

#include "include/file_view.h"

// Is File At Index Visible
bool ifaiv(const struct file_view* const fv, const fnum_t i) {
	return fv->file_list[i]->file_name[0] != '.';
}

void next_entry(struct file_view* fv) {
	if (!fv->num_files) return;
	if (fv->show_hidden) {
		if (fv->selection < fv->num_files-1) {
			fv->selection += 1;
		}
	}
	else {
		fnum_t i = fv->selection;
		/* Truth table - "Keep looking?" or "What to put in while"
		 * 1 = Keep looking
		 * 0 = Give up
		 *        visible | hidden
		 * out of array |0|0|
		 * in array     |0|1|
		 */
		do {
			i += 1;
		} while (i <= fv->num_files-1 && fv->file_list[i]->file_name[0] == '.');
		if (i > fv->num_files-1) return;
		fv->selection = i;
	}
}

void prev_entry(struct file_view* fv) {
	if (!fv->num_files) return;
	if (fv->show_hidden) {
		if (fv->selection > 0) {
			fv->selection -= 1;
		}
	}
	/* Truth table - "Perform backward search?"
	 *    .hidden | visible
	 * sel == 0 |1*|0| * = first_entry()
	 * sel != 0 |1 |1|
	 */
	else if (fv->selection == 0 && !ifaiv(fv, fv->selection)) {
		first_entry(fv);
	}
   	else if (fv->selection != 0 && ifaiv(fv, fv->selection)) {
	   	/* Truth table - "Keep looking?" or "What to put in while"
	   	 * 1 = Keep looking
	   	 * 0 = Stop looking
	   	 *  visible | hidden
	   	 * i == 0 |0|0|
	   	 * i != 0 |0|1|
	   	 */
	   	fnum_t i = fv->selection;
	   	do {
	   		i -= 1;
	   	} while (i != 0 && fv->file_list[i]->file_name[0] == '.');
	   	if (i == 0 && !ifaiv(fv, i)) {
			first_entry(fv);
		}
		else {
	   		fv->selection = i;
		}
	}
}

void first_entry(struct file_view* fv) {
	if (!fv->num_files || fv->show_hidden ||
			(!fv->show_hidden && !fv->num_files)) {
		fv->selection = 0;
	}
	else {
		fnum_t i = 0;
		while (i < fv->num_files-1 && !ifaiv(fv, i)) {
			i += 1;
		}
		if (i == fv->num_files-1 && !ifaiv(fv, i)) {
			fv->selection = 0;
		}
		else {
			fv->selection = i;
		}
	}
}

void last_entry(struct file_view* fv) {
	if (!fv->num_files) return;
	if (fv->show_hidden) {
		fv->selection = fv->num_files-1;
	}
	else {
		fnum_t i = fv->num_files-1;
		while (i != 0 && !ifaiv(fv, i)) {
			i -= 1;
		}
		if (i == 0 && !ifaiv(fv, i)) {
			fv->selection = 0;
		}
		else {
			fv->selection = i;
		}
	}
}
