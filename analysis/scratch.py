import logging
from pprint import pprint

from analysis.experiments.flush_caches import flush_caches
from analysis.tools.aws_ec2_instance import EC2Instance
from analysis.tools.stats import Stats


def main():
    logging.basicConfig(level=logging.INFO)
    logger = logging.getLogger("main")

    s = Stats()
    # i = EC2Instance(s, keep_alive=True)
    i = EC2Instance(s, instance_id='i-0cbfef42d5edc987f', keep_alive=True)
    pprint(i.get_metadata())
    i.wait_for_ready()

    e = flush_caches()
    eis = e.get_test_instances()

    i.execute(eis[0], 0)


if __name__ == '__main__':
    main()
