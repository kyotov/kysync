import os
from typing import List, Tuple, Callable

import boto3
import boto3_type_annotations.dynamodb as ddb
import pandas as pd
from boto3.dynamodb.conditions import Key
from pandas import DataFrame

from analysis.tools.test_instance import TestInstance


def _when(value):
    if "after" in value:
        return "after"
    if "before" in value:
        return "before"
    return ""


class Experiment(object):
    def __init__(self, test_instances: List[TestInstance], analyze: Callable[[DataFrame, str], None]):
        self._test_instances = tuple(test_instances)
        self._analyze = analyze

    def get_test_instances(self) -> Tuple[TestInstance]:
        return self._test_instances

    def analyze(self, tag: str) -> str:
        pd.set_option("display.width", 1000)
        pd.set_option("display.max_rows", 10)
        pd.set_option("display.max_columns", 100)

        db: ddb.ServiceResource = boto3.resource("dynamodb")
        table = db.Table("kysync_perf_log")

        response = table.query(KeyConditionExpression=Key("tag").eq(tag))
        df = DataFrame.from_records(response["Items"])

        df["when"] = df["id"].apply(lambda x: _when(x))

        # FIXME: try this: `df = df.apply(pd.to_numeric, errors='ignore')`
        for column in df.columns:
            if "_ms" in column or "_bytes" in column:
                df[column] = df[column].apply(float)

        d = os.path.dirname(os.path.realpath(__file__))
        filename = f"{d}/../results/{tag}.txt"

        self._analyze(df, filename)
        return filename
