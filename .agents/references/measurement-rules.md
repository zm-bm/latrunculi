# Measurement rules

Apply these conditions when running local checks; each stage determines which checks are needed. They do not restrict concurrent remote OpenBench workloads.

## Local execution

Run one local CPU-sensitive task at a time.

## Timing comparisons

Build both revisions with the same compiler, preset, and settings. Warm both binaries, then collect fresh baseline/candidate pairs, alternating run order and using a fresh process for each run. Use fixed CPU affinity when available. Collect timing outside tracing or profiling wrappers.

Avoid substantial competing CPU work, such as builds or other benchmarks. Ordinary background desktop activity does not by itself prevent collection. Assess recorded load, including SMT sibling activity, and timing variation across alternating blocks relative to the claimed effect. Leave small or noisy differences unresolved when the evidence cannot distinguish them.

A single run, historical baseline timing compared with a fresh candidate run, incomplete collection, or uncontrolled timing is diagnostic evidence only. Formal sample counts and decision targets belong to the [offline checks](../skills/test-latrunculi-candidate/references/offline-checks.md#paired-timing).

## Sanitizers

Select a compatible execution environment before running sanitizer tests; LeakSanitizer cannot run under `ptrace`, including tracing sandboxes. If the current sandbox is known to prevent required checks, use a supported execution mode directly, requesting escalation when required. Do not repeat a known-failing sandbox attempt. If compatibility is unknown, run one short test through normal exit under the intended sanitizer preset before the full suite. Keep all required checks enabled; resolve infrastructure failures before rerunning affected checks.
