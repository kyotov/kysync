import math
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def bars(label: str, series, position: int, width: float = 0.25, color: str = None, alpha: float = 0.5):
    index = np.arange(len(series.x))
    plt.bar(x=index + width / 2 + width * position, height=series.y, width=width,
            yerr=[series.y - series.y_min, series.y_max - series.y], color=color, alpha=alpha, label=label)


def series(df, x, y):
    t = df[[x, y]]
    t = t.groupby(x, as_index=False).agg({y: ['mean', 'min', 'max', 'count']})
    # assert 10 == t[y]['count'].min(), t[y]['count'].min()
    # assert 10 == t[y]['count'].max(), t[y]['count'].max()

    t['x'] = t[x]
    t['y'] = t[y]['mean']
    t['y_min'] = t[y]['min']
    t['y_max'] = t[y]['max']
    return t[['x', 'y', 'y_min', 'y_max']]


def similarity_plot(df, similarity, metric, threads_specs=None):
    if threads_specs is None:
        threads_specs = [1, 2, 4, 8, 16, 32, 64]

    plt.subplots()
    m0 = series(df[(df.similarity == similarity) & (df.when == 'before')],  #
                x='data_size', y=metric)
    bars('ZSync', series=m0, position=-1, width=0.1)

    position = 0
    for threads in threads_specs:
        m1 = series(df[(df.similarity == similarity) & (df.when == 'after') & (df.threads == threads)],  #
                    x='data_size', y=metric)
        bars(f'KySync (threads={threads})', series=m1, position=position, width=0.1, color='g',
             alpha=0.1 + position / 10)
        position += 1

    gb = int(math.pow(2, 30))
    plt.xticks(range(len(m1.x)), m1.x / gb)

    plt.title(f'Similarity = {similarity}%')
    plt.xlabel('Data Size (GiB)')
    plt.ylabel('MiB/s')
    plt.legend()
    plt.tight_layout()
    plt.savefig(f'similarity_{similarity}.png')


def similarity_plot_2(df, metric):
    plt.subplots()
    m0 = series(df[(df.data_size == math.pow(2, 31)) & (df.when == 'before')],  #
                x='similarity', y=metric)
    bars('ZSync', series=m0, position=-1, width=0.1)

    position = 0
    for threads in [1, 2, 4, 8, 16, 32, 64]:
        m1 = series(df[(df.data_size == math.pow(2, 31)) & (df.when == 'after') & (df.threads == threads)],  #
                    x='similarity', y=metric)
        bars(f'KySync (threads={threads})', series=m1, position=position, width=0.1, color='g',
             alpha=0.1 + position / 10)
        position += 1

    plt.xticks(range(len(m1.x)), m1.x)

    plt.title(f'Data Size = 2 GiB')
    plt.xlabel('Similarity (%)')
    plt.ylabel('MiB/s')
    plt.legend()
    plt.tight_layout()
    plt.savefig('similarity.png')


def similarity_plot_1_1(df, metric, threads_specs=None, ksync_specs_set=None, tag="", width=0.1, legend=True,
                        zsync=True):
    if threads_specs is None:
        threads_specs = [1, 2, 4, 8, 16, 32, 64]

    ksync_specs = [
        dict(when='ksync_1_0', name='KySync 1.0', color='g'),
        dict(when='ksync_1_1_a', name='KySync 1.0 + a', color='c'),
        dict(when='ksync_1_1_b', name='KySync 1.0 + b', color='m'),
        dict(when='ksync_1_1_ab', name='KySync 1.1', color='k'),
    ]

    if ksync_specs_set is None:
        ksync_specs_set = set([x['when'] for x in ksync_specs])

    plt.subplots()

    if zsync:
        m0 = series(df[(df.data_size == math.pow(2, 31)) & (df.when == 'zsync')],  #
                    x='similarity', y=metric)
        bars('ZSync', series=m0, position=-1, width=0.1)

    position = 0
    for ksync_spec in ksync_specs:
        if ksync_spec['when'] in ksync_specs_set:
            thread_id = 1
            for threads in threads_specs:
                m1 = series(
                    df[(df.data_size == math.pow(2, 31)) & (df.when == ksync_spec['when']) & (df.threads == threads)],
                    #
                    x='similarity', y=metric)
                bars(f'{ksync_spec["name"]} (threads={threads})', series=m1, position=position, width=width,
                     color=ksync_spec['color'], alpha=thread_id / len(threads_specs) * .75)
                position += 1
                thread_id += 1

    plt.xticks(range(len(m1.x)), m1.x)

    plt.title(f'Data Size = 2 GiB')
    plt.xlabel('Similarity (%)')
    plt.ylabel('MiB/s')
    if legend:
        plt.legend()
    plt.tight_layout()
    plt.savefig(f'similarity_1_1_{tag}.png')


def graphs_1():
    df = pd.read_pickle('df.data')

    df.similarity = pd.to_numeric(df.similarity)
    df.threads = pd.to_numeric(df.threads)
    df.data_size = pd.to_numeric(df.data_size)

    df['prepare_mbps'] = df.data_size / math.pow(2, 20) / (df.prepare_ms / 1000.0)
    df['sync_mbps'] = df.data_size / math.pow(2, 20) / (df.sync_ms / 1000.0)

    metric = 'sync_mbps'

    similarity_plot(df, 0, metric)
    similarity_plot(df, 95, metric)
    similarity_plot(df, 100, metric)
    similarity_plot_2(df, metric)


def graphs_1_1():
    df = pd.read_pickle('df_1_1.data')

    df.similarity = pd.to_numeric(df.similarity)
    df.threads = pd.to_numeric(df.threads)
    df.data_size = pd.to_numeric(df.data_size)

    df['prepare_mbps'] = df.data_size / math.pow(2, 20) / (df.prepare_ms / 1000.0)
    df['sync_mbps'] = df.data_size / math.pow(2, 20) / (df.sync_ms / 1000.0)

    metric = 'sync_mbps'

    similarity_plot_1_1(df, metric, threads_specs=[1, 2], ksync_specs_set={'ksync_1_0', 'ksync_1_1_ab'}, tag='t1t2')
    similarity_plot_1_1(df, metric, threads_specs=[16],
                        ksync_specs_set={'ksync_1_0', 'ksync_1_1_a', 'ksync_1_1_b', 'ksync_1_1_ab'}, tag='t16',
                        zsync=False)
    similarity_plot_1_1(df, metric, ksync_specs_set={'ksync_1_1_ab'}, tag='tall')
    # similarity_plot_1_1(df, metric, ksync_specs_set={'ksync_1_0', 'ksync_1_1_ab'}, tag='t1t2', width=0.05, legend=False)


def main():
    # graphs_1()
    graphs_1_1()
    plt.show()


if __name__ == '__main__':
    main()
