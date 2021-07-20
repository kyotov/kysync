import logging
import textwrap
import time
import os

import boto3
import paramiko
from boto3_type_annotations.ec2 import Client, ServiceResource


class Ec2Manager:
    def __init__(self, ssh_key_file, template,
                 github_username, github_password):
        self.ssh_key_file = ssh_key_file
        self.template = template
        self.github_username = github_username
        self.github_password = github_password
        logging.basicConfig(level=logging.INFO)
        self.logger = logging.getLogger()
        self.ec2: ServiceResource = boto3.resource("ec2")
        self.client: Client = boto3.client("ec2")
        # TODO: Consolidate
        self.instances = []
        self.instance_ids = []

    def __enter__(self):
        return self

    def __exit__(self, exception_type, exception_value, traceback):
        self.terminate_created_instances()

    def terminate_created_instances(self):
        for id in self.instance_ids:
            self.logger.info(f"terminating {id}...")
            self.client.terminate_instances(InstanceIds=[id])

    def create_instance(self) -> str:
        result = self.client.run_instances(LaunchTemplate=dict(
            LaunchTemplateId=self.template), MinCount=1, MaxCount=1)
        self.instances.append(result)
        self.instance_ids.append(result['Instances'][0]['InstanceId'])
        ready = False
        for i in range(60):
            try:
                shell = Shell(self.get_ip())
                exit_code = shell.execute("true")
                if exit_code == 0:
                    ready = True
                    self.logger.info("instance ready!")
                    break
            except Exception:
                time.sleep(1)
        assert ready
        self.list_running()
        self.bootstrap(result['Instances'][0]['public_ip_address'])
        print('Instance created and initialized')
        return result['Instances'][0]['InstanceId']

    def get_ip(self):
        for i in self.ec2.instances.filter(Filters=[dict(Name="instance-state-name", Values=["running"])]):
            self.logger.info(
                f"found {i.id} of type {i.instance_type} at {i.public_ip_address}!")
            return i.public_ip_address

    def list_running(self):
        # Filters: https://docs.aws.amazon.com/AWSEC2/latest/APIReference/API_DescribeInstances.html
        for i in self.ec2.instances.filter(Filters=[dict(Name="instance-state-name", Values=["running"])]):
            self.logger.info(
                f"instance {(i.id, i.instance_type, i.public_ip_address)}")

    def bootstrap(host: str):
        shell = Shell(host, s)
        shell.execute("mkdir -p ~/nvme", check=False)
        shell.execute("sudo umount ~/nvme", check=False)
        shell.execute("sudo parted /dev/nvme1n1 rm 1", check=False)
        shell.execute("sudo parted /dev/nvme1n1 mklabel msdos", check=False)
        shell.execute(
            "sudo parted /dev/nvme1n1 mkpart primary ext4 2048s -- -1")
        shell.execute("sudo mke2fs /dev/nvme1n1p1")
        shell.execute("sudo mount /dev/nvme1n1p1 ~/nvme")
        shell.execute("sudo chmod a+w ~/nvme")
        shell.execute("touch ~/nvme/test")
        shell.execute("ls -la ~/nvme")
        shell.execute("rm -rf cmake", check=False)
        shell.execute(
            "wget https://github.com/Kitware/CMake/releases/download/v3.20.5/cmake-3.20.5-linux-x86_64.tar.gz")
        shell.execute("tar xvf cmake-3.20.5-linux-x86_64.tar.gz")
        shell.execute("mv cmake-3.20.5-linux-x86_64 cmake")
        shell.execute(
            "sudo apt-get --assume-yes install ninja-build make g++", retry=10, retry_delay=10)
        shell.execute(
            f"git clone https://{GITHUB_USERNAME}:{GITHUB_PASSWORD}@github.com/kyotov/ksync.git ~/nvme/kysync")
        shell.execute(
            f"cd ~/nvme/kysync && git submodule update --init --recursive")
        # TODO: Consider removing this step
        shell.execute(
            f"cmake/bin/cmake -S ~/nvme/kysync -B ~/nvme/kysync/build -D CMAKE_BUILD_TYPE=Release -G Ninja")
        shell.execute(
            f"cmake/bin/cmake --build ~/nvme/kysync/build --config Release")

    def build(self, instance, git_tag):
        # TODO: Change to shell for instance
        shell = Shell(self.get_ip())
        shell.execute(f"git checkout {git_tag}")
        shell.execute(
            f"cmake/bin/cmake -S ~/nvme/kysync -B ~/nvme/kysync/build -D CMAKE_BUILD_TYPE=Release -G Ninja")
        shell.execute(
            f"cmake/bin/cmake --build ~/nvme/kysync/build --config Release")

    def run_perf_tests(self, instance, config, num_trials, destination_file):
        # TODO: Change to shell for instance
        shell = Shell(self.get_ip())
        log_filename = ~/nvme/kysync/build/log/perf.log
        result = os.system('rm ' + log_filename)
        print(f'Result when removing {log_filename}: {result}')
        command = (f'TEST_TAG={config.instance_tag}'
                    f' TEST_THREADS={config.num_threads}'
                    f' TEST_SIMILARITY={config.similarity}'
                    f' TEST_DATA_SIZE={config.size}'
                    f' nvme/kysync/build/src/tests/perf_tests'
                    f' "--gtest_filter=Performance.*Http"'
                    f' --gtest_repeat={num_trials}')
        self.logger.log(f'Running {command}')
        shell.execute(command)
        command = f'scp -i {self.ssh_key_file} ubuntu@{self.get_ip()}:{log_filename}  {destination_file}'
        result = os.system(command)
        print(f'Got result: {result}')


class Shell(object):
    def __init__(self, host, ssh_key_file):
        self.logger = logging.getLogger()
        self._ssh = paramiko.SSHClient()
        self._ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        key = paramiko.RSAKey.from_private_key_file(str(ssh_key_file))
        self._ssh.connect(hostname=host, username="ubuntu", pkey=key)

    def execute(self, command: str, verbose: bool = True, check: bool = True, retry: int = 1,
                retry_delay: int = 3) -> int:
        exit_status = -1
        for attempt in range(retry):
            session = self._ssh.get_transport().open_session()
            self.logger.info(
                f"(try {attempt + 1} of {retry}) executing `{command}`...")
            session.exec_command(command)
            if verbose:
                while not session.exit_status_ready():
                    result = session.recv(1024).decode('ascii').strip()
                    if result != "":
                        self.logger.info(
                            f"\n{textwrap.indent(result, '  stdout >>> ')}")

                    result = session.recv_stderr(1024).decode('ascii').strip()
                    if result != "":
                        self.logger.info(
                            f"\n{textwrap.indent(result, '  stderr >>> ')}")

            exit_status = session.recv_exit_status()

            if 0 != exit_status:
                self.logger.info(f"waiting {retry_delay} seconds...")
                time.sleep(retry_delay)
                continue

        if check:
            assert 0 == exit_status, f"exit_status: {exit_status}"

        return exit_status
