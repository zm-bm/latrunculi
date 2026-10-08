"""Read native benchmark results and compare their search work and time."""

import csv
import math
import statistics
from dataclasses import dataclass
from pathlib import Path


FORMAT = "search_measurement_v5"
CONFIG = ("limit_type", "limit_value", "threads", "hash_mb")
SIGNATURE = ("completed_depth", "static_score", "score", "nodes", "best_move", "pv")
FIELDS = {"result_format", "case", "fen", *CONFIG,
          *SIGNATURE, "total_ns"}


@dataclass(frozen=True)
class Run:
    path: Path
    rows: dict[str, dict[str, str]]
    config: tuple[str, ...]

    @property
    def total_nodes(self) -> int:
        return sum(int(row["nodes"]) for row in self.rows.values())

    @property
    def total_ns(self) -> int:
        return sum(int(row["total_ns"]) for row in self.rows.values())


def load_run(path: Path) -> Run:
    try:
        with path.open(encoding="utf-8", newline="") as source:
            reader = csv.DictReader(source, delimiter="\t")
            missing = FIELDS - set(reader.fieldnames or ())
            if missing:
                raise ValueError(f"{path}: missing fields: {','.join(sorted(missing))}")
            parsed = list(reader)
    except OSError as error:
        raise ValueError(f"{path}: {error}")
    if not parsed:
        raise ValueError(f"{path}: empty run")
    rows = {}
    config = None
    for line, row in enumerate(parsed, start=2):
        if None in row or any(row[field] is None for field in FIELDS):
            raise ValueError(f"{path}:{line}: malformed TSV row")
        case = row["case"]
        if not case or case in rows:
            raise ValueError(f"{path}:{line}: empty or duplicate case {case!r}")
        if row["result_format"] != FORMAT:
            raise ValueError(f"{path}:{line}: expected {FORMAT}")
        fields = row["fen"].split()
        if len(fields) != 6 or fields[1] not in ("w", "b"):
            raise ValueError(f"{path}:{line}: invalid six-field FEN")
        if row["limit_type"] not in ("depth", "nodes", "movetime"):
            raise ValueError(f"{path}:{line}: invalid limit type")
        for field in ("limit_value", "threads", "hash_mb", "completed_depth",
                      "nodes", "total_ns", "static_score", "score"):
            try:
                value = int(row[field])
            except ValueError as error:
                raise ValueError(f"{path}:{line}: {field} must be an integer") from error
            if field not in ("static_score", "score") and value <= 0:
                raise ValueError(f"{path}:{line}: {field} must be positive")
        if not row["best_move"] or row["pv"].split()[:1] != [row["best_move"]]:
            raise ValueError(f"{path}:{line}: best move and PV disagree")
        row_config = tuple(row[field] for field in CONFIG)
        if config is None:
            config = row_config
        elif row_config != config:
            raise ValueError(f"{path}:{line}: mixed search settings")
        rows[case] = row
    assert config is not None
    run = Run(path, rows, config)
    require_legal_pvs(run)
    return run


def require_comparable(reference: Run, other: Run) -> None:
    if reference.config != other.config:
        raise ValueError(f"{other.path}: search settings differ from {reference.path}")
    if reference.rows.keys() != other.rows.keys():
        raise ValueError(f"{other.path}: case set differs from {reference.path}")
    changed = [case for case in reference.rows
               if reference.rows[case]["fen"] != other.rows[case]["fen"]]
    if changed:
        raise ValueError(f"{other.path}: position differs for {','.join(sorted(changed))}")


def require_signatures(label: str, reference: Run, other: Run) -> None:
    require_comparable(reference, other)
    changed = [case for case in reference.rows if any(
        reference.rows[case][field] != other.rows[case][field] for field in SIGNATURE)]
    if changed:
        raise ValueError(f"{label}: signature mismatches for {','.join(sorted(changed))}")


def require_legal_pvs(run: Run) -> int:
    """Validate every continuation independently from its full starting FEN."""
    try:
        import chess
    except ModuleNotFoundError:
        raise ValueError("PV legality requires tools/analysis/requirements.txt")

    moves = 0
    for case, row in run.rows.items():
        try:
            board = chess.Board(row["fen"])
            if not board.is_valid():
                raise ValueError("invalid position")
            for ply, token in enumerate(row["pv"].split(), start=1):
                move = chess.Move.from_uci(token)
                if move not in board.legal_moves:
                    raise ValueError(f"illegal PV move {token} at ply {ply}")
                board.push(move)
                moves += 1
        except ValueError as error:
            raise ValueError(f"{run.path}: {case}: {error}")
    return moves


def summarize(pairs: list[tuple[str, Run, Run]], exact_tree: bool = False) -> dict:
    """Summarize measured pairs; warmups must not be included."""
    order_error = "use one BC/CB pair or alternate BC,CB in complete blocks"
    if not pairs:
        raise ValueError(order_error)
    if len(pairs) == 1:
        if pairs[0][0] not in ("BC", "CB"):
            raise ValueError(order_error)
    else:
        if len(pairs) % 2:
            raise ValueError(order_error)
        for index, (order, _, _) in enumerate(pairs):
            expected = "BC" if index % 2 == 0 else "CB"
            if order != expected:
                raise ValueError(order_error)
    paths = [run.path.resolve() for _, baseline, candidate in pairs
             for run in (baseline, candidate)]
    if len(set(paths)) != len(paths):
        raise ValueError("comparison requires distinct files for each run")

    baseline_ref, candidate_ref = pairs[0][1:]
    ratios = []
    for index, (_, baseline, candidate) in enumerate(pairs, start=1):
        require_comparable(baseline, candidate)
        require_signatures(f"baseline repeat {index}", baseline_ref, baseline)
        require_signatures(f"candidate repeat {index}", candidate_ref, candidate)
        ratios.append(candidate.total_ns / baseline.total_ns)
    if exact_tree:
        require_signatures("exact-tree comparison", baseline_ref, candidate_ref)

    node_ratios = [int(candidate_ref.rows[case]["nodes"]) / int(row["nodes"])
                   for case, row in baseline_ref.rows.items()]
    summary = dict(zip(CONFIG, baseline_ref.config))
    summary.update(
        cases=len(node_ratios),
        baseline_nodes=baseline_ref.total_nodes,
        candidate_nodes=candidate_ref.total_nodes,
        geometric_mean_node_ratio=math.exp(statistics.fmean(map(math.log, node_ratios))),
        total_node_ratio=candidate_ref.total_nodes / baseline_ref.total_nodes,
        root_move_changes=sum(baseline_ref.rows[c]["best_move"] !=
                              candidate_ref.rows[c]["best_move"] for c in baseline_ref.rows),
        score_changes=sum(baseline_ref.rows[c]["score"] !=
                          candidate_ref.rows[c]["score"] for c in baseline_ref.rows),
        pairs=len(pairs),
        timing="single_pair_diagnostic" if len(pairs) == 1 else "paired",
    )
    for index, ((order, _, _), ratio) in enumerate(zip(pairs, ratios), start=1):
        summary[f"pair_{index}_order"] = order
        summary[f"pair_{index}_search_time_ratio"] = ratio
    if len(pairs) == 1:
        return summary
    blocks = [math.sqrt(ratios[i] * ratios[i + 1]) for i in range(0, len(ratios), 2)]
    summary.update(
        median_search_time_ratio=statistics.median(ratios),
        minimum_search_time_ratio=min(ratios),
        maximum_search_time_ratio=max(ratios),
        candidate_wins=sum(r < 1 for r in ratios),
        bc_median_search_time_ratio=statistics.median(ratios[::2]),
        cb_median_search_time_ratio=statistics.median(ratios[1::2]),
    )
    for index, ratio in enumerate(blocks, start=1):
        summary[f"balanced_block_{index}_search_time_ratio"] = ratio
    summary["median_balanced_search_time_ratio"] = statistics.median(blocks)
    return summary


def format_summary(summary: dict) -> str:
    return "".join(f"{key}={value:.9f}\n" if isinstance(value, float) else f"{key}={value}\n"
                   for key, value in summary.items())
