# ai-activity

Desktop analytics app for Omarchy to monitor AI usage on your laptop across time ranges,
cost in USD and COP, agents, projects, models, thinking tokens, and a GitHub-style activity heatmap.

## Features

- **Time Ranges:** 5h, 12h, 24h, 7d, 30d, 6m, 1y, and All Time.
- **Costs:** Automatic TRM sync (USD to COP) with local caching and offline fallback.
- **Activity Heatmap:** 53-week calendar grid (Mon–Sun) styled with Omarchy's live theme accent.
- **Breakdown:** By Project, Model, and Agent/Subagent with input/output and thinking tokens.
- **Desktop Native:** Follows Omarchy theme colors and Hyprland's window rounding in real-time.

## Data sources

Local CLI transcripts are imported through a source registry. Every source has an enable flag and a
configurable root path, so sources are opt-in and overridable; the shipped defaults enable:

| Source | Default root | Status |
|--------|--------------|--------|
| Pi | `~/.pi/agent/sessions` | enabled |
| omp | `~/.omp/agent/sessions` | enabled |
| Claude | `~/.claude/projects` (including `subagents/`) | enabled |
| OpenCode | `~/.local/share/opencode/opencode.db` | enabled |
| Codex | — | disabled, parser not implemented yet |

Only files whose size or modification time changed are re-parsed. Claude stores cost once per
session, so per-message Claude costs are **prorated estimates** from the session total by token share.

## Build and Install

```sh
bin/build    # compile binary in build/ai-activity
bin/test     # run offscreen test suite
bin/install  # build and install Arch package via makepkg
```

## Keyboard Shortcuts

- `1` - `8`: Switch time range (5h, 12h, 24h, 7d, 30d, 6m, 1y, All)
- `R`: Refresh data & reload session logs
- `?`: Toggle shortcuts overlay
- `Ctrl++` or `Ctrl+=`: Zoom in UI scale
- `Ctrl+-`: Zoom out UI scale
- `Ctrl+0`: Reset UI scale to default (1.18)
- `Q`: Quit
