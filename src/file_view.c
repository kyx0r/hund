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
inline bool hidden(const struct file_view* const fv, const fnum_t i) {
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

/* Highlighted File Record */
inline struct file_record* hfr(const struct file_view* const fv) {
	return (fv->num_files ? fv->file_list[fv->selection] : NULL);
}

inline void first_entry(struct file_view* const fv) {
	fv->selection = fv->num_files-1;
	jump_n_entries(fv, -fv->num_files);
}

inline void last_entry(struct file_view* const fv) {
	fv->selection = 0;
	jump_n_entries(fv, fv->num_files);
}

void jump_n_entries(struct file_view* const fv, const int n) {
	if (!fv->num_files) {
		fv->selection = 0;
		return;
	}
	fnum_t N = (n > 0 ? n : -n);
	const int d = (n > 0 ? 1 : -1);
	fnum_t i = fv->selection;
	do {
		if (n < 0 && !i) return;
		i += d;
		if (n > 0 && i >= fv->num_files) return;
		if (visible(fv, i)) {
			N -= 1;
			fv->selection = i;
		}
	} while (N);
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
void file_highlight(struct file_view* const fv, const char* const N) {
	fnum_t i = 0;
	while (i < fv->num_files && strcmp(fv->file_list[i]->file_name, N)) {
		i += 1;
	}
	if (i == fv->num_files) return;
	if (visible(fv, i)) {
		fv->selection = i;
	}
	else {
		first_entry(fv);
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
	const mode_t m = fv->file_list[fv->selection]->s.st_mode;
	int err;
	if (S_ISDIR(m) || S_ISLNK(m)) {
		if ((err = enter_dir(fv->wd, fn))) return err;
	}
	else return ENOTDIR;
	if ((err = file_view_scan_dir(fv))) {
		delete_file_list(fv);
		file_view_up_dir(fv);
		// TODO ^^^ would be better if it was checked
		// before any changes to file_view
		return err;
	}
	first_entry(fv);
	return 0;
}

int file_view_up_dir(struct file_view* const fv) {
	int err;
	char prevdir[NAME_MAX];
	strncpy(prevdir, fv->wd+current_dir_i(fv->wd), NAME_MAX);
	up_dir(fv->wd);
	if ((err = file_view_scan_dir(fv))) {
		delete_file_list(fv);
		return err;
	}
	file_highlight(fv, prevdir);
	return 0;
}

void file_view_afterdel(struct file_view* const fv) {
	if (!fv->num_files) {
		fv->selection = 0;
		return;
	}
	if (fv->selection >= fv->num_files-1) {
		last_entry(fv);
	}
	if (!visible(fv, fv->selection)){
		jump_n_entries(fv, 1);
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
	int err;
	fv->num_selected = 0;
	err = scan_dir(fv->wd, &fv->file_list, &fv->num_files, &fv->num_hidden);
	if (!err) file_view_sort(fv);
	return err;
}

/*
 * Return:
 * -1: a < b
 *  0: a == b
 *  1: a > b
 *  ...just like memcmp,strcmp
 */
static int frcmp(const enum compare cmp,
		const struct file_record* const a,
		const struct file_record* const b) {
	switch (cmp) {
	case CMP_NAME:
		return strcmp(a->file_name, b->file_name);
	case CMP_SIZE:
		return (a->s.st_size < b->s.st_size ? -1 : 1);
	case CMP_DATE:
		return (a->s.st_mtim.tv_sec < b->s.st_mtim.tv_sec ? -1 : 1);
	case CMP_ISDIR:
		if (S_ISDIR(a->s.st_mode) != S_ISDIR(b->s.st_mode)) {
			return (S_ISDIR(b->s.st_mode) ? 1 : -1);
		}
		break;
	case CMP_PERM:
		return ((a->s.st_mode & 07777) - (b->s.st_mode & 07777));
	case CMP_ISEXE:
		if (EXECUTABLE(a->s.st_mode) != EXECUTABLE(b->s.st_mode)) {
			return (EXECUTABLE(b->s.st_mode) ? 1 : -1);
		}
		break;
	default: break;
	}
	return 0;
}

/*
 * D = destination
 * S = source
 */
inline static void merge(const enum compare cmp, const int scending,
		struct file_record** D, struct file_record** S,
		const fnum_t beg, const fnum_t mid, const fnum_t end) {
	fnum_t sa = beg;
	fnum_t sb = mid;
	for (fnum_t d = beg; d < end; ++d) {
		if (sa < mid && sb < end) {
			if (0 >= scending * frcmp(cmp, S[sa], S[sb])) {
				D[d] = S[sa];
				sa += 1;
			}
			else {
				D[d] = S[sb];
				sb += 1;
			}
		}
		else if (sa == mid) {
			D[d] = S[sb];
			sb += 1;
		}
		else if (sb == end) {
			D[d] = S[sa];
			sa += 1;
		}
	}
}

#define MIN(A,B) (((A) < (B)) ? (A) : (B))
void merge_sort(struct file_view* const fv, const enum compare cmp) {
	struct file_record** tmp;
	struct file_record** A = fv->file_list;
	struct file_record** B = calloc(fv->num_files,
			sizeof(struct file_record*));
	for (fnum_t L = 1; L < fv->num_files; L *= 2) {
		for (fnum_t S = 0; S < fv->num_files; S += L+L) {
			const fnum_t beg = S;
			const fnum_t mid = MIN(S+L, fv->num_files);
			const fnum_t end = MIN(S+L+L, fv->num_files);
			merge(cmp, fv->scending, B, A, beg, mid, end);
		}
		tmp = A;
		A = B;
		B = tmp;
	}
	fv->file_list = A;
	free(B);
}

void file_view_sort(struct file_view* const fv) {
	for (size_t i = 0; i < FV_ORDER_SIZE; ++i) {
		if (fv->order[i]) merge_sort(fv, fv->order[i]);
	}
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

void file_view_sorting_changed(struct file_view* const fv) {
	char before[NAME_MAX+1];
	strncpy(before, fv->file_list[fv->selection]->file_name, NAME_MAX+1);
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
