# Multi-Agent Sources — Slice 1

**Feature ID:** `multi-agent-sources`
**Branch:** `feat/multi-agent-sources` (branched from `chore/state-review-cleanup`, PR #1 open)
**Created:** 2026-11-17
**Status:** in progress

## Objective

Make the Agent breakdown show the CLIs the user actually uses. Slice 1 adds a `source`
dimension with a collision-safe dedupe key and two new parsers: omp and Claude.

## Problem / Why

`Storage::defaultSessionsDir()` only reads `~/.pi/agent/sessions` and `syncFromPiSessions`
hardcodes `agent = "pi" | "subagent"` (`src/storage.cpp:79-81, 153-154`). The `agent`
column therefore can never contain `claude`, `omp`, `codex` or `opencode`, so the
breakdown groups only Pi data. Research confirmed real local data for all four agents.

## Scope

- In: schema migration (`source`, composite UNIQUE, sync state), omp parser, Claude parser,
  source registry with opt-in flags, Backend wiring, tests, README notes.
- Out: Codex (zstd dependency), OpenCode (SQLite reader), changes to existing UI layout,
  push/PR/merge.

## Constraints

- **Preserve existing rows.** The DB migration must backfill `source='pi'` and must not
  drop or duplicate history. Test it against an old-format DB, not just a fresh one.
- `message_id` values collide across CLIs (both Claude and OpenCode emit `msg_*`), so
  dedupe must be keyed on `(source, message_id)`.
- Claude cost exists only as a session-level `cost-state` record; prorate per assistant
  message by token share and document it as an estimate. Never fabricate Claude per-line
  cost from the null `costUSD` field.
- Sources must be opt-in/overridable for tests; defaults on for `pi`, `omp`, `claude`.
- Test-first where a runnable deterministic test exists. Runner: `bin/test`.
- RDD native review: skipped per the user's explicit instruction ("ignora rdd") and because
  this runtime cannot provide immutable receipt-review transport. Ordinary verification
  is still mandatory.
- Artifact language: English. Advisory ~400 authored changed lines per task, not a cap.

## Route

- **Chosen route:** delegated direct, one writer per work unit (schema unit, then parser unit).
- **Trigger evidence:** new C++ parser files, `storage.{h,cpp}`, `backend.{h,cpp}`,
  `ai-activity.pro`, tests and README — well beyond a single mechanical file.

## Checklist

- [x] **MAS-1** Schema migration in `Storage::init`: add `source TEXT NOT NULL DEFAULT 'pi'`,
  rebuild `events` with `UNIQUE(source, message_id)`, backfill existing rows with `source='pi'`,
  recreate indexes, and add a `sync_state(source, file_path, mtime, size)` table. Test both
  a fresh DB and a legacy DB with pre-existing rows; prove two different sources can store
  the same `message_id`.
- [x] **MAS-2** Source registry + omp parser: read `~/.omp/agent/sessions/**/*.jsonl`,
  Pi-shaped records with `reasoningTokens` and `cost.total`; emit normalized events with
  `source='omp'`, agent label `omp`, project via `extractProjectFromDirName`, subagent via
  the directory heuristic. Incremental sync keyed on file mtime/size. Tests with synthetic
  fixtures.
- [x] **MAS-3** Claude parser: read `~/.claude/projects/**/*.jsonl` plus `subagents/*.jsonl`;
  `type=assistant` records with `input_tokens`, `output_tokens`,
  `cache_creation_input_tokens`, `cache_read_input_tokens`, `output_tokens_details.thinking_tokens`;
  dedupe key `uuid` (not `message.id`); subagent from `isSidechain`/`agentId`;
  project from `cwd`/slug; session-level `cost-state.totalCostUSD` prorated by token share.
  Tests with synthetic fixtures.
- [x] **MAS-4** Wire parsers into `Storage::syncSource`/Backend refresh with opt-in
  source flags, keep the existing Pi path working, and document the new sources in README.
- [x] **MAS-5** Verify: full `bin/test`, clean `bin/build`, `git diff --check`, structural
  readback, and work-unit commits.
- [ ] **MAS-6** (next slice) OpenCode source: read-only SQLite parser for
  `~/.local/share/opencode/opencode.db`. Use `session.cost`, `tokens_input`, `tokens_output`,
  `tokens_reasoning`, `tokens_cache_read`, `tokens_cache_write`, `model`, `agent`, `time_created`;
  parse `message.data` JSON for per-message granularity. Open with `mode=ro` because of WAL.
  `session.parent_id` is currently empty, so subagent detection is unverified.
- [ ] **MAS-7** (later slice) Codex source: zstd-compressed JSONL under
  `~/.codex/sessions/**/*.jsonl.zst`. Needs a zstd dependency decision (link `libzstd` vs shell
  out to `zstd -dc`). Token fields exist in `token_usage_record.payload.usage`; **no cost data
  exists** — display "not reported", never fabricate. Subagent signal unverified.
- [ ] **MAS-8** (deferred UI bug, GitHub issue #3) Agents breakdown shows only one agent:
  `Main.qml:694-697` caps the panel at `implicitHeight: theme.dp(136)` /
  `Layout.maximumHeight: theme.dp(160)` while Models uses `Layout.fillHeight: true`; rows are
  inside a `Flickable` (`BreakdownList.qml:114-117`), so the rest require inner scrolling.
  Issue: https://github.com/Juanstudy/ai-activity/issues/3

## Acceptance criteria

- Legacy DB opens, keeps its rows, and reports `source='pi'` for all of them.
- Same `message_id` from `pi` and `omp` (or `claude`) coexists as two rows.
- A synthetic omp session produces correct tokens/cost/project and `source=omp`.
- A synthetic Claude session produces correct tokens, `source=claude`, subagent flag, and
  a prorated cost that sums to the session's `totalCostUSD` (within rounding).
- Unchanged files are skipped by the incremental sync; changed files are re-parsed.
- Default Pi tests still pass; no source is read when disabled.

## Applicable checks

- `bin/test` (QtTest offscreen), `bin/build` (qmake6 + make), `git diff --check`.
- RDD: not run (user instruction + runtime limitation).

## Progress / Verification evidence

- 2026-11-17 Research confirmed all four sources have local data; omp and Claude are the
  no-dependency slice; OpenCode SQLite and Codex zstd deferred.
- 2026-11-17 Branch `feat/multi-agent-sources` created from `chore/state-review-cleanup`
  (PR #1 is the base branch; the eventual PR for this feature should stack on it).
- 2026-11-17 **MAS-1 GREEN.** `bin/test`: 15 passed, 0 failed (baseline was 13 passed).
  RED observed first: `testLegacyDatabaseMigration` failed with `no such column: source`
  and `testSourceScopedMessageId` failed inserting the same message_id under `pi`.
  Both pass after the migration; all 13 pre-existing tests remain green. `bin/build`:
  clean (no warnings). `git diff --check`: clean.
- Migration strategy: transactional `ALTER TABLE events RENAME TO events_legacy`,
  rebuild `events` with `UNIQUE(source, message_id)`, copy every legacy row with
  `source='pi'`, drop `events_legacy`, recreate the four indexes. Fresh DBs take the
  new schema directly via `CREATE TABLE IF NOT EXISTS`; if `source` exists without the
  composite UNIQUE the code logs a clear warning and leaves rows untouched.
- New `sync_state(source, file_path, mtime, size)` table plus private
  `fileSyncState`/`setFileSyncState` helpers for later incremental parsers.
- Pi insert path now binds `source='pi'`.
- 2026-11-17 **MAS-2 GREEN.** RED observed first: `bin/test` could not compile —
  `No rule to make target '../src/parsers/pishapedparser.cpp'` — because the parser API did not
  exist. GREEN after implementation: `testOmpParser` and `testIncrementalSync` pass. Design:
  `UsageParser` returns `NormalizedEvent` rows plus `FileStamp` records; `PiShapedParser` shares
  the directory walk between `PiParser` and `OmpParser`, which overrides `reasoningTokens`,
  root-level `modelID`/`providerID`, and the `omp` agent label. `SourceRegistry` owns one slot
  per source (pi/omp/claude on; codex/opencode off), each flag, root and parser overridable.
- 2026-11-17 **MAS-3 GREEN.** `testClaudeParser` passes with a synthetic session: uuid dedupe
  (a duplicate uuid with a different `message.id` is dropped), cache tokens folded into
  tokensIn, thinking tokens separated, subagent from `isSidechain`/`agentId`, project from `cwd`,
  nested `subagents/*.jsonl` read, ISO-8601 with Z/ms normalized, and prorated costs summing back
  to `totalCostUSD` within 1e-9. Files with no `cost-state` keep cost 0.
- 2026-11-17 **MAS-4 GREEN.** `Storage::syncSource` skips unchanged (mtime, size) files via
  `sync_state` and writes stamps only after a committed insert. `Backend::refresh()` now calls
  `SourceRegistry::syncAll()`; default construction syncs Pi + omp + Claude, while a custom
  sessions dir restricts the registry to Pi rooted there so existing tests never read the real
  `~/.omp`/`~/.claude`. `testBackendSourceWiring` proves both paths with a redirected HOME.
  README gained a concise "Data sources" table noting opt-in/overridable sources and prorated
  Claude costs. `syncFromPiSessions` is untouched and its tests still pass.
- 2026-11-17 Final verification: `bin/test` 20 passed, 0 failed (baseline 15 passed — five new
  tests: omp, Claude, incremental, disabled sources, backend wiring); `bin/build` succeeds with
  no warnings after adding `INCLUDEPATH += src` for the new `src/parsers/` subdir;
  `git diff --check` clean. RDD not run per user instruction. No commits made (parent commits).

## Parent verification and delivery (2026-11-17)

- Work unit `ff2413a feat(db): add source dimension and source-scoped dedupe` (MAS-1).
- Work unit `09f9815 feat(sources): add omp and Claude transcript parsers` (MAS-2..4).
- Parent re-ran `bin/test`: 20 passed, 0 failed, 0 skipped, 0 blacklisted (147ms), exit 0.
- Structural readback confirmed: Claude proration is a documented token-share estimate from
  `cost-state.totalCostUSD`; `Backend` disables omp/Claude whenever a custom sessions dir is
  supplied, so tests never read real home-directory logs; the README "Data sources" table
  matches the implemented default flags and deferred sources.
- RDD not run: explicitly skipped by the user and unavailable in this runtime.

## Deferred work (resume here next session)

Slice 1 is complete and verified. The two unchecked items above are the pending roadmap:

1. **MAS-6 — OpenCode.** Richest remaining source. Evidence gathered on 2026-11-17:
   `~/.local/share/opencode/opencode.db` (42 MB, WAL) has a `session` table with `cost`,
   `tokens_input`, `tokens_output`, `tokens_reasoning`, `tokens_cache_read`,
   `tokens_cache_write`, `model`, `agent`, `time_created`; `message.data` holds per-message
   tokens/cost. Read-only connection: `sqlite3 "file:$DB?mode=ro"`. Open with a new parser behind
   the existing `UsageParser`/`SourceRegistry` interface, add the `opencode` flag (currently
   defined but disabled), synthetic fixtures, and a README row update.
2. **MAS-7 — Codex.** `~/.codex/sessions/YYYY/MM/DD/rollout-*.jsonl.zst`; `zstd` CLI is present
   on this machine. `token_usage_record.payload.usage` carries all token counters; there is no
   cost field anywhere, so keep `costUsd = 0` and label it "not reported" in the UI/docs. Decide
   the zstd dependency (`libzstd` link vs `zstd -dc` subprocess) before implementing.

Research notes and per-source field maps are persisted in Engram under topic
`architecture/agent-sources`. The full research handoff is reachable from that project memory.

## Deferred UI bug (2026-11-17)

- GitHub issue #3: https://github.com/Juanstudy/ai-activity/issues/3 — Agents breakdown is
  height-capped and requires inner scrolling to see more than one agent. Verified in code at
  `src/Main.qml:694-697` and `src/components/BreakdownList.qml:114-117`. Intentionally deferred
  so the source work can land first; tracked as MAS-8 above.

## Delivery

User authorized publishing on 2026-11-17. This branch stacks on PR #1
(`chore/state-review-cleanup`); PR #1 must merge first (or this PR must be retargeted to main)
before these commits land on main. Push, PR creation and merge remain explicit user decisions
except where the user has already authorized them.
