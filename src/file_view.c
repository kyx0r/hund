/*
 *  Copyright (C) 2017-2018 by Michał Czarnecki <czarnecky@va.pl>
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

/* This file contains file_view-related helper functions.
 *
 * They do things that would otherwise clutter main loop.
 * The main problem that inspired separating those tasks from main loop
 * was performing visibility tests.
 *
 * They are supposed to care about:
 * - selection position and it's validity
 * - whether file is visible or not
 * - whether file_list is empty or not
 * - error codes/failure of functions that they call
 * - rescanning file_list (if needed)
 */

/* checks if file is hidden just by looking at the name */
bool hidden(const struct file_view* const fv, const fnum_t i) {
	return fv->file_list[i]->file_name[0] == '.';
}

/* checks if file is visible at the moment */
/* Truth table - is file visible at the moment?
 *          file.ext | .hidden.ext
 * show_hidden = 1 |1|1|
 * show_hidden = 0 |1|0|
 */
bool visible(const struct file_view* const fv, const fnum_t i) {
	return fv->show_hidden || !hidden(fv, i);
}

void next_entry(struct file_view* const fv) {
	if (!fv->num_files || fv->selection > fv->num_files-1) {
		fv->selection = 0;
		return;
	}
	if (fv->show_hidden) {
		if (fv->selection < fv->num_files-1) {
			fv->selection += 1;
		}
	}
	else {
		fnum_t i = fv->selection;
		do {
			i += 1;
		} while (i < fv->num_files && hidden(fv, i));
		if (i >= fv->num_files) return;
		fv->selection = i;
	}
}

void prev_entry(struct file_view* const fv) {
	if (!fv->num_files || fv->selection > fv->num_files-1) {
		fv->selection = 0;
		return;
	}
	if (fv->show_hidden) {
		if (fv->selection > 0) {
			fv->selection -= 1;
		}
	}
	/* Truth table - "Perform backward search?"
	 * .notvisible | visible
	 * sel == 0 |1*|0| * = first_entry()
	 * sel != 0 |1 |1|
	 */
	else if (fv->selection == 0 && hidden(fv, fv->selection)) {
		first_entry(fv);
	}
	else if (fv->selection != 0 && !hidden(fv, fv->selection)) {
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
		} while (i != 0 && hidden(fv, i));
		if (i == 0 && hidden(fv, i)) {
			first_entry(fv);
		}
		else {
	   		fv->selection = i;
		}
	}
}

void first_entry(struct file_view* const fv) {
	if (!fv->num_files || fv->show_hidden ||
			(!fv->show_hidden && !fv->num_files)) {
		fv->selection = 0;
	}
	else {
		fnum_t i = 0;
		while (i < fv->num_files-1 && hidden(fv, i)) {
			i += 1;
		}
		if (i == fv->num_files-1 && hidden(fv, i)) {
			fv->selection = 0;
		}
		else {
			fv->selection = i;
		}
	}
}

void last_entry(struct file_view* const fv) {
	if (!fv->num_files) {
		fv->selection = 0;
	}
	else if (fv->show_hidden) {
		fv->selection = fv->num_files-1;
	}
	else {
		fnum_t i = fv->num_files-1;
		while (i != 0 && hidden(fv, i)) {
			i -= 1;
		}
		if (i == 0 && hidden(fv, i)) {
			fv->selection = 0;
		}
		else {
			fv->selection = i;
		}
	}
}

void jump_n_entries(struct file_view* const fv, const int n) {
	// TODO optimize
	if (!fv->num_files) {
		fv->selection = 0;
	}
	fnum_t N;
	if (n < 0) {
		N = -n;
		if (fv->selection < N) {
			first_entry(fv);
		}
		else {
			for (int i = 0; i < -n; ++i) {
				prev_entry(fv);
			}
		}

	}
	else if (n > 0) {
		N = n;
		if (fv->num_files - fv->selection < N) {
			last_entry(fv);
		}
		else {
			for (int i = 0; i < n; ++i) {
				next_entry(fv);
			}
		}
	}
}

void delete_file_list(struct file_view* const fv) {
	file_list_clean(&fv->file_list, &fv->num_files);
	fv->selection = fv->num_hidden = 0;
}

bool file_on_list(struct file_view* const fv, const char* const name) {
	fnum_t i = 0;
	while (i < fv->num_files && strcmp(fv->file_list[i]->file_name, name)) {
		i += 1;
	}
	return i != fv->num_files;
}

/* Finds and highlighs file with given name */
void file_highlight(struct file_view* const fv, const char* const name) {
	fnum_t i = 0;
	while (i < fv->num_files && strcmp(fv->file_list[i]->file_name, name)) {
		i += 1;
	}
	if (i != fv->num_files) {
		if (visible(fv, i)) {
			fv->selection = i;
		}
		else {
			first_entry(fv);
		}
	}
}

bool file_find(struct file_view* const fv, const char* const name,
		fnum_t start, fnum_t end) {
	if (start <= end) {
		for (fnum_t i = start; i <= end; ++i) {
			if (visible(fv, i) &&
			    contains(fv->file_list[i]->file_name, name)) {
				fv->selection = i;
				return true;
			}
			//if (i == end) break;
		}
	}
	else if (end < start) {
		for (fnum_t i = start; i >= end; --i) {
			if (visible(fv, i) &&
					contains(fv->file_list[i]->file_name, name)) {
				fv->selection = i;
				return true;
			}
			if (i == end) break;
			// ^ prevents unsigned integer underflow
			// TODO do it better
		}
	}
	return false;
}

int file_view_enter_selected_dir(struct file_view* const fv) {
	if (!fv->num_files || !visible(fv, fv->selection)) return 0;
	const char* const fn = fv->file_list[fv->selection]->file_name;
	const struct stat* rst = &fv->file_list[fv->selection]->s;
	const struct stat* lst = fv->file_list[fv->selection]->l;
	if (!lst) return ENOENT;
	if (S_ISDIR(rst->st_mode) || S_ISDIR(lst->st_mode)) {
		if (enter_dir(fv->wd, fn)) return ENAMETOOLONG;
	}
	else return ENOTDIR;
	int err = file_view_scan_dir(fv);
	if (err) {
		delete_file_list(fv);
		file_view_up_dir(fv);
		// TODO ^^^ would be better if it was checked
		// before any changes to file_view
		return err;
	}
	file_view_sort(fv);
	first_entry(fv);
	return 0;
}

int file_view_up_dir(struct file_view* const fv) {
	char* prevdir = strncpy(malloc(NAME_MAX+1),
			fv->wd+current_dir_i(fv->wd), NAME_MAX);
	up_dir(fv->wd);
	int err = file_view_scan_dir(fv);
	if (err) {
		delete_file_list(fv);
		free(prevdir);
		return err;
	}
	file_view_sort(fv);
	file_highlight(fv, prevdir);
	free(prevdir);
	return 0;
}

void file_view_afterdel(struct file_view* const fv) {
	if (fv->num_files) {
		if (fv->selection >= fv->num_files-1) {
			last_entry(fv);
		}
		if (!visible(fv, fv->selection)){
			next_entry(fv);
		}
	}
	else {
		fv->selection = 0;
	}
}

void file_view_toggle_hidden(struct file_view* const fv) {
	fv->show_hidden = !fv->show_hidden;
	if (!fv->num_files || !visible(fv, fv->selection)) {
		first_entry(fv);
	}
	if (!fv->show_hidden) {
		for (fnum_t f = 0; f < fv->num_files; ++f) {
			if (fv->file_list[f]->selected && !visible(fv, f)) {
				fv->file_list[f]->selected = false;
				fv->num_selected -= 1;
			}
		}
	}
}

int file_view_scan_dir(struct file_view* const fv) {
	fv->num_selected = 0;
	return scan_dir(fv->wd, &fv->file_list, &fv->num_files, &fv->num_hidden);
}

void file_view_sort(struct file_view* const fv) {
	sort_file_list(fv->sorting, fv->file_list, fv->num_files);
}

char* file_view_path_to_selected(struct file_view* const fv) {
	if (!fv->num_files) return NULL;
	char* p = malloc(PATH_MAX+1);
	strncpy(p, fv->wd, PATH_MAX);
	if (enter_dir(p, fv->file_list[fv->selection]->file_name)) {
		free(p);
		return NULL;
	}
	return p;
}

void file_view_change_sorting(struct file_view* const fv, sorting_foo sorting) {
	char before[NAME_MAX+1];
	strncpy(before, fv->file_list[fv->selection]->file_name, NAME_MAX+1);
	fv->sorting = sorting;
	file_view_sort(fv);
	file_highlight(fv, before);
}

void file_view_selected_to_list(struct file_view* const fv,
		struct string_list* const list) {
	list->len = 0;
	for (fnum_t f = 0, s = 0;
	     f < fv->num_files && s < fv->num_selected; ++f) {
		if (!fv->file_list[f]->selected) continue;
		list->len += 1;
		list->str = realloc(list->str, (list->len) * sizeof(char*));
		list->str[list->len-1] = strdup(fv->file_list[f]->file_name);
		s += 1;
	}
}

void select_from_list(struct file_view* const fv,
		const struct string_list* const list) {
	for (fnum_t li = 0; li < list->len; ++li) {
		for (fnum_t s = 0; s < fv->num_files; ++s) {
			if (list->str[li]
			    && !strcmp(list->str[li],
					fv->file_list[s]->file_name)) {
				fv->file_list[s]->selected = true;
				fv->num_selected += 1;
				break;
			}
		}
	}
}

bool conflicts_with_existing(struct file_view* const fv,
		const struct string_list* const list) {
	for (fnum_t f = 0; f < list->len; ++f) {
		if (file_on_list(fv, list->str[f])) return true;
	}
	return false;
}

void remove_conflicting(struct file_view* const fv,
		struct string_list* const list) {
	struct string_list repl = { NULL, 0 };
	for (fnum_t f = 0; f < list->len; ++f) {
		if (!file_on_list(fv, list->str[f])) {
			list_push(&repl, list->str[f]);
		}
	}
	free_list(list);
	*list = repl;
}

/* Highlighted File Record */
struct file_record* hfr(const struct file_view* const fv) {
	return (fv->num_files ?
			fv->file_list[fv->selection] : NULL);
}
