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
	TEST(r == 0, "entered directory in root (respect /)");

	r = enter_dir(path2, "bin");
	TEST(r == 0 && strcmp(path2, "/usr/bin") == 0, "correct path");

	r = enter_dir(path2, ".");
	TEST(r == -2 && strcmp(path2, "/usr/bin") == 0, "path unchanged");

	r = enter_dir(path2, "..");
	TEST(r == 1 && strcmp(path2, "/usr") == 0, "up_dir used");

	char path3[PATH_MAX*2] = "";
	for (int i = 0; i < (PATH_MAX/2); i++) {
		r = enter_dir(path3, "m");
	}
	TEST(r == 0 && strlen(path3) == PATH_MAX, "path filled");

	r = enter_dir(path3, "m");
	TEST(r == -1 && strlen(path3) == PATH_MAX, "respect PATH_MAX; leave path unchanged");

	END_SECTION("path");

	END_TESTS;
}