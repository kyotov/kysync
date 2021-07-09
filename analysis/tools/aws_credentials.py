import logging
import textwrap

from analysis.tools.aws_secrets import get_secrets
from analysis.tools.ssh_client import SshClient

_AWS_KEY_ID = "aws_key_id"
_AWS_KEY_SECRET = "aws_key_secret"


def setup_credentials(ssh: SshClient, secret_name: str):
    logger = logging.getLogger("setup_credentials")

    if 0 == ssh.execute("ls .aws/credentials", check=False):
        logger.info("credentials already setup, skipping...")
        return

    logger.info("setting up credentials")

    secrets = get_secrets(secret_name)

    credentials = textwrap.dedent(f"""\
        [default]
        aws_access_key_id = {secrets[_AWS_KEY_ID]}
        aws_secret_access_key = {secrets[_AWS_KEY_SECRET]}""")

    sftp = ssh.sftp()
    if ".aws" not in sftp.listdir("."):
        sftp.mkdir(".aws")
    sftp.putfo(io.StringIO(credentials), ".aws/credentials")
