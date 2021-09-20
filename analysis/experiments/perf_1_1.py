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
        d.to_pickle('df_1_1.data')


def perf_1_1() -> Experiment:
    tag = "perf_1_1"
    repeat = 5

    thread_specs = [1, 2, 4, 8, 16, 32, 64]

    similarities = [0, 50, 75, 90, 95, 100]

    data_size = int(math.pow(2, 31))

    result = []
    for similarity in similarities:
        result.append(TestInstance(
            tag=f"{tag}-zsync-{data_size}-{similarity}",
            commitish="b3e63b3c02b81af7603092863a54ddcfc2a71d8c",
            data_size=data_size,
            similarity=similarity,
            threads=1,
            gtest_filter="Performance.Zsync_Http",
            gtest_repeat=repeat
        ))

        for threads in thread_specs:
            result.append(TestInstance(
                tag=f"{tag}-ksync_1_0-{data_size}-{similarity}-{threads}",
                commitish="b3e63b3c02b81af7603092863a54ddcfc2a71d8c",
                data_size=data_size,
                similarity=similarity,
                threads=threads,
                gtest_filter="Performance.KySync_Http",
                gtest_repeat=repeat
            ))

            result.append(TestInstance(
                tag=f"{tag}-ksync_1_1_a-{data_size}-{similarity}-{threads}",
                commitish="bd127d9bee1156a846c2609c3e9dac293f0decf9",
                data_size=data_size,
                similarity=similarity,
                threads=threads,
                gtest_filter="Performance.KySync_Http",
                gtest_repeat=repeat
            ))

            result.append(TestInstance(
                tag=f"{tag}-ksync_1_1_b-{data_size}-{similarity}-{threads}",
                commitish="95ad5a4a1cd8f3f30735ab7998b755fa6056dc4b",
                data_size=data_size,
                similarity=similarity,
                threads=threads,
                gtest_filter="Performance.KySync_Http",
                gtest_repeat=repeat
            ))

            result.append(TestInstance(
                tag=f"{tag}-ksync_1_1_ab-{data_size}-{similarity}-{threads}",
                commitish="d931b24295c3ac897d8385a53a1bb00c54ab73d5",
                data_size=data_size,
                similarity=similarity,
                threads=threads,
                gtest_filter="Performance.KySync_Http",
                gtest_repeat=repeat
            ))

    print(len(result))
    return Experiment(result, analyze=_analyze)
