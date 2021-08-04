import io
import itertools
import logging
import pathlib
import threading
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
_SSH_KEY_FILE = pathlib.Path.home() / "kysync-test.pem"

_index = itertools.count()


class EC2Instance(object):
    boto3_lock: threading.Lock = threading.Lock()
    def __init__(self, stats: Stats, instance_id: str = None, keep_alive: bool = False):
        self._is_terminated = False

        try:
            # when multiple threads create these things at the same time, boto3 complains for some reason
            # ex: https://github.com/boto/boto3/issues/1592
            EC2Instance.boto3_lock.acquire()
            ec2cli: ec2.Client = boto3.client("ec2")
            ec2res: ec2.ServiceResource = boto3.resource("ec2")
        finally:
            EC2Instance.boto3_lock.release()

        self._stats = stats
        self._start_ns = time.time_ns()

        if instance_id is None:
            terminate_script = '''#!/bin/bash
            HEARTBEAT_FILE=/tmp/heartbeat_file
            echo "Using heartbeat file at ${HEARTBEAT_FILE}"
            touch ${HEARTBEAT_FILE}
            ls --full-time ${HEARTBEAT_FILE}
            while true
            do
                sleep 10

                date
                ls --full-time ${HEARTBEAT_FILE}

                TERMINATE_TIMEOUT=120
                CURTIME=$(date +%s)
                FILETIME=$(stat ${HEARTBEAT_FILE} -c %Y)
                TIMEDIFF=$(expr ${CURTIME} - ${FILETIME})
                if [ ${TIMEDIFF} -gt ${TERMINATE_TIMEOUT} ]; then
                    date
                    echo "Terminating instance due to stale ${HEARTBEAT_FILE}, updated last at ${FILETIME}, ${TIMEDIFF} seconds ago"
                    sleep 1  # so can tail the log for debugging
                    shutdown -h now
                fi
            done
            '''
            t = ec2cli.run_instances(MinCount=1, MaxCount=1,
                                     LaunchTemplate=dict(LaunchTemplateName=_LAUNCH_TEMPLATE_NAME),
                                     InstanceType=_INSTANCE_TYPE,
                                     InstanceInitiatedShutdownBehavior="terminate",
                                     UserData=terminate_script)
            instance_id = t['Instances'][0]['InstanceId']
            self._should_terminate = not keep_alive
        else:
            self._should_terminate = False

        self._logger = logging.getLogger(instance_id)
        self._instance = ec2res.Instance(instance_id)
        self._ssh: Optional[SshClient] = None

    def wait_for_ready(self):
        self._logger.info(f"waiting for {self._instance.instance_id} to become ready...")
        self._instance.wait_until_running()
        self._instance.reload()
        self._logger.info(f"{self._instance.instance_id} / {self._instance.public_dns_name} is ready!")

        for _ in range(10):
            try:
                self._logger.info(f"connecting to {self._instance.public_dns_name} using ssh key file {str(_SSH_KEY_FILE)}")
                self._ssh = SshClient(self._instance.public_dns_name, str(_SSH_KEY_FILE))
                break
            except:
                self._logger.info(f"ssh not ready for host {self._instance.public_dns_name} ssh key file {str(_SSH_KEY_FILE)}... retrying!")
                time.sleep(3)

        if not self._ssh:
            str_err = f"error, instance {self._instance.instance_id} / {self._instance.public_dns_name} took too long to be available for ssh."
            self._logger.info(str_err)
            raise Exception(str_err)

        if self._should_terminate:
            self._spawn_heartbeat_thread()
        setup_nvme(self._ssh)
        setup_cmake(self._ssh)
        setup_packages(self._ssh)

    def _spawn_heartbeat_thread(self):
        th = threading.Thread(target=self._run_heartbeat_thread, daemon=True)
        th.start()

    def _run_heartbeat_thread(self):
        heartbeat_ssh = SshClient(self._instance.public_dns_name, str(_SSH_KEY_FILE))
        while not self._is_terminated:
            heartbeat_ssh.execute("touch /tmp/heartbeat_file")
            time.sleep(30)

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
        self._is_terminated = True
        self._logger.info(f"terminating {self._instance.instance_id}...")
        if self._ssh:
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
            tag = o['tag']
            partition_key = f"{timestamp}-{tag.split('-')[0]}"
            partitions.add(partition_key)

            o["id"] = f"{timestamp}-{next(_index)}-{tag}"
            o["tag"] = partition_key
            o["tag_full"] = tag

            o.update(self.get_metadata())
            table.put_item(Item=o)

        assert (1 == len(partitions))
        return list(partitions)[0]

    def status(self):
        if self._ssh is not None:
            return self._instance.instance_id, self._ssh.status()
        else:
            return "", ""
