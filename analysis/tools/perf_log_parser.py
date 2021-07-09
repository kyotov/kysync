import re
from typing import Dict, Iterable


def perf_log_parser(lines: Iterable[str]) -> Iterable[Dict]:
    record = {}
    for line in lines:
        if not line:
            break
        line = line.strip('\r\n\t ')

        if line == "":
            if record != {}:
                yield record
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

    if record != {}:
        yield record
