# Latrunculi tools

- [OpenBench](openbench.md): inspect, submit, stop, and download workloads.
- [Analysis](analysis/README.md): benchmark engines and investigate positions.
- [Tuning](tuning/README.md): prepare corpora, fit evaluation weights, and verify them.

## Setup

With Python 3.12+ and Git installed, create the shared environment from the
repository root:

```bash
python3 -m venv tools/.venv
tools/.venv/bin/python -m pip install -r tools/requirements.txt
```

Run tools with `tools/.venv/bin/python`, or activate the environment with
`source tools/.venv/bin/activate`. The environment is ignored by Git; rerun
the install command above after [`requirements.txt`](requirements.txt) changes.
