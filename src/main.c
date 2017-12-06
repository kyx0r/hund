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
#include <syslog.h>

#include "include/ui.h"
#include "include/task.h"

static void failed(utf8* error_buf, const utf8* f, int reason, utf8* custom) {
	if (custom) {
		snprintf(error_buf, MSG_BUFFER_SIZE, "%s failed: %s", f, custom);
	}
	else {
		snprintf(error_buf, MSG_BUFFER_SIZE, "%s failed: %s (%d)",
				f, strerror(reason), reason);
	}
}

static int spawn(const char* const prog, char* const arg) {
	// TODO how to pass more arguments?
	def_prog_mode();
	endwin();
	int ret = 0;
	int status;
	pid_t pid = fork(); // TODO vfork() wouldn't need waitpid(), but ... ???
	if (pid == 0) {
		// TODO what about std{in,out,err}?
		if (execlp(prog, prog, arg, NULL)) ret = errno;
	}
	else {
		while (waitpid(pid, &status, 0) == -1);
	}
	reset_prog_mode();
	return ret;
}

static int open_prompt(struct ui* i, utf8* const t,
		utf8* t_top, const size_t t_size) {
	// TODO cleanup
	int r = 1;
	i->prch = ' ';
	i->prompt = t;
	ui_draw(i); // TODO
	while (r != -1 && r != 0) {
		r = fill_textbox(t, &t_top, t_size, panel_window(i->status));
		if (r == -3) ui_update_geometry(i);
		ui_draw(i); // TODO only redraw hintbar
	}
	i->prompt = NULL;
	return r;
}

static void open_find(struct ui* i) {
	// TODO cleanup; make readable
	int r = 1;
	utf8 t[NAME_MAX+1];
	memset(t, 0, NAME_MAX+1);
	utf8* t_top = t;
	fnum_t sbfc = i->pv->selection;
	i->prch = '/';
	i->prompt = t;
	ui_draw(i); // TODO
	while (r != 0 && r != -1) {
		r = fill_textbox(t, &t_top, NAME_MAX, panel_window(i->status));
		if (r == -3) ui_update_geometry(i);
		if (r == -1) {
			i->pv->selection = sbfc;
		}
		else if (r == 2 || r == -2 || t_top != t) {
			fnum_t s = 0;
			fnum_t e = i->pv->num_files-1;
			bool dofind = true;
			if (r == 2) {
				if (i->pv->selection < i->pv->num_files-1) {
					s = i->pv->selection+1;
					e = i->pv->num_files-1;
				}
				else dofind = false;
			}
			else if (r == -2) {
				if (i->pv->selection > 0) {
					s = i->pv->selection-1;
					e = 0;
				}
				else dofind = false;
			}
			bool found = false;
			if (dofind) found = file_find(i->pv, t, s, e);
			if (!found) {
				//i.error = failed("find", ENOENT);
			}
		}
		ui_draw(i);
	}
	i->prompt = NULL;
}

typedef void (*mode_handler)(struct ui*, struct task*);

static void mode_help(struct ui* i, struct task* t) {
	(void)(t);
	switch (get_cmd(i)) {
	case CMD_HELP_QUIT:
		help_close(i);
		break;
	case CMD_HELP_DOWN:
		i->helpy += 1;
		break;
	case CMD_HELP_UP:
		if (i->helpy > 0) {
			i->helpy -= 1;
		}
		break;
	default: break;
	}
}

static void mode_chmod(struct ui* i, struct task* t) {
	switch (get_cmd(i)) {
	case CMD_RETURN:
		chmod_close(i);
		break;
	case CMD_CHANGE:
		if (chmod(i->chmod->path, i->chmod->m)) {
			failed(i->error, "chmod", errno, NULL);
		}
		if (lchown(i->chmod->path, i->chmod->o, i->chmod->g)) {
			failed(i->error, "chmod", errno, NULL);
		}
		scan_dir(i->pv->wd, &i->pv->file_list, &i->pv->num_files);
		chmod_close(i);
		break;
	/*case CMD_CHOWN:
		task_new(t, TASK_CHOWN, NULL, NULL,
				calloc(LOGIN_NAME_MAX+1, sizeof(char)));
		open_prompt(i, t->newname, t->newname, LOGIN_NAME_MAX);
		t->s = TASK_STATE_DATA_GATHERED;
		break;
	case CMD_CHGRP:
		task_new(t, TASK_CHGRP, NULL, NULL,
				calloc(LOGIN_NAME_MAX+1, sizeof(char)));
		open_prompt(i, t->newname, t->newname, LOGIN_NAME_MAX);
		t->s = TASK_STATE_DATA_GATHERED;
		break;*/
	case CMD_TOGGLE_UIOX: i->chmod->m ^= S_ISUID; break;
	case CMD_TOGGLE_GIOX: i->chmod->m ^= S_ISGID; break;
	case CMD_TOGGLE_SB: i->chmod->m ^= S_ISVTX; break;
	case CMD_TOGGLE_UR: i->chmod->m ^= S_IRUSR; break;
	case CMD_TOGGLE_UW: i->chmod->m ^= S_IWUSR; break;
	case CMD_TOGGLE_UX: i->chmod->m ^= S_IXUSR; break;
	case CMD_TOGGLE_GR: i->chmod->m ^= S_IRGRP; break;
	case CMD_TOGGLE_GW: i->chmod->m ^= S_IWGRP; break;
	case CMD_TOGGLE_GX: i->chmod->m ^= S_IXGRP; break;
	case CMD_TOGGLE_OR: i->chmod->m ^= S_IROTH; break;
	case CMD_TOGGLE_OW: i->chmod->m ^= S_IWOTH; break;
	case CMD_TOGGLE_OX: i->chmod->m ^= S_IXOTH; break;
	default: break;
	}
}

static void mode_wait(struct ui* i, struct task* t) {
	switch (get_cmd(i)) {
	case CMD_TASK_QUIT:
		t->s = TASK_STATE_FINISHED;
		break;
	case CMD_TASK_PAUSE:
		t->running = false;
		break;
	case CMD_TASK_RESUME:
		t->running = true;
		break;
	default: break;
	}
}

/* Dedicated for long tasks such as COPY, REMOVE, MOVE */
static void prepare_long_task(struct ui* i, struct task* t,
		enum task_type tt, const utf8* const err) {
	// TODO OOM proof
	// TODO cleanup
	if (!i->pv->num_files) return;
	utf8* src = file_view_path_to_selected(i->pv);
	utf8* dst = NULL;
	utf8* newname = NULL;
	if (!src) {
		failed(i->error, err, ENAMETOOLONG, NULL);
		free(src);
		return;
	}
	const utf8* const fn = i->pv->file_list[i->pv->selection]->file_name;
	if (tt == TASK_MOVE || tt == TASK_COPY) {
		dst = malloc(PATH_MAX+1);
		strncpy(dst, i->sv->wd, PATH_MAX);
		if (file_on_list(i->sv, fn)) {
			newname = calloc(NAME_MAX+1, sizeof(char));
			strncpy(newname, fn, NAME_MAX);
			open_prompt(i, newname,
					newname+strnlen(newname, NAME_MAX), NAME_MAX);
		}
	}
	task_new(t, tt, src, dst, newname);
	if (!newname) {
		t->s = TASK_STATE_DATA_GATHERED;
	}
}

static void mode_manager(struct ui* i, struct task* t) {
	struct file_view* tmp = NULL;
	utf8* path = NULL;
	int err = 0;
	switch (get_cmd(i)) {
	case CMD_QUIT:
		i->run = false;
		break;
	case CMD_HELP:
		help_open(i);
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
	case CMD_EDIT_FILE:
		path = file_view_path_to_selected(i->pv);
		if (path) {
			if (is_dir(path)) failed(i->error, "edit", EISDIR, NULL);
			spawn("vi", path);
			free(path);
		}
		break;
	case CMD_OPEN_FILE:
		path = file_view_path_to_selected(i->pv);
		if (path) {
			if (is_dir(path)) failed(i->error, "open", EISDIR, NULL);
			spawn("less", path);
			free(path);
		}
		break;
	/*case CMD_CD:
		task_new(t, TASK_CD,
				strncpy(malloc(PATH_MAX+1), i->pv->wd, PATH_MAX),
				calloc(PATH_MAX+1, sizeof(char)), NULL);
		open_prompt(i, t->dst, t->dst, PATH_MAX);
		t->s = TASK_STATE_DATA_GATHERED;
		break;*/
	case CMD_ENTER_DIR:
		err = file_view_enter_selected_dir(i->pv);
		if (err && err != ENOTDIR) failed(i->error, "enter dir", err, NULL);
		break;
	case CMD_UP_DIR:
		err = file_view_up_dir(i->pv);
		if (err) failed(i->error, "up dir", err, NULL);
		break;
	case CMD_COPY:
		prepare_long_task(i, t, TASK_COPY, task_strings[TASK_COPY][NOUN]);
		break;
	case CMD_MOVE:
		prepare_long_task(i, t, TASK_MOVE, task_strings[TASK_MOVE][NOUN]);
		break;
	case CMD_REMOVE:
		prepare_long_task(i, t, TASK_REMOVE, task_strings[TASK_REMOVE][NOUN]);
		break;
	case CMD_CREATE_DIR:
		{ // TODO
		utf8* npath = malloc(PATH_MAX+1);
		strncpy(npath, i->pv->wd, PATH_MAX);
		utf8* nname = calloc(NAME_MAX+1, sizeof(utf8));
		open_prompt(i, nname, nname, PATH_MAX);
		err = append_dir(npath, nname);
		if (err) {
			failed(i->error, "creating directory", err, NULL);
			free(npath);
			free(nname);
			break;
		}
		err = dir_make(npath);
		if (err) failed(i->error, "creating directory", err, NULL);
		else {
			scan_dir(i->pv->wd, &i->pv->file_list, &i->pv->num_files); // TODO error
			sort_file_list(i->pv->sorting, i->pv->file_list, i->pv->num_files);
			file_highlight(i->pv, nname);
			ui_draw(i);
		}
		free(npath);
		free(nname);
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
		path = file_view_path_to_selected(i->pv);
		if (!path) {
			failed(i->error, "chmod", ENAMETOOLONG, NULL);
			break;
		}
		chmod_open(i, path);
		break;
	case CMD_RENAME: // TODO
		{
		utf8* opath = file_view_path_to_selected(i->pv);
		utf8* npath = malloc(PATH_MAX+1);
		strcpy(npath, i->pv->wd);
		utf8* nname = calloc(NAME_MAX+1, sizeof(utf8));
		strncpy(nname, i->pv->file_list[i->pv->selection]->file_name, NAME_MAX);
		int r = open_prompt(i, nname, nname+strnlen(nname, NAME_MAX), NAME_MAX);
		if (r) break;
		err = append_dir(npath, nname); // TODO
		err = rename(opath, npath);
		if (err) failed(i->error, "renaming", err, NULL);
		scan_dir(i->pv->wd, &i->pv->file_list, &i->pv->num_files); // TODO error
		sort_file_list(i->pv->sorting, i->pv->file_list, i->pv->num_files);
		file_highlight(i->pv, nname);
		ui_draw(i);
		free(opath);
		free(npath);
		free(nname);
		}
		break;
	case CMD_TOGGLE_HIDDEN:
		file_view_toggle_hidden(i->pv);
		break;
	case CMD_REFRESH:
		err = scan_dir(i->pv->wd, &i->pv->file_list, &i->pv->num_files);
		if (err) {
			failed(i->error, "refresh", err, NULL);
		}
		else {
			sort_file_list(i->pv->sorting, i->pv->file_list, i->pv->num_files);
			file_view_afterdel(i->pv);
		}
		i->ui_needs_refresh = true;
		break;
	// TODO sort after each of these
	case CMD_SORT_BY_NAME_ASC:
		i->pv->sorting = cmp_name_asc;
		sort_file_list(i->pv->sorting, i->pv->file_list, i->pv->num_files);
		i->ui_needs_refresh = true;
		break;
	case CMD_SORT_BY_NAME_DESC: i->pv->sorting = cmp_name_desc;
		sort_file_list(i->pv->sorting, i->pv->file_list, i->pv->num_files);
		i->ui_needs_refresh = true;
		break;
	case CMD_SORT_BY_DATE_ASC: i->pv->sorting = cmp_date_asc;
		sort_file_list(i->pv->sorting, i->pv->file_list, i->pv->num_files);
		i->ui_needs_refresh = true;
		break;
	case CMD_SORT_BY_DATE_DESC: i->pv->sorting = cmp_date_desc;
		sort_file_list(i->pv->sorting, i->pv->file_list, i->pv->num_files);
		i->ui_needs_refresh = true;
		break;
	case CMD_SORT_BY_SIZE_ASC: i->pv->sorting = cmp_size_asc;
		sort_file_list(i->pv->sorting, i->pv->file_list, i->pv->num_files);
		i->ui_needs_refresh = true;
		break;
	case CMD_SORT_BY_SIZE_DESC: i->pv->sorting = cmp_size_desc;
		sort_file_list(i->pv->sorting, i->pv->file_list, i->pv->num_files);
		i->ui_needs_refresh = true;
		break;
	default:
		break;
	}
}

/* Task state handler functions...
 * handle tasks depending on their state
 */
typedef void (*task_state_handler) (struct ui*, struct task*);

/* If user filled prompt or confirmed changes
 * Task can be finished right there.
 * If task is long (move, copy, remove),
 * it is initialized here (build_file_list)
 */
static void task_state_data_gathered(struct ui* i, struct task* t) {
	int err = 0, reason = 0;
	switch (t->t) {
	/*case TASK_CHOWN:
		{
		errno = 0;
		struct passwd* pwd = getpwnam(t->newname);
		if (!pwd) failed(i->error, "chown", errno,
				"Such user does not exist");
		else {
			i->chmod->o = pwd->pw_uid;
			strncpy(i->chmod->owner, pwd->pw_name, LOGIN_NAME_MAX);
		}
		t->s = TASK_STATE_FINISHED;
		}
		break;
	case TASK_CHGRP:
		{
		errno = 0;
		struct group* grp = getgrnam(t->newname);
		if (!grp) failed(i->error, "chgrp", errno,
				"Such group does not exist");
		else {
			i->chmod->g = grp->gr_gid;
			strncpy(i->chmod->group, grp->gr_name, LOGIN_NAME_MAX);
		}
		t->s = TASK_STATE_FINISHED;
		}
		break;
	case TASK_CD:
		err = enter_dir(t->src, t->dst);
		if ((reason = ENAMETOOLONG, err) ||
			(reason = ENOENT, !file_exists(t->src)) ||
			(reason = EISDIR, !is_dir(t->src))) {

			failed(i->error, task_strings[TASK_CD][ING], reason, NULL);
			t->s = TASK_STATE_FINISHED;
			break;
		}
		strncpy(i->pv->wd, t->src, PATH_MAX);
		t->s = TASK_STATE_FINISHED;
		break;*/
	case TASK_MOVE:
		err = rename_if_same_fs(t);
		if (!err) {
			t->s = TASK_STATE_FINISHED;
			break;
		}
		if (err) {
			failed(i->error, "move via rename()", err, NULL);
			task_clean(t);
			break;
		}
	case TASK_COPY:
	case TASK_REMOVE:
		err = task_estimate_file_volume(t, t->src);
		if (err) {
			failed(i->error, "build file list", err, NULL);
			task_clean(t);
			break;
		}
		t->s = TASK_STATE_EXECUTING;
	case TASK_NONE:
		break;
	}
}

/* If task is long (removing, moving, copying)
 * it reaches TASK_STATE_EXECUTING,
 */
static void task_state_executing(struct ui* i, struct task* t) {
	wtimeout(stdscr, 5);
	int e = 0;
	if (t->running) e = do_task(t, 1024);
	if (e && t->running) {
		failed(i->error, task_strings[t->t][NOUN], e, NULL);
		t->s = TASK_STATE_FINISHED;
	}
	else {
		char sdone[SIZE_BUF_SIZE];
		char stota[SIZE_BUF_SIZE];
		pretty_size(t->size_done, sdone);
		pretty_size(t->size_total, stota);
		snprintf(i->info, MSG_BUFFER_SIZE,
			"%s %s %d/%df, %d/%dd, %s / %s",
			(t->running ? ">>" : "||"),
			task_strings[t->t][ING],
			t->files_done, t->files_total,
			t->dirs_done, t->dirs_total,
			sdone, stota);
	}
}

/* Display message and clean task */
static void task_state_finished(struct ui* i, struct task* t) {
	// TODO
	if (t->t == TASK_REMOVE || t->t == TASK_MOVE || t->t == TASK_COPY) {
		wtimeout(stdscr, -1);
	}
	// TODO
	int epv = scan_dir(i->pv->wd, &i->pv->file_list, &i->pv->num_files);
	int esv = scan_dir(i->sv->wd, &i->sv->file_list, &i->sv->num_files);
	if (epv || esv) {
		failed(i->error, task_strings[t->t][NOUN], epv | esv, NULL);
	}
	sort_file_list(i->pv->sorting, i->pv->file_list, i->pv->num_files);
	sort_file_list(i->sv->sorting, i->sv->file_list, i->sv->num_files);
	if (t->t == TASK_REMOVE) {
		file_view_afterdel(i->pv);
		snprintf(i->info, MSG_BUFFER_SIZE, "%s %s",
				task_strings[t->t][PAST], t->src);
	}
	else if (t->t == TASK_COPY) {
		snprintf(i->info, MSG_BUFFER_SIZE, "%s %s",
				task_strings[t->t][PAST], t->src);
	}
	else if (t->t == TASK_MOVE) {
		prev_entry(i->pv);
		snprintf(i->info, MSG_BUFFER_SIZE, "%s %s",
				task_strings[t->t][PAST], t->src);
	}
	task_clean(t);
	i->m = MODE_MANAGER;
}

int main(int argc, char* argv[])  {
	static const char* help = \
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
			chdir(optarg);
			break;
		case 'h':
			printf("%s\n", help);
			exit(EXIT_SUCCESS);
		default:
			exit(1);
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

	openlog(argv[0], LOG_PID, LOG_USER);
	syslog(LOG_NOTICE, "%s started", argv[0]+2);

	struct file_view fvs[2];
	memset(fvs, 0, sizeof(fvs));
	fvs[0].sorting = fvs[1].sorting = cmp_name_asc;

	struct ui i = ui_init(&fvs[0], &fvs[1]);

	for (int v = 0; v < 2; ++v) {
		get_cwd(fvs[v].wd);
		if (init_wd[v]) { // Apply argument-passed paths
			utf8* e = strndup(init_wd[v], PATH_MAX);
			int r = enter_dir(fvs[v].wd, e);
			if (r) {
				ui_end(&i);
				fprintf(stderr, "path too long: limit is %d\n", PATH_MAX);
				exit(EXIT_FAILURE);
			}
			free(e);
		}
		int r = scan_dir(fvs[v].wd, &fvs[v].file_list, &fvs[v].num_files);
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

	static const mode_handler mhs[MODE_NUM] = {
		[MODE_HELP] = mode_help,
		[MODE_MANAGER] = mode_manager,
		[MODE_CHMOD] = mode_chmod,
		[MODE_WAIT] = mode_wait,
	};

	static const task_state_handler shs[TASK_STATE_NUM] = {
		[TASK_STATE_CLEAN] = NULL,
		[TASK_STATE_GATHERING_DATA] = NULL,
		[TASK_STATE_DATA_GATHERED] = task_state_data_gathered,
		[TASK_STATE_EXECUTING] = task_state_executing,
		[TASK_STATE_FINISHED] = task_state_finished,
	};

	snprintf(i.info, MSG_BUFFER_SIZE, "Type ? for help and license notice.");
	while (i.run) {
		if (i.ui_needs_refresh) { // TODO FIXME here ?
			ui_update_geometry(&i);
		}
		ui_draw(&i);

		// Execute mode handler
		mhs[i.m](&i, &t);

		/* Cycle through all states to finish
		 * short/one-cycle long tasks quickly. */
		for (enum task_state ts = TASK_STATE_CLEAN;
				ts < TASK_STATE_NUM; ++ts) {
			if (ts == t.s && shs[ts]) shs[ts](&i, &t);
		}
	}

	for (int v = 0; v < 2; ++v) {
		delete_file_list(&fvs[v]);
	}
	task_clean(&t);
	ui_end(&i);
	closelog();
	exit(EXIT_SUCCESS);
}
