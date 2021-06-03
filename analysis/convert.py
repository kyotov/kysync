import re
from pprint import pprint

import pandas as pd
from pandas import DataFrame

records = []
# with open("../cmake-build-debug/log/perf.log", "r") as f:
# with open("../cmake-build-release/log/perf.log", "r") as f:
with open("../cmake-build-release/perf.log", "r") as f:
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

        m = re.match('^.*((Sync|GenData|Prepare|System)Command).*:(/.*)$', key)
        if m:
            key = f"{m.group(2)}{m.group(3)}"

        if key.endswith('_'):
            key = key[:-1]
        if key.startswith('profile_.'):
            key = key[len('profile_.'):]

        if key.endswith("_ms") or key.endswith("_bytes"):
            value = int(value)

        if key in record.keys():
            count = 2
            while f'{key}__{count}' in record:
                count = count + 1
            key = f'{key}__{count}'

        record[key] = value

pd.set_option("display.width", 1000)
pd.set_option("display.max_rows", 10)
pd.set_option("display.max_columns", 100)

df = DataFrame.from_records(records[1:])

columns = df.columns.values
columns.sort()
pprint(columns)
# pprint(df)

# print(tabulate(df, headers='keys', tablefmt='psql'))

#
# metrics = [
#     'gendata',
#     'prepare_1',
#     'prepare_2',
#     'sync_1',
#     'sync_2',
#     'sync_3',
#     'sync_4',
# ]
#
# for metric in metrics:
#     df[metric] = df[f"{metric}_bytes"] / math.pow(2, 20) / (df[f"{metric}_ms"] / 1000)
#
#
# def path_convert(p: str) -> str:
#     p = p.strip('"')
#     return p[0]
#
#
# df.path = df.path.apply(path_convert)
#
# print(df.columns)
# df = df[df.data_size == '10000000000']
# df = df[['path'] + metrics]
#
# data: DataFrameGroupBy = df.groupby(by="path")

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
