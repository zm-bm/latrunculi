# Running Latrunculi on Lichess

Uses the [lichess-bot](https://github.com/lichess-bot-devs/lichess-bot) bridge
in a sibling checkout.

## Setup

Requires the [engine build tools](../README.md#build), Python 3.11+, and `uv`.
From the Latrunculi repository:

```bash
./bot/setup-local.sh
```

This builds the engine, clones `lichess-bot` next to this repository if needed,
installs its dependencies using `python3`, and creates `bot/config.yml`.

Review `bot/config.yml` for engine threads, hash size, accepted time controls,
and rated/casual games. Keep variants limited to `standard` and `fromPosition`.

## Account and token

Use an unused Lichess account. Upgrading it to BOT is irreversible.
Create a token with the `bot:play` scope at
<https://lichess.org/account/oauth/token>, then save it:

```bash
read -rsp "Lichess token: " token; echo
install -m 600 /dev/null bot/token
printf '%s\n' "$token" > bot/token
unset token
```

## Run

For a new account, upgrade it and start the bot once with:

```bash
./bot/run-local.sh -u
```

For subsequent runs:

```bash
./bot/run-local.sh
```

Ctrl-C stops accepting new games and lets active games finish. The run log is
`bot/logs/lichess-bot.log`; PGNs are saved in `bot/games/`. Both are ignored by Git.
