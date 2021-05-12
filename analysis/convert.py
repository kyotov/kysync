import math

# import plotly.express as px
import matplotlib.axes
import matplotlib.pyplot as plt
import pandas as pd
from pandas import DataFrame
from pandas.core.groupby import DataFrameGroupBy

map = {'context.block_size': 'block_size',
       'context.compression': 'compression',
       'context.data_size': 'data_size',
       'context.fragment_size': 'fragment_size',
       'context.identity_reconstruction': 'identity_reconstruction',
       'context.path': 'path',
       'context.seed_data_size': 'seed_data_size',
       'context.similarity': 'similarity',
       'context.threads': 'threads',
       'gen_data://phases/1/bytes': 'gendata_bytes',
       'gen_data://phases/1/bytes//phases/1/ms': 'gendata_ms',
       'prepare://phases/1/bytes': 'prepare_1_bytes',
       'prepare://phases/1/bytes//phases/1/ms': 'prepare_1_ms',
       'prepare://phases/2/bytes': 'prepare_2_bytes',
       'prepare://phases/2/bytes//phases/2/ms': 'prepare_2_ms',
       'sync://phases/1/bytes': 'sync_1_bytes',
       'sync://phases/1/bytes//phases/1/ms': 'sync_1_ms',
       'sync://phases/2/bytes': 'sync_2_bytes',
       'sync://phases/2/bytes//phases/2/ms': 'sync_2_ms',
       'sync://phases/3/bytes': 'sync_3_bytes',
       'sync://phases/3/bytes//phases/3/ms': 'sync_3_ms',
       'sync://phases/4/bytes': 'sync_4_bytes',
       'sync://phases/4/bytes//phases/4/ms': 'sync_4_ms'}

records = []
with open("perf.log", "r") as f:
    record = None
    while True:
        line = f.readline()
        if not line:
            break
        line = line.strip()

        if line == "":
            records.append(record)
            record = {}
            continue

        key, value = line.split("=")
        key = map[key]
        if key.endswith("_ms") or key.endswith("_bytes"):
            value = int(value)
        record[key] = value

# pprint(records)
# pd.set_option("display.width", 1000)
# pd.set_option("display.max_rows", 10)
# pd.set_option("display.max_columns", 100)

df = DataFrame.from_records(records[1:])

metrics = [
    'gendata',
    'prepare_1',
    'prepare_2',
    'sync_1',
    'sync_2',
    'sync_3',
    'sync_4',
]

for metric in metrics:
    df[metric] = df[f"{metric}_bytes"] / math.pow(2, 20) / (df[f"{metric}_ms"] / 1000)


def path_convert(p: str) -> str:
    p = p.strip('"')
    return p[0]


df.path = df.path.apply(path_convert)

print(df.columns)
df = df[df.data_size == '10000000000']
df = df[['path'] + metrics]

data: DataFrameGroupBy = df.groupby(by="path")

# print(data.groups.keys())
# print(data.mean().at['C', 'prepare_1'])
# data.plot()

# fig: plt.Figure
# ax: matplotlib.axes.Axes
# fig, ax = plt.subplots()
#
# ax.bar()
#
# plt.show()

# fig = px.bar(df.groupby(by="path").mean())
# fig.update_layout(barmode="group")
# fig.show()
# df = df[df.data_size == 10000000000]
