from pandas import DataFrame

from analysis.tools.experiment import Experiment
from analysis.tools.test_instance import TestInstance


def _analyze(df: DataFrame, output_filename: str):
    # remove the size discovery run...
    df = df[df["//prepare/phase_1_bytes"] > 10000000]

    df = df[["when", "//sync/phase_2_ms"]]
    g = df.groupby("when").agg(["min", "max", "mean", "std"])

    with open(output_filename, "w") as f:
        f.write(str(g))


def pr152_wcs_set_size() -> Experiment:
    """
        commit ab76e33ba92a52fe8a042b7becae6402d8b1fe6d (HEAD -> refs/heads/wcs_set_size, refs/remotes/origin/wcs_set_size, refs/remotes/chaim/wcs_set_size)
        Author: Chaim Mintz <chaim@mintzfamily.com>
        Date:   Thu Jul 15 20:45:00 2021 -0400

            change weak checksum set to use 26 bits instead of full 32 bits to cut down on the amount of memory it has to randomly access

        commit e626f10f0b11aa6c8956dec18ef962c2277c79e2 (refs/remotes/origin/master, refs/remotes/chaim/master, refs/remotes/ashish/master, refs/heads/master)
        Merge: caa30f2 4ae9b12
        Author: Kamen Yotov <kamen@yotov.org>
        Date:   Tue Jul 6 21:18:19 2021 -0400

            Merge pull request #147 from ashishvthakkar/p7

            Simplify callback when reading data
    """
    tag = "pr152_wcs_set_size"
    data_size = 1_000_000_000
    repeat = 10
    pattern = "Performance.KySync"

    return Experiment([
        TestInstance(
            tag=f"{tag}-after",
            commitish='ab76e33ba92a52fe8a042b7becae6402d8b1fe6d',
            data_size=data_size,
            similarity=0,
            gtest_filter=pattern,
            gtest_repeat=repeat
        ),
        TestInstance(
            tag=f"{tag}-before",
            commitish='e626f10f0b11aa6c8956dec18ef962c2277c79e2',
            data_size=data_size,
            similarity=0,
            gtest_filter=pattern,
            gtest_repeat=repeat
        ),
    ], analyze=_analyze)
