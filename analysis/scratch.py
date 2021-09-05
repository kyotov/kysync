import logging
from pprint import pprint

from analysis.experiments.flush_caches import flush_caches
from analysis.tools.aws_ec2_instance import EC2Instance
from analysis.tools.stats import Stats


def main():
    logging.basicConfig(level=logging.INFO)
    logger = logging.getLogger("main")

    instance_id = 'i-0cbfef42d5edc987f'

    s = Stats()
    i = EC2Instance(s, instance_id=instance_id, keep_alive=True)
    i.wait_for_ready()
    pprint(i.get_metadata())

    e = flush_caches()
    for ti in e.get_test_instances():
        i.execute(ti, 0)


if __name__ == '__main__':
    main()
