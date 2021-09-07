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


def pr151_posix_sequential() -> Experiment:
    """
        commit 4e0e1afc80b1841876244a8eaaa366bbd3c0aa62 (HEAD -> refs/heads/posix_c_read, refs/remotes/origin/posix_c_read, refs/remotes/chaim/posix_c_read)
        Author: Chaim Mintz <chaim@mintzfamily.com>
        Date:   Mon Jul 12 10:07:46 2021 -0400

            add PosixSequentialFileReader that uses raw C/posix and use fdadvise(SEQUENTIAL) to make sequential file reads faster

        commit e626f10f0b11aa6c8956dec18ef962c2277c79e2 (refs/remotes/origin/master, refs/remotes/chaim/master, refs/remotes/ashish/master, refs/heads/master)
        Merge: caa30f2 4ae9b12
        Author: Kamen Yotov <kamen@yotov.org>
        Date:   Tue Jul 6 21:18:19 2021 -0400

            Merge pull request #147 from ashishvthakkar/p7

            Simplify callback when reading data
    """

    tag = "pr151_posix_sequential"
    data_size = 1_000_000_000
    repeat = 10
    pattern = "Performance.KySync"
    similarity = 100
    threads = 32

    return Experiment([
        TestInstance(
            tag=f"{tag}-after",
            commitish='4e0e1afc80b1841876244a8eaaa366bbd3c0aa62',
            data_size=data_size,
            similarity=similarity,
            threads=threads,
            gtest_filter=pattern,
            gtest_repeat=repeat
        ),
        TestInstance(
            tag=f"{tag}-before",
            commitish='e626f10f0b11aa6c8956dec18ef962c2277c79e2',
            data_size=data_size,
            similarity=similarity,
            threads=threads,
            gtest_filter=pattern,
            gtest_repeat=repeat
        ),
    ], analyze=_analyze)
