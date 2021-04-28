// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit test for core test infrastructure.
 *
 * Copyright (C) 2018, Google LLC.
 * Author: Brendan Higgins <brendanhiggins@google.com>
 */
#include <test/test.h>

typedef void (*test_resource_free_t)(struct test_resource *);

/*
 * Context for testing test managed resources
 * is_resource_initialized is used to test arbitrary resources
 */
struct test_test_context {
	struct test test;
	bool is_resource_initialized;
};

static int fake_resource_init(struct test_resource *res, void *context)
{
	struct test_test_context *ctx = context;

	res->allocation = &ctx->is_resource_initialized;
	ctx->is_resource_initialized = true;
	return 0;
}

static void fake_resource_free(struct test_resource *res)
{
	bool *is_resource_initialized = res->allocation;

	*is_resource_initialized = false;
}

static void test_test_init_resources(struct test *test)
{
	struct test_test_context *ctx = test->priv;

	test_init_test(&ctx->test, "testing_test_init_test");

	EXPECT_TRUE(test, list_empty(&ctx->test.resources));
}

static void test_test_alloc_resource(struct test *test)
{
	struct test_test_context *ctx = test->priv;
	struct test_resource *res;

	res = test_alloc_resource(&ctx->test,
				   fake_resource_init,
				   fake_resource_free,
				   ctx);

	ASSERT_NOT_ERR_OR_NULL(test, res);
	EXPECT_EQ(test, &ctx->is_resource_initialized, res->allocation);
	EXPECT_TRUE(test, list_is_last(&res->node, &ctx->test.resources));
	EXPECT_EQ(test,
		  (test_resource_free_t)fake_resource_free,
		  (test_resource_free_t)res->free);
}

static void test_test_free_resource(struct test *test)
{
	struct test_test_context *ctx = test->priv;
	struct test_resource *res = test_alloc_resource(&ctx->test,
							  fake_resource_init,
							  fake_resource_free,
							  ctx);

	test_free_resource(&ctx->test, res);

	EXPECT_EQ(test, false, ctx->is_resource_initialized);
	EXPECT_TRUE(test, list_empty(&ctx->test.resources));
}

static void test_test_cleanup_resources(struct test *test)
{
	int i;
	const int num_res = 5;
	struct test_test_context *ctx = test->priv;
	struct test_resource *resources[num_res];

	for (i = 0; i < num_res; i++) {
		resources[i] = test_alloc_resource(&ctx->test,
						    fake_resource_init,
						    fake_resource_free,
						    ctx);
	}

	test_cleanup(&ctx->test);

	EXPECT_TRUE(test, list_empty(&ctx->test.resources));
}

static int test_test_init(struct test *test)
{
	struct test_test_context *ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);

	if (!ctx)
		return -ENOMEM;
	test->priv = ctx;

	test_init_test(&ctx->test, "test_test_context");
	return 0;
}

static void test_test_exit(struct test *test)
{
	struct test_test_context *ctx = test->priv;

	test_cleanup(&ctx->test);
	kfree(ctx);
}

static struct test_case test_test_cases[] = {
	TEST_CASE(test_test_init_resources),
	TEST_CASE(test_test_alloc_resource),
	TEST_CASE(test_test_free_resource),
	TEST_CASE(test_test_cleanup_resources),
	{},
};

static struct test_module test_test_module = {
	.name = "test-test",
	.init = test_test_init,
	.exit = test_test_exit,
	.test_cases = test_test_cases,
};
module_test(test_test_module);
