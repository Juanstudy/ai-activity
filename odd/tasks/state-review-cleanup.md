# State Review Cleanup

**Feature ID:** `state-review-cleanup`
**Branch:** `chore/state-review-cleanup`
**Created:** 2026-11-16
**Status:** in progress

## Objective

Resolve every verified finding from the read-only state review of `ai-activity` so the
project is safe to keep building on and the README matches the shipped behavior.

## Problem / Why

A state review (2026-11-16) confirmed several concrete defects: a `.gitignore` rule that
silently drops every new source file, a fully orphaned QML component, non-hermetic unit
tests that hit a live currency API, a hardcoded OS username in production logic, ignored
database/cache write errors, minor dead/fragile code, and documentation drift. Tests were
green (11/11) but did not catch any of them.

## Scope

- In scope: the 8 verified findings (SR-1..SR-7 plus verification), their tests and docs.
- Out of scope: new features, UI redesign, refactors beyond the cited lines, dependency
  upgrades, push/PR/merge.

## Constraints

- Read-only review is over; this document authorizes mutation on the feature branch only.
- Preserve the existing streak/breakdown work (committed as `6fe5496` before this work began).
- Tests must stay green (`bin/test`, QtTest offscreen, currently 11/11).
- Artifact language: English (code, comments, UI strings, docs).
- Default test-first policy: when a runnable deterministic test and clear expected outcome
  exist, observe RED first, then GREEN, then refactor. Runner: `bin/test`.
- Work-unit commits on this branch are part of this authorized implementation; push, PR
  and merge remain the user's decisions.
- Advisory ~400 authored changed lines per task; split only if the correct solution
  naturally exceeds it (no cosmetic line-count trimming).

## Route

- **Chosen route:** delegated direct (one bounded writer).
- **Trigger evidence:** 10 non-trivial files across C++ (`storage`, `trmclient`, `backend`),
  QML (`ActivityHeatmap`, `MetricCard`, `resources.qrc`), tests, `.gitignore` and `README.md`.
  Reading that prepares the write belongs to the same writer.

## Checklist

- [x] **SR-1** Remove the bare `src/` entry from `.gitignore`; verify a new source file under
  `src/` is no longer reported by `git check-ignore`.
- [x] **SR-2** Remove the dead `MetricCard.qml` component: delete the file and its
  `resources.qrc` entry. No references may remain.
- [x] **SR-3** Make TRM tests hermetic: add an opt-out for `TrmClient`'s automatic sync,
  thread it through `Backend`, and add a test that constructs them without issuing a
  network request. Keep the existing 4200.0 cache-rate assertion.
- [x] **SR-4** Replace the hardcoded `home-juan` prefix in
  `Storage::extractProjectFromDirName` with a value derived from `QDir::homePath()`;
  keep all existing extraction results and add a portable test.
- [x] **SR-5** Check `transaction()`/`commit()` results in `Storage::syncFromPiSessions`
  (warn and roll back on failure) and report `saveCache` write failures in `TrmClient`.
- [x] **SR-6** Remove the unreachable duplicate-date branch and redundant `std::max` in
  `getStreak`; include `<algorithm>` explicitly; fix the `undefined` assignment to
  `anchors.horizontalCenter` in `ActivityHeatmap.qml`.
- [x] **SR-7** Fix README drift: document the Ctrl+/-/0 zoom shortcuts and change the
  "52-week grid" claim to 53 weeks to match `weeksCount`.
- [x] **SR-8** Verify: `bin/test` 11/11 (or more, all green), clean `bin/build`, structural
  readback of the diff, and one RDD assessment per work-unit commit.

## Acceptance criteria

- `git check-ignore -v src/__probe.qml` returns no match.
- No `MetricCard` reference remains in `src/` or `src/resources.qrc`.
- No test constructs a `TrmClient`/`Backend` that auto-fires an HTTPS request.
- No literal `home-juan` remains in `src/`.
- All write/transaction failures are logged (and DB work rolled back where applicable).
- README matches the implemented shortcuts and heatmap week count.
- Full test suite green; no new warnings attributable to these changes.

## Applicable checks

- `bin/test` — full QtTest offscreen suite.
- `bin/build` — qmake6 + make release build.
- `git diff --check` and `git status --short`.
- RDD: `gentle-ai review assess --cwd <repo> --agent opencode --base-ref <boundary> --committed-only --json`.

## Progress / Verification evidence

- 2026-11-16 Baseline: `bin/test` → 11 passed, 0 failed, 0 skipped (198ms).
- 2026-11-16 Branch `chore/state-review-cleanup` created; pre-existing WIP preserved as
  `6fe5496 feat: add streak tracking and breakdown dashboard redesign`.
- 2026-11-16 SR-2: deleted `src/components/MetricCard.qml` and removed its qrc entry;
  `grep -R "MetricCard" src/ README.md` → no matches (exit 1).
- 2026-11-16 SR-1: removed bare `src/` rule from `.gitignore` line 11;
  `git check-ignore -v src/__ignore_probe.qml` → no output, exit 1 (no rule matches).
- 2026-11-16 SR-3 RED: tests updated to pass `autoSync=false` through `TrmClient`/`Backend`
  plus new `testTrmAutoSyncOptOut`; `bin/test` failed to compile (candidate expected 3 args, 4 provided).
- 2026-11-16 SR-3 GREEN: added last-parameter `bool autoSync = true` to `TrmClient` (stored as
  `m_autoSync`) and `bool autoSyncTrm = true` to `Backend`, threaded to `m_trm`; constructor
  `syncRate()` call gated on the opt-out while `Q_INVOKABLE syncRate()` stays callable.
  `bin/test` → 12 passed, 0 failed, 0 skipped (168ms), including `testTrmAutoSyncOptOut`
  and the preserved 4200.0 assertion in `testTrmConversion`. SR-3 checked off.
- 2026-11-16 SR-4 RED: new `testProjectNameExtractionPortable` overrides `HOME` to
  `/home/portableuser`; original code returned `"home-portableuser"` where `"Home"` was expected
  (13 total, 1 failed).
- 2026-11-16 SR-4 GREEN: `Storage::extractProjectFromDirName` now derives the username from
  `QFileInfo(QDir::homePath()).fileName()` for both the exact-home check and the `home-<user>-`
  prefix, keeping the generic `home-` fallback and agent detection. `bin/test` → 13 passed,
  0 failed, 0 skipped (134ms); existing `--home-juan-*` extraction results unchanged. SR-4 checked off.
- 2026-11-16 SR-5: `syncFromPiSessions` now warns and returns 0 when `transaction()` fails,
  and warns, rolls back and returns 0 when `commit()` fails; `TrmClient::saveCache` warns on
  `open()` failure and on short/failed `write()` (no warning when a cache simply does not exist
  yet, since WriteOnly creates it). `bin/test` → 13 passed, 0 failed, 0 skipped (128ms),
  no compiler warnings. SR-5 checked off.
- 2026-11-16 SR-6: added explicit `<algorithm>` include, dropped the unreachable duplicate-date
  branch in `getStreak` (`SELECT DISTINCT` plus an in-loop guard already dedupe) and the redundant
  `std::max(longest, currentStreak)`; `ActivityHeatmap.qml` now binds the grid `Row`'s `x` to a
  centered offset when it fits (0 otherwise) instead of assigning `undefined` to
  `anchors.horizontalCenter`. `bin/test` → 13 passed, 0 failed, 0 skipped (149ms);
  `QT_FORCE_STDERR_LOGGING=1 ... testQmlLoadsCleanly` produced no anchor/undefined warnings.
  SR-6 checked off.
- 2026-11-16 SR-7: README now says 53-week grid (matching `weeksCount: 53`) and lists
  `Ctrl++`/`Ctrl+=` zoom in, `Ctrl+-` zoom out and `Ctrl+0` reset, copied from the `Shortcut`
  sequences in `Main.qml`. `bin/test` → 13 passed, 0 failed, 0 skipped (124ms). SR-7 checked off.
- 2026-11-16 SR-3..SR-7 mandatory verification (in order): `bin/test` → all 13 test slots PASS,
  `Totals: 13 passed, 0 failed, 0 skipped, 0 blacklisted, 130ms`; `bin/build` → success,
  `Built /home/juan/Projects/ai-activity/build/ai-activity`, no warnings; `git diff --check` →
  exit 0, no output; `grep -R "home-juan" src/ README.md` → no matches, exit 1. SR-8 intentionally
  left unchecked for the parent's final verification.

## Parent verification (2026-11-16)

- Work-unit commit `7338f94 chore: stop ignoring new sources and remove dead MetricCard component` (SR-1, SR-2).
- Parent spot check: re-ran `bin/test` → all 13 slots PASS, `Totals: 13 passed, 0 failed, 0 skipped, 0 blacklisted, 158ms` (exit 0); `git diff --check` → exit 0; `grep -R "home-juan\|MetricCard" src/ README.md` → no matches. Structural diff readback confirmed the in-loop `longest` update is intact, README keys match `Main.qml:58-70`, and the heatmap `x` binding uses the `gridFlickable` viewport width.
- RDD assessment: `gentle-ai review assess ... --base-ref 6fe5496 --committed-only --json` returned `risk: high` with reason `unassessable` — the active runtime is OpenCode v2.3.0 under the T3 Code relay, not an eligible immutable receipt-review transport. Canonical preflight returned `gentle-ai.review-integration.failure/v2`, code `immutable_review_transport_unsupported`, `mutation_outcome: not_started`, `retry_safe: false`, `next_action: stop`. No review authority created and no PASS invented. This is an environment limitation, not a code-risk finding. RDD mode was NOT toggled; disabling it is the user's decision.

## Next step

Feature implementation and ordinary verification are complete. Pending user decision only: RDD native review is unavailable in this runtime; the user may clone-locally disable RDD (`gentle-ai review mode disable --scope clone --cwd <repo>`) or run the review from an eligible runtime. Push / PR / merge remain the user's decisions.
