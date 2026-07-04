# UCI and Engine Cleanup Goal

Use this document with `/goal` when you want Codex to reshape the UCI and
engine orchestration layers without changing chess behavior.

## Objective

Refactor the UCI and engine boundary so:

- `Engine` is the top-level coordinator for process lifecycle, current board
  state, options, and search ownership;
- `ThreadPool` remains the execution boundary for search work;
- a dedicated UCI handler owns command parsing and protocol output;
- the UCI protocol surface, UCI config/options surface, and UCI `go` search
  options surface have each been reviewed and given clear ownership;
- low-hanging UCI compatibility gaps from the protocol definition have been
  fixed or explicitly deferred;
- UCI protocol text, debug text, and core engine objects have consistent string
  formatting and parsing conventions;
- current search behavior, move legality behavior, and UCI-visible command
  behavior remain compatible unless this document explicitly calls for a
  verified cleanup.

This is an architecture cleanup, not a search-strength project.

## Required Deliverables

Produce a small series of commits, or one clean commit if the final diff stays
compact. Prefer separate commits for UCI cleanup and `Engine` cleanup if both
touch many files.

Required final shape:

- A UCI-facing handler/writer/parser boundary exists and is named clearly.
- `uci::Protocol` is no longer a grab bag of option storage, command parsing,
  diagnostics, and line writing. If the name remains, its responsibility must be
  narrow and documented by the public API.
- The implementation notes or final summary include a command-surface review
  that classifies every currently accepted command as real UCI, debug console,
  alias, unsupported placeholder, or removed/deferred.
- The implementation notes or final summary include a protocol-compliance review
  against the DOBRO UCI protocol gist's official spec text, ignoring gist
  comments except as non-authoritative context.
- UCI config/options have a clear owner. The supported options `Hash`,
  `Threads`, and `Debug` have descriptors, validation, output formatting, and
  side-effect application reviewed together.
- UCI `go` search options have a clear owner. Current supported tokens
  `depth`, `movetime`, `nodes`, `wtime`, `btime`, `winc`, `binc`, and
  `movestogo` are parsed through one UCI-facing surface before runtime
  `SearchOptions` are derived.
- Unsupported or deferred UCI protocol/search-option tokens are documented
  rather than silently becoming accidental design decisions.
- `Engine` owns orchestration and delegates:
  - command text parsing to the UCI layer;
  - search execution to `ThreadPool`;
  - UCI line emission to the UCI writer/handler;
  - board mutation to a small internal position-state surface.
- The string-handling policy below is applied consistently to touched UCI and
  engine files.
- The rest of the repo has been audited for string formatting/parsing methods,
  and low-risk inconsistencies have been cleaned up or documented as explicit
  follow-ups.
- Existing UCI progress reporting remains intact: per-depth and root-PV-change
  `info` output should still be main-thread-only and duplicate-suppressed in
  search lifecycle code, not in the stateless writer.

## Completion Criteria

Do not mark this goal complete until:

- `cmake --build --preset release-dev` passes.
- `ctest --preset release-dev` passes.
- `cmake --build --preset release-stats-dev` passes if any touched file is
  compiled in stats builds or search reporting behavior changed.
- `ctest --preset release-stats-dev` passes under the same condition.
- `git diff --check` passes.
- Engine-level UCI tests still cover at least:
  - `uci` and `isready` handshakes;
  - `setoption`;
  - `position`;
  - `go depth`;
  - at least one time-control `go` command;
  - `stop`;
  - terminal/no-legal-move `bestmove 0000`;
  - unknown UCI command/token handling;
  - debug-console error reporting.
- Focused UCI parser/config/search-option tests cover:
  - `setoption name Hash value ...`;
  - `setoption name Threads value ...`;
  - `setoption name Debug value ...` or `debug ...`, whichever remains
    supported;
  - case-insensitive option names and check-option values;
  - `setoption name Clear Hash` if the low-hanging compatibility phase adds it;
  - invalid option names and values;
  - `info ... score mate ...` output for mate-range scores;
  - `go depth`, `go movetime`, `go nodes`, and clock-budget tokens;
  - current behavior for unknown or unsupported `go` tokens.
- Engine transcript tests cover `register` no-op, unsupported `ponderhit`
  no-op, `isready` during search, and `uci`/`readyok` flushing behavior if the
  output surface is testable through streams.
- Search-worker tests still cover progress reporting and duplicate suppression.
- The final diff includes either repo-wide string-handling cleanup beyond
  UCI/Engine or a short written explanation of why each remaining family should
  stay as-is for now.
- The final summary names any behavior intentionally changed. If no behavior was
  intentionally changed, say that explicitly.

## Protocol Reference Review

Before implementation, review the official UCI protocol text from the DOBRO gist:

- `https://gist.github.com/DOBRO/2592c6dad754ba67e6dcaec8c90165bf`

Use the gist's specification text as the protocol reference. Do not treat the
comment thread as authoritative.

The review must cover these protocol rules and classify each as already
supported, low-hanging fix, deferred, or intentionally unsupported:

- all communication is standard input/output text with newline-terminated
  commands;
- arbitrary whitespace between tokens is allowed;
- the engine should always be able to process input while searching;
- unknown commands and unknown tokens should be ignored rather than producing
  protocol-visible errors;
- commands that arrive in the wrong state, such as `stop` when not searching,
  should be ignored;
- UCI null moves from engine to GUI are written as `0000`;
- `uci` must output `id`, supported `option` lines, and `uciok`;
- `isready` must always answer `readyok`, including while searching;
- `debug on/off` can arrive while searching and should use `info string` only
  for optional debug information;
- `setoption` names and values are case-insensitive and option names/values may
  contain spaces;
- `setoption` button options do not require a value;
- `position` accepts `startpos` or `fen ...` plus optional moves;
- `go` supports the documented token family, but this engine should only
  implement tokens it can honor correctly;
- every completed `go` command must eventually produce `bestmove`;
- final search information should be available before `bestmove`;
- mate-range scores should be reported with UCI `score mate <moves>` output,
  while ordinary scores should remain `score cp <centipawns>`;
- engine option output must follow UCI option type rules.

## Reference Engine Review Gate

Before implementation, inspect the local reference engines for boundary ideas:

- `/home/rick/code/chess-engine-refs/Stockfish/src/uci.cpp`
- `/home/rick/code/chess-engine-refs/Stockfish/src/uci.h`
- `/home/rick/code/chess-engine-refs/Stockfish/src/ucioption.cpp`
- `/home/rick/code/chess-engine-refs/Stockfish/src/engine.cpp`
- `/home/rick/code/chess-engine-refs/Stockfish/src/engine.h`
- `/home/rick/code/chess-engine-refs/Ethereal/src/uci.h`
- `/home/rick/code/chess-engine-refs/Minic/Source/uci.cpp`
- `/home/rick/code/chess-engine-refs/Minic/Source/uci.hpp`

Use them for comparison only. Do not copy structure, names, constants, or
formatting wholesale. The target is a Latrunculi-shaped boundary that matches
this repo's scale.

Record the reference review in the implementation notes or final summary:

- what boundary ideas were useful;
- what was intentionally not copied;
- any UCI behavior difference that should remain.

## Current Pain Points

The current shape works, but the responsibilities are mixed:

- `uci::Protocol` writes UCI lines, formats options, formats search info,
  validates and formats PV text using `Board`/`RootLine`, stores option types,
  and emits debug diagnostics.
- UCI config mixes option descriptors, mutable values, validation, and engine
  side-effect callbacks in one type.
- `Engine` owns command dispatch, command parsing, option application, board
  history, debug commands, search lifecycle checks, and UCI error reporting.
- `SearchOptions` parses `go` text directly from `std::istringstream`, which
  couples protocol parsing to runtime search setup.
- String handling is inconsistent across the UCI/Engine boundary and the rest of
  the repo:
  - UCI output is partly `std::format`;
  - command parsing is ad hoc stream extraction;
  - object text uses a mix of `str()`, `score_text()`, `nps_text()`, and
    `std::format` support;
  - coordinate and FEN-like text uses domain-specific names such as
    `Move::str()`, `to_string(Square)`, and `Board::toFEN()`;
  - debug tables use `std::formatter` specializations and some object-level
    `str()` helpers;
  - several tests and diagnostics still use stream insertion directly;
  - error strings are assembled inline in multiple places.

## Target Boundaries

### Engine

`Engine` should be a coordinator with a small public surface:

- construct with input/output streams;
- run the command loop;
- execute one parsed command in tests if that helper remains useful;
- own board state and position history;
- own `ThreadPool`;
- apply option side effects such as TT resize and thread-pool resize;
- enforce high-level lifecycle policy, especially what commands are legal while
  search is active.

`Engine` should not be the long-term home for token-level UCI parsing or UCI line
formatting.

### UCI Handler

Create or reshape a UCI layer that owns the text protocol boundary. Reasonable
names include `uci::Handler`, `uci::CommandParser`, and `uci::Writer`; choose
the smallest set that keeps responsibilities clear.

The UCI layer should own:

- parsing raw command text into typed command data or a small dispatchable
  command representation;
- parsing `setoption`, `position`, and `go` grammar;
- formatting UCI output lines;
- flushing protocol output that GUIs or pipe-based harnesses depend on.

The UCI layer should not own:

- board state;
- TT state;
- thread-pool lifetime;
- duplicate suppression for search progress;
- search lifecycle policy that depends on engine state.

### UCI Protocol Surface

Review every command currently accepted by `Engine` before reshaping dispatch.
Classify each command in the implementation notes:

- real UCI command: `uci`, `debug`, `isready`, `setoption`, `ucinewgame`,
  `position`, `go`, `stop`, `ponderhit`, `quit`;
- debug console command: `help`, `board`, `eval`, `move`, `moves`, `perft`;
- alias: `exit`;
- empty input/no-op.

For each command, record:

- parsed input shape;
- output shape and destination (`stdout` UCI line vs `stderr` diagnostic);
- whether it is allowed while search is active;
- whether it mutates board, options, TT, or thread/search state.

If a command is only a placeholder, such as current `ponderhit`, either keep it
with explicit documentation and tests or remove/defer it deliberately. Do not
leave placeholder behavior ambiguous.

Protocol output should be reviewed as a separate surface:

- handshake lines: `id`, `option`, `uciok`, `readyok`;
- search lines: `info`, `bestmove`;
- error/status lines: `info string ...`;
- debug/developer diagnostics.

All UCI stdout lines that a GUI may wait on should flush.

### ThreadPool

`ThreadPool` remains the search execution boundary. The cleanup should not move
search implementation into the UCI or engine command parser.

Expected ownership:

- `Engine` asks `ThreadPool` to start, stop, resize, wait, or shut down.
- `ThreadPool` owns worker execution and synchronization.
- `SearchWorker` owns request-local search reporting state.
- UCI output dependencies needed by workers should be narrow and writer-like.

### UCI Config and Engine Options

Separate protocol parsing from option application:

- UCI parsing should produce a parsed option command such as name plus value.
- Engine-side option application should validate lifecycle policy and perform
  side effects.
- Option descriptors should be reusable for `uci` identification output without
  forcing command parsing or callbacks to live in the writer.

Review the supported option model as part of the goal:

- `Hash`: spin option, default/min/max, TT resize side effect;
- `Threads`: spin option, default/min/max, thread-pool resize side effect;
- `Debug`: check/debug mode, output behavior, and whether `debug on/off` should
  share the same validation path as `setoption name Debug value ...`.

The target shape should distinguish:

- option descriptors used for `uci` output;
- current option values;
- parsing and validation of option commands;
- engine-owned side effects after a valid option change.

Do not bury engine side effects inside a stateless protocol writer.

### Position Commands

Prefer a parsed position command with:

- either `startpos` or explicit FEN fields;
- a vector of move tokens or parsed UCI moves.

`Engine` should apply the parsed command to the board and keep legality checks
near board state, not inside a string parser that has no board context.

### UCI Search Options and Go Commands

Prefer parsing raw `go` limits into a UCI command object first. Then derive
runtime `SearchOptions` from the parsed limits and current board state.

This keeps UCI grammar separate from search policy such as clock allocation.

The review must cover all currently supported `go` tokens:

- `depth`;
- `movetime`;
- `nodes`;
- `wtime`;
- `btime`;
- `winc`;
- `binc`;
- `movestogo`.

Preserve current clamping/default behavior unless changing it is intentional and
tested:

- depth clamps to `[1, MAX_SEARCH_DEPTH]`;
- movetime clamps to at least 1 ms;
- nodes and clocks clamp to non-negative values;
- movestogo clamps to at least 1;
- fixed movetime overrides clock-budget allocation;
- missing increments default to zero for clock allocation;
- low calculated budgets preserve the current minimum.

Also explicitly decide and document what happens to unsupported standard UCI
`go` tokens such as `searchmoves`, `ponder`, `infinite`, `mate`, `movestogo`
combinations beyond current behavior, and unknown tokens. This refers to the
`go mate <x>` search limit, not UCI `score mate <y>` output. Do not add those
features unless the cleanup naturally needs them; a documented defer is fine.

## String Handling Standard

Apply these conventions to all touched UCI and engine code. After that boundary
is clean, audit the whole repo and apply the same conventions to low-risk
string APIs and call sites. If changing a family would create broad churn or
change a stable domain contract, document the reason and leave a follow-up
instead of forcing it into this cleanup.

### Formatting

- Use named formatting functions for protocol text, for example
  `uci::format_search_info(...)`, `uci::format_option(...)`, or
  `uci::format_bestmove(...)`.
- Prefer free functions in the owning namespace over ad hoc member methods like
  `score_text()` and `nps_text()` when the text is format-specific.
- Keep core objects format-neutral when possible. A core object should not know
  about UCI unless UCI notation is its canonical identity.
- Keep `Move::str()` only if it remains the canonical coordinate move spelling
  across the codebase. If a broader cleanup renames it, choose one convention and
  update tests deliberately.
- Use `std::format` for assembling known output shapes.
- Avoid manual `operator+` chains for protocol lines except for trivial error
  text that is not repeated.
- Avoid introducing generic `to_string()` overloads for domain objects. Generic
  names hide which wire/debug format is being produced.
- Keep existing generic conversion only when it is already a narrow primitive
  convention, such as square-coordinate spelling. If it stays, document that it
  is a coordinate formatter rather than a general object formatter.
- Prefer `std::formatter<T>` for stable developer-facing debug tables that are
  naturally rendered through `std::format("{}", value)`.
- Prefer explicit named functions for wire formats and domain notations, for
  example UCI move text, FEN text, and UCI search-info text.
- Avoid keeping both `std::formatter<T>` and `T::str()` unless they serve
  different documented audiences. If they are equivalent, choose one.

### Parsing

- Keep raw `std::istream` use at the process input boundary.
- For command parsing, prefer one UCI tokenizer/parser surface over repeated
  local `while (iss >> token)` loops.
- It is acceptable for the first cleanup to implement the parser using
  `std::istringstream` internally if the parser API does not leak stream state
  into `Engine`.
- Parse into typed data before mutating engine state.
- Keep parse errors explicit and testable.
- Keep specialized parsers near their domain when the grammar is not UCI, for
  example FEN parsing. Do not move non-UCI grammar into the UCI parser just to
  centralize every string operation.

### Debug Output

- Separate UCI protocol output from developer diagnostics.
- If debug commands remain, route them through a clearly named debug writer or
  engine console helper, not through a method that makes `uci::Protocol` format
  arbitrary engine objects.
- Avoid adding new `std::format` specializations solely to support generic
  diagnostics unless the formatted representation is stable and broadly useful.

### Tests

- Test formatted UCI lines at the formatter/writer boundary.
- Test parser behavior without constructing a full `Engine` when possible.
- Keep engine tests for integration behavior and lifecycle policy.
- Add or update focused tests for any renamed or standardized domain formatter,
  especially move text, FEN text, and diagnostics.

## Work Plan

### Phase 0: Baseline Review

Read the relevant local files before editing:

- `include/uci.hpp`
- `src/uci.cpp`
- `include/engine.hpp`
- `src/engine.cpp`
- `include/search_options.hpp`
- `src/search_options.cpp`
- `include/threading.hpp`
- `src/threading.cpp`
- `include/search_worker.hpp`
- `src/search_worker.cpp`
- `tests/uci.test.cpp`
- `tests/engine.test.cpp`
- `tests/search_options.test.cpp`
- `tests/threading.test.cpp`
- `tests/search.test.cpp`

Then inspect the reference-engine files listed above.

Deliverable for this phase: a short implementation note identifying the intended
split before code edits begin. The note must include three small matrices:

- command surface: every accepted command, its category, UCI compliance status,
  parser owner, output destination, active-search policy, and state mutations;
- output surface: every UCI output family, compliance status, flush behavior,
  and formatter/writer owner;
- option/config surface: every UCI option, option type, descriptor, value type,
  validation, `uci` output text, engine side effect, and compliance status;
- `go` search-options surface: every supported token, ignored/deferred token,
  current behavior, intended behavior, default/clamp behavior, and runtime
  `SearchOptions` field.

### Phase 1: UCI Writer and Formatting

Reshape the output side first because it has the smallest behavioral surface.

Tasks:

- Move protocol-line construction behind named UCI formatting functions or a
  narrow writer API.
- Cover every protocol output family, not only search output: identity, option
  descriptors, `uciok`, `readyok`, `info`, `info string`, `bestmove`, and debug
  diagnostics.
- Keep flushing behavior for handshake, `info`, and `bestmove` output.
- Remove generic diagnostic formatting from the protocol writer or rename it so
  it is clearly not UCI protocol output.
- Move search-info formatting out of the writer if that keeps the writer
  stateless.
- Keep `SearchWorker` duplicate suppression unchanged.

Expected tests:

- existing UCI formatter/writer tests still pass;
- add tests for any newly public formatter/parser surface;
- engine transcript tests still pass.

Commit boundary suggestion:

- `uci: split protocol writing from formatting`

### Phase 2: UCI Command Parsing

Move command grammar out of `Engine`.

Tasks:

- Introduce a parser that accepts a command line and returns a typed command or
  a parse result with an explicit error.
- Include all currently accepted UCI commands in the parser review, even if some
  handlers remain thin: `uci`, `debug`, `isready`, `setoption`, `ucinewgame`,
  `position`, `go`, `stop`, `ponderhit`, and `quit`.
- Keep debug console commands separate from real UCI commands even if they share
  the same input loop.
- Move `parse_option()` and `parse_position()` out of `Engine`.
- Move `go` token parsing out of `SearchOptions` unless doing so would make the
  first parser commit too large. If deferred, leave Phase 4 as a required
  follow-up before this goal can be marked complete.
- Keep unknown-command and bad-argument reporting compatible with current tests
  unless changing wording is intentional and tests are updated.

Expected tests:

- focused parser tests for `setoption`, `position startpos`, `position fen`,
  `position ... moves`, and representative `go` limits;
- focused parser tests or integration tests for the remaining UCI command names;
- existing engine command tests still pass.

Commit boundary suggestion:

- `uci: parse commands before engine dispatch`

### Phase 3: Engine Orchestration

After parsing and writing boundaries exist, simplify `Engine`.

Tasks:

- Make `Engine` dispatch parsed commands, not raw token streams.
- Centralize commands disallowed during active search.
- Keep board-state mutation helpers small and explicit.
- Keep `ThreadPool` as the only search execution dependency.
- Keep option side effects in `Engine` or a small engine-owned options manager,
  not in the UCI writer.
- Apply parsed config/option commands through one engine-owned path so `Hash`,
  `Threads`, and `Debug` validation and side effects do not drift.
- Keep non-UCI debug commands separate from UCI command dispatch. If they share
  the same input loop, make the distinction explicit in names and tests.

Expected tests:

- engine lifecycle tests for active-search rejection;
- option side-effect tests for TT resize, thread resize, and debug mode if
  those surfaces are exposed testably;
- board position application tests;
- existing thread-pool integration tests.

Commit boundary suggestion:

- `engine: dispatch parsed UCI commands`

### Phase 4: UCI Search Options Split

Do this only if it did not fit naturally in Phase 2.

Tasks:

- Create a parsed `go` limits object owned by the UCI layer.
- Cover the current supported tokens: `depth`, `movetime`, `nodes`, `wtime`,
  `btime`, `winc`, `binc`, and `movestogo`.
- Convert parsed limits plus current board state into runtime `SearchOptions`.
- Keep clock allocation and runtime defaults in search/engine-owned code, not in
  text parsing.
- Keep unknown or unsupported `go` token behavior explicit and tested. If the
  cleanup changes current ignore behavior, call it out as an intentional UCI
  behavior change.

Expected tests:

- existing `SearchOptions` tests either move to the parser or become runtime
  conversion tests;
- fixed-depth, movetime, node-limit, and clock-budget command tests remain
  covered.
- invalid numeric input, out-of-range values, mixed valid/invalid tokens, and
  extra tokens remain covered at the parser or conversion boundary.

Commit boundary suggestion:

- `search: derive search options from parsed go limits`

### Phase 5: Low-Hanging UCI Compatibility

After the protocol, config, and `go` parsing surfaces are explicit, add small
compatibility fixes that do not require new chess/search features.

Tasks:

- Change terminal/no-legal-move UCI output from `bestmove none` to
  `bestmove 0000`, preserving internal null-move representation separately from
  wire formatting.
- Make `setoption` option-name matching case-insensitive.
- Make check-option values case-insensitive for `true`, `false`, `on`, and
  `off`.
- Add `Clear Hash` as a UCI button option if the option model is already being
  reshaped. It should call existing `tt.clear()` and require no value.
- Treat unsupported UCI commands such as `register` as no-op/ignored unless the
  engine has emitted registration-related output, which it should not.
- Make `ponderhit` an explicit no-op while real ponder mode is unsupported;
  avoid protocol-visible chatter for it.
- Revisit unknown UCI command/token behavior. UCI-facing unknowns should be
  ignored or debug-only; local debug-console commands may keep useful diagnostics
  on `stderr`.
- Confirm `isready` flushes `readyok` immediately, including while search is
  active.
- Confirm the `uci` handshake flushes after `uciok`.
- Confirm final selected search info is available before `bestmove`. If
  duplicate suppression skips the final call because the same line was already
  emitted, document why the last emitted line satisfies the GUI-facing contract.
- Preserve and verify existing UCI mate-score output. Mate-range scores should
  still serialize as `score mate <moves>`, including negative mate values when
  the engine is getting mated; do not confuse this with unsupported `go mate`
  search-limit parsing.

Expected tests:

- engine transcript test for terminal/no-legal-move `bestmove 0000`;
- config/parser tests for case-insensitive option names and check values;
- option test for `Clear Hash` if implemented;
- engine transcript tests for `register` no-op and `ponderhit` no-op;
- engine transcript test showing `isready` returns `readyok` while search is
  active;
- protocol/search-info tests for positive and negative `score mate` output;
- existing UCI progress reporting and duplicate-suppression tests still pass.

Commit boundary suggestion:

- `uci: add low-cost protocol compatibility`

### Phase 6: Repo-Wide String Audit

After the UCI and `Engine` boundary is cleaned up, make one repo-wide pass over
string conversion and parsing APIs.

Start with:

```sh
rg -n "to_string\\(|std::format|operator<<|\\.str\\(|score_text|nps_text|string\\(\\) const|std::ostringstream|std::stringstream|std::istringstream" include src tests -g '*.hpp' -g '*.cpp'
```

Classify each hit into one of these buckets:

- protocol/wire format;
- domain notation, such as UCI move text, square text, or FEN;
- debug/developer display;
- parser/tokenizer;
- test-only string plumbing.

Tasks:

- Remove `SearchInfo::score_text()` and `SearchInfo::nps_text()` style helpers
  if UCI-specific free formatting functions make the ownership clearer.
- Decide whether `Move::str()` should stay as the canonical UCI-coordinate move
  formatter or be renamed to a more explicit function. If renamed, keep the
  change mechanical and test-covered.
- Decide whether `to_string(Square)` is acceptable as a primitive coordinate
  formatter. If it stays, avoid adding broader generic `to_string()` functions
  for other domain objects.
- Review `Board::toFEN()` and FEN parsing separately from UCI parsing. FEN is a
  domain notation and should not be hidden inside the UCI handler.
- Review debug surfaces such as board, evaluator, and search-instrumentation
  formatting. Prefer either stable `std::formatter` support or an explicit
  debug-format function, not both unless there is a clear audience split.
- Remove repeated ad hoc string assembly from production code when a named
  formatter or parser can express the same intent.
- Leave tests free to use `std::ostringstream`/`std::istringstream` as harness
  plumbing, but avoid duplicating production parsing logic in tests.

Expected tests:

- existing move, type, FEN, board, evaluator, UCI, engine, and search
  instrumentation tests still pass;
- add focused tests for any public string API rename or behavior change.

Commit boundary suggestion:

- `refactor: standardize engine string formatting`

## Non-Goals

- Do not change move generation, evaluation, search ordering, pruning,
  transposition-table policy, or time management except where needed to preserve
  existing behavior after the interface split.
- Do not copy reference-engine architecture wholesale.
- Do not add UCI features such as real ponder mode, ponder move output,
  searchmoves, `go mate` search-limit support, true infinite-search semantics,
  MultiPV, bound reporting, tablebase hits, CPU load, Chess960, copy protection,
  registration, strength limiting, or broad option families unless required to
  preserve current behavior.
- Do not turn the repo-wide string audit into a behavior-changing rewrite.
  Standardize low-risk cases after the UCI/Engine boundary is clean, and record
  explicit follow-ups for broad or semantic changes.
- Do not rename every existing `str()` method unless the rename is small,
  mechanical, and clearly improves consistency.

## Review Checklist

Before finalizing, review the diff for these risks:

- Does any search worker write directly to a broad protocol object when a narrow
  writer would do?
- Did duplicate suppression accidentally move into a stateless formatter/writer?
- Can `Engine` still be tested without real stdin/stdout plumbing?
- Are command parse errors deterministic and covered?
- Has every currently accepted UCI command been classified and tested at the
  right level?
- Are `Hash`, `Threads`, and `Debug` descriptors, validation, formatting, and
  side effects owned by the intended layer?
- Are UCI `go` search-option tokens parsed before runtime `SearchOptions` are
  derived?
- Is unsupported UCI command/search-option behavior explicit rather than
  accidental?
- Does terminal/no-legal-move output use UCI `bestmove 0000` instead of an
  internal sentinel spelling?
- Are UCI option names and check-option values case-insensitive?
- If `Clear Hash` was added, is it a value-less button option that only calls
  existing TT clear behavior?
- Are unsupported `register` and `ponderhit` commands silent no-ops unless real
  support is added later?
- Does UCI search-info formatting still emit `score mate <moves>` for mate-range
  scores, including negative mate values?
- Are debug commands clearly outside the UCI protocol contract?
- Are `uciok`, `readyok`, `info`, and `bestmove` flushed?
- Did any formatting change affect GUIs or benchmark harnesses unexpectedly?
- Did any parser refactor mutate the board before fully validating the command?

## Suggested Final Validation

Run:

```sh
cmake --build --preset release-dev
ctest --preset release-dev
cmake --build --preset release-stats-dev
ctest --preset release-stats-dev
git diff --check
```

If command parsing or output formatting changed substantially, also run a direct
UCI smoke transcript against the built engine:

```sh
printf 'uci\nisready\nposition startpos\ngo depth 3\nquit\n' | build/release-dev/bin/latrunculi
```

The transcript should include identity/options, `uciok`, `readyok`, progressive
`info depth ...` lines for the search, and one `bestmove`.
