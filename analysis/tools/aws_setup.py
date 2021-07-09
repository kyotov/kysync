import logging

from analysis.tools.ssh_client import SshClient


def setup_nvme(ssh: SshClient):
    logger = logging.getLogger("setup_nvme")

    if 0 == ssh.execute("mount | grep nvme1n1p1", check=False):
        logger.info("nvme already mounted, skipping...")
        return

    ssh.execute("""
        set -o xtrace;
        mkdir -p ~/nvme;
        sudo umount ~/nvme;
        sudo parted /dev/nvme1n1 rm 1;
        sudo parted --script /dev/nvme1n1 mklabel msdos;
        sudo parted --script /dev/nvme1n1 mkpart primary ext4 2048s -- -1
    """)

    # NOTE: there is something that needed this in a separate `execute`...
    ssh.execute("""
        sudo mke2fs /dev/nvme1n1p1 &&
        sudo mount /dev/nvme1n1p1 ~/nvme &&
        sudo chmod a+w ~/nvme
    """)


def setup_cmake(ssh: SshClient):
    logger = logging.getLogger("setup_cmake")

    if 0 == ssh.execute("cmake/bin/cmake --version", check=False):
        logger.info("cmake is already there, skipping...")
        return 0

    ssh.execute("""
        set -o xtrace;
        rm -rf cmake;
        wget https://github.com/Kitware/CMake/releases/download/v3.20.5/cmake-3.20.5-linux-x86_64.tar.gz &&
        tar xvf cmake-3.20.5-linux-x86_64.tar.gz &&
        mv cmake-3.20.5-linux-x86_64 cmake
    """)


def setup_packages(ssh: SshClient):
    ssh.execute(
        "sudo apt-get update && "
        "sudo apt-get --assume-yes install ninja-build make g++", retry=10, retry_delay=10)
