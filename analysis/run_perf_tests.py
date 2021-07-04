import os

LOG_FILENAME = "build/log/perf.log"
TESTS_RELATIVE_PATH = "./build/src/tests/perf_tests"


def clear_log_file(filename):
    result = os.system('rm ' + filename)
    print(f'Result when removing {filename}: {result}')


def run_perf_test():
    tag = 'default'
    num_threads = 32
    sizes = [1_000_000_000]
    similarity_options = [0, 50, 90, 95, 100]
    repeat_count = 10
    for size in sizes:
        for similarity in similarity_options:
            command = (f'TEST_TAG={tag}'
                       f' TEST_THREADS={num_threads}'
                       f' TEST_SIMILARITY={similarity}'
                       f' TEST_DATA_SIZE={size}'
                       f' {TESTS_RELATIVE_PATH}'
                       f' "--gtest_filter=Performance.*Http"'
                       f' --gtest_repeat={repeat_count}')
            print(f'Will run command: {command}')
            result = os.system(command)
            print(f'Result for test run with size {size}: {result}')


clear_log_file(LOG_FILENAME)
run_perf_test()
