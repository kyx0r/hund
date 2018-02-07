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

#ifndef _DEFAULT_SOURCE
	#define _DEFAULT_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>

#include "include/ui.h"
#include "include/task.h"

/*
 * GENERAL TODO
 * 2. Messages may be blocked by other messages
 * 14. IDEA: Detecting file formats -> display name of a program that
 *     would open highlighted file
 * 15. IDEA: if finding, offer selecting files
 * 16. Dir scanning via task?
 * 18. ln -> create link to highlighted file in the other panel
 * 19. IDEA: quick chmod +x or u+x in MODE_MANAGER
 */

static char* get_editor(void) {
	char* ed = getenv("VISUAL");
	if (!ed) ed = getenv("EDITOR");
	if (!ed) ed = "vi";
	return ed;
}

static int open_file_with(char* const p, char* const f) {
	char exeimg[PATH_MAX];
	strcpy(exeimg, "/usr/bin");
	append_dir(exeimg, p);
	char* const arg[] = { exeimg, f, NULL };
	return spawn(arg);
}

static void open_selected_with(struct ui* const i, char* const w) {
	char* path;
	if ((path = file_view_path_to_selected(i->pv))) {
		const mode_t m = hfr(i->pv)->s.st_mode;
		if (S_ISREG(m) || S_ISLNK(m)) {
			open_file_with(w, path);
		}
		else {
			failed(i, "edit", 0, "not regular file");
		}
		free(path);
	}
}

inline static void open_find(struct ui* const i) {
	char t[NAME_MAX+1];
	char* t_top = t;
	memset(t, 0, NAME_MAX+1);
	i->prch = '/';
	i->prompt = t;
	ui_draw(i);
	int r;
	const fnum_t S = i->pv->selection;
	const fnum_t N = i->pv->num_files;
	for (;;) {
		r = fill_textbox(i, t, &t_top, NAME_MAX);
		if (!r) {
			break;
		}
		else if (r == -1) {
			i->pv->selection = S;
			break;
		}
		else if (r == 2 || r == -2 || t_top != t) {
			fnum_t s = 0; // Start
			fnum_t e = N-1; // End
			if (r == 2 && i->pv->selection < N-1) {
				s = i->pv->selection+1;
				e = N-1;
			}
			else if (r == -2 && i->pv->selection > 0) {
				s = i->pv->selection-1;
				e = 0;
			}
			file_find(i->pv, t, s, e);
		}
		ui_draw(i);
	}
	i->prompt = NULL;
}

#if 0
inline static void estimate_volume_for_selected(struct file_view* const fv) {
	// TODO REDO
	int sink_fc = 0, sink_dc = 0;
	fnum_t f = 0, s = 0;
	char* const opath = strncpy(malloc(PATH_MAX), fv->wd, PATH_MAX);
	const bool sc = !fv->num_selected; // Single Selection
	if (sc) {
		fv->num_selected = 1;
		fv->file_list[fv->selection]->selected = true;
	}
	for (; f < fv->num_files && s < fv->num_selected; ++f) {
		const struct stat* const S = fv->file_list[f]->l;
		if (!fv->file_list[f]->selected
		    || (S && !S_ISDIR(S->st_mode))) continue;
		append_dir(opath, fv->file_list[f]->file_name);
		fv->file_list[f]->dir_volume = 0;
		/*estimate_volume(opath, &fv->file_list[f]->dir_volume,
				&sink_fc, &sink_dc); // TODO*/
		up_dir(opath);
		s += 1;
	}
	free(opath);
	if (sc) {
		fv->num_selected = 0;
		fv->file_list[fv->selection]->selected = false;
		next_entry(fv);
	}
}
#endif

inline static bool _solve_name_conflicts(struct ui* const i,
		struct string_list* const s,
		struct string_list* const r) {
	static const char* const question = "Conflicting target names.";
	static const struct select_option o[] = {
		{ KUTF8("s"), "skip" },
		{ KUTF8("r"), "rename" },
		{ KUTF8("m"), "merge" },
		{ KUTF8("a"), "abort" }
	};
	int d = ui_select(i, question, o, 4);
	if (!d) {
		remove_conflicting(i->sv, s);
		list_copy(r, s);
		return s->len;
	}
	if (d == 1 && s->len) {
		list_copy(r, s);
		bool solved;
		do {
			char tmpn[] = "/tmp/hund.XXXXXXXX";
			int tmpfd = mkstemp(tmpn);
			list_to_file(r, tmpfd);

			open_file_with(get_editor(), tmpn);
			file_to_list(tmpfd, r); // TODO err

			close(tmpfd);
			unlink(tmpn);
			if ((solved = !conflicts_with_existing(i->sv, r))) {
				return true;
			}
		} while (!solved && ui_select(i, question, o, 4));
	}
	else if (d == 2) {
		r->len = s->len;
		r->str = calloc(r->len, sizeof(char*));
		return true;
	}
	free_list(s);
	free_list(r);
	return false;
}

inline static bool _recursive_chmod(struct ui* const i) {
	static const char* const question = "Apply recursively?";
	static const struct select_option o[] = {
		{ KUTF8("n"), "no" },
		{ KUTF8("y"), "yes" },
	};
	return ui_select(i, question, o, 2);
}

static void prepare_task(struct ui* const i, struct task* const t,
		const enum task_type tt) {
	if (!i->pv->num_files) return;
	if (!i->pv->num_selected) {
		hfr(i->pv)->selected = true;
		i->pv->num_selected = 1;
	}
	struct string_list selected = { NULL, 0 };
	struct string_list renamed = { NULL, 0 };
	file_view_selected_to_list(i->pv, &selected);
	if ((tt & (TASK_MOVE | TASK_COPY))
	   && conflicts_with_existing(i->sv, &selected)) {
		if (!_solve_name_conflicts(i, &selected, &renamed)) return;
	}
	else {
		renamed.len = selected.len;
		renamed.str = calloc(renamed.len, sizeof(char*));
	}
	enum task_flags tf = TF_NONE;
	// TODO ask for recursion only for directories
	if (tt == TASK_CHMOD && _recursive_chmod(i)) {
		tf |= TF_RECURSIVE_CHMOD;
	}
	task_new(t, tt, tf, i->pv->wd, i->sv->wd, &selected, &renamed);
	if (tt == TASK_CHMOD) {
		t->chp = i->plus;
		t->chm = i->minus;
		t->cho = ((i->o[0] == i->o[1]) ? (uid_t)-1 : i->o[1]);
		t->chg = ((i->g[0] == i->g[1]) ? (gid_t)-1 : i->g[1]);
		chmod_close(i);
	}
}

inline static void _rename(struct ui* const i) {
	if (!i->pv->num_selected) {
		hfr(i->pv)->selected = true;
		i->pv->num_selected += 1;
	}
	char tmpn[] = "/tmp/hund.XXXXXXXX";
	int err;
	struct string_list sf = { NULL, 0 }; // Selected Files
	struct string_list rf = { NULL, 0 }; // Renamed Files
	static const struct select_option o[] = {
		{ KUTF8("n"), "no" },
		{ KUTF8("y"), "yes" },
		{ KUTF8("a"), "abort" }
	};
	int tmpfd = mkstemp(tmpn);
	file_view_selected_to_list(i->pv, &sf);
	err = list_to_file(&sf, tmpfd);
	bool unsolved;
	do {
		free_list(&rf);
		open_file_with(get_editor(), tmpn);
		file_to_list(tmpfd, &rf); // TODO
		if ((unsolved = duplicates_on_list(&rf))) {
			if (!ui_select(i, "There are conflicts. Retry?", o, 2)) {
				free_list(&sf);
				free_list(&rf);
				return;
			}
		}
	} while (unsolved);
	// TODO conflicts_with_existing(i->pv, &rf));
	char opath[PATH_MAX+1];
	char npath[PATH_MAX+1];
	if (blank_lines(&rf)) {
		failed(i, "rename", 0, "file contains blank lines");
	}
	else if (sf.len > rf.len) {
		failed(i, "rename", 0, "file does not contain enough lines");
	}
	else if (sf.len < rf.len) {
		failed(i, "rename", 0, "file contains too much lines");
	}
	else {
		for (fnum_t f = 0; f < sf.len; ++f) {
			if (!strcmp(sf.str[f], rf.str[f])) continue;
			strncpy(opath, i->pv->wd, PATH_MAX);
			strncpy(npath, i->pv->wd, PATH_MAX);
			if (((err = EINVAL, contains(rf.str[f], "/"))
			  || (err = append_dir(opath, sf.str[f]))
			  || (err = append_dir(npath, rf.str[f]))
			  || (rename(opath, npath) ? (err = errno) : 0))
			  && err) {
				failed(i, "rename", err, NULL);
			}
		}
		// TODO what if selection is invalid?
		file_view_scan_dir(i->pv); // TODO error
		select_from_list(i->pv, &rf);
	}
	close(tmpfd);
	unlink(tmpn);
	free_list(&sf);
	free_list(&rf);
}

inline static void _cd(struct ui* const i) {
	// TODO
	int err = 0;
	struct stat s;
	char* path = strncpy(malloc(PATH_MAX+1), i->pv->wd, PATH_MAX);
	char* cdp = calloc(PATH_MAX+1, sizeof(char));
	if (prompt(i, cdp, cdp, PATH_MAX)
	 || (err = enter_dir(path, cdp))
	 || (err = ENOENT, access(path, F_OK))
	 || (stat(path, &s) ? (err = errno) : 0)
	 || (err = ENOTDIR, !S_ISDIR(s.st_mode))) {
		if (err) failed(i, "cd", err, NULL);
	}
	else {
		strncpy(i->pv->wd, path, PATH_MAX);
		if ((err = file_view_scan_dir(i->pv))) {
			failed(i, "directory scan", err, NULL);
		}
		else {
			first_entry(i->pv);
		}
	}
	free(path);
	free(cdp);
}

inline static void _mkdir(struct ui* const i) {
	int err;
	struct string_list files = { NULL, 0 };
	char *path = NULL;
	char tmpn[] = "/tmp/hund.XXXXXXXX";
	int tmpfd = mkstemp(tmpn);
	open_file_with(get_editor(), tmpn);
	file_to_list(tmpfd, &files);
	for (fnum_t f = 0; f < files.len; ++f) {
		if (!files.str[f]) continue; // Blank lines are ignored
		path = strncpy(malloc(PATH_MAX+1), i->pv->wd, PATH_MAX);
		if (((err = EINVAL, contains(files.str[f], "/"))
		 || (err = append_dir(path, files.str[f]))
		 || (mkdir(path, MKDIR_DEFAULT_PERM) ? (err = errno) : 0))
		 && err) {
			failed(i, "creating directory", err, NULL);
		}
		free(path);
	}
	free_list(&files);
	close(tmpfd);
	unlink(tmpn);
	file_view_scan_dir(i->pv); // TODO error
}

void change_sorting(struct ui* const i) {
	// TODO visibility
	// -- SORT --
	// ???
	// TODO cursor
	char old[FV_ORDER_SIZE];
	memcpy(old, i->pv->order, FV_ORDER_SIZE);
	char* buf = i->pv->order;
	char* top = i->pv->order + strlen(buf);
	int r;
	for (;;) {
		r = fill_textbox(i, buf, &top, FV_ORDER_SIZE);
		if (!r) break;
		else if (r == -1) {
			memcpy(i->pv->order, old, FV_ORDER_SIZE);
			return;
		}
		ui_draw(i);
	}
	for (size_t j = 0; j < FV_ORDER_SIZE; ++j) {
		bool v = false;
		for (size_t k = 0; k < strlen(compare_values); ++k) {
			v = v || (compare_values[k] == i->pv->order[j]);
		}
		if (!v) {
			i->pv->order[j] = 0;
		}
	}
	for (size_t j = 0; j < FV_ORDER_SIZE; ++j) {
		if (i->pv->order[0]) continue;
		memmove(&i->pv->order[0], &i->pv->order[1], FV_ORDER_SIZE-1);
		i->pv->order[7] = 0;
	}
	file_view_sorting_changed(i->pv);
	i->ui_needs_refresh = true;
}

static void process_input(struct ui* const i, struct task* const t) {
	char *path = NULL, *name = NULL;
	struct file_view* tmp = NULL;
	struct file_record* fr = NULL; // File Record
	int err = 0;
	const enum command cmd = get_cmd(i);
	switch (cmd) {
	/* HELP */
	case CMD_HELP_QUIT:
		i->m = MODE_MANAGER;
		break;
	case CMD_HELP_DOWN:
		i->helpy += 1; // TODO
		break;
	case CMD_HELP_UP:
		if (i->helpy > 0) {
			i->helpy -= 1;
		}
		break;
	/* CHMOD */
	case CMD_RETURN:
		chmod_close(i);
		break;
	case CMD_CHANGE:
		prepare_task(i, t, TASK_CHMOD);
		break;
	case CMD_CHOWN:
		/* TODO in $VISUAL */
		name = calloc(LOGIN_NAME_MAX+1, sizeof(char));
		if (!prompt(i, name, name, LOGIN_NAME_MAX)) {
			errno = 0;
			struct passwd* pwd = getpwnam(name);
			if (!pwd) failed(i, "chown", 0,
					"Such user does not exist");
			else {
				i->o[1] = pwd->pw_uid;
				strncpy(i->user, pwd->pw_name, LOGIN_NAME_MAX);
			}
		}
		free(name);
		break;
	case CMD_CHGRP:
		/* TODO in $VISUAL */
		name = calloc(LOGIN_NAME_MAX+1, sizeof(char));
		if (!prompt(i, name, name, LOGIN_NAME_MAX)) {
			errno = 0;
			struct group* grp = getgrnam(name);
			if (!grp) failed(i, "chgrp", 0,
					"Such group does not exist");
			else {
				i->g[1] = grp->gr_gid;
				strncpy(i->group, grp->gr_name, LOGIN_NAME_MAX);
			}
		}
		free(name);
		break;

	#define PLUS(m) i->plus |= (m); i->minus &= ~(m);
	#define MINUS(m) i->minus |= (m); i->plus &= ~(m);
	#define RESET(m) i->plus &= ~(m); i->minus &= ~(m);

	case CMD_A_PLUS_R: PLUS(S_IRUSR | S_IRGRP | S_IROTH); break;
	case CMD_A_MINUS_R: MINUS(S_IRUSR | S_IRGRP | S_IROTH); break;

	case CMD_A_PLUS_W: PLUS(S_IWUSR | S_IWGRP | S_IWOTH); break;
	case CMD_A_MINUS_W: MINUS(S_IWUSR | S_IWGRP | S_IWOTH); break;

	case CMD_A_PLUS_X: PLUS(S_IXUSR | S_IXGRP | S_IXOTH); break;
	case CMD_A_MINUS_X: MINUS(S_IXUSR | S_IXGRP | S_IXOTH); break;

	case CMD_U_PLUS_R: PLUS(S_IRUSR); break;
	case CMD_U_MINUS_R: MINUS(S_IRUSR); break;

	case CMD_U_PLUS_W: PLUS(S_IWUSR); break;
	case CMD_U_MINUS_W: MINUS(S_IWUSR) break;

	case CMD_U_PLUS_X: PLUS(S_IXUSR); break;
	case CMD_U_MINUS_X: MINUS(S_IXUSR); break;

	case CMD_U_PLUS_IOX: PLUS(S_ISUID); break;
	case CMD_U_MINUS_IOX: MINUS(S_ISUID); break;

	case CMD_G_PLUS_R: PLUS(S_IRGRP); break;
	case CMD_G_MINUS_R: MINUS(S_IRGRP); break;

	case CMD_G_PLUS_W: PLUS(S_IWGRP); break;
	case CMD_G_MINUS_W: MINUS(S_IWGRP); break;

	case CMD_G_PLUS_X: PLUS(S_IXGRP); break;
	case CMD_G_MINUS_X: MINUS(S_IXGRP); break;

	case CMD_G_PLUS_IOX: PLUS(S_ISGID); break;
	case CMD_G_MINUS_IOX: MINUS(S_ISGID); break;

	case CMD_O_PLUS_R: PLUS(S_IROTH); break;
	case CMD_O_MINUS_R: MINUS(S_IROTH); break;

	case CMD_O_PLUS_W: PLUS(S_IWOTH); break;
	case CMD_O_MINUS_W: MINUS(S_IWOTH); break;

	case CMD_O_PLUS_X: PLUS(S_IXOTH); break;
	case CMD_O_MINUS_X: MINUS(S_IXOTH); break;

	case CMD_O_PLUS_SB: PLUS(S_ISVTX); break;
	case CMD_O_MINUS_SB: MINUS(S_ISVTX); break;

	case CMD_U_ZERO: MINUS(S_IRUSR | S_IWUSR | S_IXUSR); break;
	case CMD_G_ZERO: MINUS(S_IRGRP | S_IWGRP | S_IXGRP); break;
	case CMD_O_ZERO: MINUS(S_IROTH | S_IWOTH | S_IXOTH); break;

	case CMD_U_RESET: RESET(S_IRUSR | S_IWUSR | S_IXUSR); break;
	case CMD_G_RESET: RESET(S_IRGRP | S_IWGRP | S_IXGRP); break;
	case CMD_O_RESET: RESET(S_IROTH | S_IWOTH | S_IXOTH); break;
	// TODO undef

	/* WAIT */
	case CMD_TASK_QUIT:
		//t->done = true; // TODO
		break;
	case CMD_TASK_PAUSE:
		t->ts = TS_PAUSED;
		break;
	case CMD_TASK_RESUME:
		t->ts = TS_RUNNING;
		break;
	/* MANAGER */
	case CMD_QUIT:
		i->run = false;
		break;
	case CMD_HELP:
		i->m = MODE_HELP;
		break;
	case CMD_SWITCH_PANEL:
		tmp = i->pv;
		i->pv = i->sv;
		i->sv = tmp;
		if (i->pv->num_files && !visible(i->pv, i->pv->selection)) {
			first_entry(i->pv);
		}
		break;
	case CMD_DUP_PANEL:
		// TODO
		strncpy(i->sv->wd, i->pv->wd, PATH_MAX);
		if ((err = file_view_scan_dir(i->sv))) {
			delete_file_list(i->sv);
			file_view_up_dir(i->sv);
		}
		else {
			i->sv->selection = i->pv->selection;
			i->sv->show_hidden = i->pv->show_hidden;
		}
		break;
	case CMD_ENTRY_DOWN:
		jump_n_entries(i->pv, 1);
		break;
	case CMD_ENTRY_UP:
		jump_n_entries(i->pv, -1);
		break;
	case CMD_SCREEN_DOWN:
		jump_n_entries(i->pv, i->ph-1);
		break;
	case CMD_SCREEN_UP:
		jump_n_entries(i->pv, -(i->ph-1));
		break;
	case CMD_ENTER_DIR:
		err = file_view_enter_selected_dir(i->pv);
		if (err && err != ENOTDIR) failed(i, "enter dir", err, NULL);
		break;
	case CMD_UP_DIR:
		err = file_view_up_dir(i->pv);
		if (err) failed(i, "up dir", err, NULL);
		break;
	case CMD_COPY:
		prepare_task(i, t, TASK_COPY);
		break;
	case CMD_MOVE:
		prepare_task(i, t, TASK_MOVE);
		break;
	case CMD_REMOVE:
		prepare_task(i, t, TASK_REMOVE);
		break;
	case CMD_PAGER:
		name = getenv("PAGER");
		open_selected_with(i, (name ? name : "less"));
		break;
	case CMD_EDIT_FILE:
		open_selected_with(i, get_editor());
		break;
	case CMD_OPEN_FILE:
		open_selected_with(i, "xdg-open"); // TODO
		break;
	case CMD_CD:
		_cd(i);
		break;
	case CMD_CREATE_DIR:
		_mkdir(i);
		break;
	case CMD_RENAME:
		_rename(i);
		break;
	case CMD_LINK:
		break;
	case CMD_DIR_VOLUME:
		//estimate_volume_for_selected(i->pv);
		break;
	case CMD_SELECT_FILE: // TODO
		if ((fr = hfr(i->pv)) && visible(i->pv, i->pv->selection)) {
			if ((fr->selected = !fr->selected)) {
				i->pv->num_selected += 1;
			}
			else {
				i->pv->num_selected -= 1;
			}
			jump_n_entries(i->pv, 1);
		}
		break;
	case CMD_SELECT_ALL:
		i->pv->num_selected = 0;
		for (fnum_t f = 0; f < i->pv->num_files; ++f) {
			if (visible(i->pv, f)) {
				i->pv->file_list[f]->selected = true;
				i->pv->num_selected += 1;
			}
		}
		break;
	case CMD_SELECT_NONE:
		i->pv->num_selected = 0;
		for (fnum_t f = 0; f < i->pv->num_files; ++f) {
			i->pv->file_list[f]->selected = false;
		}
		break;
	case CMD_FIND:
		open_find(i);
		break;
	case CMD_ENTRY_FIRST:
		first_entry(i->pv);
		break;
	case CMD_ENTRY_LAST:
		last_entry(i->pv);
		break;
	case CMD_CHMOD:
		// TODO multiple selection
		if ((path = file_view_path_to_selected(i->pv))) {
			chmod_open(i, path);
		}
		else {
			failed(i, "chmod", ENAMETOOLONG, NULL);
		}
		break;
	case CMD_TOGGLE_HIDDEN:
		file_view_toggle_hidden(i->pv);
		break;
	case CMD_REFRESH:
		if ((err = file_view_scan_dir(i->pv))) {
			failed(i, "refresh", err, NULL);
		}
		else {
			file_view_afterdel(i->pv);
		}
		i->ui_needs_refresh = true;
		break;
	case CMD_SORT_REVERSE:
		i->pv->scending = -i->pv->scending;
		file_view_sorting_changed(i->pv);
		i->ui_needs_refresh = true;
		break;
	case CMD_SORT_CHANGE:
		change_sorting(i);
		break;
	default:
		break;
	}
}

static void task_progress(struct ui* const i,
		struct task* const t,
		const char* const S) {
	i->mt = MSG_INFO;
	int n = snprintf(i->msg, MSG_BUFFER_SIZE, // TODO
			"%s %d/%df, %d/%dd", S,
			t->files_done, t->files_total,
			t->dirs_done, t->dirs_total);
	if (t->t & (TASK_REMOVE | TASK_MOVE | TASK_COPY)) {
		char sdone[SIZE_BUF_SIZE];
		char stota[SIZE_BUF_SIZE];
		pretty_size(t->size_done, sdone);
		pretty_size(t->size_total, stota);
		snprintf(i->msg+n, MSG_BUFFER_SIZE-n,
			", %s/%s", sdone, stota);
	}
}

/* Return value: false = abort, true = go on */
inline static bool _conflict_policy(struct ui* const i,
		struct task* const t) {
	char question[64];
	snprintf(question, sizeof(question),
			"There are %d conflicts",
			t->conflicts);
	static const struct select_option o[] = {
		{ KUTF8("s"), "skip" },
		{ KUTF8("o"), "overwrite" },
		{ KUTF8("a"), "abort" }
	};
	switch (ui_select(i, question, o, 3)) {
	case 0:
		t->tf |= TF_SKIP_CONFLICTS;
		break;
	case 1:
		t->tf |= TF_OVERWRITE_CONFLICTS;
		break;
	case 2:
		return false;
	default:
		break;
	}
	return true;
}

inline static bool _symlink_policy(struct ui* const i,
		struct task* const t) {
	char question[64];
	snprintf(question, sizeof(question),
			"There are %d symlinks", t->symlinks);
	static const struct select_option o[] = {
		{ KUTF8("r"), "raw" },
		{ KUTF8("c"), "recalculate" },
		{ KUTF8("d"), "dereference" },
		{ KUTF8("s"), "skip" },
		{ KUTF8("a"), "abort" }
	};
	switch (ui_select(i, question, o, 5)) {
	case 0:
		t->tf |= TF_RAW_LINKS;
		break;
	case 2:
		t->tf |= TF_DEREF_LINKS;
		break;
	case 3:
		t->tf |= TF_SKIP_LINKS;
		break;
	case 4:
		return false;
	case 1:
	default:
		break;
	}
	return true;
}

inline static bool _confirm_removal(struct ui* const i,
		struct task* const t) {
	static const struct select_option o[] = {
		{ KUTF8("n"), "no" },
		{ KUTF8("y"), "yes" }
	};
	char question[80];
	snprintf(question, sizeof(question),
			"Remove %u files?", t->files_total);
	switch (ui_select(i, question, o, 2)) {
	case 1: return true;
	default:
	case 0: return false;
	}
}

static void task_execute(struct ui* const i, struct task* const t) {
	// TODO error handling is chaotic
	task_action ta = NULL;
	int top;
	char msg[128]; // TODO
	static const struct select_option o[] = {
		{ KUTF8("n"), "no" },
		{ KUTF8("y"), "yes" },
	};
	switch (t->ts) {
	case TS_CLEAN:
		break;
	case TS_ESTIMATE:
		i->timeout = 500;
		i->m = MODE_WAIT;
		task_do(t, 1024*10, task_action_estimate, TS_CONFIRM);
		if (t->err) t->ts = TS_FAILED;
		break;
	case TS_CONFIRM:
		t->ts = TS_RUNNING;
		if (t->t == TASK_REMOVE && !_confirm_removal(i, t)) {
			t->ts = TS_FINISHED;
		}
		else if (t->t & (TASK_COPY | TASK_MOVE)) {
			if ((t->conflicts && !_conflict_policy(i, t))
			   || (t->symlinks && !_symlink_policy(i, t))) {
				t->ts = TS_FINISHED;
			}
		}
		break;
	case TS_RUNNING:
		i->timeout = 500;
		if (t->t & (TASK_REMOVE | TASK_COPY | TASK_MOVE)) {
			ta = task_action_copyremove;
		}
		else if (t->t == TASK_CHMOD) {
			ta = task_action_chmod;
		}
		task_do(t, 1024*10, ta, TS_FINISHED);
		task_progress(i, t, ">>");
		break;
	case TS_PAUSED:
		i->timeout = -1;
		task_progress(i, t, "||");
		break;
	case TS_FAILED: // TODO
		top = snprintf(msg, sizeof(msg),
				"(%d) %s at",
				t->err, strerror(t->err));
		snprintf(msg+top, sizeof(msg)-top, "%.*s",
				(int)utf8_w2nb(t->tw.cpath,
					sizeof(msg)-top), t->tw.cpath);
		t->ts = (ui_select(i, msg, o, 2)
				? TS_RUNNING : TS_FINISHED);
		break;
	case TS_FINISHED:
		i->timeout = -1;
		if ((t->err = file_view_scan_dir(i->pv))
		   || (t->err = file_view_scan_dir(i->sv))) {
			failed(i, "operation", t->err, NULL);
		}
		else {
			if (t->t == TASK_REMOVE) file_view_afterdel(i->pv);
			else if (t->t == TASK_MOVE) jump_n_entries(i->pv, -1);
			i->mt = MSG_INFO;
			snprintf(i->msg, MSG_BUFFER_SIZE, "processed %u files",
					t->files_done);
		}
		task_clean(t);
		i->ui_needs_refresh = true;
		i->m = MODE_MANAGER;
		break;
	default:
		break;
	}
}

inline static int _init_wd(struct file_view fvs[2], char* const init_wd[2]) {
	int e;
	for (int v = 0; v < 2; ++v) {
		const char* const d = (init_wd[v] ? init_wd[v] : "");
		if (!getcwd(fvs[v].wd, PATH_MAX)) {
			memcpy(fvs[v].wd, "/", 2);
		}
		else if ((e = enter_dir(fvs[v].wd, d))) {
			return e;
		}
		const size_t s = strnlen(fvs[v].wd, PATH_MAX)-1;
		if (s && fvs[v].wd[s] == '/') {
			fvs[v].wd[s] = 0;
		}
		if ((e = file_view_scan_dir(&fvs[v]))) {
			return e;
		}
		first_entry(&fvs[v]);
	}
	return 0;
}

extern struct ui* I;

int main(int argc, char* argv[]) {
	static const char* const help = \
	"Usage: hund [OPTION...] [left panel] [right panel]\n"
	"Options:\n"
	"  -c, --chdir=PATH      change initial directory\n"
	"  -h, --help            display this help message\n"
	"Type `?` while in hund for more help\n";

	static const char sopt[] = "hc:";
	static const struct option lopt[] = {
		{"chdir", required_argument, 0, 'c'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	int o, opti = 0;
	int err;
	while ((o = getopt_long(argc, argv, sopt, lopt, &opti)) != -1) {
		switch (o) {
		case 'c':
			if (chdir(optarg)) {
				err = errno;
				fprintf(stderr, "chdir failed:"
						" %s (%d)\n",
						strerror(err), err);
				exit(EXIT_FAILURE);
			}
			break;
		case 'h':
			printf("%s\n", help);
			exit(EXIT_SUCCESS);
		default:
			exit(EXIT_FAILURE);
		}
	}

	/* If user passed initial directories in cmdline,
	 * save these paths for later
	 */
	char* init_wd[2] = { NULL, NULL };
	int init_wd_top = 0;
	while (optind < argc) {
		if (init_wd_top < 2) {
			init_wd[init_wd_top] = argv[optind];
			init_wd_top += 1;
		}
		else {
			// Only two panels, only two paths allowed
			fprintf(stderr, "invalid argument:"
					" '%s'\n", argv[optind]);
			exit(EXIT_FAILURE);
		}
		optind += 1;
	}

	struct file_view fvs[2];
	memset(fvs, 0, sizeof(fvs));
	fvs[0].scending = 1;
	fvs[0].order[0] = CMP_NAME;
	fvs[0].order[1] = CMP_ISEXE;
	fvs[0].order[2] = CMP_ISDIR;

	fvs[1].scending = 1;
	fvs[1].order[0] = CMP_NAME;
	fvs[1].order[1] = CMP_ISEXE;
	fvs[1].order[2] = CMP_ISDIR;

	struct ui i;
	ui_init(&i, &fvs[0], &fvs[1]);
	if ((err = _init_wd(fvs, init_wd))) {
		fprintf(stderr, "failed to initalize: (%d) %s\n",
				err, strerror(err));
		ui_end(&i);
		exit(EXIT_FAILURE);
	}

	struct task t;
	memset(&t, 0, sizeof(struct task));
	t.in = t.out = -1; // TODO

	i.mt = MSG_INFO;
	strcpy(i.msg, "Type ? for help and license notice.");
	while (i.run) { // TODO but task may be still running
		if (i.ui_needs_refresh) {
			ui_update_geometry(&i);
		}
		ui_draw(&i);
		process_input(&i, &t);
		task_execute(&i, &t);
	}
	for (int v = 0; v < 2; ++v) {
		delete_file_list(&fvs[v]);
	}
	task_clean(&t);
	ui_end(&i);
	memset(fvs, 0, sizeof(fvs));
	memset(&t, 0, sizeof(struct task));
	exit(EXIT_SUCCESS);
}
