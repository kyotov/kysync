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


def flush_caches() -> Experiment:
    """
        commit a170cd4bb281c8fede073b32d1e85aedd437a501 (HEAD -> flush-caches, origin/flush-caches)
        Author: Kamen Yotov <kyotov@fb.com>
        Date:   Fri Jul 30 13:05:42 2021 -0400

            flush caches
    """

    tag = "flush_caches"
    data_size = 1_000_000_000
    repeat = 3
    pattern = "Performance.KySync"
    similarity = 100
    threads = 32

    return Experiment([
        TestInstance(
            tag=f"{tag}-after",
            commitish='6d7141ec5e8593186b0eb6a850f8a7a0b36f45e0',
            data_size=data_size,
            similarity=similarity,
            threads=threads,
            flush_caches=True,
            gtest_filter=pattern,
            gtest_repeat=repeat
        ),
        TestInstance(
            tag=f"{tag}-before",
            commitish='6d7141ec5e8593186b0eb6a850f8a7a0b36f45e0',
            data_size=data_size,
            similarity=similarity,
            threads=threads,
            flush_caches=False,
            gtest_filter=pattern,
            gtest_repeat=repeat
        ),
    ], analyze=_analyze)
