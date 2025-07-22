#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <opponent_engine_path>"
    exit 1
fi

OPP_ENGINE="$1"
BOOK_FILE=./data/book-ply4-unifen-Q-0.0-0.25.pgn
OUTPUT_DIR=./logs

mkdir -p "$OUTPUT_DIR"

CUTECHESS_CMD=(
    cutechess-cli
    -debug
    -engine cmd=./bin/latrunculi
    -engine cmd="$OPP_ENGINE"
    -each proto=uci tc=40/60
    -repeat
    -rounds 10
    -games 2
    -resign movecount=5 score=1000
    -pgnout "$OUTPUT_DIR/games.pgn"
)

if [ -f "$BOOK_FILE" ]; then
    CUTECHESS_CMD+=(-openings file="$BOOK_FILE" policy=round)
else
    echo "Warning: Book file '$BOOK_FILE' not found. Running without opening book."
fi

"${CUTECHESS_CMD[@]}" |& tee "$OUTPUT_DIR/debug.log"
