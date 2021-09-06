import logging

import math
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from analysis.experiments.perf import perf


def main():
    logging.basicConfig(level=logging.INFO)
    logger = logging.getLogger("main")

    instance_id = 'i-0cbfef42d5edc987f'
    instance_id = 'i-0e3b36f1dd127dc1a'

    # s = Stats()
    # i = EC2Instance(s, instance_id=instance_id, keep_alive=True)
    # i.wait_for_ready()
    # pprint(i.get_metadata())

    # e = flush_caches()
    e = perf()
    # for ti in e.get_test_instances():
    #     i.execute(ti, 0)

    filename = e.analyze('1630847942442527700-perf')


if __name__ == '__main__':
    main()
