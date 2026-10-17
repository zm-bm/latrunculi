# Latrunculi tools

- [`./bin/bench`](analysis/README.md): compare engine behavior and runtime.
- [`./bin/tune`](tuning/README.md): prepare corpora, fit evaluation weights, and verify them.
- [`./bin/openbench-client`](openbench/README.md): inspect, submit, and download OpenBench workloads.
- [`./bin/openbench-submit`](openbench/README.md#submit-and-stop): prepare and submit standard Latrunculi tests.
- [`./bin/uci-probe`](analysis/README.md#probe-one-position): investigate individual positions.

## Setup

With Python 3.12+ and Git installed, create the shared environment from the
repository root:

```bash
python3 -m venv tools/.venv
tools/.venv/bin/python -m pip install -r tools/requirements.txt
```

Run the commands above from the repository root. The launchers select the shared
environment automatically; activation is unnecessary. They preserve your working
directory and forward arguments unchanged. The environment is ignored by Git;
rerun the install command after [`requirements.txt`](requirements.txt) changes.

## Tests

Run all tool tests from the repository root:

```bash
tools/.venv/bin/python -m unittest discover -s tools -t . -p 'test_*.py'
```

The tests use temporary fixtures; they do not contact OpenBench or run engine
games. UCI subprocess tests need an environment that supports asyncio subprocesses
without tracing; see the [analysis guide](analysis/README.md#probe-one-position).
