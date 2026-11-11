"""Schema-9 finalization context and scalar reconstruction."""

import chess


COMPLEXITY_NAMES = (
    "endgame.complexity.pawns", "endgame.complexity.flanks",
    "endgame.complexity.pawn_only", "endgame.complexity.offset",
)
BOUNDS = {
    "material.minor_pawn_balance.mg": (-16, 16),
    "material.minor_pawn_balance.eg": (-8, 8),
    "endgame.complexity.pawns.eg": (0, 16),
    "endgame.complexity.flanks.eg": (0, 96),
    "endgame.complexity.pawn_only.eg": (0, 96),
    "endgame.complexity.offset.eg": (-100, 0),
    "endgame.scale_strength.eg": (0, 64),
}


def special_ids(schema):
    roles = schema.get("finalization", {})
    strength = roles.get("strength")
    return roles.get("complexity", []) + ([] if strength is None else [strength])


def validate_schema(schema):
    roles = schema.get("finalization")
    if (not isinstance(roles, dict)
            or set(roles) != {"material", "complexity", "strength", "context"}
            or roles["context"] != "pieces-flanks-bishops-v1"):
        raise ValueError("invalid finalization roles")
    if schema["scale_limit"] != 64:
        raise ValueError("schema 9 finalization requires scale denominator 64")
    features = schema["features"]
    expected = [(roles.get("material"), "material.minor_pawn_balance"),
                (roles.get("strength"), "endgame.scale_strength")]
    complexity = roles.get("complexity")
    if (not isinstance(complexity, list) or len(complexity) not in (0, 4)
            or any(type(i) is not int for i in complexity)):
        raise ValueError("invalid complexity roles")
    expected += list(zip(complexity, COMPLEXITY_NAMES))
    names = {feature["name"] for feature in features}
    if not complexity and names.intersection(COMPLEXITY_NAMES):
        raise ValueError("complexity features require finalization roles")
    for ident, name in expected:
        if ident is None:
            if name in names:
                raise ValueError("finalization feature has no role")
            continue
        if type(ident) is not int or not 529 <= ident < len(features) or features[ident]["name"] != name:
            raise ValueError("invalid finalization feature")
    if any(features[i]["mg"] != 0 for i in special_ids(schema)):
        raise ValueError("nonlinear finalization features must be EG-only")
    for feature in features:
        for phase in ("mg", "eg"):
            bound = BOUNDS.get(f'{feature["name"]}.{phase}')
            if bound is not None and not bound[0] <= feature[phase] <= bound[1]:
                raise ValueError("finalization weight outside declared bounds")


def context_from_board(board):
    colors = (chess.WHITE, chess.BLACK)
    return {
        "pieces": [[len(board.pieces(p, c)) for p in range(chess.PAWN, chess.QUEEN + 1)] for c in colors],
        "pawn_flanks": [int(bool(board.pieces(chess.PAWN, c).mask & 0x0f0f0f0f0f0f0f0f))
                        | (int(bool(board.pieces(chess.PAWN, c).mask & 0xf0f0f0f0f0f0f0f0)) << 1) for c in colors],
        "bishop_colors": [int(bool(board.pieces(chess.BISHOP, c).mask & chess.BB_DARK_SQUARES))
                          | (int(bool(board.pieces(chess.BISHOP, c).mask & chess.BB_LIGHT_SQUARES)) << 1) for c in colors],
    }


def validate_context(context, board):
    if not isinstance(context, dict) or set(context) != {"pieces", "pawn_flanks", "bishop_colors"}:
        raise ValueError("invalid finalization context")
    pieces = context["pieces"]
    if not isinstance(pieces, list) or len(pieces) != 2 or any(
            not isinstance(row, list) or len(row) != 5 or any(type(n) is not int for n in row)
            for row in pieces):
        raise ValueError("invalid finalization piece counts")
    for name in ("pawn_flanks", "bishop_colors"):
        values = context[name]
        if not isinstance(values, list) or len(values) != 2 or any(type(n) is not int for n in values):
            raise ValueError("invalid finalization masks")
    if context != context_from_board(board):
        raise ValueError("finalization context differs from its FEN")


def target_scale(context, strong, base, material_weights):
    w, b = context["pieces"]
    target = base
    if w[2] == b[2] == 1 and context["bishop_colors"][0] ^ context["bishop_colors"][1] == 3:
        if w[1] == b[1] and w[3] == b[3] and w[4] == b[4] == 0:
            if w[1] == w[3] == 0:
                target = min(target, 32)
            elif (w[1], w[3]) in ((1, 0), (0, 1)):
                target = min(target, 48)
    npm = [sum(n * weight for n, weight in zip(c[1:], material_weights)) for c in (w, b)]
    if context["pieces"][strong][0] == 0 and npm[1 - strong] > 0 and 0 <= npm[strong] - npm[1 - strong] <= material_weights[1]:
        target = min(target, 16)
    if context["pawn_flanks"][strong] in (1, 2):
        target = min(target, base - 4)
    return target


def adjusted_eg(schema, record, raw_eg, base, material_weights):
    roles = schema["finalization"]
    coefficients = dict(record["coefficients"])
    weights = schema["features"]
    complexity = sum(coefficients.get(i, 0) * weights[i]["eg"] for i in roles["complexity"])
    sign = (raw_eg > 0) - (raw_eg < 0)
    adjusted = sign * max(abs(raw_eg) + complexity, 0)
    target = target_scale(record["finalization"], int(raw_eg < 0), base, material_weights)
    strength = weights[roles["strength"]]["eg"] if roles["strength"] is not None else 0
    effective = base - (base - target) * strength // schema["scale_limit"]
    return adjusted, effective


def expected_coefficients(schema, context):
    roles = schema["finalization"]
    w, b = context["pieces"]
    result = {}
    if roles["material"] is not None:
        result[roles["material"]] = (w[1] - w[2]) * (w[0] - 4) - (b[1] - b[2]) * (b[0] - 4)
    inputs = [w[0] + b[0], int(context["pawn_flanks"][0] | context["pawn_flanks"][1] == 3),
              int(sum(w[1:]) + sum(b[1:]) == 0), 1]
    result.update(zip(roles["complexity"], inputs))
    if roles["strength"] is not None:
        weights = {f["name"]: f["mg"] for f in schema["features"]}
        material = [weights['material.' + p] for p in ('knight', 'bishop', 'rook', 'queen')]
        active = False
        for side in (0, 1):
            base = min(schema["scale_limit"], schema["scale_base"] + schema["scale_per_pawn"] * context["pieces"][side][0])
            active |= target_scale(context, side, base, material) != base
        result[roles["strength"]] = int(active)
    return result
