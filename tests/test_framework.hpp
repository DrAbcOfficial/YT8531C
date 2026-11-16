/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Minimal unit test harness for the YT8531C core.
 */

#ifndef YT8531C_TEST_FRAMEWORK_HPP
#define YT8531C_TEST_FRAMEWORK_HPP

#include <stdio.h>

int yt_test_check(bool condition, const char *expression, const char *file,
		  int line);
int yt_test_failures(void);
int yt_test_checks(void);
void yt_test_run(const char *name, void (*fn)(void));

#define YT_CHECK(cond) \
	yt_test_check((cond), #cond, __FILE__, __LINE__)

#define YT_CHECK_EQ(actual, expected)                                        \
	do {                                                                 \
		long long a_ = (long long)(actual);                          \
		long long e_ = (long long)(expected);                        \
		if (!yt_test_check(a_ == e_, #actual " == " #expected,       \
				   __FILE__, __LINE__))                      \
			printf("    actual = %lld, expected = %lld\n", a_,   \
			       e_);                                          \
	} while (0)

#define YT_CHECK_NE(actual, other)                                          \
	do {                                                               \
		long long a_ = (long long)(actual);                        \
		long long o_ = (long long)(other);                         \
		yt_test_check(a_ != o_, #actual " != " #other, __FILE__,   \
			      __LINE__);                                   \
	} while (0)

#endif /* YT8531C_TEST_FRAMEWORK_HPP */
