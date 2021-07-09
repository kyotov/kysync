from pandas import DataFrame

from analysis.tools.experiment import Experiment
from analysis.tools.test_instance import TestInstance


def _analyze(df: DataFrame, output_filename: str):
    df['similarity'] = df['similarity'].apply(int)
    df["data_size"] = df["data_size"].apply(int)

    prepare_columns = [column for column in df.columns if column.endswith("_ms") and "/prepare/" in column]
    sync_columns = [column for column in df.columns if column.endswith("_ms") and "/sync/" in column]

    df["prepare_ms"] = sum([df[column].fillna(0) for column in prepare_columns])
    df["sync_ms"] = sum([df[column].fillna(0) for column in sync_columns])

    df = df[df["data_size"] > 10000000]
    df = df[["when", "similarity", "prepare_ms", "sync_ms"]]
    g = df.groupby(["similarity", "when"]).agg(["min", "max", "mean", "std"])

    with open(output_filename, "w") as f:
        f.write(str(g))


def ashish_zsync_ladder() -> Experiment:
    tag = "ashish_zsync_ladder"
    data_size = 1_000_000_000
    similarity_ladder = [0, 50, 90, 95, 100]
    repeat = 10

    instances = []
    for similarity in similarity_ladder:
        instances += [
            TestInstance(
                tag=f"{tag}-after",
                commitish='master',
                data_size=data_size,
                similarity=similarity,
                gtest_filter="Performance.KySync_Http",
                gtest_repeat=repeat
            ),
            TestInstance(
                tag=f"{tag}-before",
                commitish='master',
                data_size=data_size,
                similarity=similarity,
                gtest_filter="Performance.Zsync_Http",
                gtest_repeat=repeat
            )
        ]
    return Experiment(instances, analyze=_analyze)
