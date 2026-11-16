/* SPDX-License-Identifier: GPL-3.0-only */

#include "test_framework.hpp"

namespace {

int g_checks = 0;
int g_failures = 0;

} /* namespace */

int yt_test_check(bool condition, const char *expression, const char *file,
		  int line)
{
	++g_checks;
	if (!condition) {
		++g_failures;
		printf("FAIL %s:%d: %s\n", file, line, expression);
		return 0;
	}
	return 1;
}

int yt_test_failures(void)
{
	return g_failures;
}

int yt_test_checks(void)
{
	return g_checks;
}

void yt_test_run(const char *name, void (*fn)(void))
{
	int before = g_failures;

	printf("[ RUN      ] %s\n", name);
	fn();
	if (g_failures == before)
		printf("[       OK ] %s\n", name);
	else
		printf("[   FAILED ] %s\n", name);
}
