# Exploration evidence

Start with evidence from the current operational baseline. A lead may remain a question. Once it proposes a change, record whether the expected benefit is faster search, stronger play, or both, and how to check the proposed mechanism. If it is expected to make search materially slower, explain why that tradeoff may be worth testing before measuring it. A strength change has no universal node-reduction or fixed-depth slowdown cutoff.

Recheck historical ideas before implementing them. To investigate a game error, try more search budget, temporarily disable one selectivity rule, or isolate an evaluation change. Compare variants of one hypothesis without stacking unresolved changes. [Profile](../../../../tools/analysis/README.md#cpu-profiling) before hot-path or architectural work to locate CPU cost, and add search state only for a concrete use.

Fixed-depth node counts show selectivity, paired timing measures speed, and games measure strength. Correlations and cross-engine depth or nodes-per-second (NPS) comparisons may suggest leads, but cannot establish a candidate's benefit. Other engines can supply references or ideas; validate mechanisms in Latrunculi without copying their code or assuming their constants transfer.

Choose and tune a variant using development evidence. Keep its implementation and settings unchanged for independent confirmation; do not retune after seeing the confirmation results. A source group contains related positions, such as positions from one game or opening. Keep each group entirely in development or confirmation, and preserve every full starting FEN and known move history. Once inspected, confirmation groups count as development evidence in later work.

When using reference engines, pin revisions and settings. Report paired aggregate effects, uncertainty across source groups, and serious errors repaired or introduced. Consider regressions in context: one disagreement or a worse median does not decide the result. Leave sparse or unstable evidence unresolved. Reference agreement proves neither correctness nor strength; independently verified exact win/draw/loss (WDL) remains a hard check. Tuning uses grouped cross-validation, but fitted evaluation still needs games for acceptance.

Limit negative findings to the revisions, samples, and variants tested. New causal evidence or a changed baseline can justify another look.
