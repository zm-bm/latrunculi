"""Freeze legal histories and compare latrunculi with the pinned SF12 oracle."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import subprocess
import time

import chess

NETWORK_SHA = '82215d0fd0df3cde17fc47ac024c5b5736823ec390200abdd705c44660c69ee4'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def trunc_div(value, divisor):
    return (abs(value) // divisor) * (-1 if value < 0 else 1)


def adapted_score(raw):
    neural = trunc_div(raw * 5, 4) + 28
    return min(30000, max(-30000, trunc_div(neural * 202, 208)))


def prepare(output, oracle, network):
    if (output / 'corpus.jsonl').exists():
        raise ValueError('corpus already frozen')
    if digest(network) != NETWORK_SHA:
        raise ValueError('frozen network identity mismatch')
    rng = random.Random(98170)
    rows, seen = [], set()
    board, moves = chess.Board(), []
    while len(rows) < 10000:
        legal = sorted(board.legal_moves, key=lambda m: m.uci())
        if not legal or len(moves) >= 128 or board.is_game_over(claim_draw=True):
            board, moves = chess.Board(), []
            continue
        move = rng.choice(legal)
        board.push(move)
        moves.append(move.uci())
        if board.fen() not in seen:
            seen.add(board.fen())
            rows.append({'start_fen': chess.STARTING_FEN, 'moves': moves.copy(), 'fen': board.fen()})
    cases = [(chess.STARTING_FEN, []),
             ('rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1', []),
             ('r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1', ['e1g1', 'e8c8']),
             ('r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1', ['e1c1', 'e8g8']),
             ('4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1', ['e5d6']),
             ('4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1', ['e4d3']),
             ('4k3/8/8/8/8/8/8/4K3 w - - 0 1', ['e1d2', 'e8d7', '0000', '0000'])]
    for piece in 'qnrb':
        cases += [('4k3/P7/8/8/8/8/8/4K3 w - - 0 1', ['a7a8' + piece]),
                  ('1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1', ['a7b8' + piece]),
                  ('4k3/8/8/8/8/8/p7/4K3 b - - 0 1', ['a2a1' + piece]),
                  ('4k3/8/8/8/8/8/p7/1R2K3 b - - 0 1', ['a2b1' + piece])]
    for color in chess.COLORS:
        for square in [chess.D1, chess.A1, chess.C1, chess.B1, chess.D2]:
            b = chess.Board()
            b.remove_piece_at(square if color else square ^ 56)
            b.castling_rights = b.clean_castling_rights()
            cases.append((b.fen(), []))
    for start, history in cases:
        b = chess.Board(start)
        for token in history:
            if token == '0000':
                if b.is_check():
                    raise ValueError('null fixture move in check')
                b.push(chess.Move.null())
            else:
                b.push_uci(token)
        rows.append({'start_fen': start, 'moves': history, 'fen': b.fen()})
    output.mkdir(parents=True, exist_ok=True)
    corpus = output / 'corpus.jsonl'
    corpus.write_text(''.join(json.dumps(r, separators=(',', ':'))+'\n' for r in rows))
    commands = ['setoption name Threads value 1', 'setoption name Hash value 32',
                'setoption name EvalFile value ' + str(network.resolve())]
    for row in rows:
        commands.append('position fen ' + row['start_fen'])
        commands.extend('nnuepush '+m for m in row['moves'])
        commands.append('nnueraw')
    commands.append('quit')
    start = time.monotonic()
    run = subprocess.run([str(oracle.resolve())], input='\n'.join(commands)+'\n',
                         text=True, capture_output=True, timeout=300)
    (output / 'oracle.stdout.txt').write_text(run.stdout)
    (output / 'oracle.stderr.txt').write_text(run.stderr)
    run.check_returncode()
    scores = [int(line.split()[1]) for line in run.stdout.splitlines() if line.startswith('nnue_raw ')]
    if len(scores) != len(rows):
        raise ValueError(f'oracle returned {len(scores)} outputs for {len(rows)} positions')
    write(output / 'oracle-scores.json', scores)
    write(output / 'oracle-gate.json', {'passed': True, 'seed': 98170,
        'dataset_positions': 10000, 'special_cases': len(cases),
        'corpus_sha256': digest(corpus), 'network_sha256': digest(network),
        'oracle_sha256': digest(oracle), 'scores_sha256': digest(output / 'oracle-scores.json'),
        'elapsed_seconds': time.monotonic()-start})
    print((output / 'oracle-gate.json').read_text())


def check(output, executable, label, all_backends=False):
    gate = json.loads((output / 'oracle-gate.json').read_text())
    if gate.get('passed') is not True or digest(output / 'corpus.jsonl') != gate['corpus_sha256']:
        raise ValueError('failed oracle gate or corpus identity mismatch')
    if digest(output / 'oracle-scores.json') != gate['scores_sha256']:
        raise ValueError('oracle scores identity mismatch')
    rows = [json.loads(s) for s in (output / 'corpus.jsonl').read_text().splitlines()]
    expected = json.loads((output / 'oracle-scores.json').read_text())
    text = ''.join(r['start_fen']+'\t'+' '.join(r['moves'])+'\n' for r in rows)
    start = time.monotonic()
    command = [str(executable.resolve())] + (['--all-backends'] if all_backends else [])
    run = subprocess.run(command, input=text, text=True,
                         capture_output=True, timeout=300)
    (output / (label+'.stdout.jsonl')).write_text(run.stdout)
    (output / (label+'.stderr.txt')).write_text(run.stderr)
    run.check_returncode()
    actual = [json.loads(s) for s in run.stdout.splitlines()]
    if len(actual) != len(expected):
        raise ValueError(f'native returned {len(actual)} outputs for {len(expected)} positions')
    for index, (row, raw) in enumerate(zip(actual, expected)):
        if row['raw'] != raw or row['score'] != adapted_score(raw):
            raise ValueError(f'inference/adapter disagreement at {index}: {row}, expected {raw}; {rows[index]}')
        if row['network_sha256'] != NETWORK_SHA:
            raise ValueError('native network identity mismatch')
    write(output / (label+'.json'), {'passed': True, 'positions': len(actual),
        'corpus_sha256': gate['corpus_sha256'], 'executable_sha256': digest(executable),
        'network_sha256': NETWORK_SHA, 'all_backends': all_backends,
        'elapsed_seconds': time.monotonic()-start})
    print((output / (label+'.json')).read_text())


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('stage', choices=['prepare', 'check'])
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--oracle', type=Path)
    p.add_argument('--network', type=Path)
    p.add_argument('--executable', type=Path)
    p.add_argument('--label', default='parity')
    p.add_argument('--all-backends', action='store_true')
    a = p.parse_args()
    if a.stage == 'prepare':
        prepare(a.output, a.oracle, a.network)
    else:
        check(a.output, a.executable, a.label, a.all_backends)
