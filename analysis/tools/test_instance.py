class TestInstance(object):
    def __init__(self,
                 tag: str,
                 commitish: str = "master",
                 notes: str = None,
                 data_size: int = 1_000_000,
                 similarity: int = 90,
                 threads: int = 32,
                 gtest_command: str = None,
                 gtest_filter: str = None,
                 gtest_repeat: int = 1):
        if gtest_command is None:
            gtest_command = "src/tests/perf_tests"
        if gtest_filter is not None:
            gtest_filter = f"--gtest_filter={gtest_filter}"
        else:
            gtest_filter = ""

        self._tag = tag
        self._commitish = commitish
        self._notes = f"-{notes}" if notes is not None else ""
        self._hash = hash
        self._data_size = data_size
        self._similarity = similarity
        self._threads = threads
        self._gtest_command = gtest_command
        self._gtest_filter = gtest_filter
        self._gtest_repeat = gtest_repeat

    def get_commitish(self):
        return self._commitish

    def get_test_tag(self):
        return f"{self._tag}-{self._commitish}{self._notes}"

    def get_test_command(self):
        return f"TEST_TAG={self.get_test_tag()} " \
               f"TEST_DATA_SIZE={self._data_size} " \
               f"TEST_SIMILARITY={self._similarity} " \
               f"TEST_THREADS={self._threads} " \
               f"{self._gtest_command} {self._gtest_filter} --gtest_repeat={self._gtest_repeat}"
