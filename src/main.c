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
 * 1. Likes to segfault while doing anything on root's/special files.
 * 2. Messages may be blocked by other messages
 * 3. Okay,
 *    - PATH_MAX (4096) includes null-terminator
 *    - NAME_MAX (255) does not
 *    - LOGIN_NAME_MAX (32) does not
 * 8. Configuration of copy/move/remove operation
 *    - merge on conflict
 *    - copy raw links
 *    - dereference links
 * 9. Error handling.
 * 10. Transfer speed and % done
 * 11. Select needs to be less ambiguous (hints, like y=yes, a=abort, ^N=next)
 * 13. Actions like estimating volume or renaming
 *     should have visible progress too
 * 14. IDEA: Detecting file formats -> display name of a program that
 *      would open highlighted file
 */

static int editor(char* const path) {
	char exeimg[PATH_MAX];
	strcpy(exeimg, "/usr/bin");
	const char* ed = getenv("VISUAL");
	if (!ed) ed = getenv("EDITOR");
	append_dir(exeimg, ed ? ed : "vi");
	char* const arg[] = { exeimg, path, NULL };
	return spawn(arg);
}

static int pager(char* const path) {
	char exeimg[PATH_MAX];
	strcpy(exeimg, "/usr/bin");
	char* const pager = getenv("PAGER");
	append_dir(exeimg, pager ? pager : "less");
	char* const arg[] = { exeimg, path, NULL };
	return spawn(arg);
}

inline static void open_find(struct ui* const i) {
	char t[NAME_MAX+1];
	char* t_top = t;
	memset(t, 0, NAME_MAX+1);
	const fnum_t sbfc = i->pv->selection;
	i->prch = '/';
	i->prompt = t;
	ui_draw(i);
	int r;
	do {
		r = fill_textbox(i, t, &t_top, NAME_MAX);
		if (r == -1) i->pv->selection = sbfc;
		else if (r == 2 || r == -2 || t_top != t) {
			fnum_t s = 0; // Start
			fnum_t e = i->pv->num_files-1; // End
			if (r == 2 && i->pv->selection < i->pv->num_files-1) {
				s = i->pv->selection+1;
				e = i->pv->num_files-1;
			}
			else if (r == -2 && i->pv->selection > 0) {
				s = i->pv->selection-1;
				e = 0;
			}
			file_find(i->pv, t, s, e);
		}
		ui_draw(i);
	} while (r && r != -1);
	i->prompt = NULL;
}

/* Only solves copy/move/remove conflicts */
inline static bool _solve_conflicts(struct ui* const i,
		enum task_flags* const t,
		struct string_list* const s,
		struct string_list* const r) {
	static const char* const question = "There are conflicts.";
	static const struct select_option o[] = {
		{ KUTF8("s"), "skip" },
		{ KUTF8("r"), "rename" },
		{ KUTF8("m"), "merge" },
		{ KUTF8("a"), "abort" }
	};
	static const struct select_option m[] = {
		{ KUTF8("s"), "skip" },
		{ KUTF8("o"), "overwrite" },
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

			editor(tmpn);
			file_to_list(tmpfd, r); // TODO err

			close(tmpfd);
			unlink(tmpn);
			if ((solved = !conflicts_with_existing(i->sv, r))) {
				return true;
			}
		} while (!solved && ui_select(i, question, o, 4));
	}
	if (d == 2) {
		*t |= TF_MERGE;
		if (!ui_select(i, "How to handle conflicts?", m, 2)) {
			*t |= TF_SKIP_CONFLICTS;
		}
		return true;
	}
	free_list(s);
	free_list(r);
	return false;
}

static void prepare_long_task(struct ui* const i, struct task* const t,
		enum task_type tt) {
	// TODO error handling
	if (!i->pv->num_files) return;
	if (!i->pv->num_selected) {
		hfr(i->pv)->selected = true;
		i->pv->num_selected = 1;
	}
	struct string_list selected = { NULL, 0 };
	struct string_list renamed = { NULL, 0 };
	file_view_selected_to_list(i->pv, &selected);
	static const struct select_option o[] = {
		{ KUTF8("n"), "no" },
		{ KUTF8("y"), "yes" }
	};
	enum task_flags tf = TF_RAW_LINKS;
	if (tt == TASK_REMOVE && !ui_select(i, "Remove?", o, 2)) return;
	if (tt == TASK_MOVE || tt == TASK_COPY) {
		if (conflicts_with_existing(i->sv, &selected)) {
			if (!_solve_conflicts(i, &tf, &selected, &renamed)) return;
		}
		else {
			renamed.len = selected.len;
			renamed.str = calloc(renamed.len, sizeof(char*));
		}
	}
	task_new(t, tt, i->pv->wd, i->sv->wd, &selected, &renamed);
	t->tf = tf;
	estimate_volume_for_list(i->pv->wd, &selected,
			&(t->size_total), &(t->files_total),
			&(t->dirs_total));
	i->m = MODE_WAIT;
}

static void process_input(struct ui* const i, struct task* const t) {
	struct file_view* tmp = NULL;
	char *path = NULL, *cdp = NULL, *name = NULL,
	     *opath = NULL, *npath = NULL;
	int err = 0;
	int sink_fc, sink_dc; // TODO FIXME
	struct file_record* fr = NULL; // File Record
	char tmpn[] = "/tmp/hund.XXXXXXXX";
	int tmpfd;
	struct stat s;
	struct string_list files = { NULL, 0 };
	struct string_list sf = { NULL, 0 }; // Selected Files
	struct string_list rf = { NULL, 0 }; // Renamed Files
	static const struct select_option o[] = {
		{ KUTF8("n"), "no" },
		{ KUTF8("y"), "yes" },
		{ KUTF8("a"), "abort" }
	};
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
		// TODO multiple selection
		// TODO recursive ("Apply changes recursively?")
		if (chmod(i->path, i->perm[1])) {
			failed(i, "chmod", errno, NULL);
		}
		if (lchown(i->path, i->o[1], i->g[1])) {
			failed(i, "chmod", errno, NULL);
		}
		if ((err = file_view_scan_dir(i->pv))) {
			failed(i, "directory scan", err, NULL);
		}
		file_view_sort(i->pv);
		chmod_close(i);
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
	case CMD_TOGGLE_UIOX: i->perm[1] ^= S_ISUID; break;
	case CMD_TOGGLE_GIOX: i->perm[1] ^= S_ISGID; break;
	case CMD_TOGGLE_SB: i->perm[1] ^= S_ISVTX; break;
	case CMD_TOGGLE_UR: i->perm[1] ^= S_IRUSR; break;
	case CMD_TOGGLE_UW: i->perm[1] ^= S_IWUSR; break;
	case CMD_TOGGLE_UX: i->perm[1] ^= S_IXUSR; break;
	case CMD_TOGGLE_GR: i->perm[1] ^= S_IRGRP; break;
	case CMD_TOGGLE_GW: i->perm[1] ^= S_IWGRP; break;
	case CMD_TOGGLE_GX: i->perm[1] ^= S_IXGRP; break;
	case CMD_TOGGLE_OR: i->perm[1] ^= S_IROTH; break;
	case CMD_TOGGLE_OW: i->perm[1] ^= S_IWOTH; break;
	case CMD_TOGGLE_OX: i->perm[1] ^= S_IXOTH; break;
	/* WAIT */
	case CMD_TASK_QUIT:
		t->done = true; // TODO
		break;
	case CMD_TASK_PAUSE:
		t->paused = true;
		break;
	case CMD_TASK_RESUME:
		t->paused = false;
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
		break;
	case CMD_ENTRY_DOWN:
		next_entry(i->pv);
		break;
	case CMD_ENTRY_UP:
		prev_entry(i->pv);
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
		prepare_long_task(i, t, TASK_COPY);
		break;
	case CMD_MOVE:
		prepare_long_task(i, t, TASK_MOVE);
		break;
	case CMD_REMOVE:
		prepare_long_task(i, t, TASK_REMOVE);
		break;
	case CMD_EDIT_FILE:
		if ((path = file_view_path_to_selected(i->pv))) {
			if (S_ISREG(hfr(i->pv)->l->st_mode)) {
				editor(path);
			}
			else {
				failed(i, "edit", 0, "not regular file");
			}
			free(path);
		}
		break;
	case CMD_OPEN_FILE:
		if ((path = file_view_path_to_selected(i->pv))) {
			if (S_ISREG(hfr(i->pv)->l->st_mode)) {
				pager(path);
			}
			else {
				failed(i, "open", 0, "not regular file");
			}
			free(path);
		}
		break;
	case CMD_CD:
		path = strncpy(malloc(PATH_MAX+1), i->pv->wd, PATH_MAX);
		cdp = calloc(PATH_MAX+1, sizeof(char));
		if ((prompt(i, cdp, cdp, PATH_MAX)
		   || (err = ENAMETOOLONG, enter_dir(path, cdp))
		   || (err = ENOENT, access(path, F_OK))
		   || (stat(path, &s) ? (err = errno) : 0)
		   || (err = ENOTDIR, !S_ISDIR(s.st_mode)))
		   && err) {
			failed(i, "cd", err, NULL);
		}
		else {
			strncpy(i->pv->wd, path, PATH_MAX);
			if ((err = file_view_scan_dir(i->pv))) {
				failed(i, "directory scan", err, NULL);
			}
			else {
				file_view_sort(i->pv);
				first_entry(i->pv);
			}
		}
		free(path);
		free(cdp);
		break;
	case CMD_CREATE_DIR:
		tmpfd = mkstemp(tmpn);
		editor(tmpn);
		file_to_list(tmpfd, &files);
		for (fnum_t f = 0; f < files.len; ++f) {
			if (!files.str[f]) continue; // Blank lines are ignored
			path = strncpy(malloc(PATH_MAX+1), i->pv->wd, PATH_MAX);
			if (((err = EINVAL, contains(files.str[f], "/"))
			   || (err = append_dir(path, files.str[f]))
			   || (mkdir(path, MKDIR_DEFAULT_PERM) ? (err=errno) : 0))
			   && err) {
				failed(i, "creating directory", err, NULL);
			}
			free(path);
		}
		free_list(&files);
		close(tmpfd);
		unlink(tmpn);
		file_view_scan_dir(i->pv);
		file_view_sort(i->pv);
		break;
	case CMD_RENAME:
		if (!i->pv->num_selected) {
			hfr(i->pv)->selected = true;
			i->pv->num_selected += 1;
		}
		tmpfd = mkstemp(tmpn);
		file_view_selected_to_list(i->pv, &sf);
		err = list_to_file(&sf, tmpfd);
		bool unsolved;
		do {
			free_list(&rf);
			editor(tmpn);
			file_to_list(tmpfd, &rf);
			if ((unsolved = duplicates_on_list(&rf))) {
				if (!ui_select(i, "There are conflicts. Retry?", o, 2)) {
					free_list(&sf);
					free_list(&rf);
					return;
				}
			}
		} while (unsolved);
		// TODO conflicts_with_existing(i->pv, &rf));
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
			opath = malloc(PATH_MAX+1);
			npath = malloc(PATH_MAX+1);
			for (fnum_t f = 0; f < sf.len; ++f) {
				if (!strcmp(sf.str[f],
				    rf.str[f])) continue;
				strncpy(opath, i->pv->wd, PATH_MAX);
				strncpy(npath, i->pv->wd, PATH_MAX);
				if (((err = EINVAL, contains(rf.str[f], "/"))
				   || (err = append_dir(opath, sf.str[f]))
				   || (err = append_dir(npath, rf.str[f]))
				   || (rename(opath, npath) ? (err=errno) : 0))
				   && err) {
					failed(i, "rename", err, NULL);
				}
			}
			free(npath);
			free(opath);
			file_view_scan_dir(i->pv);
			file_view_sort(i->pv);
			select_from_list(i->pv, &rf);
		}
		close(tmpfd);
		unlink(tmpn);
		free_list(&sf);
		free_list(&rf);
		break;
	case CMD_DIR_VOLUME:
		// TODO don't perform on non-dirs
		// TODO
		// TODO multiple selection
		if (i->pv->file_list[i->pv->selection]->dir_volume == -1) {
			opath = file_view_path_to_selected(i->pv);
			i->pv->file_list[i->pv->selection]->dir_volume = 0;
			estimate_volume(opath, &i->pv->file_list[i->pv->selection]->dir_volume,
					&sink_fc, &sink_dc); // TODO
			free(opath);
		}
		next_entry(i->pv);
		break;
	case CMD_SELECT_FILE:
		fr = hfr(i->pv);
		if ((fr->selected = !fr->selected)) {
			i->pv->num_selected += 1;
		}
		else {
			i->pv->num_selected -= 1;
		}
		next_entry(i->pv);
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
			file_view_sort(i->pv);
			file_view_afterdel(i->pv);
		}
		i->ui_needs_refresh = true;
		break;
	case CMD_SORT_BY_NAME_ASC:
		file_view_change_sorting(i->pv, cmp_name_asc);
		i->ui_needs_refresh = true;
		break;
	case CMD_SORT_BY_NAME_DESC:
		file_view_change_sorting(i->pv, cmp_name_desc);
		i->ui_needs_refresh = true;
		break;
	case CMD_SORT_BY_DATE_ASC:
		file_view_change_sorting(i->pv, cmp_date_asc);
		i->ui_needs_refresh = true;
		break;
	case CMD_SORT_BY_DATE_DESC:
		file_view_change_sorting(i->pv, cmp_date_desc);
		i->ui_needs_refresh = true;
		break;
	case CMD_SORT_BY_SIZE_ASC:
		file_view_change_sorting(i->pv, cmp_size_asc);
		i->ui_needs_refresh = true;
		break;
	case CMD_SORT_BY_SIZE_DESC:
		file_view_change_sorting(i->pv, cmp_size_desc);
		i->ui_needs_refresh = true;
		break;
	default: break;
	}
}

/* Display message and clean task */
static void task_finish(struct ui* const i, struct task* const t) {
	i->timeout = -1;
	int err;
	if ((err = file_view_scan_dir(i->pv))
	   || (err = file_view_scan_dir(i->sv))) {
		failed(i, task_strings[t->t][NOUN], err, NULL);
	}
	else {
		file_view_sort(i->pv);
		file_view_sort(i->sv);
		if (t->t == TASK_REMOVE) file_view_afterdel(i->pv);
		else if (t->t == TASK_MOVE) prev_entry(i->pv);
		i->mt = MSG_INFO;
		snprintf(i->msg, MSG_BUFFER_SIZE, "%s from %s", // TODO
				task_strings[t->t][PAST], t->src);
	}
	task_clean(t);
	i->ui_needs_refresh = true;
	i->m = MODE_MANAGER;
}

static void task_execute(struct ui* const i, struct task* const t) {
	i->timeout = 5;
	int err;
	if (!t->paused && (err = do_task(t, 1024))) {
		failed(i, task_strings[t->t][NOUN], err, NULL);
		task_clean(t); // TODO
		i->m = MODE_MANAGER;
		wtimeout(stdscr, -1);
		return;
	}
	char sdone[SIZE_BUF_SIZE];
	char stota[SIZE_BUF_SIZE];
	pretty_size(t->size_done, sdone);
	pretty_size(t->size_total, stota);
	i->mt = MSG_INFO;
	int top = 0;
	top += snprintf(i->msg+top, MSG_BUFFER_SIZE-top,
		"%s %s: %d/%df, %d/%dd, %s/%s",
		(t->paused ? "||" : ">>"),
		task_strings[t->t][ING],
		t->files_done, t->files_total,
		t->dirs_done, t->dirs_total,
		sdone, stota);
	snprintf(i->msg+top, MSG_BUFFER_SIZE-top, ", %.*s", i->scrw-top,
			t->tw.cpath+current_dir_i(t->tw.cpath));
}

extern struct ui* I;

int main(int argc, char* argv[]) {
	static const char* const help = \
	"Usage: hund [OPTION] [left panel] [right panel]\n"
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
	while ((o = getopt_long(argc, argv, sopt, lopt, &opti)) != -1) {
		if (o == -1) break;
		switch (o) {
		case 'c':
			if (chdir(optarg)) {
				int e = errno;
				fprintf(stderr, "chdir failed:"
						" %s (%d)\n", strerror(e), e);
				exit(EXIT_FAILURE);
			}
			break;
		case 'h':
			printf("%s\n", help);
			exit(EXIT_SUCCESS);
		default: exit(EXIT_FAILURE);
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
	fvs[0].sorting = fvs[1].sorting = cmp_name_asc;

	struct ui i;
	ui_init(&i, &fvs[0], &fvs[1]);
	for (int v = 0; v < 2; ++v) {
		if (!getcwd(fvs[v].wd, PATH_MAX)) {
			fprintf(stderr, "could not read cwd; jumping to /\n");
			strncpy(fvs[v].wd, "/", 2);
		}
		if (init_wd[v]) { // Apply argument-passed paths
			char* const e = strndup(init_wd[v], PATH_MAX);
			if (enter_dir(fvs[v].wd, e)) {
				fprintf(stderr, "path too long: limit is %d;"
						" jumping to /\n", PATH_MAX);
				strncpy(fvs[v].wd, "/", 2);
			}
			free(e);
		}
		int r = file_view_scan_dir(&fvs[v]);
		if (r) {
			ui_end(&i);
			fprintf(stderr, "cannot scan directory '%s': %s (%d)\n",
					fvs[v].wd, strerror(r), r);
			exit(EXIT_FAILURE);
		}
		file_view_sort(&fvs[v]);
		first_entry(&fvs[v]);
	}

	struct task t;
	memset(&t, 0, sizeof(struct task));

	i.mt = MSG_INFO;
	snprintf(i.msg, MSG_BUFFER_SIZE,
			"Type ? for help and license notice.");
	while (i.run) {
		if (i.ui_needs_refresh) {
			ui_update_geometry(&i);
		}
		ui_draw(&i);

		process_input(&i, &t);

		if (t.t != TASK_NONE) {
			if (t.done) task_finish(&i, &t);
			else task_execute(&i, &t);
		}
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
