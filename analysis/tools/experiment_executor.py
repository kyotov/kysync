import logging
import queue
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from typing import Dict

import time

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

    def __add__(self, other):
        result = TestInstanceStatus()
        result.total = self.total + other.total
        result.queued = self.queued + other.queued
        result.running = self.running + other.running
        result.done = self.done + other.done
        return result


class ExperimentExecutor(object):
    def __init__(self, experiment: Experiment, num_instances: int = 1):
        self._logger = logging.getLogger()
        self._stats = Stats()
        self._timestamp = time.time_ns()
        self._tags = set()
        self._experiment = experiment
        self._terminated = False

        self._test_instances: Dict[TestInstance, TestInstanceStatus] = defaultdict(lambda: TestInstanceStatus())
        self._instances = []
        self._free_instances = queue.SimpleQueue()
        self._num_instances = num_instances

        self._pool = ThreadPoolExecutor(max_workers=5000)

    def _create_instance(self):
        start_ns = time.time_ns()
        instance = EC2Instance(self._stats)
        self._instances.append(instance)
        instance.wait_for_ready()
        self._free_instances.put(instance)
        self._stats.log("bootstrap", time.time_ns() - start_ns)

    def _execute_test_instance(self, ti: TestInstance):
        self._test_instances[ti].total += 1
        self._test_instances[ti].queued += 1

        start_ns = time.time_ns()
        while True:
            try:
                instance = self._free_instances.get(timeout=1)
                break
            except queue.Empty:
                if self._terminated:
                    return
        self._stats.log("waiting", time.time_ns() - start_ns)

        self._logger.info(f"{ti.get_test_tag()} execution starting...")

        start_ns = time.time_ns()
        try:
            self._test_instances[ti].queued -= 1
            self._test_instances[ti].running += 1
            tag = instance.execute(ti, self._timestamp)
            self._test_instances[ti].running -= 1
            self._test_instances[ti].done += 1
            self._tags.add(tag)
            self._logger.info(f"{ti} execution completed...")
        except Exception as e:
            self._logger.error(e)
        finally:
            self._free_instances.put(instance)
            self._stats.log("execution", time.time_ns() - start_ns)

    def _execute_experiment(self, experiment: Experiment):
        self._logger.info(f"starting experiment with {self._num_instances} machines")

        try:
            fs = []
            for _ in range(self._num_instances):
                fs.append(self._pool.submit(self._create_instance))

            for ti in experiment.get_test_instances():
                count = ti._gtest_repeat
                ti._gtest_repeat = 1
                for _ in range(count):
                    fs.append(self._pool.submit(self._execute_test_instance, ti))

            self._logger.info("all tests scheduled, waiting for completion...")
            while fs:
                done_futures = [x for x in fs if x.done()]
                fs = [x for x in fs if not x in done_futures]
                [x.result() for x in done_futures]  # will throw any exception propagated from future
                time.sleep(1)
            self._logger.info("all tests completed")

            self._stats.dump()

            assert 1 == len(self._tags), self._tags
            tag = list(self._tags)[0]
            filename = experiment.analyze(tag)
            self._logger.info(f"done for tag={tag}, results in {filename}")
        finally:
            for instance in self._instances:
                instance.terminate()
            self._terminated = True

    def run_async(self):
        return self._pool.submit(self._execute_experiment, self._experiment)

    def status(self):
        tests = []
        status_total = TestInstanceStatus()
        for (ti, tis) in self._test_instances.items():
            status_total += tis
            tests.append((ti.get_test_tag(), tis.status()))

        return dict(
            stats=self._stats.to_dict(),
            instances=dict([instance.status() for instance in self._instances]),
            tests_status_total=status_total.status(),
            tests=[(ti.get_test_tag(), tis.status()) for (ti, tis) in self._test_instances.items()]
        )
