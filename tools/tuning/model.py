"""Sparse tuning data, evaluation reconstruction, and objective calculations."""

import collections
import copy
import json
import math
from array import array
from dataclasses import dataclass

import numpy as np
from scipy import sparse, special

from . import dataset


PHASES = ("mg", "eg")
PHASE_MATERIAL_FEATURES = (
    "material.knight",
    "material.bishop",
    "material.rook",
    "material.queen",
)
PHASE_START_COUNTS = np.asarray((4, 4, 4, 2), dtype=np.int64)
DEVELOPMENT_SPLIT = "development"


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
    manifest_hash = dataset.sha256_file(manifest_path)
    data_hash = dataset.sha256_file(split_path)
    _, schema = dataset.validate_output(path)
    development = load_split(split_path, DEVELOPMENT_SPLIT, schema)
    if not len(development.targets):
        raise ValueError("development dataset is empty")
    if (dataset.sha256_file(manifest_path), dataset.sha256_file(split_path)) != (manifest_hash, data_hash):
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
