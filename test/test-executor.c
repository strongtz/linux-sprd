#include <linux/init.h>
#include <linux/printk.h>
#include <test/test.h>

extern char __test_modules_start;
extern char __test_modules_end;

static bool test_run_all_tests(void)
{
	struct test_module **module;
	struct test_module ** const test_modules_start =
			(struct test_module **) &__test_modules_start;
	struct test_module ** const test_modules_end =
			(struct test_module **) &__test_modules_end;
	bool has_test_failed = false;

	for (module = test_modules_start; module < test_modules_end; ++module) {
		if (test_run_tests(*module))
			has_test_failed = true;
	}

	return !has_test_failed;
}


int test_executor_init(void)
{
	if (test_run_all_tests())
		return 0;
	else
		return -EFAULT;
}
