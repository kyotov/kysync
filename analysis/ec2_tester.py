import json
import logging
import os
import threading
import time
import traceback

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


def shutdown_on_future_completion(future):
    try:
        future.result()  # will throw propagated exception
    except:
        traceback.print_exc()

    os._exit(0)


@app.before_first_request
def init():
    future = run_async()

    th = threading.Thread(target=shutdown_on_future_completion, args=(future,))
    th.start()


def run():
    future = run_async()
    future.result()  # will throw propagated exception


def run_async():
    e = pr151_posix_sequential()
    # e = pr152_wcs_set_size()
    # e = pr153_weakchecksum_inline()
    # e = ashish_zsync_ladder()
    app.ee = ExperimentExecutor(e, num_instances=4)
    future = app.ee.run_async()
    return future


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

    app.run(debug=True)
    #run()
