import json
from typing import Dict

import boto3
import boto3_type_annotations.secretsmanager as sm


def get_secrets(secret_name: str) -> Dict[str, str]:
    client: sm.Client = boto3.client("secretsmanager")
    value = client.get_secret_value(SecretId=secret_name)
    value = value['SecretString']
    value = json.loads(value)
    return value
