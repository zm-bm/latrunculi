import copy
import contextlib
import io
import json
import types
from unittest import mock

import numpy as np
from scipy import sparse

from . import dataset, model, fit as fitting, artifacts


def schema(features):
    return {
        "type": "schema",
        "version": dataset.PREPARATION_POLICY["schema_version"],
        "perspective": {
            "coefficients": "white",
            "fixed": "white",
            "eval": "side_to_move",
        },
        "result": "1=white_win,0=draw,-1=black_win",
        "phase_counts": ["knight", "bishop", "rook", "queen"],
        "pawn_counts": ["white", "black"],
        "phase_limit": 128,
        "phase_material_min": 0,
        "phase_material_max": 13000,
        "scale_limit": 64,
        "scale_base": 48,
        "scale_per_pawn": 4,
        "tempo": 20,
        "features": [
            {"id": feature_id, "name": name, "mg": mg, "eg": eg}
            for feature_id, (name, mg, eg) in enumerate(features)
        ],
    }


def base_schema(extra=()):
    return schema(
        [
            ("material.pawn", 100, 166),
            ("material.knight", 600, 680),
            ("material.bishop", 650, 740),
            ("material.rook", 1000, 1100),
            ("material.queen", 2000, 2150),
            *extra,
        ]
    )


def record(
    coefficients,
    *,
    source="group:game:1",
    result=0,
    fixed=(0, 0),
    phase_counts=(2, 2, 2, 1),
    pawns=(4, 4),
    turn="w",
):
    return {
        "type": "position",
        "version": dataset.PREPARATION_POLICY["schema_version"],
        "source": source,
        "result": result,
        "fen": "",
        "turn": turn,
        "phase_counts": list(phase_counts),
        "pawn_counts": list(pawns),
        "fixed": list(fixed),
        "coefficients": list(coefficients),
        "eval": 0,
    }


def make_split(records, feature_count):
    rows = []
    columns = []
    values = []
    for row, item in enumerate(records):
        for feature_id, coefficient in item["coefficients"]:
            rows.append(row)
            columns.append(feature_id)
            values.append(coefficient)
    names = np.asarray(
        [dataset.group_from_source(item["source"]) for item in records], dtype=object
    )
    group_names, groups = np.unique(names, return_inverse=True)
    groups = groups.astype(np.int32)
    return model.Split(
        coefficients=sparse.csr_matrix(
            (values, (rows, columns)),
            shape=(len(records), feature_count),
            dtype=np.int32,
        ),
        fixed=np.asarray([item["fixed"] for item in records], dtype=np.int32).reshape((-1, 2)),
        phase_counts=np.asarray(
            [item["phase_counts"] for item in records], dtype=np.int16
        ).reshape((-1, 4)),
        pawn_counts=np.asarray(
            [item["pawn_counts"] for item in records], dtype=np.int8
        ).reshape((-1, 2)),
        turns=np.asarray(
            [1 if item["turn"] == "w" else -1 for item in records], dtype=np.int8
        ),
        targets=np.asarray([(item["result"] + 1) / 2 for item in records]),
        exported=np.asarray([item["eval"] for item in records], dtype=np.int32),
        groups=groups,
        group_names=group_names,
        weights=model.group_weights(groups),
    )


def experiment(*, anchors=(), fixed=("material.pawn.mg",), mirrored=(), support=1):
    return {
        "support": {"minimum_groups": support},
        "constraints": {
            "fixed": list(fixed),
            "anchors": list(anchors),
            "mirror_files": list(mirrored),
        },
        "fit": {
            "delta_bounds": [-100, 100],
            "bounds": [],
            "regularization": [1e-9],
        },
        "optimizer": {
            "method": "L-BFGS-B",
            "maximum_iterations": 200,
            "gradient_tolerance": 1e-12,
            "function_tolerance": 1e-15,
            "maximum_line_search_steps": 50,
        },
        "validation": {"folds": 5},
    }


def fitting_schema():
    current = base_schema([
        ("pawn.isolated", 0, 0), ("pawn.backward", 0, 0),
        ("pawn.connected_link", 0, 0),
    ])
    names = {feature["name"] for feature in current["features"]}
    constraints = fitting.FIT_POLICY["constraints"]
    for anchor in constraints["anchors"]:
        for name in (anchor, fitting.mirrored_psqt_name(anchor, set(constraints["mirror_files"]))):
            if name not in names:
                current["features"].append({
                    "id": len(current["features"]), "name": name, "mg": 0, "eg": 0,
                })
                names.add(name)
    return current


def write_prepared_dataset(output, current, engine, paths, policy=None):
    policy = copy.deepcopy(dataset.PREPARATION_POLICY if policy is None else policy)
    policy.update(minimum_games=1, minimum_groups=1)
    output.mkdir(parents=True)
    records = []
    for index in range(32):
        board = dataset.chess.Board.empty()
        board.set_piece_at(index, dataset.chess.Piece.from_symbol("K"))
        board.set_piece_at(63, dataset.chess.Piece.from_symbol("k"))
        item = record(
            [[0, 1], [1, 1], [2, -1], [3, 1], [4, -1], [5, 1]]
            + ([[6, 1]] if index < 3 else []),
            source=f"g{index}:game:8", result=1, phase_counts=(0, 0, 0, 0), pawns=(0, 0),
        )
        item["fen"] = board.fen()
        item["eval"] = dataset.reconstruct(current, item)[0]
        records.append(item)
    data = output / dataset.DATA_FILE
    data.write_text("\n".join(json.dumps(item) for item in (current, *records)) + "\n")
    report, _ = dataset.validate_dataset(output, policy)
    manifest = {
        "format_version": dataset.FORMAT_VERSION,
        "identity": dataset.preparation_identity(engine, paths, policy),
        "collection": {"games.read": 32, "games.valid": 32, "groups.read": 32, "positions.sampled": 32},
        "deduplication": {"positions.exported": 32, "positions.retained": 32},
        "source_results": {"games.1-0": 32, "sampled.1-0": 32},
        "validation": report,
        "outputs": {data.name: dataset.sha256_file(data)},
    }
    artifacts.atomic_write_json(output / "manifest.json", manifest)
    return manifest


def prepared_fixture(root):
    engine = root / "exporter"
    pgn = root / "games.pgn"
    engine.write_text("exporter binary")
    pgn.write_text("original games")
    output = root / "dataset"
    current = fitting_schema()
    write_prepared_dataset(output, current, engine, [pgn])
    return output, current, engine, pgn


@contextlib.contextmanager
def mocked_optimizer(*, improve=True, interrupt_at=None):
    policy = copy.deepcopy(fitting.FIT_POLICY)
    policy["support"]["minimum_groups"] = 3
    policy["validation"]["bootstrap_samples"] = 20
    calls = 0

    def fit(split, current, parameters, fit_policy, scale, regularization):
        nonlocal calls
        calls += 1
        if calls == interrupt_at:
            raise RuntimeError("interrupted fit")
        deltas = np.asarray([
            10.0 if improve and name.startswith("pawn.isolated.") else 0.0
            for name in parameters.names
        ])
        continuous_loss = model.mean_squared_error(
            model.continuous_evaluation(split, current, parameters.expand(deltas)),
            split.targets, scale, split.weights,
        )
        penalty = regularization * float(parameters.multiplicity @ (deltas * deltas))
        rounded = parameters.rounded(deltas)
        exact_objective = model.split_metric(split, current, rounded, scale)["mean_squared_error"] + penalty
        return {
            "parameters": parameters, "rounded": rounded, "deltas": deltas,
            "optimizer": types.SimpleNamespace(nit=1, nfev=1, njev=1, fun=continuous_loss + penalty),
            "selected_iteration": 1, "continuous_loss": continuous_loss,
            "penalty": penalty, "objective": continuous_loss + penalty,
            "exact_objective": exact_objective,
        }

    with (
        mock.patch.object(fitting, "FIT_POLICY", policy),
        mock.patch.object(
            fitting, "calibrate_scale", return_value=types.SimpleNamespace(x=0.7, nit=1, nfev=2),
        ) as calibrate,
        mock.patch.object(fitting, "fit_parameters", side_effect=fit) as optimizer,
        contextlib.redirect_stdout(io.StringIO()),
    ):
        yield optimizer, calibrate


def verification_fixture(root, *, supported=True):
    output = root / "fit"
    current = fitting_schema()
    run = artifacts.write_artifact(output / "run.json", {
        "kind": "run", "dataset": {"manifest_sha256": "manifest", "data_sha256": "data"},
        "schema": current, "policy": fitting.FIT_POLICY, "tool": {}, "dependencies": {},
    })
    parent = model.baseline_weights(current).astype(np.int64)
    weights = parent.copy()
    weights[5] += 10
    candidate = artifacts.write_artifact(output / "candidate.json", {
        "kind": "candidate", "run_id": run["artifact_id"],
        "cross_validation_supported": supported,
        "weights": artifacts.weight_records(current, parent, weights),
    })
    return output, model.schema_with_weights(current, weights), candidate
