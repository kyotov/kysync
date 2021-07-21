from utils import compare_files
from ec2_manager import Ec2Instance
import pathlib
import tempfile
import logging

SSH_KEY_FILE = pathlib.Path.home() / "Downloads" / "kp1.pem"
TEMPLATE = "lt-017255c3b589b1f63"
# GITHUB_USERNAME = "kamen%40yotov.org"
GITHUB_USERNAME = "ashishvthakkar%40gmail.com"
GITHUB_PASSWORD = "..."


class PerfComparator:
    def __init__(self, num_trials, config_a, config_b):
        self._num_trials = num_trials
        self._a = config_a
        self._b = config_b
        pass

    def __enter__(self):
        return self

    def __exit__(self, exception_type, exception_value, traceback):
        return False

    def show_info(self):
        print('Comparing Run Config A: ')
        self._a.print_info()
        print('with Run Config:')
        self._b.print_info()

    def run_perf_comparison(self):
        with Ec2Instance(SSH_KEY_FILE, TEMPLATE,
                         GITHUB_USERNAME, GITHUB_PASSWORD) as ec2inst:
            ec2inst.create_instance()
            log_files = []
            with tempfile.TemporaryDirectory() as tmp_dir:
                for config in [self._a, self._b]:
                    destination_file = (f'{tmp_dir}/'
                                        f'perf_log_{config.instance_tag}.log')
                    ec2inst.build(ec2inst, config.git_tag)
                    ec2inst.run_perf_tests(
                        ec2inst, config, self._num_trials, destination_file)
                    log_files.append(destination_file)
                assert len(log_files) == 2
                return compare_files(log_files[0], log_files[1])


class RunConfig:
    def __init__(self, instance_tag, git_tag, data_size, data_similarity,
                 num_threads, block_size):
        self.instance_tag = instance_tag
        self.git_tag = git_tag
        self.data_size = data_size
        self.data_similarity = data_similarity
        self.num_threads = num_threads
        self.block_size = block_size

    def print_info(self):
        print(vars(self))


data_size = 1_000_000_000
data_similarity = 95
num_threads = 32
block_size = 1024

# 7/6 master tag
a = RunConfig('head_20210720', 'e626f10f0b11aa6c8956dec18ef962c2277c79e2',
              data_size, data_similarity, num_threads, block_size)

# 7/1 baseline tag
b = RunConfig('baseline_20210701', '0f0c33c448c9f4443e4fdde1dabe35774e8c649a',
              data_size, data_similarity, num_threads, block_size)

with PerfComparator(2, a, b) as pc:
    pc.show_info()
    dfs = pc.run_perf_comparison()
    logger = logging.getLogger()
    for config in [a, b]:
        logger.info(f'Mean for {config.instance_tag}: '
                    f'{dfs.loc[0, config.instance_tag + "_mean"]}')
    logger.info(f'Diff (a-b): {dfs.loc[0, "diff"]} at '
                f'pvalue {dfs.loc[0, "pvalue"]}')
