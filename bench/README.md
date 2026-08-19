# OpenBench Integration

This directory contains Latrunculi's deterministic engine benchmark and the
small build entry used by OpenBench. CMake remains the project build system;
the Makefile only adapts OpenBench's `EXE` and `CXX` build contract.

```bash
make -C bench EXE=latrunculi CXX=g++
./bench/latrunculi bench
```

The benchmark searches a fixed six-position suite at depth 13 with one thread
and a 32 MB transposition table. It prints aggregate nodes, elapsed time, and
nodes per second. The node count is the compatibility signature recorded by
OpenBench tests.

Local component performance remains under [measurements](../measurements/README.md).
