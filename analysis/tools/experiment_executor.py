import logging
import queue
import time
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, wait, ALL_COMPLETED
from typing import Dict

from analysis.tools.aws_ec2_instance import EC2Instance
from analysis.tools.experiment import Experiment
from analysis.tools.stats import Stats
from analysis.tools.test_instance import TestInstance


class TestInstanceStatus(object):
    def __init__(self):
        self.total = 0
        self.queued = 0
        self.running = 0
        self.done = 0

    def status(self):
        return dict(
            total=self.total,
            queued=self.queued,
            running=self.running,
            done=self.done
        )


class ExperimentExecutor(object):
    def __init__(self, experiment: Experiment, num_instances: int = 1):
        self._logger = logging.getLogger()
        self._stats = Stats()
        self._timestamp = time.time_ns()
        self._tags = set()

        self._test_instances: Dict[TestInstance, TestInstanceStatus] = defaultdict(lambda: TestInstanceStatus())
        self._instances = []
        self._free_instances = queue.SimpleQueue()

        self._pool = ThreadPoolExecutor(max_workers=100)
        self._pool.submit(self.execute_experiment, experiment)
        for _ in range(num_instances):
            self._pool.submit(self.create_instance)

    def create_instance(self):
        start_ns = time.time_ns()
        instance = EC2Instance(self._stats)
        self._instances.append(instance)
        instance.wait_for_ready()
        self._free_instances.put(instance)
        self._stats.log("bootstrap", time.time_ns() - start_ns)

    def execute_test_instance(self, ti: TestInstance):
        self._test_instances[ti].total += 1
        self._test_instances[ti].queued += 1

        start_ns = time.time_ns()
        instance = self._free_instances.get()
        self._stats.log("waiting", time.time_ns() - start_ns)

        start_ns = time.time_ns()
        try:
            self._test_instances[ti].queued -= 1
            self._test_instances[ti].running += 1
            tag = instance.execute(ti, self._timestamp)
            self._test_instances[ti].running -= 1
            self._test_instances[ti].done += 1
            self._tags.add(tag)
        except Exception as e:
            self._logger.error(e)
        finally:
            self._free_instances.put(instance)
            self._stats.log("execution", time.time_ns() - start_ns)

    def execute_experiment(self, experiment: Experiment):
        try:
            fs = []
            for ti in experiment.get_test_instances():
                count = ti._gtest_repeat
                ti._gtest_repeat = 1
                for _ in range(count):
                    fs.append(self._pool.submit(self.execute_test_instance, ti))

            self._logger.info("all tests scheduled, waiting for completion...")
            wait(fs, return_when=ALL_COMPLETED)
            self._logger.info("all tests completed")
        finally:
            for instance in self._instances:
                instance.terminate()

        self._stats.dump()

        assert 1 == len(self._tags)
        tag = list(self._tags)[0]
        filename = experiment.analyze(tag)
        self._logger.info(f"done for tag={tag}, results in {filename}")

        exit(0)

    def status(self):
        return dict(
            stats=self._stats.to_dict(),
            instances=dict([instance.status() for instance in self._instances]),
            tests=dict([(ti.get_test_tag(), tis.status()) for (ti, tis) in self._test_instances.items()])
        )
