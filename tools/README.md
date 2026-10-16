# Latrunculi tools

- [`./tools/bench`](analysis/README.md): compare engine behavior and runtime.
- [`./tools/tune`](tuning/README.md): prepare corpora, fit evaluation weights, and verify them.
- [`./tools/openbench-client`](openbench.md): inspect, submit, and download OpenBench workloads.
- [`./tools/openbench-submit`](openbench.md#submit-and-stop): prepare and submit standard Latrunculi tests.
- [`./tools/uci-probe`](analysis/README.md#probe-one-position): investigate individual positions.

## Setup

With Python 3.12+ and Git installed, create the shared environment from the
repository root:

```bash
python3 -m venv tools/.venv
tools/.venv/bin/python -m pip install -r tools/requirements.txt
```

Run the commands above from the repository root. The launchers select the shared
environment automatically; activation is unnecessary. They preserve your working
directory and forward arguments unchanged. Use `tools/.venv/bin/python` directly
for Python tests. The environment is ignored by Git; rerun the install command
after [`requirements.txt`](requirements.txt) changes.
