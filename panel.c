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

#include "panel.h"

/*
 * Checks if file is visible at the moment.
 * Invalid selections are assumed to be invisible.
 * Everything on empty list is invisible.
 * These assumptions make visible() more useful.
 *
 * Truth table - is file visible at the moment?
 *          file.ext | .hidden.ext
 * show_hidden = 1 |1|1|
 * show_hidden = 0 |1|0|
 */
bool visible(const struct panel* const fv, const fnum_t i) {
	return fv->num_files && i < fv->num_files
		&& (fv->show_hidden || fv->file_list[i]->name[0] != '.');
}

/* Highlighted File Record */
struct file* hfr(const struct panel* const fv) {
	return (visible(fv, fv->selection)
		? fv->file_list[fv->selection] : NULL);
}

inline void first_entry(struct panel* const fv) {
	fv->selection = fv->num_files-1;
	jump_n_entries(fv, -fv->num_files);
}

inline void last_entry(struct panel* const fv) {
	fv->selection = 0;
	jump_n_entries(fv, fv->num_files);
}

void jump_n_entries(struct panel* const fv, const int n) {
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

inline void delete_file_list(struct panel* const fv) {
	file_list_clean(&fv->file_list, &fv->num_files);
	fv->selection = fv->num_hidden = 0;
}

/*
 * Returns index of given file on list or -1 if not present
 */
fnum_t file_on_list(const struct panel* const fv, const char* const name) {
	fnum_t i = 0;
	while (i < fv->num_files && strcmp(fv->file_list[i]->name, name)) {
		i += 1;
	}
	return (i == fv->num_files ? (fnum_t)-1 : i);
}

/* Finds and highlighs file with given name */
void file_highlight(struct panel* const fv, const char* const N) {
	fnum_t i = 0;
	while (i < fv->num_files && strcmp(fv->file_list[i]->name, N)) {
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

bool file_find(struct panel* const fv, const char* const name,
		const fnum_t start, const fnum_t end) {
	const int d = start <= end ? 1 : -1;
	for (fnum_t i = start; (d > 0 ? i <= end : i >= end); i += d) {
		if (visible(fv, i) && contains(fv->file_list[i]->name, name)) {
			fv->selection = i;
			return true;
		}
		if (i == end) break;
	}
	return false;
}

struct file* panel_select_file(struct panel* const fv) {
	struct file* fr;
	if ((fr = hfr(fv))) {
		if ((fr->selected = !fr->selected)) {
			fv->num_selected += 1;
		}
		else {
			fv->num_selected -= 1;
		}
	}
	return fr;
}

int panel_enter_selected_dir(struct panel* const fv) {
	const struct file* H;
	int err;
	if (!(H = hfr(fv))) return 0;
	if ((err = pushd(fv->wd, &fv->wdlen, H->name, H->nl))
	|| (err = panel_scan_dir(fv))) {
		panel_up_dir(fv);
		return err;
	}
	first_entry(fv);
	return 0;
}

int panel_up_dir(struct panel* const fv) {
	int err;
	char prevdir[NAME_BUF_SIZE];
	xstrlcpy(prevdir, fv->wd+current_dir_i(fv->wd), NAME_BUF_SIZE);
	popd(fv->wd, &fv->wdlen);
	if ((err = panel_scan_dir(fv))) {
		return err;
	}
	file_highlight(fv, prevdir);
	return 0;
}

void panel_toggle_hidden(struct panel* const fv) {
	fv->show_hidden = !fv->show_hidden;
	if (!visible(fv, fv->selection)) {
		first_entry(fv);
	}
	if (fv->show_hidden) return;
	for (fnum_t f = 0; f < fv->num_files; ++f) {
		if (fv->file_list[f]->selected && !visible(fv, f)) {
			fv->file_list[f]->selected = false;
			fv->num_selected -= 1;
		}
	}
}

int panel_scan_dir(struct panel* const fv) {
	int err;
	fv->num_selected = 0;
	err = scan_dir(fv->wd, &fv->file_list, &fv->num_files, &fv->num_hidden);
	if (err) return err;
	panel_sort(fv);
	if (!fv->num_files) {
		fv->selection = 0;
	}
	if (fv->selection >= fv->num_files-1) {
		last_entry(fv);
	}
	if (!visible(fv, fv->selection)){
		jump_n_entries(fv, 1);
	}
	return 0;
}

/*
 * Return:
 * -1: a < b
 *  0: a == b
 *  1: a > b
 *  ...just like memcmp, strcmp
 */
static int frcmp(const enum key cmp,
		const struct file* const a,
		const struct file* const b) {
	const struct passwd *pa, *pb;
	const struct group *ga, *gb;
	switch (cmp) {
	case KEY_NAME:
		return strcmp(a->name, b->name);
	case KEY_SIZE:
		return (a->s.st_size < b->s.st_size ? -1 : 1);
	case KEY_ATIME:
		return (a->s.st_atim.tv_sec < b->s.st_atim.tv_sec ? -1 : 1);
	case KEY_CTIME:
		return (a->s.st_ctim.tv_sec < b->s.st_ctim.tv_sec ? -1 : 1);
	case KEY_MTIME:
		return (a->s.st_mtim.tv_sec < b->s.st_mtim.tv_sec ? -1 : 1);
	case KEY_ISDIR:
		if (S_ISDIR(a->s.st_mode) != S_ISDIR(b->s.st_mode)) {
			return (S_ISDIR(b->s.st_mode) ? 1 : -1);
		}
		break;
	case KEY_PERM:
		return ((a->s.st_mode & 07777) - (b->s.st_mode & 07777));
	case KEY_ISEXE:
		if (EXECUTABLE(a->s.st_mode) != EXECUTABLE(b->s.st_mode)) {
			return (EXECUTABLE(b->s.st_mode) ? 1 : -1);
		}
		break;
	case KEY_INODE:
		return (a->s.st_ino < b->s.st_ino ? -1 : 1);
	case KEY_UID:
		return (a->s.st_uid < b->s.st_uid ? -1 : 1);
	case KEY_GID:
		return (a->s.st_gid < b->s.st_gid ? -1 : 1);
	case KEY_USER:
		if ((pa = getpwuid(a->s.st_uid))
		&& (pb = getpwuid(b->s.st_uid))) {
			return strcmp(pa->pw_name, pb->pw_name);
		}
		break;
	case KEY_GROUP:
		if ((ga = getgrgid(a->s.st_gid))
		&& (gb = getgrgid(b->s.st_gid))) {
			return strcmp(ga->gr_name, gb->gr_name);
		}
		break;
	default:
		break;
	}
	return 0;
}

/*
 * D = destination
 * S = source
 */
inline static void merge(const enum key cmp, const int scending,
		struct file** D, struct file** S,
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

void merge_sort(struct panel* const fv, const enum key cmp) {
	// TODO inplace if possible
	struct file** tmp;
	struct file** A = fv->file_list;
	struct file** B = calloc(fv->num_files,
			sizeof(struct file*));
	for (fnum_t L = 1; L < fv->num_files; L *= 2) {
		for (fnum_t S = 0; S < fv->num_files; S += L+L) {
			const fnum_t mid = MIN(S+L, fv->num_files);
			const fnum_t end = MIN(S+L+L, fv->num_files);
			merge(cmp, fv->scending, B, A, S, mid, end);
		}
		tmp = A;
		A = B;
		B = tmp;
	}
	fv->file_list = A;
	free(B);
}

void panel_sort(struct panel* const fv) {
	for (size_t i = 0; i < FV_ORDER_SIZE; ++i) {
		if (fv->order[i]) merge_sort(fv, fv->order[i]);
	}
}

char* panel_path_to_selected(struct panel* const fv) {
	const struct file* H;
	if (!(H = hfr(fv))) return NULL;
	const size_t wdl = strnlen(fv->wd, PATH_MAX_LEN);
	char* p = malloc(wdl+1+NAME_BUF_SIZE);
	memcpy(p, fv->wd, wdl+1);
	size_t plen = wdl;
	if (pushd(p, &plen, H->name, H->nl)) {
		free(p);
		return NULL;
	}
	return p;
}

void panel_sorting_changed(struct panel* const fv) {
	char before[NAME_BUF_SIZE];
	memcpy(before, fv->file_list[fv->selection]->name,
		fv->file_list[fv->selection]->nl+1);
	panel_sort(fv);
	file_highlight(fv, before);
}

/*
 * If there is no selection, the highlighted file is selected
 */
void panel_selected_to_list(struct panel* const fv,
		struct string_list* const L) {
	fnum_t start = 0;
	fnum_t stop = fv->num_files-1;
	const struct file* fr;
	if (!fv->num_selected) {
		if (!(fr = panel_select_file(fv))) {
			memset(L, 0, sizeof(struct string_list));
			return;
		}
		start = fv->selection;
		stop = fv->selection;
	}
	L->len = 0;
	L->arr = calloc(fv->num_selected, sizeof(struct string*));
	fnum_t f = start, s = 0;
	for (; f <= stop && s < fv->num_selected; ++f) {
		if (!fv->file_list[f]->selected) continue;
		const size_t fnl = fv->file_list[f]->nl;
		L->arr[L->len] = malloc(sizeof(struct string)+fnl+1);
		L->arr[L->len]->len = fnl;
		memcpy(L->arr[L->len]->str, fv->file_list[f]->name, fnl+1);
		L->len += 1;
		s += 1;
	}
}

void select_from_list(struct panel* const fv,
		const struct string_list* const L) {
	for (fnum_t i = 0; i < L->len; ++i) {
		if (!L->arr[i]) continue;
		for (fnum_t s = 0; s < fv->num_files; ++s) {
			if (!strcmp(L->arr[i]->str, fv->file_list[s]->name)) {
				fv->file_list[s]->selected = true;
				fv->num_selected += 1;
				break;
			}
		}
	}
}

void panel_unselect_all(struct panel* const fv) {
	fv->num_selected = 0;
	for (fnum_t f = 0; f < fv->num_files; ++f) {
		fv->file_list[f]->selected = false;
	}
}
/*
 * Needed by rename operation.
 * Checks conflicts with existing files and allows complicated swaps.
 * Pointless renames ('A' -> 'A') are removed from S and R.
 * On unsolvable conflict false is retured and no data is modified.
 *
 * S - selected files
 * R - new names for selected files
 *
 * N - list of names
 * a - list of indexes to N
 * at - length of a
 *
 * TODO optimize; plenty of loops, copying, allocation
 * TODO move somewhere else?
 * TODO inline it (?); only needed once
 * TODO code repetition
 */
bool rename_prepare(const struct panel* const fv,
		struct string_list* const S,
		struct string_list* const R,
		struct string_list* const N,
		struct assign** const a, fnum_t* const at) {
	*at = 0;
	//          vvvvvv TODO calculate size
	*a = calloc(S->len, sizeof(struct assign));
	bool* tofree = calloc(S->len, sizeof(bool));
	for (fnum_t f = 0; f < R->len; ++f) {
		if (memchr(R->arr[f]->str, '/', R->arr[f]->len)) {
			// TODO signal what is wrong
			free(*a);
			*a = NULL;
			*at = 0;
			free(tofree);
			return false;
		}
		struct string* Rs = R->arr[f];
		struct string* Ss = S->arr[f];
		const fnum_t Ri = file_on_list(fv, Rs->str);
		if (Ri == (fnum_t)-1) continue;
		const fnum_t Si = string_on_list(S, Rs->str, Rs->len);
		if (Si != (fnum_t)-1) {
			const fnum_t NSi = string_on_list(N, Ss->str, Ss->len);
			if (NSi == (fnum_t)-1) {
				(*a)[*at].from = list_push(N, Ss->str, Ss->len);
			}
			else {
				(*a)[*at].from = NSi;
			}
			const fnum_t NRi = string_on_list(N, Rs->str, Rs->len);
			if (NRi == (fnum_t)-1) {
				(*a)[(*at)].to = list_push(N, Rs->str, Rs->len);
			}
			else {
				(*a)[(*at)].to = NRi;
			}
			*at += 1;
			tofree[f] = true;
		}
		else {
			free(*a);
			*a = NULL;
			*at = 0;
			free(tofree);
			return false;
		}
	}
	for (fnum_t f = 0; f < R->len; ++f) {
		if (!strcmp(S->arr[f]->str, R->arr[f]->str)) {
			tofree[f] = true;
		}
	}
	for (fnum_t f = 0; f < *at; ++f) {
		for (fnum_t g = 0; g < *at; ++g) {
			if (f == g) continue;
			if ((*a)[f].from == (*a)[g].from
			|| (*a)[f].to == (*a)[g].to) {
				free(*a);
				*a = NULL;
				*at = 0;
				free(tofree);
				return false;
			}
		}
	}
	for (fnum_t f = 0; f < *at; ++f) {
		if ((*a)[f].from == (*a)[f].to) {
			(*a)[f].from = (*a)[f].to = (fnum_t)-1;
		}
	}
	for (fnum_t f = 0; f < R->len; ++f) {
		if (!tofree[f]) continue;
		free(S->arr[f]);
		free(R->arr[f]);
		S->arr[f] = R->arr[f] = NULL;
	}
	free(tofree);
	return true;
}

bool conflicts_with_existing(struct panel* const fv,
		const struct string_list* const list) {
	for (fnum_t f = 0; f < list->len; ++f) {
		if (file_on_list(fv, list->arr[f]->str) != (fnum_t)-1) {
			return true;
		}
	}
	return false;
}

void remove_conflicting(struct panel* const fv,
		struct string_list* const list) {
	struct string_list repl = { NULL, 0 };
	for (fnum_t f = 0; f < list->len; ++f) {
		if (file_on_list(fv, list->arr[f]->str) == (fnum_t)-1) {
			list_push(&repl, list->arr[f]->str, list->arr[f]->len);
		}
	}
	list_free(list);
	*list = repl;
}
