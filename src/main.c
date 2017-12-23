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
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "include/ui.h"
#include "include/task.h"

/*
 * GENERAL TODO NOTES
 * 1. Likes to segfault while doing anything on root's/special files.
 * 2. Messages may be blocked by other messages
 * 3. Okay,
 *    - PATH_MAX includes null-terminator
 *    - NAME_MAX does not
 *    - LOGIN_NAME_MAXA does not say, so I should add 1
 * 4. Selected files that became invisible (switched visibility) should be unselected
 */

static void failed(struct ui* const i, const utf8* const f,
		const int reason, const utf8* const custom) {
	i->mt = MSG_ERROR;
	if (custom) {
		snprintf(i->msg, MSG_BUFFER_SIZE, "%s failed: %s", f, custom);
	}
	else {
		snprintf(i->msg, MSG_BUFFER_SIZE, "%s failed: %s (%d)",
				f, strerror(reason), reason);
	}
}

extern char** environ;

static int spawn(char* const arg[]) {
	def_prog_mode();
	endwin();
	int ret = 0, status, nullfd;
	pid_t pid = fork();
	if (pid == 0) {
		nullfd = open("/dev/null", O_WRONLY, 0200);
		if (dup2(nullfd, STDERR_FILENO) == -1) ret = errno;
		// TODO ???
		close(nullfd);
		if (execve(arg[0], arg, environ)) ret = errno;
	}
	else {
		while (waitpid(pid, &status, 0) == -1);
	}
	reset_prog_mode();
	return ret;
}

static int editor(char* const path) {
	char exeimg[PATH_MAX+1];
	strcpy(exeimg, "/bin");
	const char* ed = getenv("VISUAL");
	if (!ed) ed = getenv("EDITOR");
	append_dir(exeimg, ed ? ed : "vi");
	char* const arg[] = { exeimg, path, NULL };
	spawn(arg);
	return 0;
}

static int open_prompt(struct ui* const i, utf8* const t,
		utf8* t_top, const size_t t_size) {
	i->prch = '>';
	i->prompt = t;
	ui_draw(i);
	int r = 1;
	while (r != -1 && r != 0) {
		r = fill_textbox(t, &t_top, t_size, panel_window(i->status));
		if (r == -3) ui_update_geometry(i);
		ui_draw(i); // TODO only redraw hintbar
	}
	i->prompt = NULL;
	return r;
}

static void open_find(struct ui* const i) {
	char t[NAME_MAX+1];
	char* t_top = t;
	memset(t, 0, NAME_MAX+1);
	const fnum_t sbfc = i->pv->selection;
	i->prch = '/';
	i->prompt = t;
	ui_draw(i);
	int r = 1;
	while (r != 0 && r != -1) {
		r = fill_textbox(t, &t_top, NAME_MAX, panel_window(i->status));
		if (r == -3) ui_update_geometry(i);
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
			if (!file_find(i->pv, t, s, e)) {
				//i->mt = MSG_INFO;
				//snprintf(i->msg, MSG_BUFFER_SIZE, "no more matching entries");
			}
		}
		ui_draw(i);
	}
	i->prompt = NULL;
}

static void prepare_long_task(struct ui* const i, struct task* const t,
		enum task_type tt, const utf8* const err) {
	// TODO OOM proof
	// TODO cleanup
	// TODO collision -> add to conflict file (repeat until there are no collisions)
	// TODO estimate volume for selection
	if (!i->pv->num_files) return;
	utf8* src = NULL, *dst = NULL, *nn = NULL;
	if (!(src = file_view_path_to_selected(i->pv))) {
		failed(i, err, ENAMETOOLONG, NULL);
		free(src);
		return;
	}
	const utf8* const fn = i->pv->file_list[i->pv->selection]->file_name;
	if (tt == TASK_MOVE || tt == TASK_COPY) {
		dst = strncpy(malloc(PATH_MAX+1), i->sv->wd, PATH_MAX);
		if (file_on_list(i->sv, fn)) {
			nn = calloc(NAME_MAX+1, sizeof(char));
			strncpy(nn, fn, NAME_MAX);
			const size_t nnlen = strnlen(nn, NAME_MAX);
			if (open_prompt(i, nn, nn+nnlen, NAME_MAX)) {
				free(src);
				free(dst);
				free(nn);
				return;
			}
		}
	}
	int r; // TODO
	task_new(t, tt, src, dst, nn);
	if ((r = estimate_volume(t->src,
					&(t->size_total), &(t->files_total),
					&(t->dirs_total), false))) {
		failed(i, "build file list", r, NULL);
		task_clean(t);
		return;
	}
	i->m = MODE_WAIT;
}

static void process_input(struct ui* const i, struct task* const t) {
	struct file_view* tmp = NULL;
	utf8 *path = NULL, *cdp = NULL, *name = NULL,
	     *opath = NULL, *npath = NULL;
	int err = 0;
	int sink_fc, sink_dc; // TODO FIXME
	bool s;
	const enum command cmd = get_cmd(i);
	struct file_record* fr = NULL;
	char tmpn[] = "/tmp/hund.XXXXXXXX";
	int tmpfd;
	char** list;
	fnum_t n;
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
		// TODO recursive
		if (chmod(i->path, i->perm)) {
			failed(i, "chmod", errno, NULL);
		}
		if (lchown(i->path, i->o, i->g)) {
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
		name = calloc(LOGIN_NAME_MAX+1, sizeof(utf8));
		if (!open_prompt(i, name, name, LOGIN_NAME_MAX)) {
			errno = 0;
			struct passwd* pwd = getpwnam(name);
			if (!pwd) failed(i, "chown", 0, "Such user does not exist");
			else {
				i->o = pwd->pw_uid;
				strncpy(i->user, pwd->pw_name, LOGIN_NAME_MAX);
			}
		}
		free(name);
		break;
	case CMD_CHGRP:
		/* TODO in $VISUAL */
		name = calloc(LOGIN_NAME_MAX+1, sizeof(utf8));
		if (!open_prompt(i, name, name, LOGIN_NAME_MAX)) {
			errno = 0;
			struct group* grp = getgrnam(name);
			if (!grp) failed(i, "chgrp", 0, "Such group does not exist");
			else {
				i->g = grp->gr_gid;
				strncpy(i->group, grp->gr_name, LOGIN_NAME_MAX);
			}
		}
		free(name);
		break;
	case CMD_TOGGLE_UIOX: i->perm ^= S_ISUID; break;
	case CMD_TOGGLE_GIOX: i->perm ^= S_ISGID; break;
	case CMD_TOGGLE_SB: i->perm ^= S_ISVTX; break;
	case CMD_TOGGLE_UR: i->perm ^= S_IRUSR; break;
	case CMD_TOGGLE_UW: i->perm ^= S_IWUSR; break;
	case CMD_TOGGLE_UX: i->perm ^= S_IXUSR; break;
	case CMD_TOGGLE_GR: i->perm ^= S_IRGRP; break;
	case CMD_TOGGLE_GW: i->perm ^= S_IWGRP; break;
	case CMD_TOGGLE_GX: i->perm ^= S_IXGRP; break;
	case CMD_TOGGLE_OR: i->perm ^= S_IROTH; break;
	case CMD_TOGGLE_OW: i->perm ^= S_IWOTH; break;
	case CMD_TOGGLE_OX: i->perm ^= S_IXOTH; break;
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
		// TODO multiple selection
		prepare_long_task(i, t, TASK_COPY, task_strings[TASK_COPY][NOUN]);
		break;
	case CMD_MOVE:
		// TODO multiple selection
		prepare_long_task(i, t, TASK_MOVE, task_strings[TASK_MOVE][NOUN]);
		break;
	case CMD_REMOVE:
		// TODO multiple selection
		prepare_long_task(i, t, TASK_REMOVE, task_strings[TASK_REMOVE][NOUN]);
		break;
	case CMD_EDIT_FILE:
		if ((path = file_view_path_to_selected(i->pv))) {
			if (is_dir(path)) failed(i, "edit", EISDIR, NULL);
			editor(path);
			free(path);
		}
		break;
	case CMD_OPEN_FILE:
		if ((path = file_view_path_to_selected(i->pv))) {
			if (is_dir(path)) failed(i, "open", EISDIR, NULL);
			char exeimg[NAME_MAX]; // TODO
			strcpy(exeimg, "/bin");
			char* const pager = getenv("PAGER");
			append_dir(exeimg, pager ? pager : "less");
			char* const arg[] = { exeimg, path, NULL };
			spawn(arg);
			free(path);
		}
		break;
	case CMD_CD:
		path = strncpy(malloc(PATH_MAX+1), i->pv->wd, PATH_MAX);
		cdp = calloc(PATH_MAX+1, sizeof(utf8));
		if ((open_prompt(i, cdp, cdp, PATH_MAX)
		   || (err = ENAMETOOLONG, enter_dir(path, cdp))
		   || (err = ENOENT, !file_exists(path))
		   || (err = ENOTDIR, !is_dir(path)))
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
		file_lines_to_list(tmpfd, &list, &n);
		for (fnum_t f = 0; f < n; ++f) {
			if (!strnlen(list[f], NAME_MAX)) continue; // Blank lines are ignored
			path = strncpy(malloc(PATH_MAX+1), i->pv->wd, PATH_MAX);
			if (((err = EINVAL, contains(list[f], "/"))
			   || (err = append_dir(path, list[f]))
			   || (mkdir(path, MKDIR_DEFAULT_PERM) ? (err = errno) : 0))
			   && err) {
				failed(i, "creating directory", err, NULL);
			}
			free(path);
		}
		free_line_list(n, list);
		close(tmpfd);
		unlink(tmpn);
		file_view_scan_dir(i->pv);
		file_view_sort(i->pv);
		break;
	case CMD_RENAME:
		// TODO dont allow blank lines
		// TODO clearer error messages
		if (!i->pv->num_selected) {
			hfr(i->pv)->selected = true;
			i->pv->num_selected += 1;
		}
		tmpfd = mkstemp(tmpn);
		err = file_view_dump_selected_to_file(i->pv, tmpfd);
		do {
			editor(tmpn);
			file_lines_to_list(tmpfd, &list, &n);
		} while (duplicates_on_list(list, n));
		// TODO but there still may be conflicts with existing files
		opath = malloc(PATH_MAX+1);
		npath = malloc(PATH_MAX+1);
		if (n == i->pv->num_selected) {
			fnum_t a = 0, f = 0;
			for (; a < i->pv->num_files; ++a) {
				if (!i->pv->file_list[a]->selected) continue;
				const char* const fn = i->pv->file_list[a]->file_name;
				if (!strcmp(fn, list[f])) { // Ignore unchanged
					f += 1;
					continue;
				}
				//const size_t fnlen = strnlen(fn, NAME_MAX);
				strncpy(opath, i->pv->wd, PATH_MAX);
				strncpy(npath, i->pv->wd, PATH_MAX);
				if (((err = EINVAL, !strnlen(list[f], NAME_MAX))
				   || (err = EINVAL, contains(list[f], "/"))
				   || (err = append_dir(opath, fn))
				   || (err = append_dir(npath, list[f]))
				   || (rename(opath, npath) ? (err = errno) : 0))
				   && err) {
					failed(i, "rename", err, NULL);
				}
				f += 1;
			}
		}
		free(npath);
		free(opath);
		close(tmpfd);
		unlink(tmpn);
		file_view_scan_dir(i->pv);
		select_from_list(i->pv, list, n);
		free_line_list(n, list);
		file_view_sort(i->pv);
		break;
	case CMD_DIR_VOLUME:
		// TODO don't perform on non-dirs
		// TODO
		// TODO multiple selection
		if (i->pv->file_list[i->pv->selection]->dir_volume == -1) {
			opath = file_view_path_to_selected(i->pv);
			i->pv->file_list[i->pv->selection]->dir_volume = 0;
			estimate_volume(opath, &i->pv->file_list[i->pv->selection]->dir_volume,
					&sink_fc, &sink_dc, true); // TODO
			free(opath);
		}
		next_entry(i->pv);
		break;
	case CMD_SELECT_FILE:
		fr = hfr(i->pv);
		s = fr->selected = !fr->selected;
		if (s) {
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
		if (!(path = file_view_path_to_selected(i->pv))) {
			failed(i, "chmod", ENAMETOOLONG, NULL);
		}
		else chmod_open(i, path);
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
	wtimeout(stdscr, -1);
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
		snprintf(i->msg, MSG_BUFFER_SIZE, "%s %s",
				task_strings[t->t][PAST], t->src);
	}
	task_clean(t);
	i->ui_needs_refresh = true;
	i->m = MODE_MANAGER;
}

static void task_execute(struct ui* const i, struct task* const t) {
	wtimeout(stdscr, 5);
	int err = 0;
	if (!t->paused && (err = do_task(t, 1024))) {
		failed(i, task_strings[t->t][NOUN], err, NULL);
		task_clean(t); // TODO
		wtimeout(stdscr, -1);
	}
	char sdone[SIZE_BUF_SIZE];
	char stota[SIZE_BUF_SIZE];
	pretty_size(t->size_done, sdone);
	pretty_size(t->size_total, stota);
	i->mt = MSG_INFO;
	snprintf(i->msg, MSG_BUFFER_SIZE,
		"%s %s %d/%df, %d/%dd, %s / %s",
		(t->paused ? "||" : ">>"),
		task_strings[t->t][ING],
		t->files_done, t->files_total,
		t->dirs_done, t->dirs_total,
		sdone, stota);
}

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
				fprintf(stderr, "chdir failed: %s (%d)\n", strerror(e), e);
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
	utf8* init_wd[2] = { NULL, NULL };
	int init_wd_top = 0;
	while (optind < argc) {
		if (init_wd_top < 2) {
			init_wd[init_wd_top] = argv[optind];
			init_wd_top += 1;
		}
		else {
			// Only two panels, only two paths allowed
			fprintf(stderr, "invalid argument: '%s'\n", argv[optind]);
			exit(EXIT_FAILURE);
		}
		optind += 1;
	}

	struct file_view fvs[2];
	memset(fvs, 0, sizeof(fvs));
	fvs[0].sorting = fvs[1].sorting = cmp_name_asc;

	struct ui i = ui_init(&fvs[0], &fvs[1]);
	for (int v = 0; v < 2; ++v) {
		if (!getcwd(fvs[v].wd, PATH_MAX)) {
			fprintf(stderr, "could not read cwd; jumping to /\n");
			strncpy(fvs[v].wd, "/", 2);
		}
		if (init_wd[v]) { // Apply argument-passed paths
			utf8* const e = strndup(init_wd[v], PATH_MAX);
			if (enter_dir(fvs[v].wd, e)) {
				fprintf(stderr, "path too long: limit is %d; jumping to /\n", PATH_MAX);
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
		sort_file_list(fvs[v].sorting, fvs[v].file_list, fvs[v].num_files);
		first_entry(&fvs[v]);
	}

	struct task t;
	memset(&t, 0, sizeof(struct task));

	i.mt = MSG_INFO;
	snprintf(i.msg, MSG_BUFFER_SIZE, "Type ? for help and license notice.");
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
