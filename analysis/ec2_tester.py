import json
import logging
import os
import threading
import traceback

from flask import Flask

from analysis.experiments.flush_caches import flush_caches
from analysis.experiments.perf import perf
from analysis.experiments.perf_1_1 import perf_1_1
from analysis.experiments.pr152_wcs_set_size import pr152_wcs_set_size
from analysis.tools.experiment_executor import ExperimentExecutor

_NUM_INSTANCES = 10

"""
TODO:
* use disk images to start faster
* make Stats know about with
"""

app = Flask(__name__)


def shutdown_on_future_completion(future):
    try:
        future.result()  # will throw propagated exception
    except:
        traceback.print_exc()

    # https://stackoverflow.com/questions/9591350/what-is-difference-between-sys-exit0-and-os-exit0
    os._exit(0)


def run_async():
    # e = pr151_posix_sequential()
    # e = pr152_wcs_set_size()
    # e = pr153_weakchecksum_inline()
    # e = ashish_zsync_ladder()
    # e = flush_caches()
    e = perf_1_1()
    app.ee = ExperimentExecutor(e, num_instances=_NUM_INSTANCES)
    future = app.ee.run_async()
    return future


def run():
    future = run_async()
    future.result()  # will throw propagated exception


def run_async_with_termination():
    future = run_async()
    th = threading.Thread(target=shutdown_on_future_completion, args=(future,), daemon=True)
    th.start()


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

    run_with_flask = True

    if run_with_flask:
        run_async_with_termination()
        # use_reloader=False otherwise __main__ runs multiple times and the expirment runs multipe times
        # and also when it restarts it messes up the running test
        app.run(debug=True, use_reloader=False)
    else:
        run()
