"""Version 1 integer network format and reference evaluator; no torch required."""
import argparse
import array
import hashlib
import json
import math
import pathlib
import struct
import sys

MAGIC = b'LATNNUE1'
HEADER = struct.Struct('<8s9IQ')
INPUTS, HIDDEN = 768, 256
HSCALE, OSCALE = 255, 4096
PAYLOAD_BYTES = (INPUTS * HIDDEN + HIDDEN + 2 * HIDDEN) * 2 + 8


def checksum(data):
    result = 14695981039346656037
    for byte in data:
        result = ((result ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return result


def signed_round(value):
    if not math.isfinite(value):
        raise ValueError('nonfinite network parameter')
    return int(math.copysign(math.floor(abs(value) + .5), value))


def trunc_div(numerator, denominator):
    if denominator <= 0:
        raise ValueError('nonpositive divisor')
    return (abs(numerator) // denominator) * (-1 if numerator < 0 else 1)


def validate_bounds(hidden_weights, hidden_bias, output_weights, output_bias, scale):
    if not 32 <= scale <= 2048:
        raise ValueError('calibration outside [32,2048]')
    if any(not -32768 <= value <= 32767 for values in
           (hidden_weights, hidden_bias, output_weights) for value in values):
        raise ValueError('int16 parameter overflow')
    # At most 64 occupied squares, including permissive diagnostic FENs.
    accumulator_bound = max(map(abs, hidden_bias)) + 64 * max(map(abs, hidden_weights))
    output_bound = abs(output_bias) + 255 * sum(map(abs, output_weights))
    if accumulator_bound > (1 << 31) - 1 or output_bound * scale > (1 << 63) - 1:
        raise ValueError('unsafe accumulator or scaled output bound')
    return {'max_occupied_squares': 64, 'accumulator_abs_bound': accumulator_bound,
            'output_abs_bound': output_bound, 'scaled_output_abs_bound': output_bound * scale}


def encode(hidden_weights, hidden_bias, output_weights, output_bias, scale):
    if [len(hidden_weights), len(hidden_bias), len(output_weights)] != [768 * 256, 256, 512]:
        raise ValueError('wrong parameter dimensions')
    validate_bounds(hidden_weights, hidden_bias, output_weights, output_bias, scale)
    values = array.array('h', [*hidden_weights, *hidden_bias, *output_weights])
    if sys.byteorder != 'little':
        values.byteswap()
    payload = values.tobytes() + struct.pack('<q', output_bias)
    prefix = struct.pack('<8s9I', MAGIC, 1, INPUTS, HIDDEN, 2, 1, HSCALE, OSCALE, scale, len(payload))
    return prefix + struct.pack('<Q', checksum(prefix + payload)) + payload


def decode(data):
    if len(data) != HEADER.size + PAYLOAD_BYTES:
        raise ValueError('invalid payload length')
    magic, version, inputs, hidden, perspectives, orientation, hs, os, scale, length, digest = HEADER.unpack_from(data)
    if (magic, version, inputs, hidden, perspectives, orientation, hs, os, length) != (
            MAGIC, 1, INPUTS, HIDDEN, 2, 1, HSCALE, OSCALE, PAYLOAD_BYTES):
        raise ValueError('incompatible network header')
    payload = data[HEADER.size:]
    if checksum(data[:HEADER.size - 8] + payload) != digest:
        raise ValueError('network checksum mismatch')
    values = array.array('h')
    values.frombytes(payload[:-8])
    if sys.byteorder != 'little':
        values.byteswap()
    model = {'hidden_weights': values[:768 * 256], 'hidden_bias': values[768 * 256:769 * 256],
             'output_weights': values[769 * 256:], 'output_bias': struct.unpack('<q', payload[-8:])[0],
             'scale': scale}
    model['bounds'] = validate_bounds(**model)
    return model


def integer_score(model, perspectives, clamp=30000):
    accumulators = []
    for features in perspectives:
        a = list(model['hidden_bias'])
        for feature in features:
            offset = feature * 256
            for i in range(256):
                a[i] += model['hidden_weights'][offset + i]
        accumulators.extend(min(255, max(0, value)) for value in a)
    n = model['output_bias'] + sum(w * a for w, a in zip(model['output_weights'], accumulators))
    return min(clamp, max(-clamp, trunc_div(model['scale'] * n, 255 * 4096)))


def embed(source, destination):
    if source.stat().st_size != HEADER.size + PAYLOAD_BYTES:
        raise ValueError('invalid network file size')
    data = source.read_bytes()
    decode(data)
    lines = ['#pragma once', '#include <array>', '#include <cstdint>', 'namespace nnue::embedded {',
             f'inline constexpr std::array<std::uint8_t, {len(data)}> bytes = {{']
    lines.extend(','.join(str(b) for b in data[i:i + 32]) + ',' for i in range(0, len(data), 32))
    lines += ['};', f'inline constexpr char sha256[] = "{hashlib.sha256(data).hexdigest()}";', '}']
    destination.write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('network', type=pathlib.Path)
    parser.add_argument('--embed', type=pathlib.Path)
    args = parser.parse_args()
    if args.embed:
        embed(args.network, args.embed)
    else:
        model = decode(args.network.read_bytes())
        print(json.dumps({'scale': model['scale'], 'bounds': model['bounds']}, indent=2))
