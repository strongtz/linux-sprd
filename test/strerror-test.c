// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit test for strerror and strerror_r
 *
 * Copyright (C) 2019, Google LLC.
 * Author: Mike Krinkin <krinkin@google.com>
 */

#include <linux/err.h>
#include <test/strerror.h>
#include <test/test.h>


static void test_strerror_returns_null_for_unknown_errors(struct test *test)
{
	EXPECT_NULL(test, _strerror(-1));
	EXPECT_NULL(test, _strerror(MAX_ERRNO + 1));
}

static void test_strerror_r_returns_null_if_buflen_is_zero(struct test *test)
{
	EXPECT_NULL(test, strerror_r(-1, NULL, 0));
}

static void test_strerror_returns_string(struct test *test)
{
	const char *err;
	char buf[64];

	err = _strerror(EAGAIN);
	ASSERT_NOT_NULL(test, err);
	EXPECT_STREQ(test, err, "EAGAIN");

	err = strerror_r(EAGAIN, buf, sizeof(buf));
	ASSERT_NOT_NULL(test, err);
	EXPECT_STREQ(test, err, "EAGAIN");
}

static void test_strerror_r_correctly_truncates_message_to_buffer_size(
		struct test *test)
{
	const char *err;
	char buf[64];

	err = strerror_r(EAGAIN, buf, 1);
	ASSERT_NOT_NULL(test, err);
	EXPECT_EQ(test, strlen(err), 0);

	err = strerror_r(EAGAIN, buf, 2);
	ASSERT_NOT_NULL(test, err);
	EXPECT_EQ(test, strlen(err), 1);

	err = strerror_r(EAGAIN, buf, sizeof(buf));
	ASSERT_NOT_NULL(test, err);
	EXPECT_STREQ(test, err, "EAGAIN");
}

static void test_strerror_r_returns_string_for_unknown_errors(struct test *test)
{
	char buf[64];

	EXPECT_NOT_NULL(test, strerror_r(-1, buf, sizeof(buf)));
	EXPECT_NOT_NULL(test, strerror_r(MAX_ERRNO + 1, buf, sizeof(buf)));
}

static struct test_case strerror_test_cases[] = {
	TEST_CASE(test_strerror_returns_null_for_unknown_errors),
	TEST_CASE(test_strerror_r_returns_null_if_buflen_is_zero),
	TEST_CASE(test_strerror_returns_string),
	TEST_CASE(test_strerror_r_correctly_truncates_message_to_buffer_size),
	TEST_CASE(test_strerror_r_returns_string_for_unknown_errors),
	{},
};

static struct test_module strerror_test_module = {
	.name = "strerror-test",
	.test_cases = strerror_test_cases,
};
module_test(strerror_test_module);
