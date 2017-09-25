#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h> // File control
#include <sys/types.h>
#include <pwd.h>
#include <ncurses.h>
#include <locale.h>
#include <linux/limits.h>
#include <dirent.h>
#include <string.h>

int main(int argc, char* argv[])  {
	int o;
	static char sopt[] = "vht:";
	static struct option lopt[] = {
		{"chroot", required_argument, 0, 't'},
		{"verbose", no_argument, 0, 'v'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
    };
    int opti = 0;
	while ((o = getopt_long(argc, argv, sopt, lopt, &opti)) != -1) {
		if (o == -1) break;
		switch (o) {
		case 't':
			chdir(optarg);
			break;
		case 'v':
			puts("verbose?");
			break;
		case 'h':
			puts("help?");
			break;
		default:
			perror("unknown argument");
			abort();
			break;
		}
	}

	setlocale(LC_ALL, "UTF-8");
	initscr();
	cbreak();
	noecho();
	nonl();
	raw();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);	
	printw("KILL ALL HUMANS\n");

	uid_t uid = geteuid();
	struct passwd *pwd = getpwuid(uid);
	char cwd[PATH_MAX];
	char* cwd_ret = getcwd(cwd, sizeof cwd);
	if (cwd != cwd_ret) abort(); 
	printw("user: %s\nhome: %s\nshell: %s\npwd: %s\nPATH_MAX: %d\n", pwd->pw_name, pwd->pw_dir, pwd->pw_shell, cwd, PATH_MAX);
	int cwd_len = strlen(cwd);
	cwd[cwd_len] = '/';
	cwd[cwd_len+1] = 0;

	struct dirent** namelist;	
	DIR* dir = opendir(cwd);
	int num_files = scandir(cwd, &namelist, NULL, alphasort);
	for (int i = 0; i < num_files; i++) {
		printw("%d %s\n", i, namelist[i]->d_name); 
		free(namelist[i]);
	}
	free(namelist);
	closedir(dir);

	refresh();
	getch();
	endwin();
	exit(0);
}
