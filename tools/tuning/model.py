"""Sparse tuning data, evaluation reconstruction, and objective calculations."""

import collections
import copy
import json
import math
from array import array
from dataclasses import dataclass

import numpy as np
from scipy import sparse, special

from . import dataset, finalization


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
    finalization_context: np.ndarray | None = None


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
        finalization_context=None if split.finalization_context is None else split.finalization_context[mask],
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
    contexts = []

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
            if schema["version"] == 9:
                context = record["finalization"]
                contexts.append(context["pieces"][0] + context["pieces"][1]
                                + context["pawn_flanks"] + context["bishop_colors"])

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
        finalization_context=np.asarray(contexts, dtype=np.int16) if contexts else None,
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


def _endgame_scale(split, schema, raw_eg):
    stronger = (raw_eg < 0).astype(np.int8)
    pawns = split.pawn_counts[np.arange(len(raw_eg)), stronger]
    return np.minimum(
        schema["scale_limit"], schema["scale_base"] + schema["scale_per_pawn"] * pawns
    )


def _continuous_phase(split, schema, material_weights):
    phase_min = schema["phase_material_min"]
    phase_max = float(PHASE_START_COUNTS @ material_weights)
    if phase_max <= phase_min:
        raise ValueError("candidate phase maximum is not positive")
    material = split.phase_counts @ material_weights
    blend = (np.clip(material, phase_min, phase_max) - phase_min) / (phase_max - phase_min)
    return material, phase_max, blend


def _raw_scores(split, schema, weights):
    special = finalization.special_ids(schema)
    if special:
        weights = weights.copy()
        weights[special] = 0
    return split.coefficients @ weights + split.fixed


def _finalized_eg(split, schema, weights, raw, exact=False):
    """Return EG and local derivatives; discontinuous branch boundaries use zero slopes."""
    limit = schema["scale_limit"]
    base = _endgame_scale(split, schema, raw)
    if schema["version"] != 9:
        eg = signed_divide(raw * base, limit) if exact else raw * base / limit
        return eg, base / limit, None, None
    roles = schema["finalization"]
    context = split.finalization_context
    if context is None or context.shape != (len(raw), 14):
        raise ValueError("missing finalization context")
    pieces = context[:, :10].reshape((-1, 2, 5))
    target = base.copy()
    opposite = (pieces[:, 0, 2] == 1) & (pieces[:, 1, 2] == 1) & ((context[:, 12] ^ context[:, 13]) == 3)
    matching = opposite & (pieces[:, 0, 1] == pieces[:, 1, 1]) & (pieces[:, 0, 3] == pieces[:, 1, 3]) & (pieces[:, :, 4].sum(axis=1) == 0)
    pure = matching & (pieces[:, 0, 1] == 0) & (pieces[:, 0, 3] == 0)
    extra = matching & (((pieces[:, 0, 1] == 1) & (pieces[:, 0, 3] == 0)) | ((pieces[:, 0, 1] == 0) & (pieces[:, 0, 3] == 1)))
    target = np.where(pure, np.minimum(target, 32), target)
    target = np.where(extra, np.minimum(target, 48), target)
    material = weights[phase_material_ids(schema), 0]
    npm = pieces[:, :, 1:] @ material
    strong = (raw < 0).astype(np.int8)
    rows = np.arange(len(raw))
    advantage = npm[rows, strong] - npm[rows, 1 - strong]
    pawnless = (pieces[rows, strong, 0] == 0) & (npm[rows, 1 - strong] > 0) & (advantage >= 0) & (advantage <= material[1])
    target = np.where(pawnless, np.minimum(target, 16), target)
    flanks = context[rows, 10 + strong]
    target = np.where((flanks == 1) | (flanks == 2), np.minimum(target, base - 4), target)
    complexity_ids = roles["complexity"]
    complexity = split.coefficients[:, complexity_ids] @ weights[complexity_ids, 1] if complexity_ids else np.zeros_like(raw)
    sign = np.sign(raw)
    active = (raw != 0) & (np.abs(raw) + complexity > 0)
    adjusted = sign * np.maximum(np.abs(raw) + complexity, 0)
    strength = weights[roles["strength"], 1] if roles["strength"] is not None else 0
    reduction = (base - target) * strength
    effective = base - (reduction // limit if exact else reduction / limit)
    eg = signed_divide(adjusted * effective, limit) if exact else adjusted * effective / limit
    return (eg, active * effective / limit, sign * active * effective / limit,
            adjusted * (target - base) / (limit * limit))


def continuous_evaluation(split, schema, weights):
    scores = _raw_scores(split, schema, weights)
    mg = scores[:, 0]
    raw_eg = scores[:, 1]

    eg, _, _, _ = _finalized_eg(split, schema, weights, raw_eg)
    _, _, blend = _continuous_phase(split, schema, weights[phase_material_ids(schema), 0])
    white = mg * blend + eg * (1 - blend)
    return white + split.turns * schema["tempo"]


def continuous_loss_gradient(split, schema, weights, scale):
    scores = _raw_scores(split, schema, weights)
    mg = scores[:, 0]
    raw_eg = scores[:, 1]

    eg, eg_derivative, complexity_derivative, strength_derivative = _finalized_eg(split, schema, weights, raw_eg)

    ids = phase_material_ids(schema)
    material, phase_max, blend = _continuous_phase(split, schema, weights[ids, 0])
    phase_min = schema["phase_material_min"]
    denominator = phase_max - phase_min
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
    gradient[:, 1] = split.coefficients.T @ (score_gradient * (1 - blend) * eg_derivative)
    if schema["version"] == 9:
        roles = schema["finalization"]
        gradient[finalization.special_ids(schema)] = 0
        if roles["complexity"]:
            gradient[roles["complexity"], 1] = split.coefficients[:, roles["complexity"]].T @ (score_gradient * (1 - blend) * complexity_derivative)
        if roles["strength"] is not None:
            gradient[roles["strength"], 1] = np.sum(score_gradient * (1 - blend) * strength_derivative)

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
    scores = _raw_scores(split, schema, weights)
    mg = scores[:, 0]
    raw_eg = scores[:, 1]

    eg, _, _, _ = _finalized_eg(split, schema, weights, raw_eg, exact=True)

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


def exact_loss(split, schema, weights, scale):
    evaluations = exact_evaluations(split, schema, weights)
    return mean_squared_error(evaluations, split.targets, scale, split.weights)


def split_metric(split, schema, weights, scale):
    return {
        "positions": len(split.targets),
        "groups": int(np.unique(split.groups).size),
        "mean_squared_error": exact_loss(split, schema, weights, scale),
    }


def round_away_from_zero(value):
    return math.floor(value + 0.5) if value >= 0 else math.ceil(value - 0.5)
