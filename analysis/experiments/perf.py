import math
from pandas import DataFrame

from analysis.tools.experiment import Experiment
from analysis.tools.test_instance import TestInstance


def _analyze(df: DataFrame, output_filename: str):
    # remove the size discovery run...
    df = df[df["//gen_data/phase_1_bytes"] > 10000000]

    prepare_columns = [column for column in df.columns if column.endswith("_ms") and "/prepare/" in column]
    sync_columns = [column for column in df.columns if column.endswith("_ms") and "/sync/" in column]

    df["prepare_ms"] = sum([df[column].fillna(0) for column in prepare_columns])
    df["sync_ms"] = sum([df[column].fillna(0) for column in sync_columns])

    d = df
    df = df[["when", "similarity", "data_size", "threads", "prepare_ms", "sync_ms"]]
    g = df.groupby("when").agg(["min", "max", "mean", "std", "count"])
    g = df.groupby(["when", "similarity", "data_size", "threads"]).agg(["min", "max", "mean", "std", "count"])

    del d['fragment_size']

    del d['tag']
    del d['instance_id']
    del d['metadata_file_path']
    del d['data_file_path']
    del d['output_file_path']
    del d['seed_data_file_path']
    del d['compressed_file_path']

    with open(output_filename, "wb") as f:
        f.write(str(g).encode('ascii'))
        f.write(b"\n\n#RAW DATA\n\n")
        d.to_csv(f)
        # d.to_pickle('df.data')


def perf() -> Experiment:
    tag = "perf"
    repeat = 10
    commitish = "a0b70531fb56e467bd94175d317830184a4cba52"

    thread_specs = [1, 2, 4, 8, 16, 32, 64]
    # thread_specs = [32, 64]

    similarities = [0, 50, 75, 90, 95, 100]
    # similarities = [0, 100]

    data_sizes = [int(math.pow(2, x)) for x in range(27, 34, 2)]  # 128m, 512m, 2g, 8g
    # data_sizes = [int(math.pow(2, 27))]

    result = []
    for data_size in data_sizes:
        for similarity in similarities:
            if data_size < math.pow(2, 33):  # skip 8gb for zsync
                result.append(TestInstance(
                    tag=f"{tag}-before-{data_size}-{similarity}",
                    commitish=commitish,
                    data_size=data_size,
                    similarity=similarity,
                    threads=1,
                    gtest_filter="Performance.Zsync_Http",
                    gtest_repeat=repeat
                ))

            for threads in thread_specs:
                result.append(TestInstance(
                    tag=f"{tag}-after-{data_size}-{similarity}-{threads}",
                    commitish=commitish,
                    data_size=data_size,
                    similarity=similarity,
                    threads=threads,
                    gtest_filter="Performance.KySync_Http",
                    gtest_repeat=repeat
                ))

    print(len(result))
    return Experiment(result, analyze=_analyze)
