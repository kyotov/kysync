import json
import logging
import time

from flask import Flask

from analysis.tools.experiment_executor import ExperimentExecutor
from experiments.pr151_posix_sequential import pr151_posix_sequential

"""
TODO:
* flask
* exception handling
* use disk images to start faster
* make Stats know about with
"""

app = Flask(__name__)


@app.before_first_request
def init():
    e = pr151_posix_sequential()
    # e = pr152_wcs_set_size()
    # e = pr153_weakchecksum_inline()
    # e = ashish_zsync_ladder()
    app.ee = ExperimentExecutor(e, num_instances=4)


@app.route("/")
def hello_world():
    return f"""
        <html>
            <head>
                <meta http-equiv="refresh" content="1">
            </head>
            <body>
                <pre>{json.dumps(app.ee.status(), indent=2)}</pre>
            </body>
        </html>
    """


if __name__ == "__main__":
    # https://docs.python.org/3/library/logging.html#logging.LogRecord
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(process)d:%(thread)d %(filename)s:%(funcName)s:%(lineno)d\n\t%(levelname)s: %(message)s")
    logger = logging.getLogger()

    # app.run(debug=True)
    # init()

    instance_id = "i-0ea15a6900efe464f"
    timestamp = time.time_ns()

    logger.info(f"timestamp: {timestamp}")

    from analysis.tools.aws_ec2_instance import EC2Instance, Stats
    from analysis.experiments.flush_caches import flush_caches

    # s = Stats()
    # ec2 = EC2Instance(s, instance_id=instance_id, keep_alive=True)
    # ec2.wait_for_ready()
    #
    # for ti in flush_caches().get_test_instances():
    #     ec2.execute(ti, timestamp)

    flush_caches().analyze("1627666422772095000-flush_caches")