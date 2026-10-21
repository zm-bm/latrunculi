"""Tuning checkpoints, provenance, and retained result reports."""

import json
import math
import pathlib
import platform

import chess
import numpy as np
import scipy

from . import dataset, model, fit as fitting


FORMAT_VERSION = 5


def artifact_id(artifact):
    payload = {key: value for key, value in artifact.items() if key != "artifact_id"}
    return dataset.sha256_json(payload)


def load_json(path):
    return json.loads(path.read_text())


def atomic_write_json(path, value):
    text = json.dumps(value, indent=2, sort_keys=True) + "\n"
    path.parent.mkdir(parents=True, exist_ok=True)
    partial = path.with_name(path.name + ".partial")
    partial.write_text(text)
    partial.replace(path)


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
        raise ValueError("unsupported tuning format; prepare and fit into new output directories")
    if artifact.get("artifact_id") != artifact_id(artifact):
        raise ValueError(f"artifact hash mismatch: {path}")
    for key, value in (expected or {}).items():
        if key not in artifact or dataset.canonical_json(artifact[key]) != dataset.canonical_json(value):
            raise ValueError(f"artifact context mismatch ({key}): {path}")
    return artifact


def checkpoint(path, context, compute):
    if not path.exists():
        write_artifact(path, {**compute(), **context})
    return read_artifact(path, context)


def dependency_versions():
    return {
        "python": platform.python_version(),
        "chess": chess.__version__,
        "numpy": np.__version__,
        "scipy": scipy.__version__,
    }


def tool_record():
    tools = pathlib.Path(__file__).resolve().parents[1]
    sources = (
        "__init__.py", "paths.py", "tuning/__init__.py", "tuning/dataset.py", "tuning/model.py",
        "tuning/fit.py", "tuning/artifacts.py", "tuning/tune.py",
    )
    return {name: dataset.sha256_file(tools / name) for name in sources}


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
    weights = model.baseline_weights(data.schema).astype(np.int64)
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
        "metrics": model.split_metric(split, data.schema, weights, scale),
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
            "metrics": model.split_metric(split, data.schema, rounded, scale),
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
        if any(type(record["candidate"][phase]) is not int for phase in model.PHASES):
            raise ValueError("candidate weights must be integers")
        weights[feature["id"]] = [record["candidate"]["mg"], record["candidate"]["eg"]]
    return weights


def cross_validation_artifact(
    data, policy, assignments, calibrations, fits, parameters
):
    parent = model.baseline_weights(data.schema).astype(np.int64)
    testing_splits = [
        model.subset_split(data.development, assignments == fold)
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
            parent_eval = model.exact_evaluations(testing, data.schema, parent)
            candidate_eval = model.exact_evaluations(testing, data.schema, candidate)
            improvement = float(
                fitting.group_improvements(
                    testing, parent_eval, candidate_eval, scale
                ).mean()
            )
            fold_reports.append(
                {
                    "fold": fold,
                    "groups": int(np.unique(testing.groups).size),
                    "positions": len(testing.targets),
                    "baseline_loss": model.mean_squared_error(
                        parent_eval, testing.targets, scale, testing.weights
                    ),
                    "candidate_loss": model.mean_squared_error(
                        candidate_eval, testing.targets, scale, testing.weights
                    ),
                    "improvement": improvement,
                }
            )
            comparisons.append(
                (testing, data.schema, parent, candidate, scale)
            )

        improvements = np.asarray(
            [report["improvement"] for report in fold_reports], dtype=np.float64
        )
        validation = fitting.comparison_report(
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

    best, threshold, selected = fitting.select_regularization(reports)

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
            phase_index = model.PHASES.index(phase)
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
    parent = model.baseline_weights(data.schema).astype(np.int64)
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
                "baseline": model.split_metric(
                    data.development, data.schema, parent, scale
                ),
                "candidate": model.split_metric(
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


def fit_descriptor(data):
    return {
        "format_version": FORMAT_VERSION,
        "kind": "run",
        "dataset": {
            "manifest_sha256": data.manifest_sha256,
            "data_sha256": data.data_sha256,
        },
        "schema": data.schema,
        "policy": fitting.FIT_POLICY,
        "tool": tool_record(),
        "dependencies": dependency_versions(),
    }


def fit_filename(regularization):
    return f"lambda-{regularization:.0e}.json"
