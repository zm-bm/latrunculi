#!/usr/bin/env python3

import argparse
import collections
import copy
import fnmatch
import hashlib
import json
import math
import pathlib
import platform
import re
import subprocess
import sys
from array import array
from dataclasses import dataclass

import chess
import numpy as np
import scipy
from scipy import optimize, sparse, special

import dataset


PHASES = ("mg", "eg")
PHASE_MATERIAL_FEATURES = (
    "material.knight",
    "material.bishop",
    "material.rook",
    "material.queen",
)
PHASE_START_COUNTS = np.asarray((4, 4, 4, 2), dtype=np.int64)
FIT_SPLITS = ("train", "selection")
RUN_SPLITS = (*FIT_SPLITS, "validation")
STATE_FORMAT_VERSION = 1
RESULTS_PATH = pathlib.Path(__file__).with_name("results.jsonl")

PROTOCOL = {
    "dataset": {
        "schema_version": 1,
        "feature_count": 482,
        "seed": 20260902,
        "minimum_game_ply": 8,
        "maximum_positions_per_game": 6,
        "minimum_games": 40000,
        "minimum_groups": 20000,
        "splits": {"train": 80, "selection": 5, "validation": 5, "heldout": 10},
    },
    "objective": {
        "perspective": "white",
        "result_mapping": "-1=0,0=0.5,1=1",
        "sigmoid": "1/(1+10^(-k*eval/400))",
        "loss": "mean_squared_error",
        "weighting": "opening_group",
    },
    "calibration": {
        "bounds": [0.0, 4.0],
        "absolute_tolerance": 1e-12,
        "maximum_iterations": 256,
    },
    "support": {"minimum_groups": 128},
    "constraints": {
        "fixed": ["material.pawn.mg"],
        "anchors": [
            "psqt.pawn.c2",
            "psqt.knight.c3",
            "psqt.bishop.d2",
            "psqt.rook.h1",
            "psqt.queen.e1",
            "psqt.king.g1",
            "mobility.knight.6",
            "mobility.bishop.7",
            "mobility.rook.11",
            "mobility.queen.11",
        ],
        "mirror_files": ["knight", "bishop", "rook", "queen", "king"],
    },
    "fit": {
        "delta_bounds": [-100, 100],
        "bounds": [
            {"pattern": "material.pawn.eg", "minimum": 50, "maximum": 300},
            {"pattern": "material.knight.*", "minimum": 301, "maximum": 900},
            {"pattern": "material.bishop.*", "minimum": 301, "maximum": 900},
            {"pattern": "material.rook.*", "minimum": 901, "maximum": 1500},
            {"pattern": "material.queen.*", "minimum": 1501, "maximum": 3000},
        ],
        "regularization": [1e-9, 1e-8, 1e-7],
    },
    "optimizer": {
        "method": "L-BFGS-B",
        "maximum_iterations": 1000,
        "gradient_tolerance": 1e-10,
        "function_tolerance": 1e-12,
        "maximum_line_search_steps": 50,
    },
    "validation": {
        "bootstrap_samples": 2000,
        "confidence": 0.9,
        "seed": 20260903,
        "phase_buckets": [0, 32, 64, 96, 129],
        "minimum_phase_groups": 128,
    },
    "strength": {"normalized_elo_bounds": [0, 5]},
}


@dataclass
class Split:
    coefficients: sparse.csr_matrix
    fixed: np.ndarray
    phase_counts: np.ndarray
    pawn_counts: np.ndarray
    turns: np.ndarray
    targets: np.ndarray
    exported: np.ndarray
    groups: np.ndarray
    weights: np.ndarray


@dataclass
class TuningData:
    manifest_sha256: str
    schema: dict
    splits: dict


@dataclass
class ParameterMap:
    parent: np.ndarray
    members: list
    names: list
    bounds: list
    support: dict
    frozen: dict
    multiplicity: np.ndarray

    def expand(self, deltas):
        weights = self.parent.copy()
        for delta, coordinates in zip(deltas, self.members):
            for feature_id, phase in coordinates:
                weights[feature_id, phase] += delta
        return weights

    def gradient(self, full_gradient):
        return np.asarray(
            [
                sum(full_gradient[feature_id, phase] for feature_id, phase in coordinates)
                for coordinates in self.members
            ],
            dtype=np.float64,
        )

    def rounded(self, deltas):
        rounded_deltas = []
        for value, (lower, upper) in zip(deltas, self.bounds):
            rounded = round_away_from_zero(value)
            rounded_deltas.append(min(max(rounded, math.ceil(lower)), math.floor(upper)))
        return self.expand(rounded_deltas).astype(np.int64)


def sha256_file(path):
    return dataset.sha256_file(path)


def canonical_json(value):
    return dataset.canonical_json(value)


def resolve_experiment(config):
    if not isinstance(config, dict) or set(config) != {
        "version",
        "name",
        "baseline",
        "corpus",
    }:
        raise ValueError("invalid experiment configuration")
    if type(config["version"]) is not int or config["version"] != 1:
        raise ValueError("unsupported experiment version")
    if not isinstance(config["name"], str) or not re.fullmatch(
        r"[a-z0-9][a-z0-9-]*", config["name"]
    ):
        raise ValueError("experiment name must use lowercase letters, digits, and hyphens")

    baseline = config["baseline"]
    if not isinstance(baseline, dict) or set(baseline) != {"revision", "benchmark"}:
        raise ValueError("invalid baseline configuration")
    if not isinstance(baseline["revision"], str) or not re.fullmatch(
        r"[0-9a-f]{40}", baseline["revision"]
    ):
        raise ValueError("baseline revision must be a full commit hash")
    if type(baseline["benchmark"]) is not int or baseline["benchmark"] < 1:
        raise ValueError("baseline benchmark must be positive")

    corpus = config["corpus"]
    corpus_keys = {
        "description",
        "openbench_tests",
        "book",
        "time_control",
        "engine_options",
        "adjudication",
    }
    if not isinstance(corpus, dict) or set(corpus) != corpus_keys:
        raise ValueError("invalid corpus description")
    text_keys = corpus_keys - {"openbench_tests", "engine_options"}
    if any(not isinstance(corpus[key], str) or not corpus[key] for key in text_keys):
        raise ValueError("corpus text fields must be nonempty strings")
    if not isinstance(corpus["openbench_tests"], list) or not isinstance(
        corpus["engine_options"], dict
    ):
        raise ValueError("invalid corpus tests or engine options")

    resolved = copy.deepcopy(PROTOCOL)
    resolved.update(copy.deepcopy(config))
    return resolved


def load_experiment(path):
    return resolve_experiment(json.loads(path.read_text()))


def artifact_id(artifact):
    payload = {key: value for key, value in artifact.items() if key != "artifact_id"}
    return hashlib.sha256(canonical_json(payload).encode()).hexdigest()


def load_json(path):
    return json.loads(path.read_text())


def atomic_write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    partial = path.with_name(path.name + ".partial")
    partial.write_text(text)
    partial.replace(path)


def atomic_write_json(path, value):
    atomic_write(path, json.dumps(value, indent=2, sort_keys=True) + "\n")


def write_artifact(path, artifact):
    if path.exists():
        raise ValueError(f"output already exists: {path}")
    artifact["artifact_id"] = artifact_id(artifact)
    atomic_write_json(path, artifact)


def read_artifact(path):
    artifact = load_json(path)
    if artifact.get("artifact_id") != artifact_id(artifact):
        raise ValueError(f"artifact hash mismatch: {path}")
    return artifact


def read_engine_schema(engine):
    result = subprocess.run(
        [str(engine), "features"], input="", capture_output=True, text=True, check=False
    )
    if result.returncode:
        raise ValueError(result.stderr.strip() or "feature exporter failed")
    lines = result.stdout.splitlines()
    if len(lines) != 1:
        raise ValueError("feature exporter did not emit one schema record")
    return json.loads(lines[0])


def read_benchmark(engine):
    result = subprocess.run([str(engine), "bench"], capture_output=True, text=True, check=False)
    if result.returncode:
        raise ValueError(result.stderr.strip() or "engine benchmark failed")
    match = re.fullmatch(r"(\d+) nodes \d+ ms \d+ nps\n?", result.stdout)
    if not match:
        raise ValueError("engine benchmark output is malformed")
    return int(match.group(1))


def validate_schema(schema, expected_version, expected_count):
    if schema.get("type") != "schema" or schema.get("version") != expected_version:
        raise ValueError("dataset schema version mismatch")
    features = schema.get("features", [])
    ids = [feature.get("id") for feature in features]
    names = [feature.get("name") for feature in features]
    if len(features) != expected_count or ids != list(range(expected_count)):
        raise ValueError("invalid feature IDs")
    if len(names) != len(set(names)) or any(not name for name in names):
        raise ValueError("invalid feature names")


def group_weights(groups):
    counts = collections.Counter(groups)
    if not counts:
        return np.asarray([], dtype=np.float64)
    return np.asarray([1.0 / counts[group] for group in groups])


def load_split(path, name, schema):
    indices = array("i")
    values = array("i")
    indptr = array("q", [0])
    fixed = []
    phase_counts = []
    pawn_counts = []
    turns = []
    targets = []
    exported = []
    groups = []

    with path.open(encoding="utf-8") as stream:
        split_schema = json.loads(next(stream))
        if split_schema != schema:
            raise ValueError(f"{name} schema mismatch")

        for line in stream:
            record = json.loads(line)
            if record.get("type") != "position" or record.get("version") != schema["version"]:
                raise ValueError(f"{name}: invalid position record")

            for feature_id, coefficient in record["coefficients"]:
                indices.append(feature_id)
                values.append(coefficient)
            indptr.append(len(values))

            fixed.append(record["fixed"])
            phase_counts.append(record["phase_counts"])
            pawn_counts.append(record["pawn_counts"])
            turns.append(1 if record["turn"] == "w" else -1)
            targets.append((record["result"] + 1) / 2)
            exported.append(record["eval"])
            groups.append(dataset.group_from_source(record["source"]))

    _, group_array = np.unique(np.asarray(groups, dtype=object), return_inverse=True)
    group_array = group_array.astype(np.int32)
    coefficients = sparse.csr_matrix(
        (
            np.frombuffer(values, dtype=np.int32),
            np.frombuffer(indices, dtype=np.int32),
            np.frombuffer(indptr, dtype=np.int64),
        ),
        shape=(len(fixed), len(schema["features"])),
        dtype=np.int32,
    )
    coefficients.eliminate_zeros()
    split = Split(
        coefficients=coefficients,
        fixed=np.asarray(fixed, dtype=np.int32).reshape((-1, 2)),
        phase_counts=np.asarray(phase_counts, dtype=np.int16).reshape((-1, 4)),
        pawn_counts=np.asarray(pawn_counts, dtype=np.int8).reshape((-1, 2)),
        turns=np.asarray(turns, dtype=np.int8),
        targets=np.asarray(targets, dtype=np.float64),
        exported=np.asarray(exported, dtype=np.int32),
        groups=group_array,
        weights=group_weights(group_array),
    )
    parent = baseline_weights(schema).astype(np.int64)
    if not np.array_equal(exact_evaluations(split, schema, parent), white_exported(split)):
        raise ValueError(f"{name}: baseline reconstruction mismatch")
    return split


def load_dataset(path, experiment, engine=None):
    manifest_path = path / "manifest.json"
    manifest_hash = sha256_file(manifest_path)
    manifest = load_json(manifest_path)
    if manifest.get("format_version") != dataset.FORMAT_VERSION:
        raise ValueError("unsupported dataset manifest")
    if manifest.get("experiment_sha256") != dataset.sha256_json(experiment):
        raise ValueError("dataset uses a different experiment configuration")

    expected = experiment["dataset"]
    schema = None
    splits = {}
    for name in RUN_SPLITS:
        split_path = path / f"{name}.jsonl"
        if sha256_file(split_path) != manifest["outputs"][split_path.name]:
            raise ValueError(f"{name} dataset hash mismatch")
        with split_path.open(encoding="utf-8") as stream:
            current_schema = json.loads(next(stream))
        if schema is None:
            schema = current_schema
            validate_schema(schema, expected["schema_version"], expected["feature_count"])
        elif current_schema != schema:
            raise ValueError("dataset schemas differ")
        splits[name] = load_split(split_path, name, schema)
        if not len(splits[name].targets):
            raise ValueError(f"{name} dataset is empty")

    if engine is not None:
        if sha256_file(engine) != manifest["engine"]["sha256"]:
            raise ValueError("engine hash differs from dataset exporter")
        if read_engine_schema(engine) != schema:
            raise ValueError("engine feature schema differs from dataset")

    return TuningData(
        manifest_sha256=manifest_hash,
        schema=schema,
        splits=splits,
    )


def baseline_weights(schema):
    return np.asarray(
        [[feature["mg"], feature["eg"]] for feature in schema["features"]],
        dtype=np.float64,
    )


def expected_score(evaluation, scale):
    return special.expit(math.log(10) * scale * np.asarray(evaluation) / 400)


def mean_squared_error(evaluation, targets, scale, weights=None):
    error = expected_score(evaluation, scale) - targets
    squared = error * error
    return float(np.average(squared, weights=weights))


def white_exported(split):
    return split.turns * split.exported


def calibrate_scale(split, calibration):
    lower, upper = calibration["bounds"]
    result = optimize.minimize_scalar(
        lambda scale: mean_squared_error(
            white_exported(split), split.targets, scale, split.weights
        ),
        bounds=(lower, upper),
        method="bounded",
        options={
            "xatol": calibration["absolute_tolerance"],
            "maxiter": calibration["maximum_iterations"],
        },
    )
    lower_margin = result.x - lower
    upper_margin = upper - result.x
    boundary_margin = max(calibration["absolute_tolerance"] * 10, (upper - lower) * 1e-6)
    if not result.success or not math.isfinite(result.fun):
        raise ValueError("logistic-scale calibration failed")
    if min(lower_margin, upper_margin) <= boundary_margin:
        raise ValueError("logistic-scale optimum reached a search bound")
    return result


def feature_support(split, schema, minimum_groups):
    matrix = split.coefficients.tocsc()
    report = []
    for feature in schema["features"]:
        feature_id = feature["id"]
        start, end = matrix.indptr[feature_id : feature_id + 2]
        rows = matrix.indices[start:end]
        values = matrix.data[start:end]
        groups = int(np.unique(split.groups[rows]).size)
        positions = int(values.size)
        report.append(
            {
                "id": feature_id,
                "name": feature["name"],
                "groups": groups,
                "positions": positions,
                "absolute_coefficient": int(np.abs(values).sum()),
                "minimum_coefficient": int(values.min()) if positions else 0,
                "maximum_coefficient": int(values.max()) if positions else 0,
                "status": (
                    "unsupported"
                    if groups == 0
                    else "insufficient"
                    if groups < minimum_groups
                    else "active"
                ),
            }
        )
    return report


def phase_material_ids(schema):
    ids = {feature["name"]: feature["id"] for feature in schema["features"]}
    return np.asarray([ids[name] for name in PHASE_MATERIAL_FEATURES], dtype=np.int64)


def continuous_evaluation(split, schema, weights):
    scores = split.coefficients @ weights + split.fixed
    mg = scores[:, 0]
    raw_eg = scores[:, 1]

    stronger = (raw_eg < 0).astype(np.int8)
    pawns = split.pawn_counts[np.arange(len(raw_eg)), stronger]
    eg_scale = np.minimum(
        schema["scale_limit"], schema["scale_base"] + schema["scale_per_pawn"] * pawns
    ) / schema["scale_limit"]
    eg = raw_eg * eg_scale

    ids = phase_material_ids(schema)
    phase_min = schema["phase_material_min"]
    phase_max = float(PHASE_START_COUNTS @ weights[ids, 0])
    if phase_max <= phase_min:
        raise ValueError("candidate phase maximum is not positive")
    material = split.phase_counts @ weights[ids, 0]
    blend = (np.clip(material, phase_min, phase_max) - phase_min) / (phase_max - phase_min)

    white = mg * blend + eg * (1 - blend)
    return white + split.turns * schema["tempo"]


def continuous_loss_gradient(split, schema, weights, scale):
    scores = split.coefficients @ weights + split.fixed
    mg = scores[:, 0]
    raw_eg = scores[:, 1]

    stronger = (raw_eg < 0).astype(np.int8)
    pawns = split.pawn_counts[np.arange(len(raw_eg)), stronger]
    eg_scale = np.minimum(
        schema["scale_limit"], schema["scale_base"] + schema["scale_per_pawn"] * pawns
    ) / schema["scale_limit"]
    eg = raw_eg * eg_scale

    ids = phase_material_ids(schema)
    phase_min = schema["phase_material_min"]
    phase_max = float(PHASE_START_COUNTS @ weights[ids, 0])
    denominator = phase_max - phase_min
    if denominator <= 0:
        raise ValueError("candidate phase maximum is not positive")

    material = split.phase_counts @ weights[ids, 0]
    clipped = np.clip(material, phase_min, phase_max)
    blend = (clipped - phase_min) / denominator
    white = mg * blend + eg * (1 - blend) + split.turns * schema["tempo"]

    prediction = expected_score(white, scale)
    error = prediction - split.targets
    normalized_weights = split.weights / split.weights.sum()
    loss = float(np.sum(normalized_weights * error * error))
    sigmoid_derivative = math.log(10) * scale / 400
    score_gradient = (
        2
        * normalized_weights
        * error
        * prediction
        * (1 - prediction)
        * sigmoid_derivative
    )

    gradient = np.empty_like(weights)
    gradient[:, 0] = split.coefficients.T @ (score_gradient * blend)
    gradient[:, 1] = split.coefficients.T @ (score_gradient * (1 - blend) * eg_scale)

    interior = (material > phase_min) & (material < phase_max)
    phase_effect = score_gradient * (mg - eg) * interior
    numerator = material - phase_min
    for index, feature_id in enumerate(ids):
        derivative = (
            split.phase_counts[:, index] * denominator
            - numerator * PHASE_START_COUNTS[index]
        ) / (denominator * denominator)
        gradient[feature_id, 0] += float(phase_effect @ derivative)

    return loss, gradient


def schema_with_weights(schema, weights):
    if not np.array_equal(weights, np.rint(weights)):
        raise ValueError("exact scoring requires integer weights")
    candidate = copy.deepcopy(schema)
    for feature, (mg, eg) in zip(candidate["features"], weights):
        feature["mg"] = int(mg)
        feature["eg"] = int(eg)
    ids = phase_material_ids(candidate)
    candidate["phase_material_max"] = int(PHASE_START_COUNTS @ weights[ids, 0])
    return candidate


def signed_divide(values, divisor):
    return np.where(values < 0, -((-values) // divisor), values // divisor)


def exact_evaluations_and_phases(split, schema, weights):
    scores = split.coefficients @ weights + split.fixed
    mg = scores[:, 0]
    raw_eg = scores[:, 1]

    stronger = (raw_eg < 0).astype(np.int8)
    pawns = split.pawn_counts[np.arange(len(raw_eg)), stronger]
    scale = np.minimum(
        schema["scale_limit"], schema["scale_base"] + schema["scale_per_pawn"] * pawns
    )
    eg = signed_divide(raw_eg * scale, schema["scale_limit"])

    ids = phase_material_ids(schema)
    phase_min = schema["phase_material_min"]
    phase_max = int(PHASE_START_COUNTS @ weights[ids, 0])
    if phase_max <= phase_min:
        raise ValueError("candidate phase maximum is not positive")
    material = np.clip(split.phase_counts @ weights[ids, 0], phase_min, phase_max)
    phases = (
        (material - phase_min) * schema["phase_limit"] // (phase_max - phase_min)
    ).astype(np.int16)

    white = signed_divide(
        mg * phases + eg * (schema["phase_limit"] - phases),
        schema["phase_limit"],
    )
    return white + split.turns * schema["tempo"], phases


def exact_evaluations(split, schema, weights):
    return exact_evaluations_and_phases(split, schema, weights)[0]


def split_metric(split, schema, weights, scale):
    evaluations = exact_evaluations(split, schema, weights)
    return {
        "positions": len(split.targets),
        "groups": int(np.unique(split.groups).size),
        "mean_squared_error": mean_squared_error(
            evaluations, split.targets, scale, split.weights
        ),
    }


def exact_metrics(data, weights, scale):
    return {
        name: split_metric(data.splits[name], data.schema, weights, scale)
        for name in FIT_SPLITS
    }


def round_away_from_zero(value):
    return math.floor(value + 0.5) if value >= 0 else math.ceil(value - 0.5)


def coordinate_name(feature_name, phase):
    return f"{feature_name}.{PHASES[phase]}"


def mirrored_psqt_name(name, mirrored_pieces):
    parts = name.split(".")
    if len(parts) != 3 or parts[0] != "psqt" or parts[1] not in mirrored_pieces:
        return name
    square = parts[2]
    mirror = chr(ord("h") - (ord(square[0]) - ord("a"))) + square[1]
    return f"psqt.{parts[1]}.{mirror}"


def mirrored_psqt_key(name, mirrored_pieces):
    return min(name, mirrored_psqt_name(name, mirrored_pieces))


def validate_constraints(schema, experiment):
    feature_names = {feature["name"] for feature in schema["features"]}
    coordinates = {
        coordinate_name(feature_name, phase)
        for feature_name in feature_names
        for phase in range(len(PHASES))
    }
    constraints = experiment["constraints"]
    anchors = constraints["anchors"]
    fixed = constraints["fixed"]
    mirrored = constraints["mirror_files"]
    if len(anchors) != len(set(anchors)) or not set(anchors) <= feature_names:
        raise ValueError("invalid anchor")
    if len(fixed) != len(set(fixed)) or not set(fixed) <= coordinates:
        raise ValueError("invalid fixed parameter")
    allowed_mirrors = {"knight", "bishop", "rook", "queen", "king"}
    if len(mirrored) != len(set(mirrored)) or not set(mirrored) <= allowed_mirrors:
        raise ValueError("invalid mirror constraint")

    mirrored = set(mirrored)
    for name in feature_names:
        if mirrored_psqt_name(name, mirrored) not in feature_names:
            raise ValueError(f"missing mirror feature: {name}")
    for prefix in {
        ".".join(name.split(".")[:2])
        for name in feature_names
        if name.startswith("psqt.") or name.startswith("mobility.")
    }:
        if not any(anchor.startswith(prefix + ".") for anchor in anchors):
            raise ValueError(f"feature family has no anchor: {prefix}")


def build_parameter_map(schema, parent, split, experiment):
    validate_constraints(schema, experiment)
    fit = experiment["fit"]
    features = {feature["name"]: feature for feature in schema["features"]}
    fixed = set(experiment["constraints"]["fixed"])
    anchors = set(experiment["constraints"]["anchors"])
    mirrored = set(experiment["constraints"]["mirror_files"])
    minimum_support = experiment["support"]["minimum_groups"]

    groups = {}
    for name in sorted(features):
        feature_id = features[name]["id"]
        tied_name = mirrored_psqt_key(name, mirrored)
        for phase in range(len(PHASES)):
            groups.setdefault((tied_name, phase), []).append((feature_id, phase))

    matrix = split.coefficients.tocsc()
    support_cache = {}

    def group_support(feature_ids):
        key = tuple(sorted(feature_ids))
        if key not in support_cache:
            rows = [
                matrix.indices[matrix.indptr[feature_id] : matrix.indptr[feature_id + 1]]
                for feature_id in key
            ]
            support_cache[key] = (
                int(np.unique(split.groups[np.concatenate(rows)]).size)
                if any(row.size for row in rows)
                else 0
            )
        return support_cache[key]

    lower_default, upper_default = fit["delta_bounds"]
    members = []
    names = []
    bounds = []
    variable_support = {}
    frozen = {}
    used_bounds = set()
    for (name, phase), coordinates in sorted(groups.items()):
        coordinate_names = [
            coordinate_name(schema["features"][feature_id]["name"], phase)
            for feature_id, phase in coordinates
        ]
        values = {parent[feature_id, phase] for feature_id, phase in coordinates}
        if len(values) != 1:
            raise ValueError(f"tied parameters differ: {name}.{PHASES[phase]}")

        support = group_support({feature_id for feature_id, _ in coordinates})
        variable_name = f"{name}.{PHASES[phase]}"
        variable_support[variable_name] = support
        if any(value in fixed for value in coordinate_names):
            reason = "fixed"
        elif any(
            schema["features"][feature_id]["name"] in anchors
            for feature_id, _ in coordinates
        ):
            reason = "anchor"
        elif support < minimum_support:
            reason = "support"
        else:
            reason = None

        if reason:
            for coordinate in coordinate_names:
                frozen[coordinate] = reason
            continue

        lower = float(lower_default)
        upper = float(upper_default)
        base = next(iter(values))
        for index, override in enumerate(fit["bounds"]):
            if any(
                fnmatch.fnmatchcase(coordinate, override["pattern"])
                for coordinate in coordinate_names
            ):
                used_bounds.add(index)
                lower = max(lower, override["minimum"] - base)
                upper = min(upper, override["maximum"] - base)
        if lower > 0 or upper < 0 or math.ceil(lower) > math.floor(upper):
            raise ValueError(f"bounds exclude parent: {variable_name}")

        members.append(coordinates)
        names.append(variable_name)
        bounds.append((lower, upper))

    if not members:
        raise ValueError("all selected parameters are frozen")
    for index, override in enumerate(fit["bounds"]):
        if index not in used_bounds:
            raise ValueError(f"bound pattern matches nothing: {override['pattern']}")

    return ParameterMap(
        parent=parent,
        members=members,
        names=names,
        bounds=bounds,
        support=variable_support,
        frozen=frozen,
        multiplicity=np.asarray([len(group) for group in members], dtype=np.float64),
    )


def fit_parameters(data, parameters, experiment, scale, regularization):
    parent = parameters.parent
    train = data.splits["train"]
    selection = data.splits["selection"]
    initial = np.zeros(len(parameters.members), dtype=np.float64)
    parent_integer = parent.astype(np.int64)
    best = {
        "deltas": initial.copy(),
        "rounded": parent_integer,
        "iteration": 0,
        "selection": split_metric(
            selection, data.schema, parent_integer, scale
        )["mean_squared_error"],
    }
    iteration = 0

    def objective(deltas):
        weights = parameters.expand(deltas)
        loss, gradient = continuous_loss_gradient(train, data.schema, weights, scale)
        penalty = regularization * float(parameters.multiplicity @ (deltas * deltas))
        penalty_gradient = 2 * regularization * parameters.multiplicity * deltas
        return loss + penalty, parameters.gradient(gradient) + penalty_gradient

    def checkpoint(deltas):
        nonlocal iteration
        iteration += 1
        rounded = parameters.rounded(deltas)
        loss = split_metric(selection, data.schema, rounded, scale)["mean_squared_error"]
        if loss < best["selection"]:
            best["selection"] = loss
            best["deltas"] = deltas.copy()
            best["rounded"] = rounded
            best["iteration"] = iteration

    options = experiment["optimizer"]
    result = optimize.minimize(
        objective,
        initial,
        method=options["method"],
        jac=True,
        bounds=parameters.bounds,
        callback=checkpoint,
        options={
            "maxiter": options["maximum_iterations"],
            "gtol": options["gradient_tolerance"],
            "ftol": options["function_tolerance"],
            "maxls": options["maximum_line_search_steps"],
        },
    )
    if iteration < result.nit:
        checkpoint(result.x)
    if not result.success or not np.isfinite(result.fun):
        raise ValueError(f"optimizer failed: {result.message}")

    deltas = best["deltas"]
    continuous_weights = parameters.expand(deltas)
    continuous_metrics = {
        name: mean_squared_error(
            continuous_evaluation(data.splits[name], data.schema, continuous_weights),
            data.splits[name].targets,
            scale,
            data.splits[name].weights,
        )
        for name in FIT_SPLITS
    }
    penalty = regularization * float(parameters.multiplicity @ (deltas * deltas))
    return {
        "parameters": parameters,
        "rounded": best["rounded"],
        "deltas": deltas,
        "optimizer": result,
        "selected_iteration": best["iteration"],
        "continuous_metrics": continuous_metrics,
        "penalty": penalty,
        "objective": continuous_metrics["train"] + penalty,
    }


def group_improvements(split, parent_eval, candidate_eval, scale, mask=None):
    if mask is None:
        mask = np.ones(len(split.targets), dtype=bool)
    parent_error = expected_score(parent_eval[mask], scale) - split.targets[mask]
    candidate_error = expected_score(candidate_eval[mask], scale) - split.targets[mask]
    improvements = parent_error * parent_error - candidate_error * candidate_error
    _, groups = np.unique(split.groups[mask], return_inverse=True)
    totals = np.bincount(groups, weights=improvements)
    return totals / np.bincount(groups)


def bootstrap_interval(values, samples, confidence, seed):
    if not len(values):
        return {"groups": 0, "mean": None, "lower": None, "upper": None}
    rng = np.random.default_rng(seed)
    means = np.empty(samples, dtype=np.float64)
    batch_size = 128
    for start in range(0, samples, batch_size):
        stop = min(samples, start + batch_size)
        draws = rng.integers(0, len(values), size=(stop - start, len(values)))
        means[start:stop] = values[draws].mean(axis=1)
    tail = (1 - confidence) / 2
    return {
        "groups": len(values),
        "mean": float(values.mean()),
        "lower": float(np.quantile(means, tail)),
        "upper": float(np.quantile(means, 1 - tail)),
    }


def validation_report(split, schema, parent, candidate, scale, policy):
    parent_eval, phases = exact_evaluations_and_phases(split, schema, parent)
    candidate_eval = exact_evaluations(split, schema, candidate)
    overall = bootstrap_interval(
        group_improvements(split, parent_eval, candidate_eval, scale),
        policy["bootstrap_samples"],
        policy["confidence"],
        policy["seed"],
    )
    overall["passed"] = overall["lower"] is not None and overall["lower"] > 0

    phase_reports = []
    boundaries = policy["phase_buckets"]
    for index, (start, stop) in enumerate(zip(boundaries, boundaries[1:])):
        mask = (phases >= start) & (phases < stop)
        interval = bootstrap_interval(
            group_improvements(split, parent_eval, candidate_eval, scale, mask),
            policy["bootstrap_samples"],
            policy["confidence"],
            policy["seed"] + index + 1,
        )
        interval["bucket"] = f"{start}-{stop - 1 if stop <= 128 else 128}"
        interval["positions"] = int(mask.sum())
        interval["passed"] = (
            interval["groups"] < policy["minimum_phase_groups"]
            or interval["upper"] >= 0
        )
        phase_reports.append(interval)

    return {
        "confidence": policy["confidence"],
        "bootstrap_samples": policy["bootstrap_samples"],
        "mean_squared_error": {
            "baseline": mean_squared_error(
                parent_eval, split.targets, scale, split.weights
            ),
            "candidate": mean_squared_error(
                candidate_eval, split.targets, scale, split.weights
            ),
        },
        "overall": overall,
        "phases": phase_reports,
        "qualified": overall["passed"] and all(report["passed"] for report in phase_reports),
    }


def dependency_versions():
    return {
        "python": platform.python_version(),
        "chess": chess.__version__,
        "numpy": np.__version__,
        "scipy": scipy.__version__,
    }


def tool_record():
    return {
        "tune_sha256": sha256_file(pathlib.Path(__file__)),
        "dataset_sha256": sha256_file(pathlib.Path(dataset.__file__)),
    }


def weight_records(schema, parent, candidate, changed_only=False):
    records = []
    for feature, old, new in zip(schema["features"], parent, candidate):
        if changed_only and np.array_equal(old, new):
            continue
        records.append(
            {
                "id": feature["id"],
                "name": feature["name"],
                "parent": {"mg": int(old[0]), "eg": int(old[1])},
                "candidate": {"mg": int(new[0]), "eg": int(new[1])},
            }
        )
    return records


def calibration_artifact(data, experiment, result, support):
    weights = baseline_weights(data.schema).astype(np.int64)
    scale = float(result.x)
    return {
        "kind": "calibration",
        "experiment_sha256": dataset.sha256_json(experiment),
        "dataset_manifest_sha256": data.manifest_sha256,
        "objective": {**experiment["objective"], "scale": scale},
        "support": support,
        "optimizer": {
            "method": "bounded_scalar",
            "iterations": int(result.nit),
            "function_evaluations": int(result.nfev),
            "fitted_scale": scale,
        },
        "metrics": exact_metrics(data, weights, scale),
    }


def fit_artifact(data, experiment, result, regularization, scale):
    parent = result["parameters"].parent.astype(np.int64)
    rounded = result["rounded"]
    optimizer = result["optimizer"]
    return {
        "kind": "fit",
        "experiment_sha256": dataset.sha256_json(experiment),
        "dataset_manifest_sha256": data.manifest_sha256,
        "regularization": regularization,
        "constraints": {
            "variables": result["parameters"].names,
            "variable_support": result["parameters"].support,
            "frozen": result["parameters"].frozen,
            "bounds": result["parameters"].bounds,
            "multiplicity": result["parameters"].multiplicity.astype(int).tolist(),
        },
        "optimizer": {
            "method": experiment["optimizer"]["method"],
            "iterations": int(optimizer.nit),
            "function_evaluations": int(optimizer.nfev),
            "gradient_evaluations": int(optimizer.njev),
            "selected_iteration": result["selected_iteration"],
            "final_objective": float(optimizer.fun),
        },
        "continuous": {
            "metrics": result["continuous_metrics"],
            "regularization_penalty": result["penalty"],
            "objective": result["objective"],
            "deltas": {
                name: float(delta)
                for name, delta in zip(result["parameters"].names, result["deltas"])
            },
        },
        "exact_metrics": exact_metrics(data, rounded, scale),
        "weights": weight_records(data.schema, parent, rounded),
    }


def weights_from_artifact(artifact, schema):
    records = artifact.get("weights", [])
    if len(records) != len(schema["features"]):
        raise ValueError("candidate weight count mismatch")
    weights = np.empty((len(records), 2), dtype=np.int64)
    for feature, record in zip(schema["features"], records):
        if record["id"] != feature["id"] or record["name"] != feature["name"]:
            raise ValueError("candidate feature schema mismatch")
        weights[feature["id"]] = [record["candidate"]["mg"], record["candidate"]["eg"]]
    return weights


def candidate_verification(candidate, schema, engine):
    engine_hash = sha256_file(engine)
    actual_schema = read_engine_schema(engine)
    if sha256_file(engine) != engine_hash:
        raise ValueError("candidate engine changed during verification")
    expected_schema = schema_with_weights(schema, weights_from_artifact(candidate, schema))
    if actual_schema != expected_schema:
        raise ValueError("compiled engine does not match the candidate weights")
    return {
        "candidate_id": candidate["artifact_id"],
        "engine_sha256": engine_hash,
        "schema_sha256": hashlib.sha256(canonical_json(actual_schema).encode()).hexdigest(),
    }


def select_candidate(data, experiment, calibration, fits):
    scale = calibration["objective"]["scale"]
    parent = baseline_weights(data.schema).astype(np.int64)
    baseline_loss = split_metric(data.splits["selection"], data.schema, parent, scale)[
        "mean_squared_error"
    ]
    choices = [(baseline_loss, None, parent)]
    for path, fit in fits:
        weights = weights_from_artifact(fit, data.schema)
        choices.append(
            (
                fit["exact_metrics"]["selection"]["mean_squared_error"],
                path,
                weights,
            )
        )
    loss, selected_path, candidate = min(
        choices, key=lambda choice: (choice[0], choice[1] is not None, str(choice[1]))
    )
    validation = validation_report(
        data.splits["validation"],
        data.schema,
        parent,
        candidate,
        scale,
        experiment["validation"],
    )
    if selected_path is None:
        validation["qualified"] = False

    changes = weight_records(data.schema, parent, candidate, changed_only=True)
    support = fits[0][1]["constraints"]["variable_support"]
    bound_hits = []
    large_changes = []
    if selected_path is not None:
        selected_fit = next(fit for path, fit in fits if path == selected_path)
        ids = {feature["name"]: feature["id"] for feature in data.schema["features"]}
        for name, bounds in zip(
            selected_fit["constraints"]["variables"], selected_fit["constraints"]["bounds"]
        ):
            feature_name, phase = name.rsplit(".", 1)
            phase_index = PHASES.index(phase)
            feature_id = ids[feature_name]
            delta = int(candidate[feature_id, phase_index] - parent[feature_id, phase_index])
            if delta in (math.ceil(bounds[0]), math.floor(bounds[1])):
                bound_hits.append(name)
            allowed = bounds[1] if delta >= 0 else -bounds[0]
            if allowed > 0 and abs(delta) >= 0.8 * allowed:
                large_changes.append(name)

    return {
        "kind": "candidate",
        "experiment_sha256": dataset.sha256_json(experiment),
        "dataset_manifest_sha256": data.manifest_sha256,
        "selected_fit": selected_path.name if selected_path else None,
        "selection": {"baseline": baseline_loss, "candidate": loss},
        "validation": validation,
        "qualified": validation["qualified"],
        "review": {
            "changed_weights": len(changes),
            "sparse_support": [
                {"name": name, "groups": groups}
                for name, groups in support.items()
                if groups < experiment["support"]["minimum_groups"]
            ],
            "bound_hits": bound_hits,
            "large_changes": large_changes,
        },
        "changes": changes,
        "weights": weight_records(data.schema, parent, candidate),
    }


def initial_state(experiment, engine, paths):
    inputs = [{"name": path.name, "sha256": sha256_file(path)} for path in paths]
    inputs.sort(key=lambda item: (item["sha256"], item["name"]))
    return {
        "format_version": STATE_FORMAT_VERSION,
        "experiment": experiment["name"],
        "experiment_sha256": dataset.sha256_json(experiment),
        "tool": tool_record(),
        "dependencies": dependency_versions(),
        "baseline": {
            **experiment["baseline"],
            "engine_sha256": sha256_file(engine),
        },
        "inputs": inputs,
        "steps": {},
    }


def validate_state(state, experiment, engine, paths):
    if state.get("format_version") != STATE_FORMAT_VERSION:
        raise ValueError("unsupported experiment state")
    expected = initial_state(experiment, engine, paths)
    for key in (
        "experiment",
        "experiment_sha256",
        "tool",
        "dependencies",
        "baseline",
        "inputs",
    ):
        if state.get(key) != expected[key]:
            raise ValueError(f"experiment {key} changed; start a new output directory")


def record_step(state_path, name, artifact_path):
    state = load_json(state_path)
    relative = artifact_path.relative_to(state_path.parent).as_posix()
    step = {"path": relative, "sha256": sha256_file(artifact_path)}
    existing = state["steps"].get(name)
    if existing is not None and existing != step:
        raise ValueError(f"completed step changed: {name}")
    state["steps"][name] = step
    atomic_write_json(state_path, state)


def validate_steps(output, state):
    for name, step in state["steps"].items():
        path = output / step["path"]
        if not path.is_file() or sha256_file(path) != step["sha256"]:
            raise ValueError(f"completed step is missing or changed: {name}")


def fit_filename(regularization):
    return f"lambda-{regularization:.0e}.json"


def run_command(args):
    experiment_path = args.experiment.resolve()
    experiment = load_experiment(experiment_path)
    engine = args.engine.resolve()
    output = args.output.resolve()
    paths = [path.resolve() for path in args.pgn]
    if output.name != experiment["name"]:
        raise ValueError("output directory name must match the experiment name")
    if set(experiment["baseline"]["revision"]) == {"0"}:
        raise ValueError("replace the example baseline revision before running")
    if not engine.is_file() or not paths or any(not path.is_file() for path in paths):
        raise ValueError("engine and PGN inputs must exist")
    if read_benchmark(engine) != experiment["baseline"]["benchmark"]:
        raise ValueError("engine benchmark differs from the experiment baseline")
    engine_schema = read_engine_schema(engine)
    validate_schema(
        engine_schema,
        experiment["dataset"]["schema_version"],
        experiment["dataset"]["feature_count"],
    )
    validate_constraints(engine_schema, experiment)

    state_path = output / "state.json"
    if not output.exists():
        output.mkdir(parents=True)
        atomic_write_json(output / "experiment.json", experiment)
        atomic_write_json(state_path, initial_state(experiment, engine, paths))
    elif not state_path.is_file():
        raise ValueError("output directory is not a resumable experiment")

    state = load_json(state_path)
    validate_state(state, experiment, engine, paths)
    validate_steps(output, state)
    if dataset.sha256_json(load_json(output / "experiment.json")) != state["experiment_sha256"]:
        raise ValueError("stored experiment configuration changed")

    dataset_path = output / "dataset"
    if "dataset" not in state["steps"]:
        if not dataset_path.exists():
            dataset.atomic_build(engine, paths, dataset_path, experiment)
        record_step(state_path, "dataset", dataset_path / "manifest.json")
        state = load_json(state_path)

    dataset_manifest = load_json(dataset_path / "manifest.json")
    if dataset_manifest.get("inputs") != state["inputs"]:
        raise ValueError("dataset PGN provenance differs from the experiment")
    if dataset_manifest.get("engine", {}).get("sha256") != state["baseline"]["engine_sha256"]:
        raise ValueError("dataset engine provenance differs from the experiment")

    data = load_dataset(dataset_path, experiment, engine)

    calibration_path = output / "calibration.json"
    if "calibration" not in state["steps"]:
        if calibration_path.exists():
            read_artifact(calibration_path)
        else:
            support = feature_support(
                data.splits["train"], data.schema, experiment["support"]["minimum_groups"]
            )
            result = calibrate_scale(data.splits["train"], experiment["calibration"])
            write_artifact(
                calibration_path,
                calibration_artifact(data, experiment, result, support),
            )
        record_step(state_path, "calibration", calibration_path)
        state = load_json(state_path)
    calibration = read_artifact(calibration_path)

    fits = []
    parent = baseline_weights(data.schema)
    fit_steps = [
        f"fit:{regularization:.0e}" for regularization in experiment["fit"]["regularization"]
    ]
    parameters = (
        build_parameter_map(data.schema, parent, data.splits["train"], experiment)
        if any(step not in state["steps"] for step in fit_steps)
        else None
    )
    for regularization in experiment["fit"]["regularization"]:
        name = fit_filename(regularization)
        path = output / "fits" / name
        step_name = f"fit:{regularization:.0e}"
        if step_name not in state["steps"]:
            if path.exists():
                read_artifact(path)
            else:
                result = fit_parameters(
                    data,
                    parameters,
                    experiment,
                    calibration["objective"]["scale"],
                    regularization,
                )
                write_artifact(
                    path,
                    fit_artifact(
                        data,
                        experiment,
                        result,
                        regularization,
                        calibration["objective"]["scale"],
                    ),
                )
            record_step(state_path, step_name, path)
            state = load_json(state_path)
        fits.append((path, read_artifact(path)))

    candidate_path = output / "candidate.json"
    if "candidate" not in state["steps"]:
        if candidate_path.exists():
            read_artifact(candidate_path)
        else:
            candidate = select_candidate(data, experiment, calibration, fits)
            write_artifact(candidate_path, candidate)
        record_step(state_path, "candidate", candidate_path)
        state = load_json(state_path)
    read_artifact(candidate_path)

    validate_state(load_json(state_path), experiment, engine, paths)
    print_status(output, False)


def read_state(output):
    state = load_json(output / "state.json")
    if state.get("format_version") != STATE_FORMAT_VERSION:
        raise ValueError("unsupported experiment state")
    validate_steps(output, state)
    experiment = load_json(output / "experiment.json")
    if dataset.sha256_json(experiment) != state["experiment_sha256"]:
        raise ValueError("stored experiment configuration changed")
    return state, experiment


def verify_command(args):
    output = args.output.resolve()
    state, experiment = read_state(output)
    if "candidate" not in state["steps"]:
        raise ValueError("the experiment has no candidate")
    candidate = read_artifact(output / "candidate.json")
    if not candidate["qualified"]:
        raise ValueError("an unqualified candidate cannot be verified")

    manifest = load_json(output / "dataset" / "manifest.json")
    train_path = output / "dataset" / "train.jsonl"
    if sha256_file(train_path) != manifest["outputs"]["train.jsonl"]:
        raise ValueError("training dataset hash mismatch")
    schema = dataset.read_schema(train_path)
    validate_schema(
        schema,
        experiment["dataset"]["schema_version"],
        experiment["dataset"]["feature_count"],
    )
    verification = candidate_verification(candidate, schema, args.engine.resolve())
    path = output / "verification.json"
    if path.exists() and load_json(path) != verification:
        raise ValueError("candidate verification changed")
    if not path.exists():
        atomic_write_json(path, verification)
    record_step(output / "state.json", "verification", path)
    print_status(output, False)


def validate_command(args):
    output = args.output.resolve()
    state, _ = read_state(output)
    report, _ = dataset.validate_output(output / "dataset")
    manifest = load_json(output / "dataset" / "manifest.json")
    if manifest["experiment_sha256"] != state["experiment_sha256"]:
        raise ValueError("dataset uses a different experiment configuration")
    print(json.dumps(report, indent=2, sort_keys=True))


def status_record(output):
    state, _ = read_state(output)
    steps = state["steps"]
    candidate = read_artifact(output / "candidate.json") if "candidate" in steps else None
    verified = "verification" in steps
    decision = load_json(output / "decision.json") if "decision" in steps else None
    heldout = "heldout" in steps
    if decision:
        next_action = (
            "held-out data may be revealed for a release check"
            if not heldout
            else "experiment closed"
        )
    elif not candidate:
        next_action = "resume the experiment"
    elif not candidate["qualified"]:
        next_action = "close the experiment with --result offline"
    elif not verified:
        next_action = "apply candidate weights, build the engine, and run verify"
    else:
        next_action = "commit, push, run OpenBench, then close the experiment"
    return {
        "experiment": state["experiment"],
        "completed_steps": sorted(state["steps"]),
        "candidate_id": candidate["artifact_id"] if candidate else None,
        "qualified": candidate["qualified"] if candidate else None,
        "verified": verified,
        "decision": decision,
        "heldout_opened": heldout,
        "next_action": next_action,
    }


def print_status(output, as_json):
    status = status_record(output)
    if as_json:
        print(json.dumps(status, indent=2, sort_keys=True))
        return
    print(f"experiment {status['experiment']}")
    print(f"steps {len(status['completed_steps'])}")
    if status["candidate_id"]:
        print(f"candidate {status['candidate_id']}")
        print(f"qualified {str(status['qualified']).lower()}")
        print(f"verified {str(status['verified']).lower()}")
    if status["decision"]:
        print(f"decision {status['decision']['decision']}")
    print(f"next {status['next_action']}")


def status_command(args):
    print_status(args.output.resolve(), args.json)


def record_result(result):
    records = []
    if RESULTS_PATH.exists():
        records = [json.loads(line) for line in RESULTS_PATH.read_text().splitlines()]
    for current in records:
        if (
            current["experiment"] == result["experiment"]
            or current["experiment_sha256"] == result["experiment_sha256"]
        ):
            if current != result:
                raise ValueError("experiment result conflicts with the tracked ledger")
            return
    records.append(result)
    atomic_write(RESULTS_PATH, "".join(canonical_json(record) + "\n" for record in records))


def close_command(args):
    output = args.output.resolve()
    state, experiment = read_state(output)
    decision_path = output / "decision.json"
    candidate = read_artifact(output / "candidate.json")
    strength_result = args.result != "offline"
    if strength_result:
        if not candidate["qualified"]:
            raise ValueError("an unqualified candidate cannot have a strength result")
        if "verification" not in state["steps"]:
            raise ValueError("candidate engine has not been verified")
        verification = load_json(output / "verification.json")
        if verification.get("candidate_id") != candidate["artifact_id"]:
            raise ValueError("candidate verification does not match")
        if not args.openbench_test:
            raise ValueError("a strength result requires an OpenBench test ID")
    elif args.openbench_test:
        raise ValueError("an offline rejection has no OpenBench test ID")
    if not args.reason.strip():
        raise ValueError("decision reason must not be empty")

    decision = {
        "experiment": experiment["name"],
        "experiment_sha256": state["experiment_sha256"],
        "baseline": experiment["baseline"],
        "candidate_id": candidate["artifact_id"],
        "qualified": candidate["qualified"],
        "selected_fit": candidate["selected_fit"],
        "result": args.result,
        "decision": "accepted" if args.result == "upper" else "rejected",
        "reason": args.reason,
        "openbench_test": args.openbench_test,
        "normalized_elo_bounds": experiment["strength"]["normalized_elo_bounds"],
    }
    if decision_path.exists() and load_json(decision_path) != decision:
        raise ValueError("experiment decision changed")
    record_result(decision)
    if not decision_path.exists():
        atomic_write_json(decision_path, decision)
    record_step(output / "state.json", "decision", decision_path)
    print_status(output, False)


def reveal_command(args):
    output = args.output.resolve()
    state, experiment = read_state(output)
    if "decision" not in state["steps"]:
        raise ValueError("close the experiment before revealing held-out data")
    if state.get("tool") != tool_record() or state.get("dependencies") != dependency_versions():
        raise ValueError("experiment tooling changed; restore its recorded environment")
    report_path = output / "heldout-report.json"
    if "heldout" in state["steps"]:
        raise ValueError("held-out data has already been revealed")

    manifest = load_json(output / "dataset" / "manifest.json")
    heldout_path = output / "dataset" / "heldout.jsonl"
    if sha256_file(heldout_path) != manifest["outputs"]["heldout.jsonl"]:
        raise ValueError("held-out dataset hash mismatch")
    schema = dataset.read_schema(heldout_path)
    validate_schema(
        schema,
        experiment["dataset"]["schema_version"],
        experiment["dataset"]["feature_count"],
    )
    heldout = load_split(heldout_path, "heldout", schema)
    calibration = read_artifact(output / "calibration.json")
    candidate = read_artifact(output / "candidate.json")
    decision = load_json(output / "decision.json")
    parent = baseline_weights(schema).astype(np.int64)
    retained = (
        weights_from_artifact(candidate, schema)
        if decision["decision"] == "accepted"
        else parent
    )
    scale = calibration["objective"]["scale"]
    report = {
        "experiment": experiment["name"],
        "decision": decision["decision"],
        "retained": split_metric(heldout, schema, retained, scale),
    }
    if decision["decision"] == "accepted":
        report["parent"] = split_metric(heldout, schema, parent, scale)
        report["comparison"] = validation_report(
            heldout, schema, parent, retained, scale, experiment["validation"]
        )
    if report_path.exists():
        if load_json(report_path) != report:
            raise ValueError("held-out report changed")
    else:
        atomic_write_json(report_path, report)
    record_step(output / "state.json", "heldout", report_path)
    print(json.dumps(report, indent=2, sort_keys=True))


def parse_args():
    parser = argparse.ArgumentParser(description="Run Latrunculi HCE tuning experiments")
    subparsers = parser.add_subparsers(dest="command", required=True)

    run = subparsers.add_parser("run", help="run or resume an experiment")
    run.add_argument("--experiment", type=pathlib.Path, required=True)
    run.add_argument("--engine", type=pathlib.Path, required=True)
    run.add_argument("--output", type=pathlib.Path, required=True)
    run.add_argument("pgn", type=pathlib.Path, nargs="+")
    run.set_defaults(function=run_command)

    status = subparsers.add_parser("status", help="show experiment state")
    status.add_argument("--json", action="store_true")
    status.add_argument("output", type=pathlib.Path)
    status.set_defaults(function=status_command)

    verify = subparsers.add_parser("verify", help="verify the compiled candidate")
    verify.add_argument("output", type=pathlib.Path)
    verify.add_argument("--engine", type=pathlib.Path, required=True)
    verify.set_defaults(function=verify_command)

    validate = subparsers.add_parser("validate", help="validate an experiment dataset")
    validate.add_argument("output", type=pathlib.Path)
    validate.set_defaults(function=validate_command)

    close = subparsers.add_parser("close", help="record the strength decision")
    close.add_argument("output", type=pathlib.Path)
    close.add_argument(
        "--result",
        choices=("offline", "upper", "lower", "inconclusive"),
        required=True,
    )
    close.add_argument("--reason", required=True)
    close.add_argument("--openbench-test")
    close.set_defaults(function=close_command)

    reveal = subparsers.add_parser("reveal", help="score sealed held-out data")
    reveal.add_argument("output", type=pathlib.Path)
    reveal.set_defaults(function=reveal_command)

    return parser.parse_args()


def main():
    try:
        args = parse_args()
        args.function(args)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"tune: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
