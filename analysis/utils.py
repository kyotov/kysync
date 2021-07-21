import pandas as pd
from pandas import DataFrame
import matplotlib.pyplot as plt
from scipy.stats import ttest_ind

PHASES = range(1, 5)


def get_records(filename):
    records = []
    with open(filename, "r") as f:
        record = {}
        while True:
            line = f.readline()
            if not line:
                break
            line = line.strip('\r\n\t ')

            if line == "":
                if record != {}:
                    records.append(record)
                record = {}
                continue

            key, value = line.split("=")

            if key.endswith("_ms") or key.endswith("_bytes"):
                value = float(value)

            if key in record.keys():
                count = 2
                while f'{key}__{count}' in record:
                    count = count + 1
                key = f'{key}__{count}'

            record[key] = value

    if record != {}:
        records.append(record)

    return records


def get_records_df(records):
    pd.set_option("display.width", 1000)
    pd.set_option("display.max_rows", 10)
    pd.set_option("display.max_columns", 100)
    df = DataFrame.from_records(records)
    return df


def is_zsync_record(row):
    return (int(row['profile_.zsync']) == 1)


def get_time_for_phase(row, phase):
    if is_zsync_record(row):
        return 0
    else:
        return int(row[f'//sync/phase_{phase}_ms'])


def get_sync_time(row):
    if is_zsync_record(row):
        return int(row['//sync/phase_1_ms'])
    else:
        return int(row['//sync/phase_1_ms']) +  \
            int(row['//sync/phase_2_ms']) + \
            int(row['//sync/phase_3_ms']) + \
            int(row['//sync/phase_4_ms'])


def get_augmented_df(df):
    df['bytes'] = df.apply(
        lambda row: row['profile_.data_size'],
        axis=1
    )
    df['time (ms)'] = df.apply(get_sync_time, axis=1)
    for phase in PHASES:
        df[f'time for phase {phase} (ms)'] = df.apply(
            lambda row: get_time_for_phase(row, phase), axis=1)
    return df


def filter_columns(df):
    columns = ['profile_.http',
               'profile_.zsync',
               'profile_.identity_reconstruction',
               'bytes',
               'time (ms)',
               'profile_.similarity',
               'profile_.tag']
    for phase in PHASES:
        columns.append(f'time for phase {phase} (ms)')
    df = df[columns]
    df = df.rename(columns={'profile_.http': 'http',
                            'profile_.zsync': 'zsync',
                            'profile_.identity_reconstruction': 'init',
                            'profile_.similarity': 'similarity',
                            'profile_.tag': 'tag'})
    return df


def analyze(filename):
    records = get_records(filename)
    df = get_records_df(records)
    df = get_augmented_df(df)
    df = filter_columns(df)
    df = df.apply(pd.to_numeric, errors='ignore')
    return df


def plot(df):
    plt.close('all')
    df = df[df['init'] != 1]
    df['throughput'] = df['bytes'] / df['time (ms)'] * 1_000
    dfk = df[df['zsync'] == 0]
    dfz = df[df['zsync'] != 0]
    dfm = dfk.merge(dfz, on='bytes', how='outer',
                    suffixes=('_ksync', '_zsync'))
    # dfm = dfm[['bytes', throughput_ksync', 'throughput_zsync']]
    # dfm = dfm[['bytes', 'throughput_ksync', 'throughput_zsync']]
    dfm = dfm[['bytes', 'time (ms)_ksync', 'time (ms)_zsync']]
    dfm = dfm.groupby('bytes').mean()
    print(dfm)
    dfm.plot()


def filter_for_ksync(df):
    df = df[(df['init'] != 1) & (df['zsync'] == 0)]
    return df


def filter_for_zsync(df):
    df = df[(df['init'] != 1) & (df['zsync'] == 1)]
    return df


def compare(df1, df2, summary_row, tag1, tag2):
    pvalue_threshold = 0.05
    summary_row[f'{tag1}_mean'] = df1.mean()
    summary_row[f'{tag2}_mean'] = df2.mean()
    summary_row['diff'] = df1.mean() - df2.mean()
    summary_row['% change'] = round(
        summary_row['diff'] / summary_row[f'{tag2}_mean'], 4) * 100
    (statistic, pvalue) = ttest_ind(df1,
                                    df2,
                                    equal_var=False,
                                    alternative='less')
    summary_row['pvalue'] = pvalue
    summary_row['is_first_better'] = pvalue < pvalue_threshold


def add_phase_breakdown(df, summary_row, tag):
    for phase in PHASES:
        column_name = f'time for phase {phase} (ms)'
        summary_row[f'{tag}_p{phase}'] = df[column_name].mean()


def filter_for_comparison(df, similarity, size):
    return df[(df['similarity'] == similarity) & (df['bytes'] == size)]


def get_tag_prefix(df1, df2):
    tags1 = df1['tag'].unique()
    assert len(tags1) == 1
    tags2 = df2['tag'].unique()
    assert len(tags2) == 1
    if (tags1[0] == tags2[0]):
        return ('ksync', 'zsync')
    else:
        return (tags1[0], tags2[0])


def compare_all(df1, df2):
    similarities = df1['similarity'].unique()
    sizes = df1['bytes'].unique()
    (tag1, tag2) = get_tag_prefix(df1, df2)
    summary = []
    for size in sizes:
        for similarity in similarities:
            summary_row = {'size': size, 'similarity': similarity}
            df1c = filter_for_comparison(df1, similarity, size)
            df2c = filter_for_comparison(df2, similarity, size)
            compare(df1c['time (ms)'], df2c['time (ms)'],
                    summary_row, tag1, tag2)
            add_phase_breakdown(df1c, summary_row, tag1)
            add_phase_breakdown(df2c, summary_row, tag2)
            summary.append(summary_row)
    dfs = pd.DataFrame.from_dict(summary)
    print('Summary table: ')
    print(dfs)
    print(f'Average change: {dfs["% change"].mean():,.0f}%')
    print()
    is_first_better = len(dfs[~dfs['is_first_better']]) == 0
    if is_first_better:
        print('Success! First is statistically significantly better'
              ' across all similarities.')
    else:
        print('Try again... First is NOT statistically significantly better'
              ' across all similarities.')
    return dfs


def compare_files(file1, file2):
    df1 = analyze(file1)
    df2 = analyze(file2)
    return compare_all(filter_for_ksync(df1), filter_for_ksync(df2))
