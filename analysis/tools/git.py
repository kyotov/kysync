from analysis.tools.aws_secrets import get_secrets
from analysis.tools.ssh_client import SshClient

_SECRET_NAME = "kysync_test"

_GITHUB_USERNAME = "github_username"
_GITHUB_TOKEN = "github_token"
_GITHUB_REPO = "github_repo"


def checkout(ssh: SshClient, commitish: str):
    if 0 == ssh.execute("ls nvme/kysync/.git", check=False):
        ssh.execute(f"cd nvme/kysync && git fetch --recurse-submodules=yes")
    else:
        secrets = get_secrets(_SECRET_NAME)
        u = secrets[_GITHUB_USERNAME]
        t = secrets[_GITHUB_TOKEN]
        r = secrets[_GITHUB_REPO]

        ssh.execute(f"git clone --recurse-submodules https://{u}:{t}@github.com/{r} ~/nvme/kysync")

    ssh.execute(f"cd nvme/kysync && git checkout {commitish} --recurse-submodules")
