// SPDX-License-Identifier: GPL-2.0
/*
 * An API to allow a function, that may fail, to be executed, and recover in a
 * controlled manner.
 *
 * Copyright (C) 2019, Google LLC.
 * Author: Brendan Higgins <brendanhiggins@google.com>
 */

#include <test/try-catch.h>
#include <test/test.h>
#include <linux/completion.h>
#include <linux/kthread.h>

static void __noreturn test_generic_throw(struct test_try_catch *try_catch)
{
	try_catch->try_result = -EFAULT;
	complete_and_exit(try_catch->try_completion, -EFAULT);
}

static int test_generic_run_threadfn_adapter(void *data)
{
	struct test_try_catch *try_catch = data;

	try_catch->try(try_catch->context);

	complete_and_exit(try_catch->try_completion, 0);
}

static void test_generic_run_try_catch(struct test_try_catch *try_catch)
{
	DECLARE_COMPLETION_ONSTACK(try_completion);
	struct test *test = try_catch->test;
	struct task_struct *task_struct;
	int exit_code, status;

	try_catch->try_completion = &try_completion;
	try_catch->try_result = 0;
	task_struct = kthread_run(test_generic_run_threadfn_adapter,
				  try_catch,
				  "test_try_catch_thread");
	if (IS_ERR(task_struct)) {
		try_catch->catch(try_catch->context);
		return;
	}

	/*
	 * TODO(brendanhiggins@google.com): We should probably have some type of
	 * variable timeout here. The only question is what that timeout value
	 * should be.
	 *
	 * The intention has always been, at some point, to be able to label
	 * tests with some type of size bucket (unit/small, integration/medium,
	 * large/system/end-to-end, etc), where each size bucket would get a
	 * default timeout value kind of like what Bazel does:
	 * https://docs.bazel.build/versions/master/be/common-definitions.html#test.size
	 * There is still some debate to be had on exactly how we do this. (For
	 * one, we probably want to have some sort of test runner level
	 * timeout.)
	 *
	 * For more background on this topic, see:
	 * https://mike-bland.com/2011/11/01/small-medium-large.html
	 */
	status = wait_for_completion_timeout(&try_completion,
					     300 * MSEC_PER_SEC); /* 5 min */
	if (status < 0) {
		test_err(test, "try timed out\n");
		try_catch->try_result = -ETIMEDOUT;
	}

	exit_code = try_catch->try_result;

	if (!exit_code)
		return;

	if (exit_code == -EFAULT)
		try_catch->try_result = 0;
	else if (exit_code == -EINTR)
		test_err(test, "wake_up_process() was never called\n");
	else if (exit_code)
		test_err(test, "Unknown error: %d\n", exit_code);

	try_catch->catch(try_catch->context);
}

void test_generic_try_catch_init(struct test_try_catch *try_catch)
{
	try_catch->run = test_generic_run_try_catch;
	try_catch->throw = test_generic_throw;
}

void __weak test_try_catch_init_internal(struct test_try_catch *try_catch)
{
	test_generic_try_catch_init(try_catch);
}
