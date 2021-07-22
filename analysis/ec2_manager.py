import logging
import textwrap
import time
import os

import boto3
import paramiko
from boto3_type_annotations.ec2 import Client, ServiceResource


class Shell(object):
    def __init__(self, host, ssh_key_file):
        self.logger = logging.getLogger()
        self._ssh = paramiko.SSHClient()
        self._ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        key = paramiko.RSAKey.from_private_key_file(str(ssh_key_file))
        self._ssh.connect(hostname=host, username='ubuntu', pkey=key)

    def execute(self, command: str, verbose: bool = True, check: bool = True,
                retry: int = 1, retry_delay: int = 3) -> int:
        exit_status = -1
        for attempt in range(retry):
            session = self._ssh.get_transport().open_session()
            self.logger.info(
                f'(try {attempt + 1} of {retry}) executing `{command}`...')
            session.exec_command(command)
            if verbose:
                while not session.exit_status_ready():
                    result = session.recv(1024).decode('ascii').strip()
                    if result != '':
                        self.logger.info(
                            f'\n{textwrap.indent(result, "  stdout >> > ")}')

                    result = session.recv_stderr(1024).decode('ascii').strip()
                    if result != '':
                        self.logger.info(
                            f'\n{textwrap.indent(result, "  stderr >> > ")}')
            exit_status = session.recv_exit_status()
            if exit_status == 0:
                break
            else:
                self.logger.info(f'waiting {retry_delay} seconds...')
                time.sleep(retry_delay)
        if check and exit_status != 0:
            raise RuntimeError(f'Command {command}\nfailed with exit_status: '
                               f'{exit_status}')
        return exit_status


class Ec2Instance:
    def __init__(self, ssh_key_file, template,
                 github_username, github_password):
        self._ssh_key_file = ssh_key_file
        self._template = template
        self._github_username = github_username
        self._github_password = github_password
        logging.basicConfig(level=logging.INFO)
        self._logger = logging.getLogger()
        self._ec2: ServiceResource = boto3.resource('ec2')
        self._client: Client = boto3.client('ec2')
        self._perf_log_filename = '~/nvme/kysync/build/log/perf.log'
        self._instance_id = ''
        self._public_ip = ''
        self._shell = None

    def __enter__(self):
        return self

    def __exit__(self, exception_type, exception_value, traceback):
        self.terminate_created_instance()
        self._instance_id = ''
        return False

    def create_instance(self):
        result = self._client.run_instances(LaunchTemplate=dict(
            LaunchTemplateId=self._template), MinCount=1, MaxCount=1)
        assert len(result['Instances']) == 1
        self._instance_id = result['Instances'][0]['InstanceId']
        ready = False
        for i in range(60):
            try:
                self._public_ip = self.get_ip_for(self._instance_id)
                self._shell = Shell(self._public_ip, self._ssh_key_file)
                exit_code = self._shell.execute('true')
                if exit_code == 0:
                    ready = True
                    self._logger.info('Instance ready!')
                    break
            except Exception:
                time.sleep(1)
        if not ready:
            raise RuntimeError('Could not create and initialize instance')
        self.bootstrap(self._public_ip)
        print('Instance created and initialized')

    def terminate_created_instance(self):
        if self._instance_id != '':
            self._logger.info(f'terminating {self._instance_id}...')
            self._client.terminate_instances(InstanceIds=[self._instance_id])

    def get_ip_for(self, instance_id):
        instances = self._ec2.instances.filter(Filters=[dict(
            Name='instance-id', Values=[instance_id])])
        # Ensure we got exactly one instance.
        # len(instances) does not work. len(list(instances.all())) may work. We
        # currently have the below to be on the safer side.
        count = 0
        for instance in instances:
            count += 1
        assert count == 1
        for instance in instances:
            self._logger.info(f'found {instance.id} of type '
                              f'{instance.instance_type} at '
                              f'{instance.public_ip_address}!')
            return instance.public_ip_address

    def bootstrap(self, host: str):
        self._shell.execute('mkdir -p ~/nvme', check=False)
        self._shell.execute('sudo umount ~/nvme', check=False)
        self._shell.execute('sudo parted /dev/nvme1n1 rm 1', check=False)
        self._shell.execute('sudo parted /dev/nvme1n1 mklabel msdos',
                            check=False)
        self._shell.execute(
            'sudo parted /dev/nvme1n1 mkpart primary ext4 2048s -- -1')
        self._shell.execute('sudo mke2fs /dev/nvme1n1p1')
        self._shell.execute('sudo mount /dev/nvme1n1p1 ~/nvme')
        self._shell.execute('sudo chmod a+w ~/nvme')
        self._shell.execute('touch ~/nvme/test')
        self._shell.execute('ls -la ~/nvme')
        self._shell.execute('rm -rf cmake', check=False)
        self._shell.execute(
            'wget https://github.com/Kitware/CMake/releases/'
            'download/v3.20.5/cmake-3.20.5-linux-x86_64.tar.gz')
        self._shell.execute('tar xvf cmake-3.20.5-linux-x86_64.tar.gz')
        self._shell.execute('mv cmake-3.20.5-linux-x86_64 cmake')
        self._shell.execute('sudo apt-get update')
        self._shell.execute(
            'sudo apt-get --assume-yes install ninja-build make g++')
        self._shell.execute(
            'git clone '
            f'https://{self._github_username}:{self._github_password}@'
            'github.com/kyotov/ksync.git '
            '~/nvme/kysync')
        self._shell.execute(
            'cd ~/nvme/kysync && git submodule update --init --recursive')
        # TODO: Consider removing this step
        self._shell.execute(
            'cmake/bin/cmake -S ~/nvme/kysync -B ~/nvme/kysync/build '
            '-D CMAKE_BUILD_TYPE=Release -G Ninja')
        self._shell.execute(
            'cmake/bin/cmake --build ~/nvme/kysync/build --config Release')

    def build(self, git_tag):
        self._shell.execute(f'cd ~/nvme/kysync && git checkout {git_tag}')
        self._shell.execute(
            'cmake/bin/cmake -S ~/nvme/kysync -B ~/nvme/kysync/build '
            '-D CMAKE_BUILD_TYPE=Release -G Ninja')
        self._shell.execute(
            'cmake/bin/cmake --build ~/nvme/kysync/build --config Release')

    def run_perf_test(self, config, num_trials):
        self._shell.execute('cd ~/nvme/kysync')
        command = (f'TEST_TAG={config.instance_tag}'
                   f' TEST_THREADS={config.num_threads}'
                   f' TEST_SIMILARITY={config.data_similarity}'
                   f' TEST_DATA_SIZE={config.data_size}'
                   f' nvme/kysync/build/src/tests/perf_tests'
                   f' "--gtest_filter=Performance.*Http"'
                   f' --gtest_repeat={num_trials}')
        self._logger.info(f'Running {command}')
        self._shell.execute(command)

    def clear_perf_log(self):
        self._shell.execute(f'rm -f {self._perf_log_filename}', check=False)

    def download_perf_log(self, destination_file):
        command = (f'scp -i {self._ssh_key_file}'
                   f' -o StrictHostKeyChecking=no'
                   f' -o UserKnownHostsFile=/dev/null'
                   f' ubuntu@{self._public_ip}:{self._perf_log_filename}'
                   f' {destination_file}')
        result = os.system(command)
        self._logger.info(f'Got command return code: {result}')
        if result != 0:
            raise RuntimeError(f'Error executing: {command}. '
                               f'Got result: {result}')
