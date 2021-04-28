/* SPDX-License-Identifier: GPL-2.0 */
/*
 * An API to allow a function, that may fail, to be executed, and recover in a
 * controlled manner.
 *
 * Copyright (C) 2019, Google LLC.
 * Author: Brendan Higgins <brendanhiggins@google.com>
 */

#ifndef _TEST_TRY_CATCH_H
#define _TEST_TRY_CATCH_H

#include <linux/types.h>

typedef void (*test_try_catch_func_t)(void *);

struct test;

/*
 * struct test_try_catch - provides a generic way to run code which might fail.
 * @context: used to pass user data to the try and catch functions.
 *
 * test_try_catch provides a generic, architecture independent way to execute
 * an arbitrary function of type test_try_catch_func_t which may bail out by
 * calling test_try_catch_throw(). If test_try_catch_throw() is called, @try
 * is stopped at the site of invocation and @catch is catch is called.
 *
 * struct test_try_catch provides a generic interface for the functionality
 * needed to implement test->abort() which in turn is needed for implementing
 * assertions. Assertions allow stating a precondition for a test simplifying
 * how test cases are written and presented.
 *
 * Assertions are like expectations, except they abort (call
 * test_try_catch_throw()) when the specified condition is not met. This is
 * useful when you look at a test case as a logical statement about some piece
 * of code, where assertions are the premises for the test case, and the
 * conclusion is a set of predicates, rather expectations, that must all be
 * true. If your premises are violated, it does not makes sense to continue.
 */
struct test_try_catch {
	/* private: internal use only. */
	void (*run)(struct test_try_catch *try_catch);
	void __noreturn (*throw)(struct test_try_catch *try_catch);
	struct test *test;
	struct completion *try_completion;
	int try_result;
	test_try_catch_func_t try;
	test_try_catch_func_t catch;
	void *context;
};

/*
 * Exposed to be overridden for other architectures.
 */
void test_try_catch_init_internal(struct test_try_catch *try_catch);

static inline void test_try_catch_init(struct test_try_catch *try_catch,
					struct test *test,
					test_try_catch_func_t try,
					test_try_catch_func_t catch)
{
	try_catch->test = test;
	test_try_catch_init_internal(try_catch);
	try_catch->try = try;
	try_catch->catch = catch;
}

static inline void test_try_catch_run(struct test_try_catch *try_catch,
				       void *context)
{
	try_catch->context = context;
	try_catch->run(try_catch);
}

static inline void __noreturn test_try_catch_throw(
		struct test_try_catch *try_catch)
{
	try_catch->throw(try_catch);
}

static inline int test_try_catch_get_result(struct test_try_catch *try_catch)
{
	return try_catch->try_result;
}

/*
 * Exposed for testing only.
 */
void test_generic_try_catch_init(struct test_try_catch *try_catch);

#endif /* _TEST_TRY_CATCH_H */
