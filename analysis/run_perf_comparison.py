from perf_comparator import LadderOverrideConfig, RunConfig, PerfComparator
import logging


def run_comparison():
    data_size = 250_000_000
    data_similarity = 95
    num_threads = 32
    block_size = 1024

    # 7/6 master tag
    a = RunConfig('head_20210720',
                  'e626f10f0b11aa6c8956dec18ef962c2277c79e2',
                  data_size, data_similarity, num_threads, block_size)

    # 7/1 baseline tag
    b = RunConfig('baseline_20210701',
                  '0f0c33c448c9f4443e4fdde1dabe35774e8c649a',
                  data_size, data_similarity, num_threads, block_size)

    with PerfComparator(2, a, b) as pc:
        dfs = pc.run_perf_comparison()
        logger = logging.getLogger()
        for config in [a, b]:
            logger.info(f'Mean for {config.instance_tag}: '
                        f'{dfs.loc[0, config.instance_tag + "_mean"]}')
        logger.info(f'Diff (a-b): {dfs.loc[0, "diff"]} at '
                    f'pvalue {dfs.loc[0, "pvalue"]}')
        pc.show_info()


def run_ladder():
    data_sizes = [250_000_000, 1_000_000_000]
    data_similarities = [100, 95]
    num_threads = 32
    block_size = 1024

    # 7/6 master tag
    a = RunConfig('a',
                  'e626f10f0b11aa6c8956dec18ef962c2277c79e2',
                  data_sizes[0], data_similarities[0], num_threads, block_size)

    # 7/1 baseline tag
    b = RunConfig('b',
                  '0f0c33c448c9f4443e4fdde1dabe35774e8c649a',
                  data_sizes[0], data_similarities[0], num_threads, block_size)

    ladder_config = LadderOverrideConfig(data_sizes, data_similarities)
    with PerfComparator(2, a, b, ladder_config) as pc:
        pc.run_perf_comparison()
        pc.show_info()


# run_comparison()
run_ladder()
