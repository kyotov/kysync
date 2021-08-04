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


def run():
    future = run_async()
    future.result()  # will throw propagated exception


def run_async_with_termination():
    future = run_async()

    th = threading.Thread(target=shutdown_on_future_completion, args=(future,), daemon=True)
    th.start()


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

    run_with_flask = True
    
    if run_with_flask:
        run_async_with_termination()
        # use_reloader=False otherwise __main__ runs multiple times and the expirment runs multipe times
        # and also when it restarts it messes up the running test
        app.run(debug=True, use_reloader=False)
    else:
        run()
