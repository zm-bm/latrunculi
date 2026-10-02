#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
bridge_dir="${LICHESS_BOT_DIR:-"$(dirname -- "$project_dir")/lichess-bot"}"
config_file="${LATRUNCULI_BOT_CONFIG:-"$project_dir/bot/config.yml"}"

if [[ -z "${LICHESS_BOT_TOKEN:-}" && -f "$project_dir/bot/token" ]]; then
  IFS= read -r LICHESS_BOT_TOKEN < "$project_dir/bot/token"
  export LICHESS_BOT_TOKEN
fi

if [[ -z "${LICHESS_BOT_TOKEN:-}" ]]; then
  echo "Set LICHESS_BOT_TOKEN or save it in bot/token (chmod 600)." >&2
  exit 2
fi
if [[ ! -x "$project_dir/build/release/latrunculi" ]]; then
  echo "Engine is not built. Run ./bot/setup-local.sh first." >&2
  exit 2
fi
if [[ ! -x "$bridge_dir/.venv/bin/python" ]]; then
  echo "lichess-bot environment is missing. Run ./bot/setup-local.sh first." >&2
  exit 2
fi
if [[ ! -f "$config_file" ]]; then
  echo "Missing $config_file. Copy bot/config.yml.example to bot/config.yml." >&2
  exit 2
fi

mkdir -p "$project_dir/bot/games" "$project_dir/bot/logs"
cd "$bridge_dir"
exec .venv/bin/python lichess-bot.py \
  --config "$config_file" \
  --logfile "$project_dir/bot/logs/lichess-bot.log" \
  "$@"
