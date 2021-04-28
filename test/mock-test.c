// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit test for mock.h.
 *
 * Copyright (C) 2018, Google LLC.
 * Author: Brendan Higgins <brendanhiggins@google.com>
 */

#include <test/test.h>
#include <test/mock.h>

#include "test-mock.h"

struct mock_test_context {
	struct MOCK(test)	*mock_test;
	struct mock		*mock;
};

static void mock_test_do_expect_basic(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;
	int param0 = 5, param1 = -4;
	static const char * const two_param_types[] = {"int", "int"};
	const void *two_params[] = {&param0, &param1};
	struct mock_param_matcher *matchers_any_two[] = {any(trgt), any(trgt)};
	struct mock_expectation *expectation;
	const void *ret;

	expectation = mock_add_matcher(mock,
				       "",
				       NULL,
				       matchers_any_two,
				       ARRAY_SIZE(matchers_any_two));
	expectation->action = int_return(trgt, 5);
	EXPECT_EQ(test, 0, expectation->times_called);

	ret = mock->do_expect(mock,
			      "",
			      NULL,
			      two_param_types,
			      two_params,
			      ARRAY_SIZE(two_params));
	ASSERT_NOT_ERR_OR_NULL(test, ret);
	EXPECT_EQ(test, 5, *((int *) ret));
	EXPECT_EQ(test, 1, expectation->times_called);
}

static void mock_test_ptr_eq(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;
	void *param0 = ctx, *param1 = trgt;
	static const char * const two_param_types[] = {"void *", "void *"};
	const void *two_params[] = {&param0, &param1};
	struct mock_param_matcher *matchers_two_ptrs[] = {
		ptr_eq(trgt, param0), ptr_eq(trgt, param1)
	};
	struct mock_expectation *expectation;
	const void *ret;

	expectation = mock_add_matcher(mock,
				       "",
				       NULL,
				       matchers_two_ptrs,
				       ARRAY_SIZE(matchers_two_ptrs));
	expectation->action = int_return(trgt, 0);
	EXPECT_EQ(test, 0, expectation->times_called);

	ret = mock->do_expect(mock,
			      "",
			      NULL,
			      two_param_types,
			      two_params,
			      ARRAY_SIZE(two_params));
	ASSERT_NOT_ERR_OR_NULL(test, ret);
	EXPECT_EQ(test, 1, expectation->times_called);
}

static void mock_test_ptr_eq_not_equal(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;
	void *param0 = ctx, *param1 = trgt;
	static const char * const two_param_types[] = {"void *", "void *"};
	const void *two_params[] = {&param0, &param1};
	struct mock_param_matcher *matchers_two_ptrs[] = {
		ptr_eq(trgt, param0), ptr_eq(trgt, param1 - 1)
	};
	struct mock_expectation *expectation;
	const void *ret;

	expectation = mock_add_matcher(mock,
				       "",
				       NULL,
				       matchers_two_ptrs,
				       ARRAY_SIZE(matchers_two_ptrs));
	expectation->action = int_return(trgt, 0);
	EXPECT_EQ(test, 0, expectation->times_called);

	ret = mock->do_expect(mock,
			      "",
			      NULL,
			      two_param_types,
			      two_params,
			      ARRAY_SIZE(two_params));
	EXPECT_FALSE(test, ret);
	EXPECT_EQ(test, 0, expectation->times_called);
}

/*
 * In order for us to be able to rely on EXPECT_CALL to validate other behavior,
 * we need to test that unsatisfied EXPECT_CALL causes a test failure.
 */
static void mock_test_failed_expect_call_fails_test(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct mock *mock = ctx->mock;

	/* mock is a pretend mock belonging to our mocked_test */

	/* Put an expectation on mocked mock */
	EXPECT_CALL(fail(mock, any(mock_get_trgt(mock_test))));

	/*
	 * Expect that mock_test will fail because the above won't be satisfied
	 */
	EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test)));

	/*
	 * Validate expectations of mocked mock, which should fail mocked test
	 */
	mock_validate_expectations(mock);

	/* Validate mock_test's expectations, that is, it should have failed */
	mock_validate_expectations(mock_get_ctrl(mock_test));
	EXPECT_FALSE(test, mock_get_trgt(mock_test)->success);
}

static void mock_test_do_expect_default_return(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = NICE_MOCK(ctx->mock_test);
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;
	int param0 = 5, param1 = -5;
	static const char * const two_param_types[] = {"int", "int"};
	const void *two_params[] = {&param0, &param1};
	struct mock_param_matcher *matchers[] = {
		int_eq(trgt, 5),
		int_eq(trgt, -4)
	};
	struct mock_expectation *expectation;
	const void *ret;

	expectation = mock_add_matcher(mock,
				       "test_printk",
				       test_printk,
				       matchers,
				       ARRAY_SIZE(matchers));
	expectation->action = int_return(trgt, 5);
	EXPECT_EQ(test, 0, expectation->times_called);

	EXPECT_FALSE(test, mock_set_default_action(mock,
						   "test_printk",
						   test_printk,
						   int_return(trgt, -4)));

	ret = mock->do_expect(mock,
			      "test_printk",
			      test_printk,
			      two_param_types,
			      two_params,
			      ARRAY_SIZE(two_params));
	ASSERT_NOT_ERR_OR_NULL(test, ret);
	EXPECT_EQ(test, -4, *((int *) ret));
	EXPECT_EQ(test, 0, expectation->times_called);
}

/**
 * DOC: Testing the failure condition of different mock types.
 *
 * The following tests will test the behaviour of expectations under different
 * conditions. For example, what happens when an expectation:
 * - is not satisfied at the end of the test
 * - is fulfilled but the expected function is called again
 * - a function is called without expectations set on it
 *
 * For each of these conditions, there may be variations between the different
 * types of mocks: nice mocks, naggy mocks (the default) and strict mocks.
 *
 * More information about these mocks can be found in the kernel documentation
 * under Documentation/test/api/class-and-function-mocking
 */

/* Method called on strict mock with no expectations will fail */
static void mock_test_strict_no_expectations_will_fail(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;
	int param0 = 5, param1 = -5;
	static const char * const two_param_types[] = {"int", "int"};
	const void *two_params[] = {&param0, &param1};
	struct mock_expectation *expectation;

	mock->type = MOCK_TYPE_STRICT;

	mock_set_default_action(mock,
				"test_printk",
				test_printk,
				int_return(trgt, -4));

	expectation = EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test)));

	mock->do_expect(mock, "test_printk", test_printk, two_param_types,
		two_params, ARRAY_SIZE(two_params));
	mock_validate_expectations(mock);
}

/*
 * Method called on naggy mock with no expectations will not fail, but will show
 * a warning message
 */
static void mock_test_naggy_no_expectations_no_fail(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;
	int param0 = 5, param1 = -5;
	static const char * const two_param_types[] = {"int", "int"};
	const void *two_params[] = {&param0, &param1};
	struct mock_expectation *expectation;

	mock->type = MOCK_TYPE_NAGGY;

	mock_set_default_action(mock, "test_printk", test_printk,
		int_return(trgt, -4));

	expectation = Never(EXPECT_CALL(fail(mock_get_ctrl(mock_test),
					     any(test))));

	EXPECT_CALL(mock_vprintk(mock_get_ctrl(mock_test), any(test),
		va_format_cmp(test, str_contains(test,
			"Method was called with no expectations declared"),
		any(test))));

	mock->do_expect(mock,
			"test_printk",
			test_printk,
			two_param_types,
			two_params,
			ARRAY_SIZE(two_params));
	mock_validate_expectations(mock);
}

/* Method called on nice mock with no expectations will do nothing. */
static void mock_test_nice_no_expectations_do_nothing(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;
	int param0 = 5, param1 = -5;
	static const char * const two_param_types[] = {"int", "int"};
	const void *two_params[] = {&param0, &param1};
	struct mock_expectation *expectation;

	mock->type = MOCK_TYPE_NICE;

	mock_set_default_action(mock,
				"test_printk",
				test_printk,
				int_return(trgt, -4));

	expectation = Never(EXPECT_CALL(fail(mock_get_ctrl(mock_test),
					     any(test))));

	expectation = Never(EXPECT_CALL(mock_vprintk(mock_get_ctrl(mock_test),
						     any(test),
						     any(test))));

	mock->do_expect(mock,
			"test_printk",
			test_printk,
			two_param_types,
			two_params,
			ARRAY_SIZE(two_params));
	mock_validate_expectations(mock);
}

/* Test that method called on a mock (of any type) with no matching expectations
 * will fail test and print all the tried expectations.
 */
static void
run_method_called_but_no_matching_expectation_test(struct test *test,
						   enum mock_type mock_type)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;
	int param0 = 5, param1 = -5;
	static const char * const two_param_types[] = {"int", "int"};
	const void *two_params[] = {&param0, &param1};
	struct mock_expectation *handle;
	struct mock_param_matcher *two_matchers[] = {
		int_eq(trgt, 100),
		int_eq(trgt, 100)
	};
	mock_add_matcher(mock, "test_printk", test_printk, two_matchers,
		ARRAY_SIZE(two_matchers));
	handle = EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test)));

	mock->type = mock_type;

	mock->do_expect(mock, "test_printk", test_printk, two_param_types,
		two_params, ARRAY_SIZE(two_params));
}

static void mock_test_naggy_no_matching_expectations_fail(struct test *test)
{
	run_method_called_but_no_matching_expectation_test(test,
							   MOCK_TYPE_NAGGY);
}

static void mock_test_strict_no_matching_expectations_fail(struct test *test)
{
	run_method_called_but_no_matching_expectation_test(test,
							   MOCK_TYPE_STRICT);
}

static void mock_test_nice_no_matching_expectations_fail(struct test *test)
{
	run_method_called_but_no_matching_expectation_test(test,
							   MOCK_TYPE_NICE);
}

static void mock_test_mock_validate_expectations(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;
	struct mock_param_matcher *matchers[] = {
		int_eq(trgt, 5),
		int_eq(trgt, -4)
	};
	struct mock_expectation *expectation;

	EXPECT_EQ(test, mock_get_trgt(mock_test), mock->test);

	expectation = mock_add_matcher(mock,
				       "test_printk",
				       test_printk,
				       matchers,
				       ARRAY_SIZE(matchers));
	expectation->times_called = 0;
	expectation->min_calls_expected = 1;
	expectation->max_calls_expected = 1;

	EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test)));

	mock_validate_expectations(mock);
}

static void mock_test_validate_clears_expectations(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;
	struct mock_param_matcher *matchers[] = {
		int_eq(trgt, 5),
		int_eq(trgt, -4)
	};
	int param0 = 5, param1 = -4;
	static const char * const two_param_types[] = {"int", "int"};
	const void *two_params[] = {&param0, &param1};

	struct mock_expectation *expectation;

	mock->type = MOCK_TYPE_STRICT;

	/* If all goes well, the mock_test should not fail. */
	expectation = Never(EXPECT_CALL(fail(mock_get_ctrl(mock_test),
					     any(test))));

	/* Add an arbitrary matcher for 0 calls */
	expectation = mock_add_matcher(mock, "test_printk", test_printk,
		matchers, ARRAY_SIZE(matchers));
	expectation->times_called = 0;
	expectation->min_calls_expected = 0;
	expectation->max_calls_expected = 0;

	/* Should have 0 calls and should clear the previous expectation */
	mock_validate_expectations(mock);

	/* Add a new matcher for 1 call */
	expectation = mock_add_matcher(mock, "test_printk", test_printk,
		matchers, ARRAY_SIZE(matchers));
	expectation->times_called = 0;
	expectation->min_calls_expected = 1;
	expectation->max_calls_expected = 1;

	/* Satisfy previous matcher */
	mock->do_expect(mock, "test_printk", test_printk, two_param_types,
		two_params, ARRAY_SIZE(two_params));

	/*
	 * Validate previous satisfy; if we didn't clear the previous
	 * expectation, it would fail the mock_test.
	 */
	mock_validate_expectations(mock);
}


static void mock_stub(int a) { }

/* Common references for InSequence tests */
static int param_len = 1;
static const char * const param_type[] = {"int"};

static const void *a_params[] = { &(int){1} };
static const void *b_params[] = { &(int){2} };
static const void *c_params[] = { &(int){3} };

/* Simple test of InSequence, a -> b -> c */
static void mock_test_in_sequence_simple_pass(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;

	struct mock_param_matcher *a_matchers[] = { int_eq(trgt, 1) };
	struct mock_param_matcher *b_matchers[] = { int_eq(trgt, 2) };
	struct mock_param_matcher *c_matchers[] = { int_eq(trgt, 3) };

	struct mock_expectation *c = mock_add_matcher(mock, "c", mock_stub,
		c_matchers, param_len);
	struct mock_expectation *b = mock_add_matcher(mock, "b", mock_stub,
		b_matchers, param_len);
	struct mock_expectation *a = mock_add_matcher(mock, "a", mock_stub,
		a_matchers, param_len);

	InSequence(test, a, b, c);

	Never(EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test))));

	mock->do_expect(mock, "a", mock_stub, param_type, a_params, param_len);
	mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);
	mock->do_expect(mock, "c", mock_stub, param_type, c_params, param_len);

	mock_validate_expectations(mock);
}

static void mock_test_in_sequence_simple_fail(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;

	struct mock_param_matcher *a_matchers[] = { int_eq(trgt, 1) };
	struct mock_param_matcher *b_matchers[] = { int_eq(trgt, 2) };
	struct mock_param_matcher *c_matchers[] = { int_eq(trgt, 3) };

	struct mock_expectation *c = mock_add_matcher(mock, "c", mock_stub,
		c_matchers, param_len);
	struct mock_expectation *b = mock_add_matcher(mock, "b", mock_stub,
		b_matchers, param_len);
	struct mock_expectation *a = mock_add_matcher(mock, "a", mock_stub,
		a_matchers, param_len);

	InSequence(test, a, b, c);

	EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test)));

	mock->do_expect(mock, "a", mock_stub, param_type, a_params, param_len);
	mock->do_expect(mock, "c", mock_stub, param_type, c_params, param_len);
	mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);
}

/* More complex test of InSequence on two chains in v formation:
 *   a -> c
 *   b -> c
 */
static void mock_test_in_sequence_abc_success(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;

	struct mock_param_matcher *a_matchers[] = { int_eq(trgt, 1) };
	struct mock_param_matcher *b_matchers[] = { int_eq(trgt, 2) };
	struct mock_param_matcher *c_matchers[] = { int_eq(trgt, 3) };

	struct mock_expectation *c = mock_add_matcher(mock, "c", mock_stub,
		c_matchers, param_len);
	struct mock_expectation *b = mock_add_matcher(mock, "b", mock_stub,
		b_matchers, param_len);
	struct mock_expectation *a = mock_add_matcher(mock, "a", mock_stub,
		a_matchers, param_len);

	InSequence(test, a, c);
	InSequence(test, b, c);

	Never(EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test))));

	mock->do_expect(mock, "a", mock_stub, param_type, a_params, param_len);
	mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);
	mock->do_expect(mock, "c", mock_stub, param_type, c_params, param_len);
}

static void mock_test_in_sequence_bac_success(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;

	struct mock_param_matcher *a_matchers[] = { int_eq(trgt, 1) };
	struct mock_param_matcher *b_matchers[] = { int_eq(trgt, 2) };
	struct mock_param_matcher *c_matchers[] = { int_eq(trgt, 3) };

	struct mock_expectation *c = mock_add_matcher(mock, "c", mock_stub,
		c_matchers, param_len);
	struct mock_expectation *b = mock_add_matcher(mock, "b", mock_stub,
		b_matchers, param_len);
	struct mock_expectation *a = mock_add_matcher(mock, "a", mock_stub,
		a_matchers, param_len);

	InSequence(test, a, c);
	InSequence(test, b, c);

	Never(EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test))));

	mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);
	mock->do_expect(mock, "a", mock_stub, param_type, a_params, param_len);
	mock->do_expect(mock, "c", mock_stub, param_type, c_params, param_len);
}

static void mock_test_in_sequence_no_a_fail(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;

	struct mock_param_matcher *a_matchers[] = { int_eq(trgt, 1) };
	struct mock_param_matcher *b_matchers[] = { int_eq(trgt, 2) };
	struct mock_param_matcher *c_matchers[] = { int_eq(trgt, 3) };

	struct mock_expectation *c = mock_add_matcher(mock, "c", mock_stub,
		c_matchers, param_len);
	struct mock_expectation *b = mock_add_matcher(mock, "b", mock_stub,
		b_matchers, param_len);
	struct mock_expectation *a = mock_add_matcher(mock, "a", mock_stub,
		a_matchers, param_len);

	InSequence(test, a, c);
	InSequence(test, b, c);

	EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test)));

	mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);
	mock->do_expect(mock, "c", mock_stub, param_type, c_params, param_len);
}

static void mock_test_in_sequence_retire_on_saturation(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;

	struct mock_param_matcher *a_matchers[] = { int_eq(trgt, 1) };
	struct mock_param_matcher *b_matchers[] = { int_eq(trgt, 2) };
	struct mock_param_matcher *c_matchers[] = { int_eq(trgt, 3) };

	struct mock_expectation *c = mock_add_matcher(mock, "c", mock_stub,
                c_matchers, param_len);
	struct mock_expectation *b = mock_add_matcher(mock, "b", mock_stub,
                b_matchers, param_len);
	struct mock_expectation *a_1 = mock_add_matcher(mock, "a", mock_stub,
                a_matchers, param_len);
        struct mock_expectation *a_2 = mock_add_matcher(mock, "a", mock_stub,
                a_matchers, param_len);

	InSequence(test, a_1, b, a_2, c);

	Never(EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test))));

        mock->do_expect(mock, "a", mock_stub, param_type, a_params, param_len);
	mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);
        mock->do_expect(mock, "a", mock_stub, param_type, a_params, param_len);
	mock->do_expect(mock, "c", mock_stub, param_type, c_params, param_len);

        mock_validate_expectations(mock);
}

static void mock_test_atleast(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;

	struct mock_param_matcher *a_matchers[] = { int_eq(trgt, 1) };
	struct mock_param_matcher *b_matchers[] = { int_eq(trgt, 2) };

        struct mock_expectation *a = mock_add_matcher(mock, "a", mock_stub,
                a_matchers, param_len);
	struct mock_expectation *b = mock_add_matcher(mock, "b", mock_stub,
                b_matchers, param_len);

        AtLeast(2, a);
        AtLeast(1, b);
	Never(EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test))));

        mock->do_expect(mock, "a", mock_stub, param_type, a_params, param_len);
	mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);
        mock->do_expect(mock, "a", mock_stub, param_type, a_params, param_len);
        mock->do_expect(mock, "a", mock_stub, param_type, a_params, param_len);

        mock_validate_expectations(mock);
}

static void mock_test_atleast_fail(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;

	struct mock_param_matcher *b_matchers[] = { int_eq(trgt, 2) };

        struct mock_expectation *b = mock_add_matcher(mock, "b", mock_stub,
                b_matchers, param_len);

        AtLeast(2, b);
        EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test)));

	mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);

        mock_validate_expectations(mock);
}

static void mock_test_atmost(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;

	struct mock_param_matcher *a_matchers[] = { int_eq(trgt, 1) };
	struct mock_param_matcher *b_matchers[] = { int_eq(trgt, 2) };
	struct mock_param_matcher *c_matchers[] = { int_eq(trgt, 3) };

        struct mock_expectation *a = mock_add_matcher(mock, "a", mock_stub,
                a_matchers, param_len);
        struct mock_expectation *b = mock_add_matcher(mock, "b", mock_stub,
                b_matchers, param_len);
	struct mock_expectation *c = mock_add_matcher(mock, "c", mock_stub,
                c_matchers, param_len);

        AtMost(2, a);
        AtMost(1, b);
        AtMost(2, c);
	Never(EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test))));

        mock->do_expect(mock, "a", mock_stub, param_type, a_params, param_len);
        mock->do_expect(mock, "a", mock_stub, param_type, a_params, param_len);
	mock->do_expect(mock, "c", mock_stub, param_type, c_params, param_len);

        mock_validate_expectations(mock);
}

static void mock_test_atmost_fail(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;

	struct mock_param_matcher *b_matchers[] = { int_eq(trgt, 2) };

        struct mock_expectation *b = mock_add_matcher(mock, "b", mock_stub,
                b_matchers, param_len);

        AtMost(2, b);
        EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test)));

	mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);
        mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);
	mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);

        mock_validate_expectations(mock);
}

static void mock_test_between(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;

	struct mock_param_matcher *b_matchers[] = { int_eq(trgt, 2) };

        struct mock_expectation *b = mock_add_matcher(mock, "b", mock_stub,
                b_matchers, param_len);

        Between(2, 4, b);
        Never(EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test))));

	mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);
        mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);
	mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);

        mock_validate_expectations(mock);
}

static void mock_test_between_fail(struct test *test)
{
	struct mock_test_context *ctx = test->priv;
	struct MOCK(test) *mock_test = ctx->mock_test;
	struct test *trgt = mock_get_trgt(mock_test);
	struct mock *mock = ctx->mock;

	struct mock_param_matcher *a_matchers[] = { int_eq(trgt, 1) };
	struct mock_param_matcher *b_matchers[] = { int_eq(trgt, 2) };

        struct mock_expectation *a = mock_add_matcher(mock, "a", mock_stub,
                a_matchers, param_len);
        struct mock_expectation *b = mock_add_matcher(mock, "b", mock_stub,
                b_matchers, param_len);

        Between(2, 3, a);
        Between(1, 2, b);
        Times(2, EXPECT_CALL(fail(mock_get_ctrl(mock_test), any(test))));

	mock->do_expect(mock, "a", mock_stub, param_type, a_params, param_len);
	mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);
        mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);
	mock->do_expect(mock, "b", mock_stub, param_type, b_params, param_len);

        mock_validate_expectations(mock);
}

void *do_mocked_fail(struct mock_action *this, const void **params, int len)
{
	static const int ret;
	struct test_stream * const *stream_ptr = params[0];
	struct test_stream *stream = *stream_ptr;

	stream->set_level(stream, KERN_ERR);
	stream->commit(stream);
	return (void *) &ret;
}

static struct mock_action mocked_fail = {
	.do_action = do_mocked_fail
};

static int mock_test_init(struct test *test)
{
	struct mock_test_context *ctx;

	ctx = test_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	test->priv = ctx;

	ctx->mock_test = CONSTRUCT_MOCK(test, test);
	if (!ctx->mock_test)
		return -EINVAL;

	ctx->mock = test_kzalloc(test, sizeof(*ctx->mock), GFP_KERNEL);
	if (!ctx->mock)
		return -ENOMEM;
	mock_init_ctrl(mock_get_trgt(ctx->mock_test), ctx->mock);

	/* This test suite tests the behaviour of the error messages printed
	 * when mocks fail, which requires the mocked fail to commit the
	 * stream.
	 */
	mock_set_default_action(mock_get_ctrl(ctx->mock_test),
		"fail", fail, &mocked_fail);
	return 0;
}

static struct test_case mock_test_cases[] = {
	TEST_CASE(mock_test_do_expect_basic),
	TEST_CASE(mock_test_ptr_eq),
	TEST_CASE(mock_test_ptr_eq_not_equal),
	TEST_CASE(mock_test_failed_expect_call_fails_test),
	TEST_CASE(mock_test_do_expect_default_return),
	TEST_CASE(mock_test_mock_validate_expectations),
	TEST_CASE(mock_test_strict_no_expectations_will_fail),
	TEST_CASE(mock_test_naggy_no_expectations_no_fail),
	TEST_CASE(mock_test_nice_no_expectations_do_nothing),
	TEST_CASE(mock_test_strict_no_matching_expectations_fail),
	TEST_CASE(mock_test_naggy_no_matching_expectations_fail),
	TEST_CASE(mock_test_nice_no_matching_expectations_fail),
	TEST_CASE(mock_test_validate_clears_expectations),
	TEST_CASE(mock_test_in_sequence_simple_pass),
	TEST_CASE(mock_test_in_sequence_simple_fail),
	TEST_CASE(mock_test_in_sequence_abc_success),
	TEST_CASE(mock_test_in_sequence_bac_success),
	TEST_CASE(mock_test_in_sequence_no_a_fail),
        TEST_CASE(mock_test_in_sequence_retire_on_saturation),
        TEST_CASE(mock_test_atleast),
        TEST_CASE(mock_test_atleast_fail),
        TEST_CASE(mock_test_atmost),
        TEST_CASE(mock_test_atmost_fail),
        TEST_CASE(mock_test_between),
        TEST_CASE(mock_test_between_fail),
        {},
};

static struct test_module mock_test_module = {
	.name = "mock-test",
	.init = mock_test_init,
	.test_cases = mock_test_cases,
};

module_test(mock_test_module);
