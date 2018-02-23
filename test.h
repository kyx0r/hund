/* A bunch of simple macros for basic testing
 * Intended to be replaced (or improved) later on
 */

#ifndef TESTING_H
#define TESTING_H

#include <stdio.h>
#include <stdbool.h>

#define SETUP_TESTS \
	printf("[info] running tests\n"); \
	int total = 0, passed = 0, section_total = 0, section_passed = 0

#define SECTION(S) \
	printf("[info] entering section '%s'\n", (S)); \
	section_total = 0; section_passed = 0

#define END_SECTION(S) \
	printf("[done] section '%s' finished; passed tests: %d/%d\n", (S), section_passed, section_total)

#define END_TESTS \
	printf("[done] all tests finished; passed tests: %d/%d\n", passed, total); \
	return 0;

/* E = Expression to test. Must return bool
 * M = Addional message explaining test
 *
 * Macro executes E and prints out a M,
 * with stringified E and a file:line in case of fail
 *
 */

#define TEST(E, M) \
	total += 1; section_total += 1; { \
		bool v = (E); \
		if (v) { passed += 1; section_passed += 1; } \
		else { printf("[fail] %s:%d %s //%s\n", __FILE__, __LINE__, (#E), (M)); } \
	}

#endif
