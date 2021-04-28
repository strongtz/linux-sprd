// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit test for core test infrastructure.
 *
 * Copyright (C) 2018, Google LLC.
 * Author: Brendan Higgins <brendanhiggins@google.com>
 */
#include <test/test.h>

static void test_death_test_catches_segfault(struct test *test)
{
	void (*invalid_func)(void) = (void (*)(void)) SIZE_MAX;

	ASSERT_SIGSEGV(test, invalid_func());
}

static struct test_case test_death_test_cases[] = {
	TEST_CASE(test_death_test_catches_segfault),
	{},
};

static struct test_module test_death_test_module = {
	.name = "test-death-test",
	.test_cases = test_death_test_cases,
};
module_test(test_death_test_module);
