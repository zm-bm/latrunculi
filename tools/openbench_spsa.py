"""Offline SPSA preparation: engine metadata plus an explicit campaign recipe."""
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess

from . import engine
from . import openbench_spsa_client as client

RECIPES = Path(__file__).parent / 'resources/openbench/recipes'


def parameter_rows(metadata, recipe):
    if not isinstance(metadata, dict) or metadata.get('format') != 'latrunculi.search-parameters.v1' or not isinstance(metadata.get('parameters'), list):
        raise ValueError('Unsupported engine parameter metadata')
    definitions = {}
    for item in metadata['parameters']:
        if (not isinstance(item, dict) or set(item) != {'name', 'type', 'default', 'min', 'max'}
                or not isinstance(item['name'], str)
                or item['type'] not in ('int', 'float') or not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*', item['name'])
                or item['name'].lower() in definitions
                or any(type(item[key]) not in (int, float) or not math.isfinite(item[key]) for key in ['default', 'min', 'max'])
                or (item['type'] == 'int' and any(type(item[key]) is not int for key in ['default', 'min', 'max']))
                or not item['min'] <= item['default'] <= item['max']):
            raise ValueError('Malformed engine parameter metadata')
        definitions[item['name'].lower()] = item
    if not isinstance(recipe, dict) or recipe.get('format') != 'latrunculi.spsa-recipe.v1' or not isinstance(recipe.get('parameters'), list) or not isinstance(recipe.get('schedule'), dict):
        raise ValueError('Unsupported SPSA recipe')
    rows = []
    for item in recipe['parameters']:
        if (not isinstance(item, dict) or set(item) != {'name', 'min', 'max', 'c_end', 'r_end'}
                or not isinstance(item['name'], str)
                or any(type(item[key]) not in (int, float) or not math.isfinite(item[key]) for key in ['min', 'max'])):
            raise ValueError('Malformed recipe parameter')
        definition = definitions.get(item['name'].lower())
        if definition is None:
            raise ValueError('Recipe names an unknown parameter: '+item['name'])
        if definition['type'] == 'int' and any(type(item[key]) is not int for key in ['min', 'max']):
            raise ValueError('Integer recipe bounds must be integers')
        if not definition['min'] <= item['min'] <= definition['default'] <= item['max'] <= definition['max']:
            raise ValueError('Recipe bounds disagree with engine metadata: '+item['name'])
        rows.append((definition['name'], definition['type'], definition['default'],
                     item['min'], item['max'], item['c_end'], item['r_end']))
    return client.parameters(client.parameter_text(rows))


def prepare(args, revision, write_settings):
    output = args.output.absolute()
    if output.exists() or output.is_symlink():
        raise ValueError('Output already exists; choose a new directory')
    recipe_path = args.recipe
    if not recipe_path.is_file():
        raise ValueError('SPSA recipe does not exist')
    recipe_bytes = recipe_path.read_bytes()
    recipe = json.loads(recipe_bytes)
    binary = engine.executable(args.candidate_engine)
    identity = hashlib.sha256(binary.read_bytes()).hexdigest()
    exported = subprocess.run([str(binary), 'spsa'], capture_output=True, text=True,
                              check=True, timeout=args.timeout)
    metadata = json.loads(exported.stdout)
    rows = parameter_rows(metadata, recipe)
    discovery = subprocess.run([str(binary)], input='uci\nquit\n', capture_output=True,
                               text=True, check=True, timeout=args.timeout)
    for item in metadata['parameters']:
        line = (f"option name {item['name']} type spin default {item['default']} min {item['min']} max {item['max']}"
                if item['type'] == 'int' else f"option name {item['name']} type string default {item['default']}")
        if discovery.stdout.splitlines().count(line) != 1:
            raise ValueError('Engine metadata disagrees with UCI options: '+item['name'])
    settings = json.loads((RECIPES.parent / 'spsa.json').read_text())
    settings.update(recipe['schedule'])
    settings.update(dev_engine=args.engine, dev_branch=revision(args.candidate),
                    dev_bench=1, info=args.description, spsa_inputs=client.parameter_text(rows),
                    spsa_iterations=1)
    settings = client.normalize(settings)
    games_per_iteration = 2 * int(settings['spsa_pairs_per'])
    if args.games % games_per_iteration:
        raise ValueError(f'Game budget must be divisible by {games_per_iteration}')
    settings['spsa_iterations'] = args.games // games_per_iteration
    output.mkdir(parents=True)
    (output/'metadata.json').write_text(exported.stdout)
    (output/'uci.txt').write_text(discovery.stdout)
    (output/'recipe.json').write_bytes(recipe_bytes)
    settings['dev_bench'] = engine.bench_nodes(binary, output/'candidate-bench.txt', timeout=args.timeout)
    if hashlib.sha256(binary.read_bytes()).hexdigest() != identity:
        raise ValueError('Candidate binary changed during preparation')
    write_settings(output/'candidate.json', {'revision': settings['dev_branch'], 'binary': str(binary),
        'sha256': identity, 'recipe_sha256': hashlib.sha256(recipe_bytes).hexdigest(),
        'worker_build_path': 'bench/tuning'})
    path = output/'settings.json'
    write_settings(path, client.normalize(settings))
    print(f'Settings: {path}', flush=True)
    return path
