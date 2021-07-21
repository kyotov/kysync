import utils
import sys

LOG_FILENAME = 'build/log/perf.log'
if len(sys.argv) == 1:
    df = utils.analyze(LOG_FILENAME)
    utils.plot(df)
elif len(sys.argv) == 2:
    df = utils.analyze(sys.argv[1])
    utils.compare_all(utils.filter_for_ksync(df), utils.filter_for_zsync(df))
elif len(sys.argv) == 3:
    dfs = utils.compare_files(sys.argv[1], sys.argv[2])
    print(f'prev mean: {dfs.loc[0, "p6_mean"]}')
    print(f'diff: {dfs.loc[5, "diff"]}')
