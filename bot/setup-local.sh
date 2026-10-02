#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
bridge_dir="${LICHESS_BOT_DIR:-"$(dirname -- "$project_dir")/lichess-bot"}"

cmake --preset release -S "$project_dir"
cmake --build "$project_dir/build/release" --parallel

if [[ ! -f "$bridge_dir/lichess-bot.py" ]]; then
  git clone https://github.com/lichess-bot-devs/lichess-bot.git "$bridge_dir"
fi

uv venv --clear --python python3 "$bridge_dir/.venv"
uv pip install --python "$bridge_dir/.venv/bin/python" -r "$bridge_dir/requirements.txt"

if [[ ! -f "$project_dir/bot/config.yml" ]]; then
  cp "$project_dir/bot/config.yml.example" "$project_dir/bot/config.yml"
fi
mkdir -p "$project_dir/bot/games"

printf 'Local bot dependencies are ready. See %s\n' "$project_dir/bot/README.md"
