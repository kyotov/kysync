import logging
import pprint
import time
from collections import defaultdict


class Stats(object):
    def __init__(self):
        self._start_ns = time.time_ns()
        self._stats = defaultdict(lambda: 0.0)

    def log(self, category: str, ns: int):
        x = ns / 60e9
        self._stats[f"{category}_sum"] += x
        self._stats[f"{category}_count"] += 1

    def to_dict(self):
        now = time.time_ns()
        self.log("wall_clock", now - self._start_ns)
        self._start_ns = now

        return self._stats

    def dump(self):
        logging.getLogger("STATS").info(f"\n{pprint.pformat(self.to_dict(), indent=2)}")
