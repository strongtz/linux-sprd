import re
from datetime import datetime

from collections import namedtuple

TestResult = namedtuple('TestResult', ['status','modules','log'])

TestModule = namedtuple('TestModule', ['status','name','cases'])

TestCase = namedtuple('TestCase', ['status','name','log'])

class TestStatus(object):
	SUCCESS = 'SUCCESS'
	FAILURE = 'FAILURE'
	TEST_CRASHED = 'TEST_CRASHED'
	TIMED_OUT = 'TIMED_OUT'
	KERNEL_CRASHED = 'KERNEL_CRASHED'

kunit_start_re = re.compile('console .* enabled')
kunit_end_re = re.compile('List of all partitions:')

TIMED_OUT_LOG_ENTRY = 'Timeout Reached - Process Terminated'

class KernelCrashException(Exception):
	pass

def isolate_kunit_output(kernel_output):
	started = False
	for line in kernel_output:
		if kunit_start_re.match(line):
			started = True
		elif kunit_end_re.match(line):
			return
		elif started:
			yield line
	# Output ended without encountering end marker, kernel probably panicked
	# or crashed unexpectedly.
	raise KernelCrashException()

def raw_output(kernel_output):
	for line in kernel_output:
		print(line)

DIVIDER = "=" * 30

RESET = '\033[0;0m'

def red(text):
	return '\033[1;31m' + text + RESET

def yellow(text):
	return '\033[1;33m' + text + RESET

def green(text):
	return '\033[1;32m' + text + RESET

def timestamp(message):
	return '[%s] %s' % (datetime.now().strftime('%H:%M:%S'), message)

def timestamp_log(log):
	for m in log:
		yield timestamp(m)

def parse_run_tests(kernel_output):
	test_case_output = re.compile('^kunit .*?: (.*)$')

	test_module_success = re.compile('^kunit (.*): all tests passed')
	test_module_fail = re.compile('^kunit (.*): one or more tests failed')

	test_case_success = re.compile('^kunit (.*): (.*) passed')
	test_case_fail = re.compile('^kunit (.*): (.*) failed')
	test_case_crash = re.compile('^kunit (.*): (.*) crashed')

	total_tests = set()
	failed_tests = set()
	crashed_tests = set()

	test_status = TestStatus.SUCCESS
	modules = []
	log_list = []

	def get_test_module_name(match):
		return match.group(1)
	def get_test_case_name(match):
		return match.group(2)

	def get_test_name(match):
		return match.group(1) + ":" + match.group(2)

	current_case_log = []
	current_module_cases = []
	did_kernel_crash = False
	did_timeout = False

	def end_one_test(match, log):
		del log[:]
		total_tests.add(get_test_name(match))

	print(timestamp(DIVIDER))
	try:
		for line in kernel_output:
			log_list.append(line)

			# Ignore module output:
			module_success = test_module_success.match(line)
			if module_success:
				print(timestamp(DIVIDER))
				modules.append(
				  TestModule(TestStatus.SUCCESS,
					     get_test_module_name(module_success),
					     current_module_cases))
				current_module_cases = []
				continue
			module_fail = test_module_fail.match(line)
			if module_fail:
				print(timestamp(DIVIDER))
				modules.append(
				  TestModule(TestStatus.FAILURE,
					     get_test_module_name(module_fail),
					     current_module_cases))
				current_module_cases = []
				continue

			match = re.match(test_case_success, line)
			if match:
				print(timestamp(green("[PASSED] ") + get_test_name(match)))
				current_module_cases.append(
					TestCase(TestStatus.SUCCESS,
						 get_test_case_name(match),
						 '\n'.join(current_case_log)))
				end_one_test(match, current_case_log)
				continue

			match = re.match(test_case_fail, line)
			# Crashed tests will report as both failed and crashed. We only
			# want to show and count it once.
			if match and get_test_name(match) not in crashed_tests:
				failed_tests.add(get_test_name(match))
				print(timestamp(red("[FAILED] " + get_test_name(match))))
				for out in timestamp_log(map(yellow, current_case_log)):
					print(out)
				print(timestamp(""))
				current_module_cases.append(
					TestCase(TestStatus.FAILURE,
						 get_test_case_name(match),
						 '\n'.join(current_case_log)))
				if test_status != TestStatus.TEST_CRASHED:
					test_status = TestStatus.FAILURE
				end_one_test(match, current_case_log)
				continue

			match = re.match(test_case_crash, line)
			if match:
				crashed_tests.add(get_test_name(match))
				print(timestamp(yellow("[CRASH] " + get_test_name(match))))
				for out in timestamp_log(current_case_log):
					print(out)
				print(timestamp(""))
				current_module_cases.append(
					TestCase(TestStatus.TEST_CRASHED,
						 get_test_case_name(match),
						 '\n'.join(current_case_log)))
				test_status = TestStatus.TEST_CRASHED
				end_one_test(match, current_case_log)
				continue

			if line.strip() == TIMED_OUT_LOG_ENTRY:
				print(timestamp(red("[TIMED-OUT] Process Terminated")))
				did_timeout = True
				test_status = TestStatus.TIMED_OUT
				break

			# Strip off the `kunit module-name:` prefix
			match = re.match(test_case_output, line)
			if match:
				current_case_log.append(match.group(1))
			else:
				current_case_log.append(line)
	except KernelCrashException:
		did_kernel_crash = True
		test_status = TestStatus.KERNEL_CRASHED
		print(timestamp(red("The KUnit kernel crashed unexpectedly and was " +
							"unable to finish running tests!")))
		print(timestamp(red("These are the logs from the most " +
							"recently running test:")))
		print(timestamp(DIVIDER))
		for out in timestamp_log(current_case_log):
			print(out)
		print(timestamp(DIVIDER))

	fmt = green if (len(failed_tests) + len(crashed_tests) == 0
			and not did_kernel_crash and not did_timeout) else red
	message = "Testing complete."
	if did_kernel_crash:
		message = "Before the crash:"
	elif did_timeout:
		message = "Before timing out:"

	print(timestamp(fmt(message + " %d tests run. %d failed. %d crashed." %
				(len(total_tests), len(failed_tests), len(crashed_tests)))))

	return TestResult(test_status, modules, '\n'.join(log_list))
