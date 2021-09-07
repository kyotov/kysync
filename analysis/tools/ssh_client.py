import io
import logging
import textwrap
import time
from typing import List

import paramiko


class SshClient(object):
    def __init__(self, host: str, key_file: str):
        self._ssh = paramiko.SSHClient()
        self._ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        key = paramiko.RSAKey.from_private_key_file(key_file)
        self._ssh.connect(hostname=host, username="ubuntu", pkey=key)
        self._logger = logging.getLogger("SSH")
        self._status = []

    def sftp(self) -> paramiko.SFTPClient:
        return self._ssh.open_sftp()

    def execute(self, command: str, check: bool = True, retry: int = 1, retry_delay: int = 3) -> int:
        exit_status = -1

        self._status = [time.asctime(), command]
        self._logger.info(f"executing `{command}`...")

        for attempt in range(retry):
            if attempt > 0:  # don't log the first attempt
                self._logger.info(f"try {attempt + 1} of {retry}")

            session = self._ssh.get_transport().open_session()
            session.exec_command(command)

            stdout = io.BytesIO()
            stderr = io.BytesIO()

            last_round = False
            while not (session.exit_status_ready() and last_round):
                if session.exit_status_ready():
                    last_round = True

                if session.recv_ready() or last_round:
                    data = session.recv(1_000_000)
                    self._logger.debug(f"{len(data)} bytes received on stdout")
                    stdout.write(data)

                if session.recv_stderr_ready() or last_round:
                    data = session.recv_stderr(1_000_000)
                    self._logger.debug(f"{len(data)} bytes received on stderr")
                    stderr.write(data)

                time.sleep(1)

            exit_status = session.recv_exit_status()
            if 0 == exit_status:
                break

            stdout.seek(0)
            log = stdout.read()
            if log:
                self._logger.error(textwrap.indent(log.decode("utf-8"), ' stdout >>> '));

            stderr.seek(0)
            log = stderr.read()
            if log:
                self._logger.error(textwrap.indent(log.decode("utf-8"), ' stderr >>> '));

            if attempt < retry - 1:
                self._logger.info(f"waiting {retry_delay} seconds...")
            time.sleep(retry_delay)

        if check:
            assert 0 == exit_status, f"exit_status: {exit_status}"

        self._status.append(f"exit_status = {exit_status}")
        return exit_status

    def status(self):
        return self._status
