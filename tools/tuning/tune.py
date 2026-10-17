"""Prepare data, fit weights, and verify compiled Latrunculi evaluation."""

import argparse
import copy
import hashlib
import json
import pathlib
import subprocess
import sys

from . import dataset, model, fit as fitting, artifacts


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


def candidate_verification(candidate, schema, engine):
    engine_hash = dataset.sha256_file(engine)
    actual_schema = read_engine_schema(engine)
    if dataset.sha256_file(engine) != engine_hash:
        raise ValueError("candidate engine changed during verification")
    expected_schema = model.schema_with_weights(schema, artifacts.weights_from_artifact(candidate, schema))
    if actual_schema != expected_schema:
        raise ValueError("compiled engine does not match the candidate weights")
    return {
        "candidate_id": candidate["artifact_id"],
        "engine_sha256": engine_hash,
        "schema_sha256": hashlib.sha256(dataset.canonical_json(actual_schema).encode()).hexdigest(),
    }


def prepare_command(args):
    engine = args.engine.resolve()
    paths = [path.resolve() for path in args.pgn]
    output = args.output.resolve()
    identity = dataset.preparation_identity(engine, paths)
    schema = read_engine_schema(engine)
    dataset.validate_schema(schema)
    if not output.exists():
        dataset.atomic_build(engine, paths, output)
    report, actual_schema = dataset.validate_output(output)
    manifest = artifacts.load_json(output / "manifest.json")
    if manifest["identity"] != identity:
        raise ValueError("preparation inputs, policy, or tools changed; use a new output directory")
    if actual_schema != schema or dataset.preparation_identity(engine, paths) != identity:
        raise ValueError("preparation inputs or exporter changed")
    print(json.dumps(report, indent=2, sort_keys=True))


def fit_command(args):
    dataset_path = args.dataset.resolve()
    output = args.output.resolve()
    data = model.load_dataset(dataset_path)
    policy = copy.deepcopy(fitting.FIT_POLICY)
    fitting.validate_constraints(data.schema, policy)
    descriptor = artifacts.fit_descriptor(data)
    run_path = output / "run.json"
    if not run_path.exists() and output.exists():
        if any(path.name != "run.json.partial" for path in output.iterdir()):
            raise ValueError("output is not a version-5 fit; prepare and fit into new output directories")
    run = artifacts.checkpoint(run_path, descriptor, lambda: descriptor)
    common = {"format_version": artifacts.FORMAT_VERSION, "run_id": run["artifact_id"]}
    assignments = fitting.fold_assignments(data.development, policy["validation"])
    parent = model.baseline_weights(data.schema)
    parameters = fitting.build_parameter_map(
        data.schema, parent, data.development, policy, assignments
    )

    def calibration_for(split, path, fold):
        def compute():
            support = (
                fitting.feature_support(split, data.schema, policy["support"]["minimum_groups"])
                if fold is None else None
            )
            result = fitting.calibrate_scale(split, policy["calibration"])
            return artifacts.calibration_artifact(data, split, policy, result, support, fold)

        return artifacts.checkpoint(
            path, {**common, "kind": "calibration", "fold": fold}, compute
        )

    def fit_for(split, path, fold, regularization, calibration):
        scale = calibration["objective"]["scale"]

        def compute():
            result = fitting.fit_parameters(
                split, data.schema, parameters, policy, scale, regularization
            )
            return artifacts.fit_artifact(data, split, policy, result, regularization, scale, fold)

        return artifacts.checkpoint(
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
        training = model.subset_split(data.development, assignments != fold)
        directory = output / "cross-validation" / f"fold-{fold}"
        calibration = calibration_for(training, directory / "calibration.json", fold)
        calibrations.append(calibration)
        for regularization in policy["fit"]["regularization"]:
            fits[regularization].append(
                fit_for(training, directory / artifacts.fit_filename(regularization),
                        fold, regularization, calibration)
            )
    del training

    cross_validation = artifacts.checkpoint(
        output / "cross-validation.json",
        {
            **common, "kind": "cross_validation",
            "calibration_ids": [item["artifact_id"] for item in calibrations],
            "fit_ids": {
                artifacts.fit_filename(regularization): [item["artifact_id"] for item in records]
                for regularization, records in fits.items()
            },
        },
        lambda: artifacts.cross_validation_artifact(
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
    candidate = artifacts.checkpoint(
        output / "candidate.json",
        {
            **common, "kind": "candidate",
            "cross_validation_id": cross_validation["artifact_id"],
            "fit_id": final_fit["artifact_id"] if final_fit else None,
            "calibration_id": calibration["artifact_id"] if calibration else None,
        },
        lambda: artifacts.candidate_artifact(data, policy, cross_validation, calibration, final_fit),
    )
    if (dataset.sha256_file(dataset_path / "manifest.json"),
        dataset.sha256_file(dataset_path / dataset.DATA_FILE)) != (data.manifest_sha256, data.data_sha256):
        raise ValueError("prepared dataset changed during fitting")
    artifacts.read_artifact(run_path, artifacts.fit_descriptor(data))
    print(json.dumps({
        "run_id": run["artifact_id"],
        "candidate_id": candidate["artifact_id"],
        "cross_validation_supported": candidate["cross_validation_supported"],
        "changes": len(candidate["changes"]),
    }, indent=2, sort_keys=True))


def verify_command(args):
    output = args.output.resolve()
    if not (output / "run.json").is_file():
        raise ValueError("no version-5 fit; prepare and fit into new output directories")
    run = artifacts.read_artifact(output / "run.json", {"kind": "run"})
    dataset.validate_schema(run["schema"])
    candidate = artifacts.read_artifact(output / "candidate.json", {
        "kind": "candidate", "run_id": run["artifact_id"],
    })
    if not candidate["cross_validation_supported"]:
        raise ValueError("cross-validation did not support a candidate")
    verification = candidate_verification(candidate, run["schema"], args.engine.resolve())
    print(json.dumps({"format_version": artifacts.FORMAT_VERSION, **verification}, indent=2, sort_keys=True))


def parse_args():
    parser = argparse.ArgumentParser(prog="tune", description=__doc__)
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
