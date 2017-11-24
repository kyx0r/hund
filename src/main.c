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
#include <getopt.h>
#include <string.h>
#include <syslog.h>

#include "include/ui.h"
#include "include/task.h"

#define UNUSED_ARGUMENT(E) (void)(E);

static void failed(utf8* error, const utf8* f, int e) {
	snprintf(error, MSG_BUFFER_SIZE, "%s failed: %s (%d)", f, strerror(e), e);
}

static void progress(utf8* info, struct task* t) {
	const utf8* what = "?";
	switch (t->t) {
	case TASK_COPY: what = "copying"; break;
	case TASK_MOVE: what = "moving"; break;
	case TASK_RM: what = "removing"; break;
	default: break;
	}
	snprintf(info, MSG_BUFFER_SIZE,
			"%s %d/%df, %d/%dd, %lu/%luB", what,
			t->files_done, t->files_total,
			t->dirs_done, t->dirs_total,
			t->size_done, t->size_total);
}

static int open_file(const utf8* const path) {
	if (is_dir(path)) return EISDIR;
	static const utf8* prog = "less";
	utf8* cmd = malloc(strlen(path)+strlen(prog)+2+1+1);
	snprintf(cmd, PATH_MAX, "%s '%s'", prog, path); // TODO recognize format
	ui_system(cmd);
	free(cmd);
	return 0;
}

/* Mode handlers are functions that take care of
 * preparing tasks in response to user input.
 */
typedef void (*mode_handler)(struct ui*, struct task*);

static void mode_help(struct ui* i, struct task* t) {
	UNUSED_ARGUMENT(t);
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
	UNUSED_ARGUMENT(i);
	UNUSED_ARGUMENT(t);

	/*switch (get_cmd(i)) {
	case CMD_RETURN:
		chmod_close(i);
		break;
	case CMD_CHANGE:
		syslog(LOG_DEBUG, "chmod %s: %o, %d:%d",
				i->chmod->path, i->chmod->m, i->chmod->o, i->chmod->g);
		if (chmod(i->chmod->path, i->chmod->m)) {
			failed(i->error, "chmod", errno);
		}
		if (lchown(i->chmod->path, i->chmod->o, i->chmod->g)) {
			// TODO o and g must be valid
			failed(i->error, "chmod", errno);
		}
		scan_dir(i->pv->wd, &i->pv->file_list, &i->pv->num_files);
		chmod_close(i);
		break;
	case CMD_CHOWN:
		i->chmod->tmp = calloc(LOGIN_NAME_MAX, sizeof(char));
		t->t = TASK_CHOWN;
		t->s = TASK_STATE_GATHERING_DATA;
		prompt_open(i, i->chmod->tmp, i->chmod->tmp, LOGIN_NAME_MAX);
		break;
	case CMD_CHGRP:
		i->chmod->tmp = calloc(LOGIN_NAME_MAX, sizeof(char));
		t->t = TASK_CHGRP;
		t->s = TASK_STATE_GATHERING_DATA;
		prompt_open(i, i->chmod->tmp, i->chmod->tmp, LOGIN_NAME_MAX);
		break;
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
	}*/
}

static void mode_find(struct ui* i, struct task* t) {
	UNUSED_ARGUMENT(t);
	int r = fill_textbox(i->find->t, &i->find->t_top,
			i->find->t_size, 1, panel_window(i->status));
	if (r == -1) {
		find_close(i, false);
	}
	else if (r == 0) {
		find_close(i, true);
	}
	else if (r == 2 || r == -2 || i->find->t_top != i->find->t) {
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
		if (dofind) found = file_find(i->pv, i->find->t, s, e);
		if (!found) {
			//i.error = failed("find", ENOENT);
		}
	}
	else {
		i->pv->selection = i->find->sbfc;
	}
}

static void mode_prompt(struct ui* i, struct task* t) {
	int r = fill_textbox(i->prompt->tb, &i->prompt->tb_top,
			i->prompt->tb_size, 0, panel_window(i->status));
	if (!r || r == -1) {
		if (t->t != TASK_NONE && t->s == TASK_STATE_GATHERING_DATA) {
			t->s = TASK_STATE_DATA_GATHERED;
		}
		prompt_close(i);
	}
}

static void mode_wait(struct ui* i, struct task* t) {
	UNUSED_ARGUMENT(i);
	UNUSED_ARGUMENT(t);
	/*switch (get_cmd(i)) {
	default: break;
	}*/
}

/* Dedicated for long tasks such as COPY, REMOVE, MOVE */
static void prepare_long_task(struct ui* i, struct task* t,
		enum task_type tt, const utf8* const err) {
	if (!i->pv->num_files) return;
	utf8* src = file_view_path_to_selected(i->pv);
	utf8* dst = NULL;
	utf8* newname = NULL;
	if (!src) {
		failed(i->error, err, ENAMETOOLONG);
		free(src);
		return;
	}
	const utf8* const fn = i->pv->file_list[i->pv->selection]->file_name;
	if (tt == TASK_MOVE || tt == TASK_COPY) {
		dst = malloc(PATH_MAX);
		strcpy(dst, i->sv->wd);
		if (file_on_list(i->sv, fn)) {
			newname = calloc(NAME_MAX, sizeof(utf8));
			strcpy(newname, fn);
			i->m = MODE_WAIT; // TODO FIXME
			// ^ doing so will make prompt set mode to MODE_WAIT
			prompt_open(i, newname, newname+strlen(newname), NAME_MAX);
		}
	}
	task_new(t, tt, src, dst, newname);
	if (i->m != MODE_PROMPT) i->m = MODE_WAIT; // TODO FIXME
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
	case CMD_OPEN_FILE:
		path = file_view_path_to_selected(i->pv);
		if (path) {
			open_file(path);
			free(path);
		}
		break;
	case CMD_CD:
		task_new(t, TASK_CD, strcpy(malloc(PATH_MAX), i->pv->wd),
				calloc(PATH_MAX, sizeof(char)), NULL);
		prompt_open(i, t->dst, t->dst, PATH_MAX);
		break;
	case CMD_ENTER_DIR:
		err = file_view_enter_selected_dir(i->pv);
		if (err && err != ENOTDIR) failed(i->error, "enter dir", err);
		break;
	case CMD_UP_DIR:
		err = file_view_up_dir(i->pv);
		if (err) failed(i->error, "up dir", err);
		break;
	case CMD_COPY:
		prepare_long_task(i, t, TASK_COPY, "copy");
		break;
	case CMD_MOVE:
		prepare_long_task(i, t, TASK_MOVE, "move");
		break;
	case CMD_REMOVE:
		prepare_long_task(i, t, TASK_RM, "remove");
		break;
	case CMD_CREATE_DIR:
		task_new(t, TASK_MKDIR, strdup(i->pv->wd),
				NULL, calloc(NAME_MAX, sizeof(char)));
		prompt_open(i, t->newname, t->newname, PATH_MAX);
		break;
	case CMD_FIND:
		path = calloc(NAME_MAX, sizeof(char));
		find_open(i, path, path, NAME_MAX);
		break;
	case CMD_ENTRY_FIRST:
		first_entry(i->pv);
		break;
	case CMD_ENTRY_LAST:
		last_entry(i->pv);
		break;
	/*case CMD_CHMOD:
		path = file_view_path_to_selected(i->pv);
		if (!path) {
			failed(i->error, "chmod", ENAMETOOLONG);
			break;
		}
		chmod_open(i, path, i->pv->file_list[i->pv->selection]->s.st_mode);
		break;*/
	case CMD_RENAME:
		task_new(t, TASK_RENAME, file_view_path_to_selected(i->pv), strdup(i->pv->wd),
				strcpy(calloc(NAME_MAX, sizeof(char)),
					i->pv->file_list[i->pv->selection]->file_name));
		prompt_open(i, t->newname, t->newname+strlen(t->newname), PATH_MAX);
		break;
	case CMD_TOGGLE_HIDDEN:
		file_view_toggle_hidden(i->pv);
		break;
	case CMD_REFRESH:
		err = scan_dir(i->pv->wd, &i->pv->file_list, &i->pv->num_files);
		if (err) {
			failed(i->error, "refresh", err);
		}
		else {
			sort_file_list(i->pv->file_list, i->pv->num_files);
			file_view_afterdel(i->pv);
		}
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
void task_state_data_gathered(struct ui* i, struct task* t) {
	switch (t->t) {
	/*case TASK_CHOWN:
		{
		syslog(LOG_DEBUG, "chown %s", i.chmod->tmp);
		struct passwd* pwd = getpwnam(i.chmod->tmp);
		if (errno) {
			i.error = failed("chown", errno);
		}
		else {
			i.chmod->o = pwd->pw_uid;
			strcpy(i.chmod->owner, pwd->pw_name);
		}
		t.s = TASK_STATE_FINISHED;
		}
		break;
	case TASK_CHGRP:
		syslog(LOG_DEBUG, "chgrp %s", i.chmod->tmp);
		struct group* grp = getgrnam(i.chmod->tmp);
		if (errno) {
			i.error = failed("chgrp", errno);
		}
		else {
			i.chmod->g = grp->gr_gid;
			strcpy(i.chmod->group, grp->gr_name);
		}
		t.s = TASK_STATE_FINISHED;
		break;*/
	case TASK_MKDIR:
		{
		utf8* path = malloc(PATH_MAX);
		strcpy(path, t->src);
		strcat(path, "/");
		strcat(path, t->newname);
		int err = dir_make(path);
		if (err) failed(i->error, "mkdir", err);
		t->s = TASK_STATE_FINISHED;
		}
		break;
	case TASK_CD:
		{
		syslog(LOG_DEBUG, "%s %s", t->src, t->dst);
		int err = enter_dir(t->src, t->dst);
		int reason;
		if ((reason = ENAMETOOLONG, err) ||
			(reason = ENOENT, !file_exists(t->src)) ||
			(reason = EISDIR, !is_dir(t->src))) {

			failed(i->error, "cd", reason);
			t->s = TASK_STATE_FINISHED;
			break;
		}
		strcpy(i->pv->wd, t->src);
		t->s = TASK_STATE_FINISHED;
		}
		break;
	case TASK_RENAME:
		{
		utf8* path = malloc(PATH_MAX);
		strcpy(path, t->dst);
		strcat(path, "/");
		strcat(path, t->newname);
		int err = rename(t->src, path);
		if (err) failed(i->error, "rename", err);
		t->s = TASK_STATE_FINISHED;
		}
		break;
	case TASK_COPY:
	case TASK_RM:
	case TASK_MOVE:
		{
		int r = task_build_file_list(t);
		if (r) {
			failed(i->error, "build file list", r);
			task_clean(t); //TODO
			break;
		}
		t->s = TASK_STATE_EXECUTING;
		}
	case TASK_NONE:
		break;
	}
}

/* If task is long (removing, moving, copying)
 * it reaches TASK_STATE_EXECUTING,
 */
void task_state_executing(struct ui* i, struct task* t) {
	wtimeout(stdscr, 20);
	int e = do_task(t, 1024);
	if (e) {
		failed(i->error, "task", e);
		t->s = TASK_STATE_FINISHED;
	}
	else progress(i->info, t);
}

/* Display message and clean task */
void task_state_finished(struct ui* i, struct task* t) {
	if (t->t == TASK_RM || t->t == TASK_MOVE || t->t == TASK_COPY) {
		wtimeout(stdscr, -1);
	}
	// TODO
	int epv = scan_dir(i->pv->wd, &i->pv->file_list, &i->pv->num_files);
	int esv = scan_dir(i->sv->wd, &i->sv->file_list, &i->sv->num_files);
	if (epv || esv) {
		failed(i->error, "task", epv | esv);
	}
	sort_file_list(i->pv->file_list, i->pv->num_files);
	sort_file_list(i->sv->file_list, i->sv->num_files);
	utf8* what = "?";
	if (t->t == TASK_RM) {
		what = "removed";
		file_view_afterdel(i->pv);
		snprintf(i->info, MSG_BUFFER_SIZE, "%s %s", what, t->src);
	}
	else if (t->t == TASK_COPY) {
		what = "copied";
		snprintf(i->info, MSG_BUFFER_SIZE, "%s %s", what, t->src);
	}
	else if (t->t == TASK_MOVE) {
		what = "moved";
		prev_entry(i->pv);
		snprintf(i->info, MSG_BUFFER_SIZE, "%s %s", what, t->src);
	}
	else if (t->t == TASK_CD) {
		what = "opened";
		first_entry(i->pv);
	}
	else if (t->t == TASK_MKDIR) {
		what = "created";
		file_highlight(i->pv, t->newname);
	}
	else if (t->t == TASK_RENAME) {
		what = "renamed";
	}
	task_clean(t);
	i->m = MODE_MANAGER;
}

int main(int argc, char* argv[])  {
	static const char* help = \
	"Usage: hund [OPTION] [left panel] [right panel]\n"
	"Options:\n"
	"  -c, --chdir=PATH\t\tchange initial directory\n"
	"  -v, --verbose\t\tbe verbose\n"
	"  -h, --help\t\tdisplay this help message\n"
	"Type `?` while in hund for keybindings\n";

	static const char sopt[] = "vhc:";
	static const struct option lopt[] = {
		{"chdir", required_argument, 0, 'c'},
		{"verbose", no_argument, 0, 'v'},
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
		case 'v':
			puts("woof!");
			exit(EXIT_SUCCESS);
		case 'h':
			printf(help);
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

	struct ui i = ui_init(&fvs[0], &fvs[1]);

	for (int v = 0; v < 2; ++v) {
		get_cwd(fvs[v].wd);
		if (init_wd[v]) { // Apply user-passed paths
			utf8* e = strdup(init_wd[v]);
			int r = enter_dir(fvs[v].wd, e);
			if (r) {
				ui_end(&i);
				fprintf(stderr, "path too long: it's %lu, limit is %d\n",
						strlen(fvs[v].wd), PATH_MAX);
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
		sort_file_list(fvs[v].file_list, fvs[v].num_files);
		first_entry(&fvs[v]);
	}

	struct task t;
	memset(&t, 0, sizeof(struct task));

	static const mode_handler mhs[MODE_NUM] = {
		[MODE_HELP] = mode_help,
		[MODE_MANAGER] = mode_manager,
		[MODE_CHMOD] = mode_chmod,
		[MODE_PROMPT] = mode_prompt,
		[MODE_WAIT] = mode_wait,
		[MODE_FIND] = mode_find,
	};

	static const task_state_handler shs[TASK_STATE_NUM] = {
		[TASK_STATE_CLEAN] = NULL,
		[TASK_STATE_GATHERING_DATA] = NULL,
		[TASK_STATE_DATA_GATHERED] = task_state_data_gathered,
		[TASK_STATE_EXECUTING] = task_state_executing,
		[TASK_STATE_FINISHED] = task_state_finished,
	};

	while (i.run || t.checklist) { // TODO task state
		ui_update_geometry(&i);
		ui_draw(&i);

		mhs[i.m](&i, &t);

		/* Cycle through all states to finish short tasks quickly. */
		for (enum task_state ts = TASK_STATE_CLEAN; ts < TASK_STATE_NUM; ++ts) {
			if (ts == t.s && shs[ts]) shs[ts](&i, &t);
		}
	}

	for (int v = 0; v < 2; ++v) {
		delete_file_list(&fvs[v]);
	}
	task_clean(&t);

	ui_end(&i);
	syslog(LOG_NOTICE, "hund finished");
	closelog();
	exit(EXIT_SUCCESS);
}
