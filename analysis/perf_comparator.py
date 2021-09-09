from utils import compare_files
from ec2_manager import Ec2Instance
import pathlib
import tempfile

SSH_KEY_FILE = pathlib.Path.home() / "Downloads" / "kp1.pem"
# TEMPLATE = "lt-017255c3b589b1f63"  # m5d
TEMPLATE = "lt-092a2b537fed74e54"  # m5d2x
# GITHUB_USERNAME = "kamen%40yotov.org"
GITHUB_USERNAME = "ashishvthakkar%40gmail.com"
GITHUB_PASSWORD = "..."


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


class LadderOverrideConfig:
    def __init__(self, sizes, similarities):
        self.data_sizes = sizes
        self.data_similarities = similarities


class PerfComparator:
    def __init__(self, num_trials, config_a: RunConfig, config_b: RunConfig,
                 ladder_override_config: LadderOverrideConfig = None):
        self._num_trials = num_trials
        self._a = config_a
        self._b = config_b
        self._ladder_override_config = ladder_override_config
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
                    ec2inst.build(config.git_tag)
                    ec2inst.clear_perf_log()
                    if self._ladder_override_config is not None:
                        self.run_ladder_tests(ec2inst, config)
                    else:
                        ec2inst.run_perf_test(config, self._num_trials)
                    ec2inst.download_perf_log(destination_file)
                    log_files.append(destination_file)
                assert len(log_files) == 2
                return compare_files(log_files[0], log_files[1])

    def run_ladder_tests(self, ec2inst: Ec2Instance, config: RunConfig):
        for size in self._ladder_override_config.data_sizes:
            for similarity in self._ladder_override_config.data_similarities:
                config.data_size = size
                config.data_similarity = similarity
                ec2inst.run_perf_test(config, self._num_trials)
