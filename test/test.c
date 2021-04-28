// SPDX-License-Identifier: GPL-2.0
/*
 * Base unit test (KUnit) API.
 *
 * Copyright (C) 2018, Google LLC.
 * Author: Brendan Higgins <brendanhiggins@google.com>
 */

#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <test/test.h>
#include <test/try-catch.h>

struct test_global_context {
	struct list_head initcalls;
};

static struct test_global_context test_global_context = {
	.initcalls = LIST_HEAD_INIT(test_global_context.initcalls),
};

void test_install_initcall(struct test_initcall *initcall)
{
	list_add_tail(&initcall->node, &test_global_context.initcalls);
}

static bool test_get_success(struct test *test)
{
	unsigned long flags;
	bool success;

	spin_lock_irqsave(&test->lock, flags);
	success = test->success;
	spin_unlock_irqrestore(&test->lock, flags);

	return success;
}

static void test_set_success(struct test *test, bool success)
{
	unsigned long flags;

	spin_lock_irqsave(&test->lock, flags);
	test->success = success;
	spin_unlock_irqrestore(&test->lock, flags);
}

static bool test_get_death_test(struct test *test)
{
	unsigned long flags;
	bool death_test;

	spin_lock_irqsave(&test->lock, flags);
	death_test = test->death_test;
	spin_unlock_irqrestore(&test->lock, flags);

	return death_test;
}

static int test_vprintk_emit(const struct test *test,
			     int level,
			     const char *fmt,
			     va_list args)
{
	return vprintk_emit(0, level, NULL, 0, fmt, args);
}

static int test_printk_emit(const struct test *test,
			    int level,
			    const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = test_vprintk_emit(test, level, fmt, args);
	va_end(args);

	return ret;
}

static void test_vprintk(const struct test *test,
			 const char *level,
			 struct va_format *vaf)
{
	test_printk_emit(test,
			 level[1] - '0',
			 "kunit %s: %pV", test->name, vaf);
}

static void test_fail(struct test *test, struct test_stream *stream)
{
	test_set_success(test, false);
	stream->set_level(stream, KERN_ERR);
	stream->commit(stream);
}

static void __noreturn test_abort(struct test *test)
{
	test_set_death_test(test, true);
	test_try_catch_throw(&test->try_catch);

	/*
	 * Throw could not abort from test.
	 *
	 * XXX: we should never reach this line! As test_try_catch_throw is
	 * marked __noreturn.
	 */
	WARN_ONCE(true, "Throw could not abort from test!\n");
}

int test_init_test(struct test *test, const char *name)
{
	INIT_LIST_HEAD(&test->resources);
	INIT_LIST_HEAD(&test->post_conditions);
	spin_lock_init(&test->lock);
	test->name = name;
	test->vprintk = test_vprintk;
	test->fail = test_fail;
	test->abort = test_abort;

	return 0;
}

static void test_case_internal_cleanup(struct test *test)
{
	struct test_initcall *initcall;

	list_for_each_entry(initcall, &test_global_context.initcalls, node) {
		initcall->exit(initcall);
	}

	test_cleanup(test);
}

/*
 * Initializes and runs test case. Does not clean up or do post validations.
 */
static void test_run_case_internal(struct test *test,
				   struct test_module *module,
				   struct test_case *test_case)
{
	struct test_initcall *initcall;
	int ret;

	list_for_each_entry(initcall, &test_global_context.initcalls, node) {
		ret = initcall->init(initcall, test);
		if (ret) {
			test_err(test, "failed to initialize: %d", ret);
			test_set_success(test, false);
			return;
		}
	}

	if (module->init) {
		ret = module->init(test);
		if (ret) {
			test_err(test, "failed to initialize: %d", ret);
			test_set_success(test, false);
			return;
		}
	}

	test_case->run_case(test);
}

/*
 * Handles an unexpected crash in a test case.
 */
static void test_handle_test_crash(struct test *test,
				   struct test_module *module,
				   struct test_case *test_case)
{
	test_err(test, "%s crashed", test_case->name);
	/*
	 * TODO(brendanhiggins@google.com): This prints the stack trace up
	 * through this frame, not up to the frame that caused the crash.
	 */
	show_stack(NULL, NULL);

	test_case_internal_cleanup(test);
}

struct test_try_catch_context {
	struct test *test;
	struct test_module *module;
	struct test_case *test_case;
};


/*
 * Performs post validations and cleanup after a test case was run.
 * XXX: Should ONLY BE CALLED AFTER test_run_case_internal!
 */
static void test_run_case_cleanup(struct test *test,
				  struct test_module *module,
				  struct test_case *test_case)
{
	struct test_post_condition *condition, *condition_safe;

	list_for_each_entry_safe(condition,
				 condition_safe,
				 &test->post_conditions,
				 node) {
		condition->validate(condition);
		list_del(&condition->node);
	}

	if (module->exit)
		module->exit(test);

	test_case_internal_cleanup(test);
}

static void test_try_run_case(void *data)
{
	struct test_try_catch_context *ctx = data;
	struct test *test = ctx->test;
	struct test_module *module = ctx->module;
	struct test_case *test_case = ctx->test_case;

	/*
	 * test_run_case_internal may encounter a fatal error; if it does,
	 * abort will be called, this thread will exit, and finally the parent
	 * thread will resume control and handle any necessary clean up.
	 */
	test_run_case_internal(test, module, test_case);
	/* This line may never be reached. */
	test_run_case_cleanup(test, module, test_case);
}

static void test_catch_run_case(void *data)
{
	struct test_try_catch_context *ctx = data;
	struct test *test = ctx->test;
	struct test_module *module = ctx->module;
	struct test_case *test_case = ctx->test_case;
	int try_exit_code = test_try_catch_get_result(&test->try_catch);

	if (try_exit_code) {
		test_set_success(test, false);
		/*
		 * Test case could not finish, we have no idea what state it is
		 * in, so don't do clean up.
		 */
		if (try_exit_code == -ETIMEDOUT)
			test_err(test, "test case timed out\n");
		/*
		 * Unknown internal error occurred preventing test case from
		 * running, so there is nothing to clean up.
		 */
		else
			test_err(test, "internal error occurred preventing test case from running: %d\n",
				  try_exit_code);
		return;
	}

	if (test_get_death_test(test)) {
		/*
		 * EXPECTED DEATH: test_run_case_internal encountered
		 * anticipated fatal error. Everything should be in a safe
		 * state.
		 */
		test_run_case_cleanup(test, module, test_case);
	} else {
		/*
		 * UNEXPECTED DEATH: test_run_case_internal encountered an
		 * unanticipated fatal error. We have no idea what the state of
		 * the test case is in.
		 */
		test_handle_test_crash(test, module, test_case);
		test_set_success(test, false);
	}
}

/*
 * Performs all logic to run a test case. It also catches most errors that
 * occurs in a test case and reports them as failures.
 */
static bool test_run_case_catch_errors(struct test *test,
				       struct test_module *module,
				       struct test_case *test_case)
{
	struct test_try_catch *try_catch = &test->try_catch;
	struct test_try_catch_context context;

	test_set_success(test, true);
	test_set_death_test(test, false);

	test_try_catch_init(try_catch,
			    test,
			    test_try_run_case,
			    test_catch_run_case);
	context.test = test;
	context.module = module;
	context.test_case = test_case;
	test_try_catch_run(try_catch, &context);

	return test_get_success(test);
}

int test_run_tests(struct test_module *module)
{
	bool all_passed = true, success;
	struct test_case *test_case;
	struct test test;
	int ret;

	ret = test_init_test(&test, module->name);
	if (ret)
		return ret;

	for (test_case = module->test_cases; test_case->run_case; test_case++) {
		success = test_run_case_catch_errors(&test, module, test_case);
		if (!success)
			all_passed = false;

		test_info(&test,
			  "%s %s",
			  test_case->name,
			  success ? "passed" : "failed");
	}

	if (all_passed)
		test_info(&test, "all tests passed");
	else
		test_info(&test, "one or more tests failed");

	return 0;
}

struct test_resource *test_alloc_resource(struct test *test,
					  int (*init)(struct test_resource *,
						      void *),
					  void (*free)(struct test_resource *),
					  void *context)
{
	struct test_resource *res;
	int ret;

	res = kzalloc(sizeof(*res), GFP_KERNEL);
	if (!res)
		return NULL;

	ret = init(res, context);
	if (ret)
		return NULL;

	res->free = free;
	list_add_tail(&res->node, &test->resources);

	return res;
}

void test_free_resource(struct test *test, struct test_resource *res)
{
	res->free(res);
	list_del(&res->node);
	kfree(res);
}

struct test_kmalloc_params {
	size_t size;
	gfp_t gfp;
};

static int test_kmalloc_init(struct test_resource *res, void *context)
{
	struct test_kmalloc_params *params = context;

	res->allocation = kmalloc(params->size, params->gfp);
	if (!res->allocation)
		return -ENOMEM;

	return 0;
}

static void test_kmalloc_free(struct test_resource *res)
{
	kfree(res->allocation);
}

void *test_kmalloc(struct test *test, size_t size, gfp_t gfp)
{
	struct test_kmalloc_params params;
	struct test_resource *res;

	params.size = size;
	params.gfp = gfp;

	res = test_alloc_resource(test,
				  test_kmalloc_init,
				  test_kmalloc_free,
				  &params);

	if (res)
		return res->allocation;
	else
		return NULL;
}

void test_cleanup(struct test *test)
{
	struct test_resource *resource, *resource_safe;

	list_for_each_entry_safe(resource,
				 resource_safe,
				 &test->resources,
				 node) {
		test_free_resource(test, resource);
	}
}

void test_printk(const char *level,
		 const struct test *test,
		 const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	test->vprintk(test, level, &vaf);

	va_end(args);
}
