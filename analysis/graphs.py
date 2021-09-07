import math
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def main():
    df = pd.read_pickle('df.data')

    df.similarity = pd.to_numeric(df.similarity)
    df.threads = pd.to_numeric(df.threads)
    df.data_size = pd.to_numeric(df.data_size)

    df['prepare_mbps'] = df.data_size / math.pow(2, 20) / (df.prepare_ms / 1000.0);
    df['sync_mbps'] = df.data_size / math.pow(2, 20) / (df.sync_ms / 1000.0);

    metric = 'sync_mbps'

    def series(df, x, y):
        t = df[[x, y]]
        t = t.groupby(x, as_index=False).agg({y: ['mean', 'min', 'max', 'count']})
        assert 10 == t[y]['count'].min(), t[y]['count'].min()
        assert 10 == t[y]['count'].max(), t[y]['count'].max()

        t['x'] = t[x]
        t['y'] = t[y]['mean']
        t['y_min'] = t[y]['min']
        t['y_max'] = t[y]['max']
        return t[['x', 'y', 'y_min', 'y_max']]

    def bars(label: str, series, position: int, width: float = 0.25, color: str = None, alpha: float = 0.5):
        index = np.arange(len(series.x))
        plt.bar(x=index + width / 2 + width * position, height=series.y, width=width,
                yerr=[series.y - series.y_min, series.y_max - series.y], color=color, alpha=alpha, label=label)

    def similarity_plot(similarity):
        plt.subplots()
        m0 = series(df[(df.similarity == similarity) & (df.when == 'before')],  #
                    x='data_size', y=metric)
        bars('ZSync', series=m0, position=-1, width=0.1)

        position = 0
        for threads in [1, 2, 4, 8, 16, 32, 64]:
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

    def similarity_plot_2():
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

    similarity_plot(0)
    similarity_plot(95)
    similarity_plot(100)
    similarity_plot_2()
    plt.show()


if __name__ == '__main__':
    main()
