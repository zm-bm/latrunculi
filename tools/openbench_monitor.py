"""Observe a frozen SPSA workload and distinguish completion from an early stop."""

import argparse
import datetime
import hashlib
import json
from pathlib import Path
import time

from openbench_client import cli

from . import openbench_spsa_client as spsa


def classify(snapshot, workload_id, settings):
    target = 2 * int(settings['spsa_iterations']) * int(settings['spsa_pairs_per'])
    info = snapshot['info']
    if (info['id'] != workload_id or snapshot['settings'] != settings
            or snapshot['target_games'] != target):
        raise ValueError('SPSA identity or settings changed; refusing to act')
    if (type(info['games']) is not int or info['games'] < 0
            or any(type(info[key]) is not bool for key in ['finished', 'error', 'deleted'])):
        raise ValueError('Malformed SPSA progress')
    if info['deleted']:
        return 'deleted'
    if info['error']:
        return 'correctness-stop'
    if info['finished']:
        return 'completed' if info['games'] >= target else 'premature-stop'
    return 'budget-stop' if info['games'] >= target else 'observing'


def watch(config, workload_id, settings, output, interval):
    output.mkdir(parents=True, exist_ok=False)
    cli.write_json(output / 'settings.json', settings)
    identity = dict(workload_id=workload_id,
                    settings_sha256=hashlib.sha256((output / 'settings.json').read_bytes()).hexdigest())
    while True:
        state = dict(identity, utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
        try:
            snapshot = spsa.inspect(config, workload_id)
        except (cli.OpenBenchError, OSError, cli.HTTPException) as error:
            # An unavailable server is not evidence of completion. Retain the
            # failure and retry; never stop or restart a workload on an uncertain read.
            state.update(status='inspection-error', failure=cli.failure_text(error, config))
        else:
            cli.write_json(output / 'latest.json', snapshot, replace=True)
            try:
                status = classify(snapshot, workload_id, settings)
            except (KeyError, TypeError, ValueError) as error:
                state.update(status='identity-error', failure=str(error))
            else:
                info = snapshot['info']
                state.update(status=status, games=info['games'], target_games=snapshot['target_games'])
                if status in ('correctness-stop', 'budget-stop') and not info['finished']:
                    # The stop helper records and independently verifies the action.
                    try:
                        spsa.stop(config, workload_id, output / 'stop')
                    except (cli.OpenBenchError, OSError, cli.HTTPException) as error:
                        state.update(status='stop-error', failure=cli.failure_text(error, config))
                    else:
                        if status == 'budget-stop':
                            state['status'] = 'completed'
        cli.write_json(output / 'state.json', state, replace=True)
        with (output / 'history.jsonl').open('a') as log:
            log.write(json.dumps(state) + '\n')
        print(json.dumps(state), flush=True)
        status = state['status']
        if status == 'completed':
            return 0
        if status not in ('observing', 'inspection-error'):
            return 2
        time.sleep(interval)


def main(argv, config_path):
    parser = argparse.ArgumentParser(prog='openbench monitor-spsa')
    parser.add_argument('id', type=cli.positive_int)
    parser.add_argument('--settings', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='new evidence directory')
    parser.add_argument('--interval', type=cli.positive_int, default=60, help='poll interval in seconds')
    args = parser.parse_args(argv)
    config = None
    try:
        settings = spsa.normalize(json.loads(args.settings.read_text()))
        config = cli.read_config(config_path)
        return watch(config, args.id, settings, args.output, args.interval)
    except KeyboardInterrupt:
        return 130
    except (OSError, ValueError, cli.OpenBenchError, cli.HTTPException) as error:
        message = cli.failure_text(error, config) if config else str(error)
        print('error: ' + message, flush=True)
        return 1
