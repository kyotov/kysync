import io
import itertools
import logging
import pathlib
import time
from typing import Optional, Dict, Iterable

import boto3
import boto3_type_annotations.dynamodb as dynamodb
import boto3_type_annotations.ec2 as ec2

from analysis.tools.aws_setup import setup_nvme, setup_cmake, setup_packages
from analysis.tools.git import checkout
from analysis.tools.perf_log_parser import perf_log_parser
from analysis.tools.ssh_client import SshClient
from analysis.tools.stats import Stats
from analysis.tools.test_instance import TestInstance

# TODO: make these parameters!
_INSTANCE_TYPE = "m5d.2xlarge"
_LAUNCH_TEMPLATE_NAME = "test1"
_SSH_KEY_FILE = pathlib.Path.home() / "Downloads" / "kp2.pem"

_index = itertools.count()


class EC2Instance(object):
    def __init__(self, stats: Stats, instance_id: str = None, keep_alive: bool = False):
        ec2cli: ec2.Client = boto3.client("ec2")
        ec2res: ec2.ServiceResource = boto3.resource("ec2")

        self._stats = stats
        self._start_ns = time.time_ns()

        if instance_id is None:
            t = ec2cli.run_instances(MinCount=1, MaxCount=1,
                                     LaunchTemplate=dict(LaunchTemplateName=_LAUNCH_TEMPLATE_NAME),
                                     InstanceType=_INSTANCE_TYPE)
            instance_id = t['Instances'][0]['InstanceId']
            self._terminate = not keep_alive
        else:
            self._terminate = False

        self._logger = logging.getLogger(instance_id)
        self._instance = ec2res.Instance(instance_id)
        self._ssh: Optional[SshClient] = None

    def wait_for_ready(self):
        self._logger.info(f"waiting for {self._instance.instance_id} to become ready...")
        self._instance.wait_until_running()
        self._instance.reload()
        self._logger.info(f"{self._instance.instance_id} is ready!")

        for _ in range(10):
            try:
                self._ssh = SshClient(self._instance.public_dns_name, str(_SSH_KEY_FILE))
                break
            except:
                self._logger.info("ssh not ready... retrying!")
                time.sleep(3)

        setup_nvme(self._ssh)
        setup_cmake(self._ssh)
        setup_packages(self._ssh)

    def execute(self, ti: TestInstance, timestamp: int) -> str:
        self._logger.info(f"executing on {self._instance.instance_id}")
        checkout(self._ssh, ti.get_commitish())

        self._ssh.execute(
            "mkdir -p nvme/kysync/build &&"
            "cd nvme/kysync/build && "
            "~/cmake/bin/cmake -S .. -B . -D CMAKE_BUILD_TYPE=Release -G Ninja && "
            "~/cmake/bin/cmake --build . --config Release && "
            "(rm log/perf.log || true) && "
            + ti.get_test_command())

        return self.upload_results(timestamp)

    def terminate(self):
        self._logger.info(f"terminating {self._instance.instance_id}...")
        self._ssh.execute("echo terminate")
        self._stats.log("instance", time.time_ns() - self._start_ns)
        self._instance.terminate()

    def get_metadata(self) -> Dict[str, str]:
        return dict(
            instance_id=self._instance.instance_id,
            instance_type=self._instance.instance_type,
            instance_architecture=self._instance.architecture,
        )

    def get_file_object(self, remote_path: str) -> Iterable[str]:
        log = io.BytesIO()
        self._ssh.sftp().getfo("nvme/kysync/build/log/perf.log", log)

        log.seek(0)
        while True:
            line = log.readline().decode("utf-8")
            if line:
                yield line
            else:
                break

    def upload_results(self, timestamp: int) -> str:
        global _index

        db: dynamodb.ServiceResource = boto3.resource("dynamodb")
        table = db.Table("kysync_perf_log")

        partitions = set()
        for o in perf_log_parser(self.get_file_object("nvme/kysync/build/log/perf.log")):
            partition_key = f"{timestamp}-{o['tag'].split('-')[0]}"
            partitions.add(partition_key)

            o["id"] = f"{timestamp}-{next(_index)}-{o['tag']}"
            o["tag"] = partition_key

            o.update(self.get_metadata())
            table.put_item(Item=o)

        assert (1 == len(partitions))
        return list(partitions)[0]

    def status(self):
        if self._ssh is not None:
            return self._instance.instance_id, self._ssh.status()
        else:
            return "", ""