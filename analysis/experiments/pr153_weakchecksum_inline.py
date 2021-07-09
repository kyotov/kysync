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


def pr153_weakchecksum_inline() -> Experiment:
    """
        commit 823387a2d67433a2ed032bb1edf2b578a3c99804 (HEAD -> refs/heads/weakchecksum_inline, refs/remotes/origin/weakchecksum_inline, refs/remotes/chaim/weakchecksum_inline)
        Author: Chaim Mintz <chaim@mintzfamily.com>
        Date:   Thu Jul 15 21:07:48 2021 -0400

            change WeakChecksum to a template function so it gets inlined

        commit e626f10f0b11aa6c8956dec18ef962c2277c79e2 (refs/remotes/origin/master, refs/remotes/chaim/master, refs/remotes/ashish/master, refs/heads/master)
        Merge: caa30f2 4ae9b12
        Author: Kamen Yotov <kamen@yotov.org>
        Date:   Tue Jul 6 21:18:19 2021 -0400

            Merge pull request #147 from ashishvthakkar/p7

            Simplify callback when reading data

    """
    tag = "pr153_weakchecksum_inline"
    data_size = 1_000_000_000
    repeat = 10
    pattern = "Performance.KySync"

    return Experiment([
        TestInstance(
            tag=f"{tag}-after",
            commitish='823387a2d67433a2ed032bb1edf2b578a3c99804',
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
