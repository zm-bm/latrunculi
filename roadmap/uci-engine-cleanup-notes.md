# UCI and Engine Cleanup Implementation Notes

These notes capture the Phase 0 review for `roadmap/uci-engine-cleanup.md`.
They are intentionally factual: current behavior, protocol status, and the
intended split before code reshaping starts.

Protocol reference reviewed:

- DOBRO gist, official UCI specification text:
  `https://gist.github.com/DOBRO/2592c6dad754ba67e6dcaec8c90165bf`

Local reference engines reviewed:

- Stockfish: `src/uci.cpp`, `src/uci.h`, `src/ucioption.cpp`,
  `src/engine.cpp`, `src/engine.h`
- Ethereal: `src/uci.h`
- Minic: `Source/uci.cpp`, `Source/uci.hpp`

## Intended Split

- Keep `Engine` as the coordinator for board state, option side effects,
  lifecycle policy, and `ThreadPool` ownership.
- Add or reshape a UCI-facing parser/writer boundary so raw command text and
  UCI line formatting stop living in `Engine`.
- Keep `SearchWorker` responsible for search-progress duplicate suppression.
  The UCI writer should remain stateless.
- Split UCI option descriptors/parsing from engine-owned option side effects.
- Parse `go` grammar into a UCI limits object before deriving runtime
  `SearchOptions`.

## Reference Review

Useful ideas:

- Stockfish keeps a UCI-facing loop/adapter that parses command text, formats
  UCI output, and delegates stateful work to `Engine`.
- Stockfish keeps option descriptors and option values in an option model,
  including case-insensitive option lookup.
- Stockfish keeps search update callbacks narrow: search reports structured
  info and UCI formatting happens at the boundary.
- Minic and Ethereal show the same broad split at smaller scale: UCI functions
  parse command text and pass work to engine/search globals, while search still
  owns its own runtime state.

Not copied:

- Stockfish's large callback and option system is too broad for this codebase.
- Minic's global state and protocol-visible unsupported-token chatter should
  not be copied.
- Ethereal's C-style command surface is useful as a separation example, not as
  a naming or ownership model.

Behavior differences to keep unless explicitly changed:

- Debug console commands remain available for local development.
- Unsupported advanced UCI features stay deferred instead of being stubbed as
  fake support.
- Search progress output remains main-worker-only and duplicate-suppressed in
  search lifecycle code.

## Protocol Rule Review

| Rule | Current Status | Intended Status |
|---|---|---|
| stdin/stdout newline text commands | Supported | Keep |
| arbitrary whitespace between tokens | Mostly supported by stream extraction | Keep and cover in parser tests |
| process input while searching | Partially supported; command loop can read, but some commands report errors | Keep active-search policy explicit |
| unknown commands/tokens ignored | Not supported for commands; unknown command emits `info string` | Low-hanging UCI compatibility fix |
| wrong-state commands ignored | Mixed; `stop` idle is harmless, `setoption`/`ucinewgame` emit errors while searching | Make policy explicit per command |
| UCI null move as `0000` | Not supported; no-legal-move emits `bestmove none` | Low-hanging UCI compatibility fix |
| `uci` id/options/`uciok` | Supported | Keep and flush after `uciok` |
| `isready` immediate `readyok` while searching | Probably supported | Preserve and add transcript test |
| `debug on/off` any time | Supported through `Config`, but invalid values surface as `info string` errors | Parse and validate in UCI layer; keep side effect in engine/config owner |
| `setoption` case-insensitive names/values | Not supported | Low-hanging UCI compatibility fix |
| button options do not require value | Not supported; every setoption currently requires value | Add only if `Clear Hash` is implemented |
| `position startpos`/`fen` plus moves | Supported with engine-owned parser | Move parsing to UCI layer, application stays engine-owned |
| `go` documented token family | Current subset supported by `SearchOptions`; advanced tokens ignored accidentally | Parse explicitly; defer unsupported features |
| every completed `go` emits `bestmove` | Supported | Keep |
| final info before `bestmove` | Supported through `SearchWorker::report_final_result` unless duplicate suppressed | Keep and document duplicate-suppression behavior |
| mate-range score output | Supported for positive mate scores; negative coverage needs confirmation | Add positive and negative tests |
| option output type rules | Spin/check supported | Keep; add button only for `Clear Hash` |

## Command Surface Matrix

| Command | Category | Compliance Status | Parser Owner Now | Output Destination | Active Search Policy Now | Mutations |
|---|---|---|---|---|---|---|
| empty line | no-op | Supported | `Engine::execute` | none | Allowed | none |
| `uci` | real UCI | Supported | `Engine::execute` | stdout UCI | Allowed | none |
| `debug` | real UCI | Partial | `Engine::set_debug` | errors via stdout `info string` | Allowed | `config.debug` |
| `isready` | real UCI | Supported | `Engine::is_ready` | stdout UCI | Allowed | none |
| `setoption` | real UCI | Partial | `Engine::parse_option` | errors via stdout `info string` | Rejected while searching | config, TT resize, thread resize |
| `ucinewgame` | real UCI | Supported with stricter search rejection | `Engine::new_game` | errors via stdout `info string` | Rejected while searching | clears TT |
| `position` | real UCI | Supported with strict invalid-move errors | `Engine::parse_position` | errors via stdout `info string` | Allowed | board/history |
| `go` | real UCI | Partial | `SearchOptions` constructor | stdout UCI/search output | Duplicate search emits `info string` | starts `ThreadPool` |
| `stop` | real UCI | Supported | `Engine::stop` | search later emits bestmove if active | Allowed; idle no-op | stop flags |
| `ponderhit` | unsupported placeholder | Not compliant; emits protocol-visible chatter | `Engine::ponder_hit` | stdout `info string` | Allowed | none |
| `quit` | real UCI | Supported | `Engine::quit` | none directly | Allowed | shuts down pool |
| `register` | unsupported UCI | Not accepted; unknown command error | none | stdout `info string` | Allowed as unknown | none |
| `help` | debug console | Supported | `Engine::help` | stderr | Allowed | none |
| `board` | debug console | Supported | `Engine::display_board` | stderr | Allowed | none |
| `eval` | debug console | Supported | `Engine::evaluate` | stderr | Allowed | none |
| `move` | debug console | Supported | `Engine::move` | stdout `info string` for invalid move | Allowed | board/history |
| `moves` | debug console | Supported | `Engine::moves` | stderr | Allowed | none |
| `perft` | debug console | Supported | `Engine::perft` | stderr | Allowed | none |
| `exit` | alias | Supported | `Engine::quit` | none | Allowed | shuts down pool |
| unknown command | UCI/debug boundary gap | Not compliant for UCI | `Engine::execute` | stdout `info string` | Allowed | none |

## Output Surface Matrix

| Output | Current Owner | Compliance Status | Flush Behavior | Intended Owner |
|---|---|---|---|---|
| `id name`/`id author` | `uci::Protocol::identify` | Supported | flush after `uciok` not explicit | UCI writer plus formatters |
| `option` spin/check lines | `uci::Protocol::identify` and private `format_option` | Supported for existing options | flush after `uciok` not explicit | option descriptor formatter |
| `uciok` | `uci::Protocol::identify` | Supported | not explicit | UCI writer |
| `readyok` | `uci::Protocol::ready` | Supported | not explicit | UCI writer |
| `info depth ...` | `SearchWorker` calls `Protocol::info(SearchInfo)` | Supported | explicit flush | search formatter plus writer |
| `info string ...` | `Protocol::info(std::string)` | Overused for errors/unknowns | explicit flush | UCI writer for real debug info; stderr for local diagnostics |
| `bestmove ...` | `SearchWorker` calls `Protocol::bestmove` | Mostly supported; null move spelling wrong | explicit flush | UCI writer |
| debug help/board/eval/moves/perft | `Protocol::help`/`diagnostic_output`/board perft | Not UCI protocol | stderr except invalid move uses stdout | debug console writer or engine-owned diagnostics |

## Option/Config Surface Matrix

| Option | Type | Descriptor | Value Type | Validation | UCI Output | Side Effect | Status |
|---|---|---|---|---|---|---|---|
| `Hash` | spin | default `DEFAULT_HASH`, min 1, max 2048 | `int` MB | `SpinOption::set`, case-sensitive name | `option name Hash type spin ...` | `tt.resize(value)` callback | Supported, ownership mixed |
| `Threads` | spin | default `DEFAULT_THREADS`, min 1, max 64 | `int` count | `SpinOption::set`, case-sensitive name | `option name Threads type spin ...` | `thread_pool.resize(value)` callback | Supported, ownership mixed |
| `Debug` | check | default false | `bool` | `CheckOption::set`, lowercase only | `option name Debug type check default false` | toggles `config.debug` | Partial |
| `Clear Hash` | button | none | no value | not implemented | not emitted | should call `tt.clear()` | Low-hanging optional fix |

## Go Search-Options Surface Matrix

| Token | Current Behavior | Intended Behavior | Default/Clamp | Runtime Field |
|---|---|---|---|---|
| `depth` | parsed by `SearchOptions(std::istringstream&)` | UCI parsed limits then derive runtime option | `[1, MAX_SEARCH_DEPTH]` | `depth` |
| `movetime` | parsed | UCI parsed limits then derive runtime option | min 1 ms; overrides clocks | `movetime` |
| `nodes` | parsed | UCI parsed limits then derive runtime option | min 0 | `nodes` |
| `wtime` | parsed | UCI parsed limits then derive runtime option | min 0 | `wtime` |
| `btime` | parsed | UCI parsed limits then derive runtime option | min 0 | `btime` |
| `winc` | parsed | UCI parsed limits then derive runtime option | min 0; missing increment becomes 0 in budget | `winc` |
| `binc` | parsed | UCI parsed limits then derive runtime option | min 0; missing increment becomes 0 in budget | `binc` |
| `movestogo` | parsed | UCI parsed limits then derive runtime option | min 1; default budget divisor 30 | `movestogo` |
| `searchmoves` | ignored accidentally | explicitly deferred | none | none |
| `ponder` | ignored accidentally | explicitly deferred | none | none |
| `infinite` | ignored accidentally, effectively max depth | explicitly deferred until true infinite semantics exist | none | none |
| `mate` | ignored accidentally | explicitly deferred; not confused with `score mate` | none | none |
| unknown token | ignored after stream recovery | explicitly ignored for UCI compatibility | none | none |

## First Implementation Target

Start with the output side:

- expose named UCI line formatters for `option`, `search info`, and `bestmove`;
- make `Protocol` a narrow writer around those formatters;
- keep config values and side effects out of the writer beyond reading
  descriptors needed for `uci` output;
- keep search-progress duplicate suppression in `SearchWorker`;
- add tests around any newly public formatter API, especially `bestmove 0000`
  once null move wire formatting is fixed.

## String Audit Result

Audit command run:

```sh
rg -n "to_string\\(|std::format|operator<<|\\.str\\(|score_text|nps_text|string\\(\\) const|std::ostringstream|std::stringstream|std::istringstream" include src tests -g '*.hpp' -g '*.cpp'
```

Low-risk cleanup applied:

- `SearchInfo::score_text()` and `SearchInfo::nps_text()` were removed.
  UCI score and NPS text are now private UCI formatter helpers because that
  spelling is protocol-specific.

Remaining string families:

| Family | Classification | Decision |
|---|---|---|
| `Move::str()` | domain notation; canonical coordinate move spelling | Keep for now. It is widely used for move matching, tests, debug output, and UCI move text. A rename would be broad and mechanical but not required for this cleanup. |
| `to_string(Square)` | primitive coordinate formatter | Keep as the narrow square-coordinate convention. Do not add broader generic `to_string()` overloads for richer domain objects. |
| `Board::toFEN()` and FEN parsing streams | domain notation/parser | Keep near board/FEN code. FEN is not owned by the UCI handler even though UCI `position fen` transports it. |
| `std::formatter<Board>`, `EvaluatorDebug`, `SearchInstrumentation` | debug/developer display | Keep as stable debug formatting surfaces routed through the debug writer. |
| `std::ostringstream`/`std::istringstream` in tests | test harness plumbing | Keep. These are stream harnesses, not production parser ownership. |
| `std::istringstream` in `src/uci.cpp` | UCI parser boundary | Keep. The parser may use streams internally as long as stream state does not leak into `Engine`. |
| `std::istringstream` in `src/engine.cpp` for `move`/`perft` arguments | debug-console parser boundary | Keep until debug-console commands are split into a separate typed parser. They are not UCI protocol parsing. |

Follow-ups left explicit:

- If move text is renamed later, do it mechanically across the repo with focused
  move/text tests.
- If debug-console command parsing grows, split debug-console command argument
  parsing out of `Engine` the same way UCI command parsing was split.
- If board/FEN string APIs are revisited, keep them as domain notation APIs,
  not UCI formatter APIs.
