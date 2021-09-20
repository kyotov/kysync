import logging
from pprint import pprint

import math
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from analysis.experiments.perf_1_1 import perf_1_1
from analysis.experiments.perf import perf
from analysis.tools.aws_ec2_instance import EC2Instance
from analysis.tools.stats import Stats


def main():
    logging.basicConfig(level=logging.INFO)
    logger = logging.getLogger("main")

    instance_id = 'i-07063e5096305c692'
    # instance_id = None

    # s = Stats()
    # i = EC2Instance(s, instance_id=instance_id, keep_alive=True)
    # i.wait_for_ready()
    # pprint(i.get_metadata())

    # e = flush_caches()
    e = perf_1_1()
    # tis = e.get_test_instances()
    # for ti in tis[0:1]:
    #     i.execute(ti, 0)

    filename = e.analyze('1632106358857227100-perf_1_1')


if __name__ == '__main__':
    main()
