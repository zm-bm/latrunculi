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
DEVELOPMENT_SPLIT = "development"
FORMAT_VERSION = 4

FIT_POLICY = {
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
        "regularization": [
            1e-9,
            3e-9,
            1e-8,
            3e-8,
            1e-7,
            3e-7,
            1e-6,
            3e-6,
            1e-5,
        ],
    },
    "optimizer": {
        "method": "L-BFGS-B",
        "maximum_iterations": 1000,
        "gradient_tolerance": 1e-10,
        "function_tolerance": 1e-12,
        "maximum_line_search_steps": 50,
    },
    "validation": {
        "folds": 5,
        "fold_seed": 20260904,
        "bootstrap_samples": 2000,
        "confidence": 0.9,
        "bootstrap_seed": 20260905,
        "phase_buckets": [0, 32, 64, 96, 129],
        "minimum_phase_groups": 128,
    },
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
    group_names: np.ndarray
    weights: np.ndarray


@dataclass
class TuningData:
    manifest_sha256: str
    schema: dict
    development: Split
    data_sha256: str


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

    def rounded_deltas(self, deltas):
        rounded_deltas = []
        for value, (lower, upper) in zip(deltas, self.bounds):
            rounded = round_away_from_zero(value)
            rounded_deltas.append(min(max(rounded, math.ceil(lower)), math.floor(upper)))
        return np.asarray(rounded_deltas, dtype=np.int64)

    def rounded(self, deltas):
        return self.expand(self.rounded_deltas(deltas)).astype(np.int64)


def sha256_file(path):
    return dataset.sha256_file(path)


def canonical_json(value):
    return dataset.canonical_json(value)


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
    artifact = {**artifact, "format_version": FORMAT_VERSION}
    artifact["artifact_id"] = artifact_id(artifact)
    atomic_write_json(path, artifact)
    return artifact


def read_artifact(path, expected=None):
    artifact = load_json(path)
    if not isinstance(artifact, dict):
        raise ValueError(f"invalid tuning artifact: {path}")
    if type(artifact.get("format_version")) is not int or artifact["format_version"] != FORMAT_VERSION:
        raise ValueError("unsupported tuning format; use the original tool revision for old runs")
    if artifact.get("artifact_id") != artifact_id(artifact):
        raise ValueError(f"artifact hash mismatch: {path}")
    for key, value in (expected or {}).items():
        if key not in artifact or canonical_json(artifact[key]) != canonical_json(value):
            raise ValueError(f"artifact context mismatch ({key}): {path}")
    return artifact


def checkpoint(path, context, compute):
    if not path.exists():
        write_artifact(path, {**compute(), **context})
    return read_artifact(path, context)


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


validate_schema = dataset.validate_schema


def group_weights(groups):
    counts = collections.Counter(groups)
    if not counts:
        return np.asarray([], dtype=np.float64)
    return np.asarray([1.0 / counts[group] for group in groups])


def subset_split(split, mask):
    groups = split.groups[mask]
    return Split(
        coefficients=split.coefficients[mask],
        fixed=split.fixed[mask],
        phase_counts=split.phase_counts[mask],
        pawn_counts=split.pawn_counts[mask],
        turns=split.turns[mask],
        targets=split.targets[mask],
        exported=split.exported[mask],
        groups=groups,
        group_names=split.group_names,
        weights=group_weights(groups),
    )


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

    group_names, group_array = np.unique(
        np.asarray(groups, dtype=object), return_inverse=True
    )
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
        group_names=group_names,
        weights=group_weights(group_array),
    )
    parent = baseline_weights(schema).astype(np.int64)
    if not np.array_equal(exact_evaluations(split, schema, parent), white_exported(split)):
        raise ValueError(f"{name}: baseline reconstruction mismatch")
    return split


def load_dataset(path):
    manifest_path = path / "manifest.json"
    split_path = path / dataset.DATA_FILE
    manifest_hash = sha256_file(manifest_path)
    data_hash = sha256_file(split_path)
    _, schema = dataset.validate_output(path)
    development = load_split(split_path, DEVELOPMENT_SPLIT, schema)
    if not len(development.targets):
        raise ValueError("development dataset is empty")
    if (sha256_file(manifest_path), sha256_file(split_path)) != (manifest_hash, data_hash):
        raise ValueError("prepared dataset changed while loading")
    return TuningData(manifest_hash, schema, development, data_hash)


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


def validate_constraints(schema, policy):
    feature_names = {feature["name"] for feature in schema["features"]}
    coordinates = {
        coordinate_name(feature_name, phase)
        for feature_name in feature_names
        for phase in range(len(PHASES))
    }
    constraints = policy["constraints"]
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


def build_parameter_map(schema, parent, split, policy, assignments=None):
    validate_constraints(schema, policy)
    if not isinstance(split, Split) or not len(split.targets):
        raise ValueError("parameter support requires training data")
    fit = policy["fit"]
    features = {feature["name"]: feature for feature in schema["features"]}
    fixed = set(policy["constraints"]["fixed"])
    anchors = set(policy["constraints"]["anchors"])
    mirrored = set(policy["constraints"]["mirror_files"])
    minimum_support = policy["support"]["minimum_groups"]

    groups = {}
    for name in sorted(features):
        feature_id = features[name]["id"]
        tied_name = mirrored_psqt_key(name, mirrored)
        for phase in range(len(PHASES)):
            groups.setdefault((tied_name, phase), []).append((feature_id, phase))

    matrix = split.coefficients.tocsc()
    group_folds = None
    if assignments is not None:
        assignments = np.asarray(assignments)
        if assignments.shape != split.groups.shape:
            raise ValueError("invalid fold assignments")
        group_folds = np.full(len(split.group_names), -1, dtype=np.int8)
        for group, fold in zip(split.groups, assignments):
            if group_folds[group] not in (-1, fold):
                raise ValueError("opening group crosses folds")
            group_folds[group] = fold
        fold_count = policy["validation"]["folds"]
        if np.any((assignments < 0) | (assignments >= fold_count)):
            raise ValueError("invalid fold assignments")

    support_cache = {}

    def group_support(feature_ids):
        key = tuple(sorted(feature_ids))
        if key not in support_cache:
            rows = [
                matrix.indices[
                    matrix.indptr[feature_id] : matrix.indptr[feature_id + 1]
                ]
                for feature_id in key
            ]
            groups = (
                np.unique(split.groups[np.concatenate(rows)])
                if any(row.size for row in rows)
                else np.asarray([], dtype=np.int32)
            )
            support = len(groups)
            if group_folds is not None and support:
                excluded_counts = np.bincount(
                    group_folds[groups], minlength=fold_count
                )
                support -= int(excluded_counts.max())
            support_cache[key] = support
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


def fit_parameters(split, schema, parameters, policy, scale, regularization):
    parent = parameters.parent
    initial = np.zeros(len(parameters.members), dtype=np.float64)
    parent_integer = parent.astype(np.int64)
    best = {
        "deltas": initial.copy(),
        "rounded": parent_integer,
        "iteration": 0,
        "objective": split_metric(split, schema, parent_integer, scale)[
            "mean_squared_error"
        ],
    }
    iteration = 0

    def objective(deltas):
        weights = parameters.expand(deltas)
        loss, gradient = continuous_loss_gradient(split, schema, weights, scale)
        penalty = regularization * float(parameters.multiplicity @ (deltas * deltas))
        penalty_gradient = 2 * regularization * parameters.multiplicity * deltas
        return loss + penalty, parameters.gradient(gradient) + penalty_gradient

    def checkpoint(deltas):
        nonlocal iteration
        iteration += 1
        rounded_deltas = parameters.rounded_deltas(deltas)
        rounded = parameters.expand(rounded_deltas).astype(np.int64)
        exact_objective = split_metric(split, schema, rounded, scale)[
            "mean_squared_error"
        ] + regularization * float(
            parameters.multiplicity @ (rounded_deltas * rounded_deltas)
        )
        if exact_objective < best["objective"]:
            best["objective"] = exact_objective
            best["deltas"] = deltas.copy()
            best["rounded"] = rounded
            best["iteration"] = iteration

    options = policy["optimizer"]
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
    continuous_loss = mean_squared_error(
        continuous_evaluation(split, schema, continuous_weights),
        split.targets,
        scale,
        split.weights,
    )
    penalty = regularization * float(parameters.multiplicity @ (deltas * deltas))
    return {
        "parameters": parameters,
        "rounded": best["rounded"],
        "deltas": deltas,
        "optimizer": result,
        "selected_iteration": best["iteration"],
        "continuous_loss": continuous_loss,
        "penalty": penalty,
        "objective": continuous_loss + penalty,
        "exact_objective": best["objective"],
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


def group_errors(split, evaluation, scale, mask=None):
    if mask is None:
        mask = np.ones(len(split.targets), dtype=bool)
    errors = expected_score(evaluation[mask], scale) - split.targets[mask]
    _, groups = np.unique(split.groups[mask], return_inverse=True)
    totals = np.bincount(groups, weights=errors * errors)
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


def comparison_report(comparisons, policy, seed):
    evaluated = []
    for split, schema, parent, candidate, scale in comparisons:
        parent_eval, phases = exact_evaluations_and_phases(split, schema, parent)
        candidate_eval = exact_evaluations(split, schema, candidate)
        evaluated.append((split, parent_eval, candidate_eval, phases, scale))

    improvements = np.concatenate(
        [
            group_improvements(split, parent_eval, candidate_eval, scale)
            for split, parent_eval, candidate_eval, _, scale in evaluated
        ]
    )
    overall = bootstrap_interval(
        improvements,
        policy["bootstrap_samples"],
        policy["confidence"],
        seed,
    )
    overall["passed"] = overall["lower"] is not None and overall["lower"] > 0

    phase_reports = []
    boundaries = policy["phase_buckets"]
    for index, (start, stop) in enumerate(zip(boundaries, boundaries[1:])):
        masks = [
            (phases >= start) & (phases < stop)
            for _, _, _, phases, _ in evaluated
        ]
        chunks = [
            group_improvements(split, parent_eval, candidate_eval, scale, mask)
            for (split, parent_eval, candidate_eval, _, scale), mask in zip(
                evaluated, masks
            )
            if mask.any()
        ]
        values = np.concatenate(chunks) if chunks else np.asarray([])
        interval = bootstrap_interval(
            values,
            policy["bootstrap_samples"],
            policy["confidence"],
            seed + index + 1,
        )
        interval["bucket"] = f"{start}-{stop - 1 if stop <= 128 else 128}"
        interval["positions"] = sum(int(mask.sum()) for mask in masks)
        interval["passed"] = (
            interval["groups"] < policy["minimum_phase_groups"]
            or interval["upper"] >= 0
        )
        phase_reports.append(interval)

    return {
        "confidence": policy["confidence"],
        "bootstrap_samples": policy["bootstrap_samples"],
        "mean_squared_error": {
            "baseline": float(
                np.concatenate(
                    [
                        group_errors(split, parent_eval, scale)
                        for split, parent_eval, _, _, scale in evaluated
                    ]
                ).mean()
            ),
            "candidate": float(
                np.concatenate(
                    [
                        group_errors(split, candidate_eval, scale)
                        for split, _, candidate_eval, _, scale in evaluated
                    ]
                ).mean()
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


def calibration_artifact(data, split, policy, result, support, fold=None):
    weights = baseline_weights(data.schema).astype(np.int64)
    scale = float(result.x)
    artifact = {
        "kind": "calibration",
        "dataset_manifest_sha256": data.manifest_sha256,
        "objective": {**policy["objective"], "scale": scale},
        "optimizer": {
            "method": "bounded_scalar",
            "iterations": int(result.nit),
            "function_evaluations": int(result.nfev),
            "fitted_scale": scale,
        },
        "metrics": split_metric(split, data.schema, weights, scale),
    }
    if support is not None:
        artifact["support"] = support
    if fold is not None:
        artifact["fold"] = fold
    return artifact


def fit_artifact(data, split, policy, result, regularization, scale, fold=None):
    parent = result["parameters"].parent.astype(np.int64)
    rounded = result["rounded"]
    optimizer = result["optimizer"]
    artifact = {
        "kind": "fit",
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
            "method": policy["optimizer"]["method"],
            "iterations": int(optimizer.nit),
            "function_evaluations": int(optimizer.nfev),
            "gradient_evaluations": int(optimizer.njev),
            "selected_iteration": result["selected_iteration"],
            "final_objective": float(optimizer.fun),
        },
        "continuous": {
            "loss": result["continuous_loss"],
            "regularization_penalty": result["penalty"],
            "objective": result["objective"],
            "deltas": {
                name: float(delta)
                for name, delta in zip(result["parameters"].names, result["deltas"])
            },
        },
        "exact": {
            "metrics": split_metric(split, data.schema, rounded, scale),
            "objective": result["exact_objective"],
        },
        "weights": weight_records(data.schema, parent, rounded),
    }
    if fold is not None:
        artifact["fold"] = fold
    return artifact


def weights_from_artifact(artifact, schema):
    records = artifact.get("weights", [])
    if len(records) != len(schema["features"]):
        raise ValueError("candidate weight count mismatch")
    weights = np.empty((len(records), 2), dtype=np.int64)
    for feature, record in zip(schema["features"], records):
        if record["id"] != feature["id"] or record["name"] != feature["name"]:
            raise ValueError("candidate feature schema mismatch")
        if any(type(record["candidate"][phase]) is not int for phase in PHASES):
            raise ValueError("candidate weights must be integers")
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


def fold_assignments(split, policy):
    count = policy["folds"]
    group_assignments = np.asarray(
        [
            int.from_bytes(
                hashlib.sha256(f"{policy['fold_seed']}:{name}".encode()).digest()[:8],
                "big",
            )
            % count
            for name in split.group_names
        ],
        dtype=np.int8,
    )
    assignments = group_assignments[split.groups]
    if set(assignments) != set(range(policy["folds"])):
        raise ValueError("cross-validation has an empty fold")
    return assignments


def select_regularization(reports):
    eligible = [report for report in reports if report["eligible"]]
    if not eligible:
        return None, None, None
    best = max(eligible, key=lambda report: report["mean_improvement"])
    threshold = best["mean_improvement"] - best["standard_error"]
    selected = max(
        (
            report
            for report in eligible
            if report["mean_improvement"] >= threshold
        ),
        key=lambda report: report["regularization"],
    )
    return best, threshold, selected


def cross_validation_artifact(
    data, policy, assignments, calibrations, fits, parameters
):
    parent = baseline_weights(data.schema).astype(np.int64)
    testing_splits = [
        subset_split(data.development, assignments == fold)
        for fold in range(policy["validation"]["folds"])
    ]
    reports = []
    for regularization in policy["fit"]["regularization"]:
        comparisons = []
        fold_reports = []
        for fold, (testing, calibration, fit) in enumerate(
            zip(testing_splits, calibrations, fits[regularization])
        ):
            scale = calibration["objective"]["scale"]
            candidate = weights_from_artifact(fit, data.schema)
            parent_eval = exact_evaluations(testing, data.schema, parent)
            candidate_eval = exact_evaluations(testing, data.schema, candidate)
            improvement = float(
                group_improvements(
                    testing, parent_eval, candidate_eval, scale
                ).mean()
            )
            fold_reports.append(
                {
                    "fold": fold,
                    "groups": int(np.unique(testing.groups).size),
                    "positions": len(testing.targets),
                    "baseline_loss": split_metric(
                        testing, data.schema, parent, scale
                    )["mean_squared_error"],
                    "candidate_loss": split_metric(
                        testing, data.schema, candidate, scale
                    )["mean_squared_error"],
                    "improvement": improvement,
                }
            )
            comparisons.append(
                (testing, data.schema, parent, candidate, scale)
            )

        improvements = np.asarray(
            [report["improvement"] for report in fold_reports], dtype=np.float64
        )
        validation = comparison_report(
            comparisons,
            policy["validation"],
            policy["validation"]["bootstrap_seed"],
        )
        reports.append(
            {
                "regularization": regularization,
                "folds": fold_reports,
                "mean_improvement": float(improvements.mean()),
                "standard_error": float(
                    improvements.std(ddof=1) / math.sqrt(len(improvements))
                ),
                "validation": validation,
                "eligible": validation["qualified"],
            }
        )

    best, threshold, selected = select_regularization(reports)

    return {
        "kind": "cross_validation",
        "dataset_manifest_sha256": data.manifest_sha256,
        "fold_count": len(calibrations),
        "folds": [
            {
                "fold": fold,
                "training_groups": len(data.development.group_names)
                - int(np.unique(testing.groups).size),
                "training_positions": len(data.development.targets)
                - len(testing.targets),
                "testing_groups": int(np.unique(testing.groups).size),
                "testing_positions": len(testing.targets),
                "scale": calibration["objective"]["scale"],
            }
            for fold, (testing, calibration) in enumerate(
                zip(testing_splits, calibrations)
            )
        ],
        "constraints": {
            "variables": parameters.names,
            "variable_support": parameters.support,
            "frozen": parameters.frozen,
        },
        "regularization": reports,
        "best_regularization": best["regularization"] if best else None,
        "one_standard_error_threshold": threshold,
        "selected_regularization": selected["regularization"] if selected else None,
        "supported": selected is not None,
    }


def candidate_review(schema, parent, candidate, fit, minimum_support):
    changes = weight_records(schema, parent, candidate, changed_only=True)
    support = fit["constraints"]["variable_support"] if fit else {}
    bound_hits = []
    large_changes = []
    if fit is not None:
        ids = {feature["name"]: feature["id"] for feature in schema["features"]}
        for name, bounds in zip(
            fit["constraints"]["variables"], fit["constraints"]["bounds"]
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
        "changed_weights": len(changes),
        "sparse_support": [
            {"name": name, "groups": groups}
            for name, groups in support.items()
            if groups < minimum_support
        ],
        "bound_hits": bound_hits,
        "large_changes": large_changes,
    }, changes


def candidate_artifact(data, policy, cross_validation, calibration=None, fit=None):
    parent = baseline_weights(data.schema).astype(np.int64)
    candidate = weights_from_artifact(fit, data.schema) if fit else parent
    review, changes = candidate_review(
        data.schema,
        parent,
        candidate,
        fit,
        policy["support"]["minimum_groups"],
    )
    scale = calibration["objective"]["scale"] if calibration else None
    return {
        "kind": "candidate",
        "dataset_manifest_sha256": data.manifest_sha256,
        "cross_validation_id": cross_validation["artifact_id"],
        "cross_validation_supported": cross_validation["supported"],
        "selected_regularization": cross_validation["selected_regularization"],
        "scale": scale,
        "development": (
            {
                "baseline": split_metric(
                    data.development, data.schema, parent, scale
                ),
                "candidate": split_metric(
                    data.development, data.schema, candidate, scale
                ),
            }
            if fit
            else None
        ),
        "review": review,
        "changes": changes,
        "weights": weight_records(data.schema, parent, candidate),
    }


def prepare_command(args):
    engine = args.engine.resolve()
    paths = [path.resolve() for path in args.pgn]
    output = args.output.resolve()
    identity = dataset.preparation_identity(engine, paths)
    schema = read_engine_schema(engine)
    validate_schema(schema)
    if not output.exists():
        dataset.atomic_build(engine, paths, output)
    report, actual_schema = dataset.validate_output(output)
    manifest = load_json(output / "manifest.json")
    if manifest["identity"] != identity:
        raise ValueError("preparation inputs, policy, or tools changed; use a new output directory")
    if actual_schema != schema or dataset.preparation_identity(engine, paths) != identity:
        raise ValueError("preparation inputs or exporter changed")
    print(json.dumps(report, indent=2, sort_keys=True))


def fit_descriptor(data):
    return {
        "format_version": FORMAT_VERSION,
        "kind": "run",
        "dataset": {
            "manifest_sha256": data.manifest_sha256,
            "data_sha256": data.data_sha256,
        },
        "schema": data.schema,
        "policy": FIT_POLICY,
        "tool": tool_record(),
        "dependencies": dependency_versions(),
    }


def fit_filename(regularization):
    return f"lambda-{regularization:.0e}.json"


def fit_command(args):
    dataset_path = args.dataset.resolve()
    output = args.output.resolve()
    data = load_dataset(dataset_path)
    policy = copy.deepcopy(FIT_POLICY)
    validate_constraints(data.schema, policy)
    descriptor = fit_descriptor(data)
    run_path = output / "run.json"
    if not run_path.exists() and output.exists():
        if any(path.name != "run.json.partial" for path in output.iterdir()):
            raise ValueError("output is not a version-4 fit; use the original tool revision for old runs")
    run = checkpoint(run_path, descriptor, lambda: descriptor)
    common = {"format_version": FORMAT_VERSION, "run_id": run["artifact_id"]}
    assignments = fold_assignments(data.development, policy["validation"])
    parent = baseline_weights(data.schema)
    parameters = build_parameter_map(
        data.schema, parent, data.development, policy, assignments
    )

    def calibration_for(split, path, fold):
        def compute():
            support = (
                feature_support(split, data.schema, policy["support"]["minimum_groups"])
                if fold is None else None
            )
            result = calibrate_scale(split, policy["calibration"])
            return calibration_artifact(data, split, policy, result, support, fold)

        return checkpoint(
            path, {**common, "kind": "calibration", "fold": fold}, compute
        )

    def fit_for(split, path, fold, regularization, calibration):
        scale = calibration["objective"]["scale"]

        def compute():
            result = fit_parameters(
                split, data.schema, parameters, policy, scale, regularization
            )
            return fit_artifact(data, split, policy, result, regularization, scale, fold)

        return checkpoint(
            path,
            {
                **common, "kind": "fit", "fold": fold,
                "regularization": regularization,
                "calibration_id": calibration["artifact_id"],
            },
            compute,
        )

    calibrations = []
    fits = {regularization: [] for regularization in policy["fit"]["regularization"]}
    for fold in range(policy["validation"]["folds"]):
        training = subset_split(data.development, assignments != fold)
        directory = output / "cross-validation" / f"fold-{fold}"
        calibration = calibration_for(training, directory / "calibration.json", fold)
        calibrations.append(calibration)
        for regularization in policy["fit"]["regularization"]:
            fits[regularization].append(
                fit_for(training, directory / fit_filename(regularization),
                        fold, regularization, calibration)
            )
    del training

    cross_validation = checkpoint(
        output / "cross-validation.json",
        {
            **common, "kind": "cross_validation",
            "calibration_ids": [item["artifact_id"] for item in calibrations],
            "fit_ids": {
                fit_filename(regularization): [item["artifact_id"] for item in records]
                for regularization, records in fits.items()
            },
        },
        lambda: cross_validation_artifact(
            data, policy, assignments, calibrations, fits, parameters
        ),
    )
    calibration = None
    final_fit = None
    if cross_validation["supported"]:
        calibration = calibration_for(data.development, output / "calibration.json", None)
        final_fit = fit_for(
            data.development, output / "fit.json", None,
            cross_validation["selected_regularization"], calibration,
        )
    candidate = checkpoint(
        output / "candidate.json",
        {
            **common, "kind": "candidate",
            "cross_validation_id": cross_validation["artifact_id"],
            "fit_id": final_fit["artifact_id"] if final_fit else None,
            "calibration_id": calibration["artifact_id"] if calibration else None,
        },
        lambda: candidate_artifact(data, policy, cross_validation, calibration, final_fit),
    )
    if (sha256_file(dataset_path / "manifest.json"),
        sha256_file(dataset_path / dataset.DATA_FILE)) != (data.manifest_sha256, data.data_sha256):
        raise ValueError("prepared dataset changed during fitting")
    read_artifact(run_path, fit_descriptor(data))
    print(json.dumps({
        "run_id": run["artifact_id"],
        "candidate_id": candidate["artifact_id"],
        "cross_validation_supported": candidate["cross_validation_supported"],
        "changes": len(candidate["changes"]),
    }, indent=2, sort_keys=True))


def verify_command(args):
    output = args.output.resolve()
    if not (output / "run.json").is_file():
        raise ValueError("no version-4 fit; use the original tool revision for old runs")
    run = read_artifact(output / "run.json", {"kind": "run"})
    validate_schema(run["schema"])
    candidate = read_artifact(output / "candidate.json", {
        "kind": "candidate", "run_id": run["artifact_id"],
    })
    if not candidate["cross_validation_supported"]:
        raise ValueError("cross-validation did not support a candidate")
    verification = candidate_verification(candidate, run["schema"], args.engine.resolve())
    print(json.dumps({"format_version": FORMAT_VERSION, **verification}, indent=2, sort_keys=True))


def parse_args():
    parser = argparse.ArgumentParser(description="Prepare data, fit weights, and verify Latrunculi evaluation")
    subparsers = parser.add_subparsers(dest="command", required=True)

    prepare = subparsers.add_parser("prepare", help="prepare or validate a settled dataset")
    prepare.add_argument("--engine", type=pathlib.Path, required=True)
    prepare.add_argument("--output", type=pathlib.Path, required=True)
    prepare.add_argument("pgn", type=pathlib.Path, nargs="+")
    prepare.set_defaults(function=prepare_command)

    fit = subparsers.add_parser("fit", help="fit or resume weights from a prepared dataset")
    fit.add_argument("--dataset", type=pathlib.Path, required=True)
    fit.add_argument("--output", type=pathlib.Path, required=True)
    fit.set_defaults(function=fit_command)

    verify = subparsers.add_parser("verify", help="check compiled weights and evaluation invariants")
    verify.add_argument("output", type=pathlib.Path)
    verify.add_argument("--engine", type=pathlib.Path, required=True)
    verify.set_defaults(function=verify_command)

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
