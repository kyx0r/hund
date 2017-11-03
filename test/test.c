#include "test.h"
#include "../src/include/path.h"

int main() {
	SETUP_TESTS;

	SECTION("path");

	char path0[PATH_MAX] = "/home/user/.config/fancyproggie";
	char home0[PATH_MAX] = "/home/user";
	int r = prettify_path(path0, home0);
	TEST(r == 0 && strcpy(path0, "~/.config/fancyproggie"), "path changed");

	char path1[PATH_MAX] = "/etc/X11/xinit/xinitrc.d";
	r = prettify_path(path1, home0);
	TEST(r == -1 && strcmp(path1, "/etc/X11/xinit/xinitrc.d") == 0, "path remained unchanged");

	char path2[PATH_MAX] = "/";
	r = enter_dir(path2, "usr");
	TEST(r == 0 && strcmp(path2, "/usr") == 0, "entered directory in root (respect /)");

	r = enter_dir(path2, "bin");
	TEST(r == 0 && strcmp(path2, "/usr/bin") == 0, "correct path");

	r = enter_dir(path2, ".");
	TEST(r == 0 && strcmp(path2, "/usr/bin") == 0, "path unchanged");

	r = enter_dir(path2, "..");
	TEST(r == 0 && strcmp(path2, "/usr") == 0, "up_dir used");

	char path3[PATH_MAX*2] = "/";
	memset(path3+1, 0, sizeof(path3) - 1);
	memset(path3+1, 'a', PATH_MAX-3);
	r = enter_dir(path3, "b");
	TEST(r == 0 && strlen(path3) == PATH_MAX, "path filled");

	r = enter_dir(path3, "end");
	TEST(r == -1 && strlen(path3) == PATH_MAX, "respect PATH_MAX; leave path unchanged");

	char path4[PATH_MAX] = "/bin";
	char cd[NAME_MAX];
	current_dir(path4, cd);
	TEST(strcmp(cd, "bin") == 0, "");

	enter_dir(path4, "wat");
	current_dir(path4, cd);
	TEST(strcmp(cd, "wat") == 0, "");

	strcpy(path4, "/");
	current_dir(path4, cd);
	TEST(strcmp(cd, "/") == 0, "");

	TEST(!path_is_relative("/etc/netctl"), "absolute");
	TEST(!path_is_relative("~/.config"), "absolute");
	TEST(path_is_relative("./netctl"), "relative");
	TEST(path_is_relative("etc/netctl"), "relative");

	char path5[PATH_MAX] = "/dope/";
	char dir5[] = "./wat/lol/../wut";
	enter_dir(path5, dir5);
	TEST(strcmp(path5, "/dope/wat/wut") == 0, "");

	enter_dir(path5, "/home/user");
	TEST(strcmp(path5, "/home/user") == 0, "");

	char path6[PATH_MAX];
	enter_dir(path6, "~");
	struct passwd* pwd = get_pwd();
	TEST(strcmp(path6, pwd->pw_dir) == 0, "");

	END_SECTION("path");

	END_TESTS;
}
