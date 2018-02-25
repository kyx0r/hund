/* A bunch of simple macros for basic testing
 * Intended to be replaced (or improved) later on
 */

#ifndef TESTING_H
#define TESTING_H

#include <stdio.h>
#include <stdbool.h>

#define SETUP_TESTS \
	printf("[\x1b[36minfo\x1b[0m] running tests\n"); \
	unsigned total = 0, passed = 0, section_total = 0, section_passed = 0

#define SECTION(S) \
	printf("[\x1b[36minfo\x1b[0m] entering section '%s'\n", (S)); \
	section_total = 0; section_passed = 0

#define END_SECTION(S) \
	printf("[\x1b[32mdone\x1b[0m] section '%s' finished; passed tests: %d/%d\n", (S), section_passed, section_total)

#define END_TESTS \
	printf("[\x1b[32mdone\x1b[0m] all tests finished; passed tests: %d/%d\n", passed, total); \
	return 0;

#define TEST(E, M) \
	do { \
		total += 1; \
		section_total += 1; \
		if (E) { \
			passed += 1; \
			section_passed += 1; \
		} \
		else \
			printf("[\x1b[31mfail\x1b[0m] %s:%d '%s' is false //%s\n", \
				__FILE__, __LINE__, (#E), (M)); \
	} \
	while (0);

/* A - tested string
 * B - expected string
 */
#define TESTSTR(A, B, M) \
	do { \
		total += 1; \
		section_total += 1; \
		if (!strcmp(A, B)) { \
			passed += 1; \
			section_passed += 1; \
		} \
		else \
			printf("[\x1b[31mfail\x1b[0m] %s:%d '%s' != '%s' //%s\n", \
				__FILE__, __LINE__, (A), (B), (M)); \
	} \
	while (0);

#define TESTVAL(A, B, M) \
	do { \
		total += 1; \
		section_total += 1; \
		if ((A) == (B)) { \
			passed += 1; \
			section_passed += 1; \
		} \
		else \
			printf("[\x1b[31mfail\x1b[0m] %s:%d '%s'(%lld) != '%s'(%lld) //%s\n", \
				__FILE__, __LINE__, \
				(#A), (long long)(A), \
				(#B), (long long)(B), (M)); \
	} \
	while (0);


#endif
