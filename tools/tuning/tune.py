#!/usr/bin/env python3

import argparse
import copy
import fnmatch
import hashlib
import json
import math
import pathlib
import platform
import subprocess
import sys
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
PHASE_START_COUNTS = np.asarray((4, 4, 4, 2), dtype=np.float64)
SPLIT_FILES = {
    "train": "train.jsonl",
    "validation": "validation.jsonl",
    "validation_endgame": "validation-endgame.jsonl",
}


@dataclass
class Split:
    records: list
    coefficients: sparse.csr_matrix
    fixed: np.ndarray
    phase_counts: np.ndarray
    pawn_counts: np.ndarray
    turns: np.ndarray
    targets: np.ndarray
    exported: np.ndarray


@dataclass
class TuningData:
    manifest: dict
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

    def expand(self, deltas):
        weights = self.parent.copy()
        for delta, coordinates in zip(deltas, self.members):
            for feature_id, phase in coordinates:
                weights[feature_id, phase] += delta
        return weights

    def gradient(self, full_gradient):
        return np.array(
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
            rounded = min(max(rounded, math.ceil(lower)), math.floor(upper))
            rounded_deltas.append(rounded)
        return self.expand(rounded_deltas).astype(np.int64)


def sha256_file(path):
    return dataset.sha256_file(path)


def canonical_json(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False)


def artifact_id(artifact):
    payload = {key: value for key, value in artifact.items() if key != "candidate_id"}
    return hashlib.sha256(canonical_json(payload).encode()).hexdigest()


def load_json(path):
    return json.loads(path.read_text())


def load_config(path):
    config = load_json(path)
    required = {
        "version",
        "dataset",
        "objective",
        "calibration",
        "support",
        "constraints",
        "optimizer",
    }
    if set(config) != required or config["version"] != 1:
        raise ValueError("invalid tuning configuration")

    if config["objective"] != {
        "perspective": "white",
        "result_mapping": "-1=0,0=0.5,1=1",
        "sigmoid": "1/(1+10^(-k*eval/400))",
        "loss": "mean_squared_error",
        "weighting": "position",
    }:
        raise ValueError("unsupported objective")

    minimum = config["support"].get("minimum_positions")
    if set(config["support"]) != {"minimum_positions"} or not isinstance(minimum, int):
        raise ValueError("invalid support policy")
    if minimum < 1:
        raise ValueError("minimum support must be positive")

    return config


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


def read_engine_schema(engine):
    result = subprocess.run(
        [str(engine), "features"],
        input="",
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode:
        raise ValueError(result.stderr.strip() or "feature exporter failed")
    lines = result.stdout.splitlines()
    if len(lines) != 1:
        raise ValueError("feature exporter did not emit one schema record")
    return json.loads(lines[0])


def load_split(path, name, schema):
    records = []
    rows = []
    columns = []
    values = []
    fixed = []
    phase_counts = []
    pawn_counts = []
    turns = []
    targets = []
    exported = []

    with path.open(encoding="utf-8") as stream:
        split_schema = json.loads(next(stream))
        if split_schema != schema:
            raise ValueError(f"{name} schema mismatch")

        for row, line in enumerate(stream):
            record = json.loads(line)
            if record.get("type") != "position" or record.get("version") != schema["version"]:
                raise ValueError(f"{name}: invalid position record")
            rebuilt, _ = dataset.reconstruct(schema, record)
            if rebuilt != record["eval"]:
                raise ValueError(f"{name}: baseline reconstruction mismatch")

            for feature_id, coefficient in record["coefficients"]:
                rows.append(row)
                columns.append(feature_id)
                values.append(coefficient)

            records.append(record)
            fixed.append(record["fixed"])
            phase_counts.append(record["phase_counts"])
            pawn_counts.append(record["pawn_counts"])
            turns.append(1 if record["turn"] == "w" else -1)
            targets.append((record["result"] + 1) / 2)
            exported.append(record["eval"])

    coefficients = sparse.csr_matrix(
        (values, (rows, columns)),
        shape=(len(records), len(schema["features"])),
        dtype=np.float64,
    )
    return Split(
        records=records,
        coefficients=coefficients,
        fixed=np.asarray(fixed, dtype=np.float64),
        phase_counts=np.asarray(phase_counts, dtype=np.float64),
        pawn_counts=np.asarray(pawn_counts, dtype=np.int8),
        turns=np.asarray(turns, dtype=np.float64),
        targets=np.asarray(targets, dtype=np.float64),
        exported=np.asarray(exported, dtype=np.float64),
    )


def load_dataset(path, config, engine=None):
    manifest_path = path / "manifest.json"
    manifest_hash = sha256_file(manifest_path)
    expected = config["dataset"]
    if manifest_hash != expected["manifest_sha256"]:
        raise ValueError("dataset manifest hash mismatch")

    manifest = load_json(manifest_path)
    if manifest["config"]["schema_version"] != expected["schema_version"]:
        raise ValueError("manifest schema version mismatch")
    if manifest["inputs"] != expected["inputs"]:
        raise ValueError("dataset corpus mismatch")

    if engine is not None:
        if not engine.is_file():
            raise ValueError("engine does not exist")
        if sha256_file(engine) != manifest["engine"]["sha256"]:
            raise ValueError("engine hash differs from dataset exporter")

    schema = None
    splits = {}
    for name, filename in SPLIT_FILES.items():
        split_path = path / filename
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

    if engine is not None and read_engine_schema(engine) != schema:
        raise ValueError("engine feature schema differs from dataset")

    return TuningData(
        manifest=manifest,
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


def mean_squared_error(evaluation, targets, scale):
    error = expected_score(evaluation, scale) - targets
    return float(np.mean(error * error))


def white_exported(split):
    return split.turns * split.exported


def calibrate_scale(split, calibration):
    lower, upper = calibration["bounds"]
    if not 0 <= lower < upper:
        raise ValueError("invalid calibration bounds")

    result = optimize.minimize_scalar(
        lambda scale: mean_squared_error(white_exported(split), split.targets, scale),
        bounds=(lower, upper),
        method="bounded",
        options={
            "xatol": calibration["absolute_tolerance"],
            "maxiter": calibration["maximum_iterations"],
        },
    )
    if not result.success or not math.isfinite(result.fun) or result.x <= 0:
        raise ValueError("logistic-scale calibration failed")
    return result


def feature_support(split, schema, minimum_positions):
    matrix = split.coefficients.tocsc()
    report = []
    for feature in schema["features"]:
        feature_id = feature["id"]
        values = matrix.data[matrix.indptr[feature_id] : matrix.indptr[feature_id + 1]]
        positions = int(values.size)
        report.append(
            {
                "id": feature_id,
                "name": feature["name"],
                "positions": positions,
                "absolute_coefficient": int(np.abs(values).sum()),
                "minimum_coefficient": int(values.min()) if positions else 0,
                "maximum_coefficient": int(values.max()) if positions else 0,
                "status": (
                    "unsupported"
                    if positions == 0
                    else "insufficient"
                    if positions < minimum_positions
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
    blend = np.clip(material, phase_min, phase_max)
    blend = (blend - phase_min) / (phase_max - phase_min)

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
    loss = float(np.mean(error * error))
    sigmoid_derivative = math.log(10) * scale / 400
    score_gradient = (
        2 * error * prediction * (1 - prediction) * sigmoid_derivative / len(split.targets)
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
    candidate["phase_material_max"] = int(
        PHASE_START_COUNTS @ weights[ids, 0]
    )
    return candidate


def exact_evaluations(split, schema, weights):
    candidate_schema = schema_with_weights(schema, weights)
    evaluations = []
    for record in split.records:
        side_value, _ = dataset.reconstruct(candidate_schema, record)
        evaluations.append(side_value if record["turn"] == "w" else -side_value)
    return np.asarray(evaluations, dtype=np.float64)


def exact_metrics(data, weights, scale):
    return {
        name: {
            "positions": len(split.records),
            "mean_squared_error": mean_squared_error(
                exact_evaluations(split, data.schema, weights), split.targets, scale
            ),
        }
        for name, split in data.splits.items()
    }


def candidate_acceptance(policy, metrics):
    validation_improvement = (
        metrics["parent"]["validation"]["mean_squared_error"]
        - metrics["candidate"]["validation"]["mean_squared_error"]
    )
    endgame_regression = (
        metrics["candidate"]["validation_endgame"]["mean_squared_error"]
        - metrics["parent"]["validation_endgame"]["mean_squared_error"]
    )
    return {
        "passed": (
            validation_improvement >= policy["minimum_validation_improvement"]
            and endgame_regression <= policy["maximum_endgame_regression"]
        ),
        "validation_improvement": validation_improvement,
        "endgame_regression": endgame_regression,
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


def matching_names(schema, patterns):
    return {
        feature["name"]
        for feature in schema["features"]
        if any(fnmatch.fnmatchcase(feature["name"], pattern) for pattern in patterns)
    }


def validate_constraints(schema, config):
    feature_names = {feature["name"] for feature in schema["features"]}
    coordinates = {
        coordinate_name(feature_name, phase)
        for feature_name in feature_names
        for phase in range(len(PHASES))
    }
    constraints = config["constraints"]
    if set(constraints) != {"anchors", "fixed", "mirror_files"}:
        raise ValueError("invalid constraints")
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


def effective_support(coefficients, feature_ids):
    if len(feature_ids) == 1:
        return int(coefficients[:, feature_ids[0]].count_nonzero())
    combined = np.asarray(coefficients[:, feature_ids].sum(axis=1)).ravel()
    return int(np.count_nonzero(combined))


def build_parameter_map(schema, parent, coefficients, config, stage):
    validate_constraints(schema, config)
    if set(stage) != {
        "name",
        "features",
        "phases",
        "delta_bounds",
        "bounds",
        "regularization",
        "acceptance",
    }:
        raise ValueError("invalid stage configuration")
    acceptance = stage["acceptance"]
    if set(acceptance) != {
        "minimum_validation_improvement",
        "maximum_endgame_regression",
    } or any(
        not isinstance(value, (int, float)) or not math.isfinite(value) or value < 0
        for value in acceptance.values()
    ):
        raise ValueError("invalid acceptance policy")
    if not stage["features"] or not stage["phases"]:
        raise ValueError("stage selects no parameters")
    if len(stage["features"]) != len(set(stage["features"])):
        raise ValueError("duplicate feature pattern")
    if len(stage["phases"]) != len(set(stage["phases"])) or any(
        phase not in PHASES for phase in stage["phases"]
    ):
        raise ValueError("invalid stage phase")

    for pattern in stage["features"]:
        if not matching_names(schema, [pattern]):
            raise ValueError(f"feature pattern matches nothing: {pattern}")
    selected_names = matching_names(schema, stage["features"])

    features = {feature["name"]: feature for feature in schema["features"]}
    phases = [PHASES.index(phase) for phase in stage["phases"]]
    fixed = set(config["constraints"]["fixed"])
    anchors = set(config["constraints"]["anchors"])
    mirrored = set(config["constraints"]["mirror_files"])
    minimum_support = config["support"]["minimum_positions"]

    for name in selected_names:
        partner = mirrored_psqt_name(name, mirrored)
        if partner not in selected_names:
            raise ValueError(f"selection splits mirror tie: {name}")

    for prefix in {
        ".".join(name.split(".")[:2])
        for name in selected_names
        if name.startswith("psqt.") or name.startswith("mobility.")
    }:
        if not any(
            anchor in selected_names and anchor.startswith(prefix + ".") for anchor in anchors
        ):
            raise ValueError(f"selected family has no anchor: {prefix}")

    groups = {}
    for name in sorted(selected_names):
        feature_id = features[name]["id"]
        tied_name = mirrored_psqt_key(name, mirrored)
        for phase in phases:
            key = (tied_name, phase)
            groups.setdefault(key, []).append((feature_id, phase))

    lower_default, upper_default = stage["delta_bounds"]
    if lower_default > 0 or upper_default < 0:
        raise ValueError("invalid delta bounds")

    selected_coordinates = {
        coordinate_name(name, phase) for name in selected_names for phase in phases
    }
    for override in stage["bounds"]:
        if set(override) != {"pattern", "minimum", "maximum"}:
            raise ValueError("invalid bound")
        if not any(
            fnmatch.fnmatchcase(coordinate, override["pattern"])
            for coordinate in selected_coordinates
        ):
            raise ValueError(f"bound pattern matches nothing: {override['pattern']}")

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

        support_count = effective_support(
            coefficients, sorted({feature_id for feature_id, _ in coordinates})
        )
        variable_name = f"{name}.{PHASES[phase]}"

        if any(value in fixed for value in coordinate_names):
            reason = "fixed"
        elif any(
            schema["features"][feature_id]["name"] in anchors
            for feature_id, _ in coordinates
        ):
            reason = "anchor"
        elif support_count < minimum_support:
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
        for index, override in enumerate(stage["bounds"]):
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
        variable_support[variable_name] = support_count

    if not members:
        raise ValueError("all selected parameters are frozen")
    if len(used_bounds) != len(stage["bounds"]):
        raise ValueError("bound applies only to frozen parameters")
    return ParameterMap(
        parent=parent,
        members=members,
        names=names,
        bounds=bounds,
        support=variable_support,
        frozen=frozen,
    )


def fit_parameters(data, parent, config, stage, scale):
    parameters = build_parameter_map(
        data.schema, parent, data.splits["train"].coefficients, config, stage
    )
    regularization = float(stage["regularization"])
    if regularization < 0:
        raise ValueError("regularization must be nonnegative")

    train = data.splits["train"]
    validation = data.splits["validation"]
    initial = np.zeros(len(parameters.members), dtype=np.float64)
    best = {
        "deltas": initial.copy(),
        "iteration": 0,
        "validation": mean_squared_error(
            continuous_evaluation(validation, data.schema, parent), validation.targets, scale
        ),
    }
    iteration = 0

    def objective(deltas):
        weights = parameters.expand(deltas)
        loss, gradient = continuous_loss_gradient(train, data.schema, weights, scale)
        penalty = regularization * float(deltas @ deltas)
        return loss + penalty, parameters.gradient(gradient) + 2 * regularization * deltas

    def checkpoint(deltas):
        nonlocal iteration
        iteration += 1
        weights = parameters.expand(deltas)
        loss = mean_squared_error(
            continuous_evaluation(validation, data.schema, weights), validation.targets, scale
        )
        if loss < best["validation"]:
            best["validation"] = loss
            best["deltas"] = deltas.copy()
            best["iteration"] = iteration

    options = config["optimizer"]
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
        iteration = result.nit - 1
        checkpoint(result.x)
    if not result.success or not np.isfinite(result.fun):
        raise ValueError(f"optimizer failed: {result.message}")

    deltas = best["deltas"]
    continuous_weights = parameters.expand(deltas)
    metrics = {
        name: mean_squared_error(
            continuous_evaluation(split, data.schema, continuous_weights),
            split.targets,
            scale,
        )
        for name, split in data.splits.items()
    }
    penalty = regularization * float(deltas @ deltas)
    return {
        "parameters": parameters,
        "rounded": parameters.rounded(deltas),
        "deltas": deltas,
        "result": result,
        "selected_iteration": best["iteration"],
        "continuous_metrics": metrics,
        "penalty": penalty,
        "objective": metrics["train"] + penalty,
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
        "version": 1,
        "tune_sha256": sha256_file(pathlib.Path(__file__)),
        "dataset_sha256": sha256_file(pathlib.Path(dataset.__file__)),
    }


def weight_records(schema, parent, candidate):
    return [
        {
            "id": feature["id"],
            "name": feature["name"],
            "parent": {"mg": int(old[0]), "eg": int(old[1])},
            "candidate": {"mg": int(new[0]), "eg": int(new[1])},
        }
        for feature, old, new in zip(schema["features"], parent, candidate)
    ]


def dataset_record(data, config):
    return {
        "manifest_sha256": data.manifest_sha256,
        "schema_version": data.schema["version"],
        "feature_count": len(data.schema["features"]),
        "inputs": data.manifest["inputs"],
        "used_outputs": {
            filename: data.manifest["outputs"][filename] for filename in SPLIT_FILES.values()
        },
        "corpus_policy": config["dataset"]["corpus_policy"],
    }


def write_artifact(path, artifact):
    if path.exists():
        raise ValueError(f"output already exists: {path}")
    artifact["candidate_id"] = artifact_id(artifact)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(artifact, indent=2, sort_keys=True) + "\n")


def read_artifact(path):
    artifact = load_json(path)
    if artifact.get("candidate_id") != artifact_id(artifact):
        raise ValueError("parent candidate ID mismatch")
    return artifact


def validate_parent_artifact(artifact, data, config):
    if artifact.get("artifact_version") != 1 or artifact.get("kind") not in {
        "calibration",
        "candidate",
    }:
        raise ValueError("unsupported parent candidate")
    if artifact.get("dataset") != dataset_record(data, config):
        raise ValueError("parent candidate uses a different dataset")
    if artifact.get("configuration") != config:
        raise ValueError("parent candidate uses a different configuration")

    objective = artifact.get("objective", {})
    scale = objective.get("scale")
    fixed_objective = {key: value for key, value in objective.items() if key != "scale"}
    if (
        fixed_objective != config["objective"]
        or not isinstance(scale, (int, float))
        or not math.isfinite(scale)
        or scale <= 0
    ):
        raise ValueError("parent candidate uses a different objective")


def artifact_weights(artifact, schema):
    records = artifact.get("weights", [])
    if len(records) != len(schema["features"]):
        raise ValueError("parent weight count mismatch")
    weights = np.empty((len(records), 2), dtype=np.float64)
    for feature, record in zip(schema["features"], records):
        if record["id"] != feature["id"] or record["name"] != feature["name"]:
            raise ValueError("parent feature schema mismatch")
        weights[feature["id"]] = [record["candidate"]["mg"], record["candidate"]["eg"]]
    if not np.array_equal(weights, np.rint(weights)):
        raise ValueError("parent weights must be integers")
    return weights


def calibration_artifact(data, config, engine, scale_result, support):
    weights = baseline_weights(data.schema).astype(np.int64)
    fitted_scale = float(scale_result.x)
    objective = {**config["objective"], "scale": fitted_scale}

    artifact = {
        "artifact_version": 1,
        "kind": "calibration",
        "dataset": dataset_record(data, config),
        "configuration": config,
        "parent": {
            "type": "source",
            "revision": config["dataset"]["starting_revision"],
            "engine_sha256": sha256_file(engine),
        },
        "objective": objective,
        "tool": tool_record(),
        "dependencies": dependency_versions(),
        "support": support,
        "optimizer": {
            "method": "bounded_scalar",
            "success": bool(scale_result.success),
            "iterations": int(scale_result.nit),
            "function_evaluations": int(scale_result.nfev),
            "message": str(scale_result.message),
            "fitted_scale": fitted_scale,
        },
        "metrics": exact_metrics(data, weights, fitted_scale),
        "weights": weight_records(data.schema, weights, weights),
    }
    return artifact


def fit_artifact(data, config, stage, parent_path, parent_artifact, result, support):
    parent = result["parameters"].parent.astype(np.int64)
    candidate = result["rounded"]
    optimizer = result["result"]
    metrics = {
        "parent": exact_metrics(data, parent, parent_artifact["objective"]["scale"]),
        "candidate": exact_metrics(
            data, candidate, parent_artifact["objective"]["scale"]
        ),
    }
    artifact = {
        "artifact_version": 1,
        "kind": "candidate",
        "stage": stage["name"],
        "dataset": dataset_record(data, config),
        "configuration": config,
        "stage_configuration": stage,
        "parent": {
            "type": "candidate",
            "candidate_id": parent_artifact["candidate_id"],
            "sha256": sha256_file(parent_path),
        },
        "objective": parent_artifact["objective"],
        "tool": tool_record(),
        "dependencies": dependency_versions(),
        "support": support,
        "constraints": {
            "variables": result["parameters"].names,
            "variable_support": result["parameters"].support,
            "frozen": result["parameters"].frozen,
            "bounds": result["parameters"].bounds,
            "regularization": stage["regularization"],
        },
        "optimizer": {
            "method": config["optimizer"]["method"],
            "success": bool(optimizer.success),
            "status": int(optimizer.status),
            "message": str(optimizer.message),
            "iterations": int(optimizer.nit),
            "function_evaluations": int(optimizer.nfev),
            "gradient_evaluations": int(optimizer.njev),
            "final_objective": float(optimizer.fun),
        },
        "continuous": {
            "selected_iteration": result["selected_iteration"],
            "metrics": result["continuous_metrics"],
            "regularization_penalty": result["penalty"],
            "objective": result["objective"],
            "deltas": {
                name: float(delta)
                for name, delta in zip(result["parameters"].names, result["deltas"])
            },
        },
        "metrics": metrics,
        "acceptance": candidate_acceptance(stage["acceptance"], metrics),
        "weights": weight_records(data.schema, parent, candidate),
    }
    return artifact


def calibrate_command(args):
    config = load_config(args.config)
    data = load_dataset(args.dataset, config, args.engine)
    validate_constraints(data.schema, config)
    support = feature_support(
        data.splits["train"], data.schema, config["support"]["minimum_positions"]
    )
    result = calibrate_scale(data.splits["train"], config["calibration"])
    artifact = calibration_artifact(data, config, args.engine, result, support)
    write_artifact(args.output, artifact)
    print(f"scale {result.x:.10f}")
    print(f"train {artifact['metrics']['train']['mean_squared_error']:.10f}")
    print(f"validation {artifact['metrics']['validation']['mean_squared_error']:.10f}")
    print(
        "validation-endgame "
        f"{artifact['metrics']['validation_endgame']['mean_squared_error']:.10f}"
    )
    print(f"candidate {artifact['candidate_id']}")


def fit_command(args):
    config = load_config(args.config)
    stage = load_json(args.stage)
    data = load_dataset(args.dataset, config)
    parent_artifact = read_artifact(args.parent)
    validate_parent_artifact(parent_artifact, data, config)
    parent = artifact_weights(parent_artifact, data.schema)
    support = feature_support(
        data.splits["train"], data.schema, config["support"]["minimum_positions"]
    )
    result = fit_parameters(
        data, parent, config, stage, parent_artifact["objective"]["scale"]
    )
    artifact = fit_artifact(data, config, stage, args.parent, parent_artifact, result, support)
    write_artifact(args.output, artifact)
    print(f"stage {stage['name']}")
    print(
        "train "
        f"{artifact['metrics']['parent']['train']['mean_squared_error']:.10f} -> "
        f"{artifact['metrics']['candidate']['train']['mean_squared_error']:.10f}"
    )
    print(
        "validation "
        f"{artifact['metrics']['parent']['validation']['mean_squared_error']:.10f} -> "
        f"{artifact['metrics']['candidate']['validation']['mean_squared_error']:.10f}"
    )
    print(
        "validation-endgame "
        f"{artifact['metrics']['parent']['validation_endgame']['mean_squared_error']:.10f} -> "
        f"{artifact['metrics']['candidate']['validation_endgame']['mean_squared_error']:.10f}"
    )
    print(f"accepted {str(artifact['acceptance']['passed']).lower()}")
    print(f"candidate {artifact['candidate_id']}")


def parse_args():
    parser = argparse.ArgumentParser(description="Calibrate and tune Latrunculi HCE features")
    subparsers = parser.add_subparsers(dest="command", required=True)

    calibrate = subparsers.add_parser("calibrate", help="fit the fixed Texel scale")
    calibrate.add_argument("--config", type=pathlib.Path, required=True)
    calibrate.add_argument("--dataset", type=pathlib.Path, required=True)
    calibrate.add_argument("--engine", type=pathlib.Path, required=True)
    calibrate.add_argument("--output", type=pathlib.Path, required=True)
    calibrate.set_defaults(function=calibrate_command)

    fit = subparsers.add_parser("fit", help="fit a selected feature stage")
    fit.add_argument("--config", type=pathlib.Path, required=True)
    fit.add_argument("--stage", type=pathlib.Path, required=True)
    fit.add_argument("--dataset", type=pathlib.Path, required=True)
    fit.add_argument("--parent", type=pathlib.Path, required=True)
    fit.add_argument("--output", type=pathlib.Path, required=True)
    fit.set_defaults(function=fit_command)

    return parser.parse_args()


def main():
    try:
        args = parse_args()
        args.function(args)
        return 0
    except (OSError, ValueError, KeyError) as error:
        print(f"tune: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
