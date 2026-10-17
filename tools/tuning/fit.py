"""Tuning policy, constrained optimization, and grouped validation."""

import fnmatch
import hashlib
import math
from dataclasses import dataclass

import numpy as np
from scipy import optimize

from . import model


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
            rounded = model.round_away_from_zero(value)
            rounded_deltas.append(min(max(rounded, math.ceil(lower)), math.floor(upper)))
        return np.asarray(rounded_deltas, dtype=np.int64)

    def rounded(self, deltas):
        return self.expand(self.rounded_deltas(deltas)).astype(np.int64)


def calibrate_scale(split, calibration):
    lower, upper = calibration["bounds"]
    result = optimize.minimize_scalar(
        lambda scale: model.mean_squared_error(
            model.white_exported(split), split.targets, scale, split.weights
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


def coordinate_name(feature_name, phase):
    return f"{feature_name}.{model.PHASES[phase]}"


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
        for phase in range(len(model.PHASES))
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
    if not isinstance(split, model.Split) or not len(split.targets):
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
        for phase in range(len(model.PHASES)):
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
            raise ValueError(f"tied parameters differ: {name}.{model.PHASES[phase]}")

        support = group_support({feature_id for feature_id, _ in coordinates})
        variable_name = f"{name}.{model.PHASES[phase]}"
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
        "objective": model.split_metric(split, schema, parent_integer, scale)[
            "mean_squared_error"
        ],
    }
    iteration = 0

    def objective(deltas):
        weights = parameters.expand(deltas)
        loss, gradient = model.continuous_loss_gradient(split, schema, weights, scale)
        penalty = regularization * float(parameters.multiplicity @ (deltas * deltas))
        penalty_gradient = 2 * regularization * parameters.multiplicity * deltas
        return loss + penalty, parameters.gradient(gradient) + penalty_gradient

    def checkpoint(deltas):
        nonlocal iteration
        iteration += 1
        rounded_deltas = parameters.rounded_deltas(deltas)
        rounded = parameters.expand(rounded_deltas).astype(np.int64)
        exact_objective = model.split_metric(split, schema, rounded, scale)[
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
    continuous_loss = model.mean_squared_error(
        model.continuous_evaluation(split, schema, continuous_weights),
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
    parent_error = model.expected_score(parent_eval[mask], scale) - split.targets[mask]
    candidate_error = model.expected_score(candidate_eval[mask], scale) - split.targets[mask]
    improvements = parent_error * parent_error - candidate_error * candidate_error
    _, groups = np.unique(split.groups[mask], return_inverse=True)
    totals = np.bincount(groups, weights=improvements)
    return totals / np.bincount(groups)


def group_errors(split, evaluation, scale, mask=None):
    if mask is None:
        mask = np.ones(len(split.targets), dtype=bool)
    errors = model.expected_score(evaluation[mask], scale) - split.targets[mask]
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
        parent_eval, phases = model.exact_evaluations_and_phases(split, schema, parent)
        candidate_eval = model.exact_evaluations(split, schema, candidate)
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
