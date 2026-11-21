# T3 Source Support

**Feature ID:** `t3-source`
**Branch:** `feat/t3-source`
**Status:** in progress

## Specs

- **S1**: "pues quiero agregarle soporte a t3" — Add support for T3 Code / Antigravity as a data source in ai-activity.
- **S2**: Parse Antigravity `transcript.jsonl` files under `~/.t3/userdata/providers/antigravity/.../brain/<conv_id>/.system_generated/logs/transcript.jsonl`.
- **S3**: Extract `PLANNER_RESPONSE` events: `model`, `input_tokens`, `output_tokens`, `cache_read_tokens`, timestamp from `created_at` (ISO 8601), and unique `messageId` per step.
- **S4**: Resolve project name by cross-referencing `statev2.sqlite` (`orchestration_v2_projection_provider_threads` -> `orchestration_v2_projection_threads` -> `projection_projects`), converting workspace_root to relative path where possible, with fallback to directory context or "No project" when SQLite is absent.
- **S5**: Register source `t3` with root path `~/.t3/userdata` (enabled by default) in `SourceRegistry`, update `README.md`, and support incremental sync via `sync_state`.
- **S6**: Comprehensive test suite in `tests/ai-activity_tests.cpp` covering parsing, project resolution, incremental sync, and hermetic execution without real `~/.t3`.

## Tasks

- [x] **T3-1**: Implement `T3Parser` (`src/parsers/t3parser.h`, `src/parsers/t3parser.cpp`) implementing `UsageParser` (S1, S2, S3, S4, S5). Route: inline. Commit: `60d1499`.
- [x] **T3-2**: Integrate `T3Parser` into `SourceRegistry`, `ai-activity.pro`, and document in `README.md` (S1, S5). Route: inline. Commit: `60d1499`.
- [x] **T3-3**: Add unit tests in `tests/ai-activity_tests.cpp` with synthetic `transcript.jsonl` and SQLite fixtures (S6). Route: inline. Commit: `60d1499`.
- [x] **T3-4**: Verification with `bin/test`, `bin/build`, and git check (S1-S6). Route: inline. Commit: `60d1499`.

## Verification Evidence

- 2026-11-21 `bin/test`: 22 passed, 0 failed, 0 skipped in 529ms.
- 2026-11-21 `bin/build`: Clean compilation of `/home/juan/Projects/ai-activity/build/ai-activity`.
- 2026-11-21 `git diff --check`: Clean (no whitespace or conflict marker errors).

## Log

- **L1**: 2026-11-21 User requested: "pues quiero agregarle soporte a t3"
- **L2**: 2026-11-21 Architecture exploration: verified `~/.t3/userdata/providers/antigravity/.../brain/<conv_id>/.system_generated/logs/transcript.jsonl` for tokens and `~/.t3/userdata/statev2.sqlite` for deterministic conv_id -> project mapping.
- **L3**: 2026-11-21 Implementation: T3Parser implemented with QDir::Hidden directory traversal, SQLite read-only mapping, SourceRegistry registration, hermetic test support, and full unit test coverage.
