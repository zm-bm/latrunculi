"""SPSA preparation and form workflow for the pinned OpenBench server."""

import hashlib
import json
import math
from pathlib import Path
import re
import subprocess

from openbench_client import cli

from . import engine

SCHEDULE = ("spsa_reporting_type", "spsa_distribution_type", "spsa_alpha", "spsa_gamma",
            "spsa_A_ratio", "spsa_iterations", "spsa_pairs_per", "spsa_inputs")
OMITTED = {"test_bounds", "test_confidence", "test_max_games", "workload_size"}
FIELDS = (cli.SETTING_FIELDS - OMITTED - {f"base_{key}" for key in cli.SIDE_FIELDS}) | set(SCHEDULE)


def parameters(text):
    if not isinstance(text, str):
        raise cli.OpenBenchError("SPSA parameters must be text")
    rows = []
    names = set()
    for line in text.strip().splitlines():
        fields = [x.strip() for x in line.split(",")]
        if len(fields) != 7:
            raise cli.OpenBenchError("SPSA parameters require seven comma-separated fields")
        name, kind, *numbers = fields
        try:
            start, low, high, c_end, r_end = map(float, numbers)
        except ValueError:
            raise cli.OpenBenchError("Invalid SPSA number") from None
        if (not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", name) or name.lower() in names
                or kind not in ("int", "float") or not all(map(math.isfinite, (start, low, high, c_end, r_end)))
                or not low <= start <= high or low >= high or c_end <= 0 or r_end <= 0
                or (kind == "int" and (c_end < .5 or any(x != int(x) for x in (start, low, high))))):
            raise cli.OpenBenchError(f"Invalid SPSA parameter: {name}")
        names.add(name.lower())
        rows.append((name, kind, start, low, high, c_end, r_end))
    if not rows:
        raise cli.OpenBenchError("SPSA parameters are empty")
    return rows


def parameter_text(rows):
    return "\n".join(", ".join(str(x) for x in row) for row in rows)


def normalize(settings):
    if not isinstance(settings, dict) or set(settings) != FIELDS or settings.get("test_mode") != "SPSA":
        raise cli.OpenBenchError("Invalid or incomplete SPSA settings fields")
    result = dict(settings)
    for key in ("spsa_iterations", "spsa_pairs_per"):
        if not re.fullmatch(r"[1-9][0-9]*", str(result[key])):
            raise cli.OpenBenchError(f"Invalid setting: {key}")
        result[key] = str(int(result[key]))
    for key in ("spsa_alpha", "spsa_gamma", "spsa_A_ratio"):
        try:
            value = float(result[key])
        except (ValueError, TypeError):
            raise cli.OpenBenchError(f"Invalid setting: {key}") from None
        if not math.isfinite(value) or value <= 0:
            raise cli.OpenBenchError(f"Invalid setting: {key}")
        result[key] = str(value)
    for key, choices in (("spsa_reporting_type", ("BATCHED", "BULK")),
                         ("spsa_distribution_type", ("SINGLE", "MULTIPLE"))):
        if result[key] not in choices:
            raise cli.OpenBenchError(f"Invalid setting: {key}")
    result["spsa_inputs"] = parameter_text(parameters(result["spsa_inputs"]))
    # Reuse the client's validation of revisions, options, books, and adjudication.
    common = {k: v for k, v in result.items() if k not in SCHEDULE}
    common.update({f"base_{k}": result[f"dev_{k}"] for k in cli.SIDE_FIELDS})
    common.update(test_mode="GAMES", test_bounds="N/A", test_confidence="N/A",
                  test_max_games=2 * int(result["spsa_iterations"]) * int(result["spsa_pairs_per"]),
                  workload_size=result["spsa_pairs_per"])
    common = cli.normalize_settings(common)
    result.update({k: v for k, v in common.items() if k in FIELDS and k != "test_mode"})
    option_names = {x.split("=", 1)[0].lower() for x in common["dev_options"].split() if "=" in x}
    if option_names & {row[0].lower() for row in parameters(result["spsa_inputs"])}:
        raise cli.OpenBenchError("Tuned parameters must not also appear in dev_options")
    return result


def prepare(args, revision, write_settings):
    output = args.output.absolute()
    if output.exists() or output.is_symlink():
        raise ValueError(f"output already exists: {output}")
    if args.games % 8:
        raise ValueError("SPSA games must be divisible by 8 (four color pairs per point)")
    sha = revision(args.candidate)
    binary = engine.executable(args.candidate_engine)
    identity = hashlib.sha256(binary.read_bytes()).hexdigest()
    exported = subprocess.run([str(binary), "spsa", "--group", args.group],
                              capture_output=True, text=True, check=True, timeout=args.timeout)
    rows = parameters(exported.stdout)
    discovery = subprocess.run([str(binary)], input="uci\nquit\n", capture_output=True,
                               text=True, check=True, timeout=args.timeout)
    for name, kind, initial, low, high, *_ in rows:
        pattern = (rf"^option name {re.escape(name)} type spin default (\S+) min (\S+) max (\S+)$"
                   if kind == "int" else rf"^option name {re.escape(name)} type string default (\S+)$")
        matches = re.findall(pattern, discovery.stdout, re.MULTILINE)
        expected = (initial, low, high) if kind == "int" else (initial,)
        found = tuple(map(float, matches[0] if kind == "int" else [matches[0]])) if len(matches) == 1 else ()
        if found != expected:
            raise ValueError(f"SPSA export disagrees with UCI discovery: {name}")
    output.mkdir(parents=True, exist_ok=False)
    (output / "parameters.txt").write_text(parameter_text(rows))
    (output / "uci.txt").write_text(discovery.stdout)
    bench = engine.bench_nodes(binary, output / "candidate-bench.txt", timeout=args.timeout)
    if hashlib.sha256(binary.read_bytes()).hexdigest() != identity:
        raise ValueError("Candidate binary changed during preparation")
    settings = json.loads((Path(__file__).parent / "resources/openbench/spsa.json").read_text())
    settings.update(dev_branch=sha, dev_bench=bench, info=args.description,
                    spsa_inputs=parameter_text(rows), spsa_iterations=args.games // 8)
    settings = normalize(settings)
    write_settings(output / "candidate.json", {"revision": sha, "binary": str(binary),
                                               "sha256": identity, "bench": bench, "group": args.group})
    write_settings(output / "settings.json", settings)
    print(f"Settings: {output / 'settings.json'}", flush=True)
    return output / "settings.json"


def api_text(config, workload_id, kind):
    with cli.api_request(config, f"/api/spsa/{workload_id}/{kind}/") as response:
        text = response.read().decode("utf-8")
    if text.lstrip().startswith(("{", "<")):
        raise cli.OpenBenchError("Unexpected SPSA API response")
    return text


def schedule_from_page(page):
    labels = {"Alpha": "spsa_alpha", "Gamma": "spsa_gamma", "A-Ratio": "spsa_A_ratio",
              "Reporting": "spsa_reporting_type", "Distribution": "spsa_distribution_type",
              "Pairs Per Point": "spsa_pairs_per"}
    values = {}
    for row in page.root.all("tr"):
        cells = [" ".join(cell.text().split()) for cell in row.all("td")]
        if len(cells) == 2 and cells[0] in labels:
            values[labels[cells[0]]] = cells[1]
    match = re.search(r"\d+/(\d+) Iterations", " ".join(page.root.text().split()))
    if len(values) != len(labels) or not match:
        raise cli.OpenBenchError("Incomplete SPSA schedule readback")
    values["spsa_iterations"] = match[1]
    return values


def readback(config, session, workload_id, response=None):
    response = response or cli.workload_status(config, workload_id)
    info = response["info"]
    if info.get("test_mode") != "SPSA":
        raise cli.OpenBenchError("Workload is not an SPSA tune")
    schedule = schedule_from_page(session.fetch(f"/tune/{workload_id}/"))
    budget = 2 * int(schedule["spsa_iterations"]) * int(schedule["spsa_pairs_per"])
    # A tune must use the identical build and fixed options on both sides.
    common = cli.readback_settings(dict(info, test_mode="GAMES", max_games=budget))
    if any(common[f"dev_{key}"] != common[f"base_{key}"] for key in cli.SIDE_FIELDS):
        raise cli.OpenBenchError("SPSA engines differ between sides")
    if common["workload_size"] != schedule["spsa_pairs_per"]:
        raise cli.OpenBenchError("SPSA pair count differs from workload size")
    settings = {k: v for k, v in common.items() if k in FIELDS}
    settings.update(schedule, test_mode="SPSA", spsa_inputs=api_text(config, workload_id, "inputs"))
    return normalize(settings)


def inspect(config, workload_id):
    session = cli.Session(config)
    session.login()
    status = cli.workload_status(config, workload_id)
    settings = readback(config, session, workload_id, status)
    return {**status, "settings": settings,
            "target_games": 2 * int(settings["spsa_iterations"]) * int(settings["spsa_pairs_per"]),
            "values": api_text(config, workload_id, "outputs"),
            "digest": api_text(config, workload_id, "digest")}


def preflight(config, settings):
    available = cli.server_config(config)
    if settings["dev_engine"] not in available.get("engines", []) or settings["book_name"] not in available.get("books", {}):
        raise cli.OpenBenchError("Requested engine or book is not configured")
    session = cli.Session(config)
    index = cli.index_workloads(session.login(), session.base)
    for item in index["workloads"]:
        if item["type"] != "tune" or item["state"] == "finished":
            continue
        response = cli.workload_status(config, item["id"])
        info = response["info"]
        if not info.get("finished") and info.get("dev", {}).get("sha") == settings["dev_branch"]:
            actual = readback(config, session, item["id"], response)
            if cli.comparable(actual) == cli.comparable(settings):
                raise cli.OpenBenchError(f"Equivalent active or pending workload: {item['id']}")
    return session, {"settings": settings, "configuration": available, "index": index}


def submit(config, settings, output=None, dry_run=False):
    settings = normalize(settings)
    if not dry_run and (output is None or output.exists() or output.is_symlink()):
        raise cli.OpenBenchError("Submission requires a new --output directory; inspect any previous attempt")
    session, checks = preflight(config, settings)
    if dry_run:
        return {"dry_run": True, **checks}
    form = session.fetch("/tune/new/")
    tokens = [node.attrs.get("value") for node in form.root.all("input")
              if node.attrs.get("name") == "csrfmiddlewaretoken"]
    if len(tokens) != 1 or not tokens[0]:
        raise cli.OpenBenchError("Unrecognized SPSA submission form")
    path, record = cli.action_record(config, output, "submission", preflight=checks)
    try:
        record["attempted"] = True
        cli.write_json(path, record, replace=True)
        try:
            session.fetch("/tune/new/", dict(settings, csrfmiddlewaretoken=tokens[0]),
                          redirects=("/index/", "/tune/new/", "/login/"))
        except (cli.OpenBenchError, OSError, cli.HTTPException) as exc:
            record["action_error"] = cli.failure_text(exc, config)
            cli.write_json(path, record, replace=True)
        # Read independently even after a lost response. Never repeat the POST.
        after = cli.index_workloads(session.fetch("/index/"), session.base)
        record["index_after"] = after
        before_ids = {item["id"] for item in checks["index"]["workloads"]}
        matches = []
        record["readbacks"] = {}
        for item in after["workloads"]:
            if item["type"] != "tune" or item["id"] in before_ids:
                continue
            response = cli.workload_status(config, item["id"])
            actual = readback(config, session, item["id"], response)
            record["readbacks"][str(item["id"])] = {**response, "settings": actual}
            cli.write_json(path, record, replace=True)
            if actual == settings:
                matches.append(item["id"])
        if len(matches) != 1:
            raise cli.OpenBenchError("Submission ambiguous or settings differ; inspect submission.json and server state; do not resubmit")
        workload_id = matches[0]
        info = record["readbacks"][str(workload_id)]["info"]
        record.update(workload_id=workload_id, url=session.base + f"/tune/{workload_id}/")
        if (any(type(info.get(key)) is not bool for key in ("approved", "finished", "deleted", "error", "use_penta", "use_tri"))
                or info["deleted"] or info["error"] or not info["use_penta"] or info["use_tri"]):
            raise cli.OpenBenchError("Created tune has unexpected state or pairing settings")
        record.update(verified=True, state="finished" if info["finished"] else
                      "approved" if info["approved"] else "pending")
        cli.write_json(path, record, replace=True)
        return {"record": str(path), **record}
    except BaseException as exc:
        record["failure"] = cli.failure_text(exc, config)
        cli.write_json(path, record, replace=True)
        raise


def stop(config, workload_id, output):
    if output.exists() or output.is_symlink():
        raise cli.OpenBenchError("Stop requires a new --output directory; inspect any previous attempt")
    before = cli.workload_status(config, workload_id)
    info = before["info"]
    if info.get("test_mode") not in ("SPRT", "GAMES", "SPSA"):
        raise cli.OpenBenchError("Unsupported workload type for stop")
    route = "tune" if info["test_mode"] == "SPSA" else "test"
    path, record = cli.action_record(config, output, "stop", workload_id=workload_id,
                                     url=config["OPENBENCH_SERVER"] + f"/{route}/{workload_id}/", before=before)
    try:
        if info.get("finished") is True:
            record.update(verified=True, state="already_finished", after=before)
        else:
            if info.get("deleted") or type(info.get("finished")) is not bool:
                raise cli.OpenBenchError("Cannot stop a deleted or malformed workload")
            session = cli.Session(config)
            session.login()
            record["attempted"] = True
            cli.write_json(path, record, replace=True)
            try:
                session.fetch(f"/{route}/{workload_id}/STOP/", redirects=("/index/", "/login/"))
            except (cli.OpenBenchError, OSError, cli.HTTPException, KeyboardInterrupt) as exc:
                record["action_error"] = cli.failure_text(exc, config)
            record["after"] = cli.workload_status(config, workload_id)
            cli.write_json(path, record, replace=True)
            after = record["after"]["info"]
            if after.get("finished") is not True or after.get("deleted") or any(after.get(side) != info.get(side) for side in ("dev", "base")):
                raise cli.OpenBenchError("Stop did not verify; inspect stop.json and status")
            record.update(verified=True, state="finished")
        cli.write_json(path, record, replace=True)
        return {"record": str(path), **record}
    except BaseException as exc:
        record["failure"] = cli.failure_text(exc, config)
        cli.write_json(path, record, replace=True)
        raise
